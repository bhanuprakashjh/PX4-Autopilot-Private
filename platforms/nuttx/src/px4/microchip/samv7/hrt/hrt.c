/****************************************************************************
 *
 *   Copyright (c) 2016-2020 PX4 Development Team. All rights reserved.
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
#if HRT_TIMER == 0
#  define HRT_TIMER_BASE      SAM_TC012_BASE
#  define HRT_TIMER_VECTOR    SAM_IRQ_TC0
#  define HRT_TIMER_CLOCK     BOARD_MCK_FREQUENCY
#  define HRT_TIMER_PCER      (1 << SAM_PID_TC0)
#else
#  error HRT_TIMER must be 0 for SAMV71 TC0
#endif

#define HRT_TIMER_DIVISOR     32
#define HRT_TIMER_FREQ        (HRT_TIMER_CLOCK / HRT_TIMER_DIVISOR)

/**
* Minimum/maximum deadlines.
*
* The high-resolution timer need only guarantee that it not wrap more than
* once in the 50ms period for absolute time to be consistently maintained.
*/
#define HRT_INTERVAL_MIN	50
#define HRT_INTERVAL_MAX	50000

/*
* Period of the free-running counter, in timer ticks.
*/
#define HRT_COUNTER_PERIOD	UINT32_MAX
#define HRT_COUNTER_PERIOD_TICKS (UINT64_C(1) << 32)

/*
 * Scaling helpers between timer ticks and microseconds.
 */
static inline uint64_t hrt_ticks_to_usec(uint64_t ticks)
{
	return (ticks * 1000000ULL) / HRT_TIMER_FREQ;
}

static inline uint64_t hrt_usec_to_ticks(hrt_abstime usec)
{
	return (usec * HRT_TIMER_FREQ + 999999ULL) / 1000000ULL;
}

#define rCCR   (HRT_TIMER_BASE + SAM_TC_CCR_OFFSET)
#define rCMR   (HRT_TIMER_BASE + SAM_TC_CMR_OFFSET)
#define rCV    (HRT_TIMER_BASE + SAM_TC_CV_OFFSET)
#define rRA    (HRT_TIMER_BASE + SAM_TC_RA_OFFSET)
#define rRB    (HRT_TIMER_BASE + SAM_TC_RB_OFFSET)
#define rRC    (HRT_TIMER_BASE + SAM_TC_RC_OFFSET)
#define rSR    (HRT_TIMER_BASE + SAM_TC_SR_OFFSET)
#define rIER   (HRT_TIMER_BASE + SAM_TC_IER_OFFSET)
#define rIDR   (HRT_TIMER_BASE + SAM_TC_IDR_OFFSET)
#define rIMR   (HRT_TIMER_BASE + SAM_TC_IMR_OFFSET)

/*
 * Queue of callout entries.
 */
static struct sq_queue_s  callout_queue;

/* latency baseline (last compare value applied) */
static uint32_t           latency_baseline;

/* timer count at interrupt (for latency purposes) */
static uint32_t           latency_actual;

/* latency histogram */
const uint16_t latency_bucket_count = LATENCY_BUCKET_COUNT;
const uint16_t latency_buckets[LATENCY_BUCKET_COUNT] = { 1, 2, 5, 10, 20, 50, 100, 1000 };
__EXPORT uint32_t latency_counters[LATENCY_BUCKET_COUNT + 1];

static volatile uint64_t hrt_absolute_time_base;
static volatile uint32_t hrt_last_counter_value;
static volatile uint32_t hrt_counter_wrap_count;

static volatile bool hrt_selftest_expected;
static volatile bool hrt_selftest_done;

static const hrt_abstime kHrtSelftestDelayUs = 200;
static const hrt_abstime kHrtSelftestTimeoutUs = 2000;

/* timer-specific functions */
static void hrt_tim_init(void);
static int  hrt_tim_isr(int irq, void *context, void *args);
static void hrt_latency_update(void);
static uint64_t hrt_ticks_locked(uint32_t *count);
static hrt_abstime hrt_absolute_time_locked(void);
static bool hrt_run_selftest(void);

