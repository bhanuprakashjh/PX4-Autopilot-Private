/****************************************************************************
 *
 *   Copyright (c) 2024 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/**
 * @file hrt.c
 *
 * High-resolution timer for SAMV7 using TC0 (Timer/Counter 0)
 *
 * FIXED VERSION with all 3 critical bugs resolved:
 *   1. Time units conversion (ticks → microseconds)
 *   2. Overflow interrupt handling (counter wraparound)
 *   3. Callback interrupt scheduling (RA compare)
 */

#include <px4_platform_common/px4_config.h>
#include <nuttx/arch.h>
#include <nuttx/irq.h>

#include <sys/types.h>
#include <stdbool.h>

#include <assert.h>
#include <debug.h>
#include <time.h>
#include <queue.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>

#include <board_config.h>
#include <drivers/drv_hrt.h>

#include "arm_internal.h"
#include "hardware/sam_pmc.h"
#include "hardware/sam_tc.h"

#ifdef CONFIG_DEBUG_HRT
#  define hrtinfo _info
#else
#  define hrtinfo(x...)
#endif

#ifdef HRT_TIMER

/* HRT configuration for SAMV7 TC0 */
#if HRT_TIMER == 0
# define HRT_TIMER_BASE		SAM_TC012_BASE
# define HRT_TIMER_CHANNEL	0
# define HRT_TIMER_VECTOR	SAM_IRQ_TC0
# define HRT_TIMER_CLOCK	BOARD_MCK_FREQUENCY
# define HRT_TIMER_PCER		(1 << SAM_PID_TC0)
#else
# error HRT_TIMER must be 0 for SAMV7 (TC0 Channel 0)
#endif

/* Minimum/maximum deadlines */
#define HRT_INTERVAL_MIN	50
#define HRT_INTERVAL_MAX	50000

/* HRT clock divisor - use MCK/32 for ~4.69MHz (close to 1MHz ideal) */
#define HRT_DIVISOR		32

/* Actual timer frequency after prescaler */
#define HRT_ACTUAL_FREQ		(HRT_TIMER_CLOCK / HRT_DIVISOR)

/* Timer register addresses for TC0 Channel 0 */
#define rCCR	(HRT_TIMER_BASE + SAM_TC_CCR_OFFSET)
#define rCMR	(HRT_TIMER_BASE + SAM_TC_CMR_OFFSET)
#define rCV	(HRT_TIMER_BASE + SAM_TC_CV_OFFSET)
#define rRA	(HRT_TIMER_BASE + SAM_TC_RA_OFFSET)
#define rRC	(HRT_TIMER_BASE + SAM_TC_RC_OFFSET)
#define rSR	(HRT_TIMER_BASE + SAM_TC_SR_OFFSET)
#define rIER	(HRT_TIMER_BASE + SAM_TC_IER_OFFSET)
#define rIDR	(HRT_TIMER_BASE + SAM_TC_IDR_OFFSET)
#define rIMR	(HRT_TIMER_BASE + SAM_TC_IMR_OFFSET)

/* TC Channel Control Register bits */
#define TC_CCR_CLKEN		(1 << 0)
#define TC_CCR_CLKDIS		(1 << 1)
#define TC_CCR_SWTRG		(1 << 2)

/* TC interrupt flags (from hardware/sam_tc.h) */
#define TC_INT_CPCS		(1 << 4)  /* RC Compare */
#define TC_INT_CPAS		(1 << 2)  /* RA Compare */

/* TC register definitions - use NuttX hardware definitions */
/* NuttX provides these in hardware/sam_tc.h */

/* Callout list */
static struct sq_queue_s callout_queue;

/* Latency histogram */
__EXPORT uint32_t latency_actual_min = UINT32_MAX;
__EXPORT uint32_t latency_actual_max = 0;

/* HRT clock counter - stored in TIMER TICKS (not microseconds) */
static volatile uint64_t hrt_absolute_time_base;
static volatile uint32_t hrt_counter_wrap_count;

/* Forward declarations */
static void hrt_call_invoke(void);
static void hrt_call_reschedule(void);

/****************************************************************************
 * FIX 1: Time Units Conversion
 *
 * CRITICAL: This function MUST return microseconds, not timer ticks!
 * PX4 assumes hrt_abstime is in microseconds throughout the codebase.
 ****************************************************************************/
hrt_abstime hrt_absolute_time(void)
{
	uint32_t count;
	irqstate_t flags;
	uint64_t abstime_ticks;
	uint64_t abstime_usec;

	flags = enter_critical_section();

	/* Read current counter value (in timer ticks) */
	count = getreg32(rCV);

	/* Calculate absolute time in timer ticks */
	abstime_ticks = hrt_absolute_time_base + count;

	leave_critical_section(flags);

	/* ✅ FIX: Convert timer ticks to microseconds */
	/* Formula: usec = (ticks * 1,000,000) / frequency */
	/* With MCK/32: usec = (ticks * 1,000,000) / 4,687,500 */
	abstime_usec = (abstime_ticks * 1000000ULL) / HRT_ACTUAL_FREQ;

	return abstime_usec;
}

