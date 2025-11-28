# HRT 1 MHz PCK6 Implementation Plan

## Overview

Convert the SAMV7 HRT from MCK/32 (4.6875 MHz) to PCK6 (1 MHz exact) to eliminate all conversion math and achieve identity mapping: **1 tick = 1 microsecond**.

## Current State

- **Clock Source:** MCK/32 = 150 MHz / 32 = 4,687,500 Hz
- **Problem:** Requires expensive 64-bit multiply/divide for tick↔usec conversion
- **Result:** ~93x timing error, complex math, potential integer overflow issues

## Target State

- **Clock Source:** PCK6 = MCK / 150 = 150 MHz / 150 = 1,000,000 Hz (exactly 1 MHz)
- **Benefit:** `1 tick = 1 microsecond` - no conversion needed
- **Counter Wrap:** 32-bit @ 1 MHz = 4,295 seconds (~71.5 minutes) before wrap

---

## Implementation Steps

### Step 1: Add Missing CCFG_PCCR Register Definition

**File:** `platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/hardware/sam_matrix.h`

The CCFG_PCCR (Peripheral Clock Configuration Register) is missing from NuttX headers.

```c
/* Add after line 91 (SAM_MATRIX_CCFG_SMCNFCS_OFFSET): */
#define SAM_MATRIX_CCFG_PCCR_OFFSET      0x0118 /* Peripheral Clock Configuration Register */

/* Add after line 149 (SAM_MATRIX_CCFG_SMCNFCS): */
#define SAM_MATRIX_CCFG_PCCR             (SAM_MATRIX_BASE+SAM_MATRIX_CCFG_PCCR_OFFSET)

/* Add bit definitions after line 269: */
#define MATRIX_CCFG_PCCR_TC0CC           (1 << 20) /* Bit 20: TC0 Clock Configuration (0=PCK6, 1=PCK7) */
```

**Register Address:** 0x40088000 + 0x118 = 0x40088118

---

### Step 2: Configure PCK6 in Board Init

**File:** `boards/microchip/samv71-xult-clickboards/src/init.c`

Add early PCK6 configuration:

```c
#include "sam_pck.h"
#include "hardware/sam_matrix.h"

/* In samv71xult_board_initialize() or samv71xult_boardinitialize(): */

void configure_hrt_clock(void)
{
    uint32_t actual_freq;
    uint32_t regval;

    /* Step 1: Configure PCK6 for 1 MHz from MCK (150 MHz / 150 = 1 MHz) */
    actual_freq = sam_pck_configure(PCK6, PCKSRC_MCK, 1000000);

    if (actual_freq != 1000000) {
        syslog(LOG_ERR, "[hrt] PCK6 configuration failed: got %lu Hz\n",
               (unsigned long)actual_freq);
        return;
    }

    /* Step 2: Enable PCK6 */
    sam_pck_enable(PCK6, true);

    /* Step 3: Wait for PCK6 ready (PMC_SR.PCKRDY6) */
    while ((getreg32(SAM_PMC_SR) & PMC_INT_PCKRDY6) == 0);

    /* Step 4: Route PCK6 to TC0 via CCFG_PCCR.TC0CC = 0 */
    regval = getreg32(SAM_MATRIX_CCFG_PCCR);
    regval &= ~MATRIX_CCFG_PCCR_TC0CC;  /* Clear bit = select PCK6 */
    putreg32(regval, SAM_MATRIX_CCFG_PCCR);

    syslog(LOG_INFO, "[hrt] PCK6 configured: %lu Hz routed to TC0\n",
           (unsigned long)actual_freq);
}
```

**Call this function early in board initialization, before hrt_init().**

---

### Step 3: Update HRT Driver

**File:** `platforms/nuttx/src/px4/microchip/samv7/hrt/hrt.c`

#### 3.1 Change Clock Definitions

```c
/* Replace the current definitions: */

/* OLD (MCK/32): */
#define HRT_TIMER_DIVISOR     32
#define HRT_TIMER_FREQ        (HRT_TIMER_CLOCK / HRT_TIMER_DIVISOR)

/* NEW (PCK6 @ 1 MHz): */
#define HRT_TIMER_FREQ        1000000UL  /* 1 MHz from PCK6 */

/* Identity conversion macros - no math needed! */
#define HRT_COUNTER_SCALE(_c)   (_c)              /* ticks → usec */
#define HRT_USEC_TO_TICKS(_us)  (_us)             /* usec → ticks */
```

#### 3.2 Change TC Clock Selection in hrt_tim_init()

```c
/* OLD: */
uint32_t cmr = TC_CMR_WAVE | TC_CMR_WAVSEL_UP | TC_CMR_TCCLKS_MCK32;

/* NEW: */
uint32_t cmr = TC_CMR_WAVE | TC_CMR_WAVSEL_UP | TC_CMR_TCCLKS_PCK6;
```