/* callout list manipulation */
static void hrt_call_internal(struct hrt_call *entry, hrt_abstime deadline, hrt_abstime interval, hrt_callout callout,
			      void *arg);
static void hrt_call_enter(struct hrt_call *entry);
static void hrt_call_reschedule(void);
static void hrt_call_invoke(void);

static uint64_t hrt_ticks_locked(uint32_t *count_out)
{
	uint32_t count = getreg32(rCV);

	if (count < hrt_last_counter_value) {
		hrt_absolute_time_base += HRT_COUNTER_PERIOD_TICKS;
		hrt_counter_wrap_count++;
	}

	hrt_last_counter_value = count;

	if (count_out != NULL) {
		*count_out = count;
	}

	return hrt_absolute_time_base + count;
}

static hrt_abstime hrt_absolute_time_locked(void)
{
	return hrt_ticks_to_usec(hrt_ticks_locked(NULL));
}

static bool hrt_run_selftest(void)
{
	const uint32_t selftest_ticks = (uint32_t)hrt_usec_to_ticks(kHrtSelftestDelayUs);
	hrt_abstime waited = 0;

	irqstate_t flags = px4_enter_critical_section();

	hrt_selftest_expected = true;
	hrt_selftest_done = false;

	uint32_t current = getreg32(rCV);
	uint32_t ra = current + selftest_ticks;
	latency_baseline = ra;

	putreg32(ra, rRA);
	putreg32(TC_INT_CPAS, rIER);

	px4_leave_critical_section(flags);

	while (!hrt_selftest_done && waited < kHrtSelftestTimeoutUs) {
		up_udelay(50);
		waited += 50;
	}

	flags = px4_enter_critical_section();

	if (sq_peek(&callout_queue) == NULL) {
		putreg32(TC_INT_CPAS, rIDR);
	}

	hrt_selftest_expected = false;
	px4_leave_critical_section(flags);

	return hrt_selftest_done;
}


/**
 * Initialize the timer we are going to use.
 */
static void hrt_tim_init(void)
{
	/* Enable the TC peripheral clock */
	uint32_t regval = getreg32(SAM_PMC_PCER0);
	regval |= HRT_TIMER_PCER;
	putreg32(regval, SAM_PMC_PCER0);

	/* Disable TC while we configure it */
	putreg32(TC_CCR_CLKDIS, rCCR);

	/* Waveform mode, reset on RC compare, clocked from MCK/32 */
	uint32_t cmr = TC_CMR_WAVE | TC_CMR_WAVSEL_UP | TC_CMR_TCCLKS_MCK32;
	putreg32(cmr, rCMR);

	/* Set initial compare registers */
	putreg32(0xFFFFFFFF, rRC);
	putreg32(0xFFFFFFFF, rRA);

	/* Disable/clear interrupts */
	putreg32(0xFFFFFFFF, rIDR);
	(void)getreg32(rSR);

	/* Attach interrupt handler and enable overflow interrupt */
	irq_attach(HRT_TIMER_VECTOR, hrt_tim_isr, NULL);
	putreg32(TC_INT_CPCS, rIER);

	/* Start timer */
	putreg32(TC_CCR_CLKEN | TC_CCR_SWTRG, rCCR);

	/* Enable interrupts at the NVIC */
	up_enable_irq(HRT_TIMER_VECTOR);

	hrt_last_counter_value = getreg32(rCV);
	hrt_selftest_expected = false;
	hrt_selftest_done = false;

	if (!hrt_run_selftest()) {
		syslog(LOG_ERR, "[hrt] TC0 self-test failed (no CPAS interrupt)\n");

	} else {
		syslog(LOG_INFO, "[hrt] TC0 self-test passed\n");
	}
}

/**
 * Handle the compare interrupt by calling the callout dispatcher
 * and then re-scheduling the next deadline.
 */