/****************************************************************************
 * FIX 2: Overflow Interrupt Handling
 *
 * Initialize HRT with overflow interrupt enabled
 ****************************************************************************/
void hrt_init(void)
{
	syslog(LOG_INFO, "[hrt] Initializing HRT (TC0 CH0, MCK/%d = %lu Hz)\n",
	       HRT_DIVISOR, (unsigned long)HRT_ACTUAL_FREQ);

	sq_init(&callout_queue);

	/* Enable peripheral clock for TC0 */
	uint32_t regval = getreg32(SAM_PMC_PCER0);
	regval |= HRT_TIMER_PCER;
	putreg32(regval, SAM_PMC_PCER0);

	/* Disable TC clock */
	putreg32(TC_CCR_CLKDIS, rCCR);

	/* Configure TC channel mode:
	 * - Waveform mode (required for compare interrupts)
	 * - Up counting with automatic reset on RC compare
	 * - MCK/32 prescaler (~4.69 MHz for 150 MHz MCK)
	 */
	uint32_t cmr = TC_CMR_WAVE | TC_CMR_WAVSEL_UP | TC_CMR_TCCLKS_MCK32;

	putreg32(cmr, rCMR);

	/* Set RC to maximum value for free-running 32-bit counter */
	putreg32(0xFFFFFFFF, rRC);

	/* Set RA to maximum initially (will be updated by reschedule) */
	putreg32(0xFFFFFFFF, rRA);

	/* Disable all interrupts initially */
	putreg32(0xFFFFFFFF, rIDR);

	/* Clear status */
	(void)getreg32(rSR);

	/* ✅ FIX: Attach interrupt handler BEFORE enabling interrupts */
	irq_attach(HRT_TIMER_VECTOR, hrt_tim_isr, NULL);

	/* ✅ FIX: Enable RC compare interrupt (overflow detection) */
	putreg32(TC_INT_CPCS, rIER);

	/* Enable TC clock and trigger */
	putreg32(TC_CCR_CLKEN | TC_CCR_SWTRG, rCCR);

	/* ✅ FIX: Enable IRQ at NVIC level */
	up_enable_irq(HRT_TIMER_VECTOR);

	/* Initialize absolute time base (in ticks) */
	hrt_absolute_time_base = 0;
	hrt_counter_wrap_count = 0;

	syslog(LOG_INFO, "[hrt] HRT initialized successfully\n");

	/* Test that timer is running */
	uint32_t cv1 = getreg32(rCV);
	for (volatile int i = 0; i < 100000; i++);
	uint32_t cv2 = getreg32(rCV);

	if (cv2 > cv1) {
		syslog(LOG_INFO, "[hrt] Counter test OK: %lu ticks in 100k loops\n",
		       (unsigned long)(cv2 - cv1));
	} else {
		syslog(LOG_ERR, "[hrt] Counter test FAILED: not incrementing!\n");
	}

	hrtinfo("HRT initialized\n");
}

/****************************************************************************
 * Process pending callouts
 ****************************************************************************/
static void hrt_call_invoke(void)
{
	struct hrt_call *call;
	hrt_abstime now = hrt_absolute_time();

	while (true) {
		call = (struct hrt_call *)sq_peek(&callout_queue);

		if (call == NULL) {
			break;
		}

		if (call->deadline > now) {
			break;
		}

		sq_rem(&call->link, &callout_queue);

		/* Invoke callback */
		if (call->callout) {
			call->callout(call->arg);
		}

		/* If periodic, reschedule */
		if (call->period > 0) {
			call->deadline += call->period;
			hrt_call_at(call, call->deadline, call->callout, call->arg);
		} else {
			call->deadline = 0;
		}
	}
}

/****************************************************************************
 * FIX 3: Callback Interrupt Scheduling
 *
 * Program RA register to fire interrupt at next callback deadline
 ****************************************************************************/
static void hrt_call_reschedule(void)
{
	hrt_abstime now_usec = hrt_absolute_time();
	struct hrt_call *next = (struct hrt_call *)sq_peek(&callout_queue);

	if (next == NULL) {
		/* No pending callouts - disable RA compare interrupt */
		putreg32(TC_INT_CPAS, rIDR);
		hrtinfo("No pending callouts, RA interrupt disabled\n");
		return;
	}

	hrt_abstime deadline_usec = next->deadline;

	/* Ensure deadline is in the future */
	if (deadline_usec <= now_usec) {
		deadline_usec = now_usec + HRT_INTERVAL_MIN;
	}

	/* ✅ FIX: Convert deadline from microseconds to timer ticks */
	uint64_t deadline_ticks = (deadline_usec * HRT_ACTUAL_FREQ) / 1000000ULL;

	/* Calculate RA value relative to current base */
	/* RA interrupt fires when CV (current counter) == RA */
	uint32_t ra_value;

	if (deadline_ticks >= hrt_absolute_time_base) {
		/* Normal case: deadline is in current epoch */
		ra_value = (uint32_t)(deadline_ticks - hrt_absolute_time_base);
	} else {
		/* Edge case: deadline already passed, fire ASAP */
		uint32_t current = getreg32(rCV);
		ra_value = current + (HRT_INTERVAL_MIN * HRT_ACTUAL_FREQ / 1000000);
	}

	/* ✅ FIX: Program RA register for compare interrupt */
	putreg32(ra_value, rRA);

	/* ✅ FIX: Enable RA compare interrupt */
	putreg32(TC_INT_CPAS, rIER);

	hrtinfo("Next callback scheduled: deadline=%llu µs, RA=0x%08lx\n",
	        deadline_usec, (unsigned long)ra_value);
}