#### 3.3 Simplify hrt_absolute_time()

```c
hrt_abstime hrt_absolute_time(void)
{
    irqstate_t flags = px4_enter_critical_section();

    uint32_t count = getreg32(rCV);

    /* Handle wrap detection */
    if (count < hrt_last_counter_value) {
        hrt_absolute_time_base += 0x100000000ULL;
        hrt_counter_wrap_count++;
    }
    hrt_last_counter_value = count;

    /* With 1 MHz: ticks = microseconds directly! No conversion needed. */
    uint64_t abstime = hrt_absolute_time_base + count;

    px4_leave_critical_section(flags);

    return abstime;
}
```

#### 3.4 Simplify hrt_call_reschedule()

```c
/* OLD: Complex conversion with multiply-inverse */
uint64_t delta_ticks = hrt_usec_to_ticks(delta_us);

/* NEW: Identity */
uint64_t delta_ticks = delta_us;  /* 1 MHz: ticks = usec */
```

#### 3.5 Remove All Conversion Functions

Delete:
- `fast_div_by_75()`
- `hrt_ticks_to_usec()`
- `hrt_usec_to_ticks()`
- `DIV_BY_75_MAGIC`, `DIV_BY_75_SHIFT` constants

---

### Step 4: Add PMC_INT_PCKRDY6 Definition (if missing)

**File:** `platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/hardware/sam_pmc.h`

Check if this exists, add if missing:

```c
#define PMC_INT_PCKRDY6          (1 << 14)  /* Bit 14: Programmable Clock 6 Ready */
```

---

### Step 5: Fallback Configuration (Optional)

Add compile-time option to fall back to MCK/32 if PCK6 fails:

```c
/* In hrt.c or board Kconfig: */
#ifdef CONFIG_SAMV7_HRT_USE_PCK6
    #define HRT_TIMER_FREQ     1000000UL
    #define HRT_CLOCK_SOURCE   TC_CMR_TCCLKS_PCK6
#else
    #define HRT_TIMER_FREQ     (BOARD_MCK_FREQUENCY / 32)
    #define HRT_CLOCK_SOURCE   TC_CMR_TCCLKS_MCK32
#endif
```

---

## Verification Steps

### Test 1: PCK6 Frequency Verification

```c
/* Add to board init after configure_hrt_clock(): */
uint32_t pck6_freq = sam_pck_frequency(PCK6);
syslog(LOG_INFO, "[hrt] PCK6 actual frequency: %lu Hz\n", (unsigned long)pck6_freq);
```

### Test 2: HRT Self-Test

```bash
nsh> hrt_test
# Should show "Elapsed time ~1000000 in 1 sec"
```

### Test 3: CPU Idle Percentage

```bash
nsh> top
# idle should be ~99% not 5199%
```

### Test 4: Timer Interrupt Rate

Verify TC0 interrupt fires at expected rate for scheduled callbacks.

---

## File Summary

| File | Changes |
|------|---------|
| `sam_matrix.h` | Add CCFG_PCCR register and TC0CC bit definitions |
| `sam_pmc.h` | Add PMC_INT_PCKRDY6 if missing |
| `init.c` | Add `configure_hrt_clock()` function |
| `hrt.c` | Change to PCK6, remove all conversion math |

---

## Datasheet References

- **CCFG_PCCR:** DS60001527H §19.4.8, p. 109
- **TC Clock Selection:** DS60001527H §49.6.3, p. 1484, Table p. 1512
- **PMC PCK Configuration:** DS60001527H §31.12 & §31.20.13, p. 289
- **Async Clock Rule:** PCK must be ≤ MCK/2 (1 MHz << 75 MHz, OK)

---

## Risk Assessment

| Risk | Mitigation |
|------|------------|
| PCK6 already in use | Check NuttX config, use PCK7 if needed |
| Wrong CCFG_PCCR address | Verify against datasheet Table 19-4 |
| sam_pck_configure() returns wrong freq | Check BOARD_MCK_FREQUENCY is 150000000 |
| Counter wrap every 71.5 min | RC compare interrupt handles this |

---

## Estimated Effort

| Task | Time |
|------|------|
| Add register definitions | 30 min |
| Implement board init PCK6 setup | 1 hour |
| Modify hrt.c | 1-2 hours |
| Test and debug | 2-4 hours |
| **Total** | **4-8 hours** |

---

## Success Criteria

1. `hrt_test` shows elapsed time ≈ 1,000,000 µs for 1 second
2. `top` shows idle CPU ~99%
3. No timing drift over extended operation
4. All PX4 modules work correctly with accurate timing

---

**Document Created:** 2025-11-27
**Status:** Ready for implementation