static int
hrt_tim_isr(int irq, void *context, void *arg)
{
	uint32_t status = getreg32(rSR);
	bool need_reschedule = false;

	if (status & TC_INT_CPCS) {
		need_reschedule = true;
	}

	if (status & TC_INT_CPAS) {
		latency_actual = getreg32(rCV);
		putreg32(TC_INT_CPAS, rIDR);

		if (hrt_selftest_expected) {
			hrt_selftest_done = true;
		}

		hrt_latency_update();
		hrt_call_invoke();
		need_reschedule = true;
	}

	if (need_reschedule) {
		hrt_call_reschedule();
	}

	return OK;
}

/**
 * Fetch a never-wrapping absolute time value in microseconds from
 * some arbitrary epoch shortly after system start.
 */
hrt_abstime
hrt_absolute_time(void)
{
	irqstate_t flags = px4_enter_critical_section();
	hrt_abstime abstime = hrt_absolute_time_locked();
	px4_leave_critical_section(flags);

	return abstime;
}

/**
 * Store the absolute time in an interrupt-safe fashion
 */
void
hrt_store_absolute_time(volatile hrt_abstime *t)
{
	irqstate_t flags = px4_enter_critical_section();
	*t = hrt_absolute_time();
	px4_leave_critical_section(flags);
}

/**
 * Initialize the high-resolution timing module.
 */
void
hrt_init(void)
{
	sq_init(&callout_queue);
	memset(latency_counters, 0, sizeof(latency_counters));
	latency_actual = 0;
	latency_baseline = 0;
	hrt_absolute_time_base = 0;
	hrt_last_counter_value = 0;
	hrt_counter_wrap_count = 0;
	hrt_selftest_expected = false;
	hrt_selftest_done = false;
	hrt_tim_init();
}

/**
 * Call callout(arg) after interval has elapsed.
 */
void
hrt_call_after(struct hrt_call *entry, hrt_abstime delay, hrt_callout callout, void *arg)
{
	hrt_call_internal(entry,
			  hrt_absolute_time() + delay,
			  0,
			  callout,
			  arg);
}

/**
 * Call callout(arg) at calltime.
 */
void
hrt_call_at(struct hrt_call *entry, hrt_abstime calltime, hrt_callout callout, void *arg)
{
	hrt_call_internal(entry, calltime, 0, callout, arg);
}

/**
 * Call callout(arg) every period.
 */
void
hrt_call_every(struct hrt_call *entry, hrt_abstime delay, hrt_abstime interval, hrt_callout callout, void *arg)
{
	hrt_call_internal(entry,
			  hrt_absolute_time() + delay,
			  interval,
			  callout,
			  arg);
}

static void
hrt_call_internal(struct hrt_call *entry, hrt_abstime deadline, hrt_abstime interval, hrt_callout callout, void *arg)
{
	irqstate_t flags = px4_enter_critical_section();

	/* if the entry is currently queued, remove it */
	/* note that we are using a potentially uninitialized
	   entry->link here, but it is safe as sq_rem() doesn't
	   dereference the passed node unless it is found in the
	   list. So we potentially waste a bit of time searching the
	   queue for the uninitialized entry->link but we don't do
	   anything actually unsafe.
	*/
	if (entry->deadline != 0) {
		sq_rem(&entry->link, &callout_queue);
	}

	entry->deadline = deadline;
	entry->period = interval;
	entry->callout = callout;
	entry->arg = arg;

	hrt_call_enter(entry);

	px4_leave_critical_section(flags);
}

/**
 * If this returns true, the call has been invoked and removed from the callout list.
 *
 * Always returns false for repeating callouts.
 */
bool
hrt_called(struct hrt_call *entry)
{
	return (entry->deadline == 0);
}

/**
 * Remove the entry from the callout list.
 */
void
hrt_cancel(struct hrt_call *entry)
{
	irqstate_t flags = px4_enter_critical_section();

	sq_rem(&entry->link, &callout_queue);
	entry->deadline = 0;

	/* if this is a periodic call being removed by the callout, prevent it from
	 * being re-entered when the callout returns.
	 */
	entry->period = 0;

	px4_leave_critical_section(flags);
}