/****************************************************************************
 * FIX 2 & 3: HRT Interrupt Handler
 *
 * Handles two interrupt sources:
 *   1. RC compare (CPCS) - Counter overflow at 0xFFFFFFFF
 *   2. RA compare (CPAS) - Scheduled callback deadline reached
 ****************************************************************************/
static int hrt_tim_isr(int irq, void *context, void *arg)
{
	uint32_t status;

	/* Read and clear status (reading SR clears interrupt flags) */
	status = getreg32(rSR);

	/* ✅ FIX: Handle counter overflow (RC compare) */
	if (status & TC_INT_CPCS) {
		/* Add full 32-bit range to base time (in TICKS) */
		hrt_absolute_time_base += 0x100000000ULL;
		hrt_counter_wrap_count++;

		hrtinfo("HRT overflow #%lu (base now 0x%llx ticks)\n",
		        (unsigned long)hrt_counter_wrap_count,
		        hrt_absolute_time_base);
	}

	/* ✅ FIX: Handle scheduled callback (RA compare) */
	if (status & TC_INT_CPAS) {
		/* Disable RA interrupt until next callback scheduled */
		putreg32(TC_INT_CPAS, rIDR);

		/* Process all callbacks whose deadline has passed */
		hrt_call_invoke();

		/* Schedule next callback (will re-enable RA interrupt if needed) */
		hrt_call_reschedule();
	}

	return OK;
}

/****************************************************************************
 * Schedule callback at absolute time
 ****************************************************************************/
void hrt_call_at(struct hrt_call *entry, hrt_abstime calltime, hrt_callout callout, void *arg)
{
	if (entry == NULL || callout == NULL) {
		return;
	}

	irqstate_t flags = enter_critical_section();

	/* Remove from queue if already scheduled */
	sq_rem(&entry->link, &callout_queue);

	entry->deadline = calltime;
	entry->callout = callout;
	entry->arg = arg;

	/* Insert into queue in deadline order (sorted list) */
	struct hrt_call *call;
	struct hrt_call *prev = NULL;

	for (call = (struct hrt_call *)sq_peek(&callout_queue); call != NULL;
	     call = (struct hrt_call *)sq_next(&call->link)) {
		if (call->deadline > calltime) {
			break;
		}

		prev = call;
	}

	if (prev == NULL) {
		sq_addfirst(&entry->link, &callout_queue);

	} else {
		sq_addafter(&prev->link, &entry->link, &callout_queue);
	}

	/* Reschedule interrupt for earliest deadline */
	hrt_call_reschedule();

	leave_critical_section(flags);
}

/****************************************************************************
 * Schedule callback after delay
 ****************************************************************************/
void hrt_call_after(struct hrt_call *entry, hrt_abstime delay, hrt_callout callout, void *arg)
{
	hrt_call_at(entry, hrt_absolute_time() + delay, callout, arg);
}

/****************************************************************************
 * Schedule periodic callback
 ****************************************************************************/
void hrt_call_every(struct hrt_call *entry, hrt_abstime delay, hrt_abstime interval, hrt_callout callout, void *arg)
{
	entry->period = interval;
	hrt_call_after(entry, delay, callout, arg);
}

/****************************************************************************
 * Cancel scheduled callback
 ****************************************************************************/
void hrt_cancel(struct hrt_call *entry)
{
	irqstate_t flags = enter_critical_section();

	sq_rem(&entry->link, &callout_queue);
	entry->deadline = 0;
	entry->period = 0;

	/* Reschedule for next callback (or disable if queue empty) */
	hrt_call_reschedule();

	leave_critical_section(flags);
}

/****************************************************************************
 * Store current time (for CPU load monitoring)
 ****************************************************************************/
void hrt_store_absolute_time(volatile hrt_abstime *t)
{
	*t = hrt_absolute_time();
}

/****************************************************************************
 * Get elapsed time since stored time
 ****************************************************************************/
hrt_abstime hrt_elapsed_time(const volatile hrt_abstime *then)
{
	irqstate_t flags = enter_critical_section();

	hrt_abstime delta = hrt_absolute_time() - *then;

	leave_critical_section(flags);

	return delta;
}

#endif /* HRT_TIMER */