static void
hrt_call_enter(struct hrt_call *entry)
{
	struct hrt_call	*call, *next;

	call = (struct hrt_call *)sq_peek(&callout_queue);

	if ((call == NULL) || (entry->deadline < call->deadline)) {
		sq_addfirst(&entry->link, &callout_queue);
		hrtinfo("call enter at head, reschedule\n");
		/* we changed the next deadline, reschedule the timer event */
		hrt_call_reschedule();

	} else {
		do {
			next = (struct hrt_call *)sq_next(&call->link);

			if ((next == NULL) || (entry->deadline < next->deadline)) {
				hrtinfo("call enter after head\n");
				sq_addafter(&call->link, &entry->link, &callout_queue);
				break;
			}
		} while ((call = next) != NULL);
	}

	hrtinfo("scheduled\n");
}

static void
hrt_call_invoke(void)
{
	struct hrt_call	*call;
	hrt_abstime deadline;

	while (true) {
		/* get the current time */
		hrt_abstime now = hrt_absolute_time();

		call = (struct hrt_call *)sq_peek(&callout_queue);

		if (call == NULL) {
			break;
		}

		if (call->deadline > now) {
			break;
		}

		sq_rem(&call->link, &callout_queue);
		hrtinfo("call pop\n");

		/* save the intended deadline for periodic calls */
		deadline = call->deadline;

		/* zero the deadline, as the call has occurred */
		call->deadline = 0;

		/* invoke the callout (if there is one) */
		if (call->callout) {
			hrtinfo("call %p: %p(%p)\n", call, call->callout, call->arg);
			call->callout(call->arg);
		}

		/* if the callout has a non-zero period, it has to be re-entered */
		if (call->period != 0) {
			// re-check call->deadline to allow for
			// callouts to re-schedule themselves
			// using hrt_call_delay()
			if (call->deadline <= now) {
				call->deadline = deadline + call->period;
			}

			hrt_call_enter(call);
		}
	}
}

/**
 * Reschedule the next timer interrupt.
 *
 * This routine must be called with interrupts disabled.
 */
static void
hrt_call_reschedule()
{
	irqstate_t flags = px4_enter_critical_section();

	struct hrt_call *next = (struct hrt_call *)sq_peek(&callout_queue);

	if (next == NULL) {
		putreg32(TC_INT_CPAS, rIDR);
		px4_leave_critical_section(flags);
		return;
	}

	uint32_t current_count;
	uint64_t now_ticks = hrt_ticks_locked(&current_count);
	hrt_abstime now = hrt_ticks_to_usec(now_ticks);
	hrt_abstime deadline = now + HRT_INTERVAL_MAX;

	if (next->deadline <= (now + HRT_INTERVAL_MIN)) {
		deadline = now + HRT_INTERVAL_MIN;

	} else if (next->deadline < deadline) {
		deadline = next->deadline;
	}

	hrt_abstime delta_us = deadline - now;
	uint64_t delta_ticks = hrt_usec_to_ticks(delta_us);

	if (delta_ticks == 0) {
		delta_ticks = 1;
	}

	uint32_t ra = current_count + (uint32_t)delta_ticks;
	latency_baseline = ra;

	putreg32(ra, rRA);
	putreg32(TC_INT_CPAS, rIER);

	px4_leave_critical_section(flags);
}

static void
hrt_latency_update(void)
{
	uint32_t latency = latency_actual - latency_baseline;
	unsigned	index;

	/* bounded buckets */
	for (index = 0; index < LATENCY_BUCKET_COUNT; index++) {
		if (latency <= latency_buckets[index]) {
			latency_counters[index]++;
			return;
		}
	}

	/* catch-all at the end */
	latency_counters[index]++;
}

void
hrt_call_init(struct hrt_call *entry)
{
	memset(entry, 0, sizeof(*entry));
}

void
hrt_call_delay(struct hrt_call *entry, hrt_abstime delay)
{
	entry->deadline = hrt_absolute_time() + delay;
}

#endif /* HRT_TIMER */
