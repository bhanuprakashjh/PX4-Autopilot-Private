# HRT Fix Implementation Plan

**File:** `platforms/nuttx/src/px4/microchip/samv7/hrt/hrt.c`

This document provides a step-by-step plan to fix the three critical HRT issues.

---

## Problem Summary

### Current Issues:

1. **❌ Wrong Time Units** - Returns timer ticks (4.6875 MHz) instead of microseconds (1 MHz)
   - Impact: ALL timing in PX4 off by 4.7x factor

2. **❌ No Overflow Handling** - Counter wraps after 15.3 minutes, no interrupt
   - Impact: Time jumps backwards, system crash

3. **❌ No Callback Interrupts** - Uses polling instead of hardware compare
   - Impact: High latency (50-500µs vs 2-5µs), high CPU overhead

---

## Fix Strategy

### Approach:

We'll make **incremental fixes** and test each one:

1. **Fix 1:** Time units conversion (CRITICAL - must fix first)
2. **Fix 2:** Overflow interrupt (CRITICAL - prevents crash)
3. **Fix 3:** Callback interrupts (HIGH - improves performance)

Each fix builds on the previous one and can be tested independently.

---

## Fix 1: Time Units Conversion 🔴 CRITICAL

### Current Code (WRONG):

```c
// Line 89: Calculates actual frequency
#define HRT_ACTUAL_FREQ  (HRT_TIMER_CLOCK / HRT_DIVISOR)
// With MCK=150MHz, DIVISOR=32: 150MHz/32 = 4,687,500 Hz

// Lines 124-141: hrt_absolute_time() - RETURNS TICKS NOT MICROSECONDS!
hrt_abstime hrt_absolute_time(void)
{
    uint32_t count;
    irqstate_t flags;
    uint64_t abstime;

    flags = enter_critical_section();

    // Read 32-bit counter value (in timer ticks!)
    count = getreg32(rCV);

    // Calculate absolute time (still in ticks!)
    abstime = hrt_absolute_time_base + count;

    leave_critical_section(flags);

    return abstime;  // BUG: Returning ticks, not microseconds!
}
```

**Problem:** Returns ticks at 4.6875 MHz, but PX4 expects microseconds (1 MHz).

**Example:**
```
Counter value: 46,875 ticks
Should return:  10,000 µs (10 milliseconds)
Actually returns: 46,875 (interpreted as 46,875 µs = 46.875 ms)
ERROR: 4.7x too large!
```

---

### Fixed Code:

**Change location:** Lines 124-141 in hrt.c

```c
/**
 * Get absolute time in microseconds
 *
 * CRITICAL: This function MUST return microseconds, not timer ticks!
 * PX4 assumes 1 unit = 1 microsecond everywhere in the codebase.
 */
hrt_abstime hrt_absolute_time(void)
{
    uint32_t count;
    irqstate_t flags;
    uint64_t abstime_ticks;
    uint64_t abstime_usec;

    flags = enter_critical_section();

    // Read 32-bit counter value (in timer ticks at 4.6875 MHz or 18.75 MHz)
    count = getreg32(rCV);

    // Calculate absolute time in timer ticks
    abstime_ticks = hrt_absolute_time_base + count;

    // Convert timer ticks to microseconds
    // Formula: usec = (ticks * 1,000,000) / frequency
    // With MCK/32: usec = (ticks * 1,000,000) / 4,687,500
    // Simplifies to: usec = (ticks * 32) / 150
    abstime_usec = (abstime_ticks * 1000000ULL) / HRT_ACTUAL_FREQ;

    leave_critical_section(flags);

    return abstime_usec;  // Now returns microseconds!
}
```

---

### Why This Works:

**Math Explanation:**

```
Timer Configuration:
- MCK = 150 MHz
- Prescaler = MCK/32
- Timer frequency = 150,000,000 / 32 = 4,687,500 Hz
- Timer period = 1 / 4,687,500 = 0.2133 µs per tick

Conversion:
- Ticks to µs: multiply by 0.2133
- Or: (ticks * 1,000,000) / 4,687,500
- Simplified: (ticks * 32) / 150

Example:
- Counter = 46,875 ticks
- Time = (46,875 * 1,000,000) / 4,687,500
- Time = 46,875,000,000 / 4,687,500
- Time = 10,000 µs = 10 ms ✓ CORRECT!
```

---

### Testing Fix 1:

**Build and flash:**
```bash
make microchip_samv71-xult-clickboards_default
make microchip_samv71-xult-clickboards_default upload
```

**Test:**
```bash
nsh> # Read time twice with known delay
nsh> hrt_absolute_time
# Note value (e.g., 5,000,000)

# Wait exactly 1 second
nsh> sleep 1

nsh> hrt_absolute_time
# Should be ~1,000,000 higher (1 second = 1,000,000 µs)

# Test with logger
nsh> logger start
nsh> listener sensor_gyro
# Check timestamps increment by ~10,000 µs at 100 Hz (correct)
# NOT ~47,000 ticks (wrong)
```

**Success Criteria:**
- ✅ Time increases by ~1,000,000 per second (not 4,687,500)
- ✅ Sensor timestamps reasonable (8,000-10,000 µs intervals at 100Hz)
- ✅ No "time went backwards" errors

---

## Fix 2: Overflow Interrupt Handling 🔴 CRITICAL

### Current Code (WRONG):

```c
// Lines 146-205: hrt_init() - NO INTERRUPT ATTACHED!
void hrt_init(void)
{
    sq_init(&callout_queue);

    // Enable TC0 clock...
    // Configure timer...
    // Set RC to 0xFFFFFFFF...

    // Disable all interrupts
    putreg32(0xFFFFFFFF, rIDR);  // BUG: Disables overflow interrupt!

    // Clear status
    (void)getreg32(rSR);

    // Enable and start timer
    putreg32(TC_CCR_CLKEN | TC_CCR_SWTRG, rCCR);

    // NO irq_attach() call!
    // NO interrupt enable!

    hrt_absolute_time_base = 0;
    hrt_counter_wrap_count = 0;
}
```

**Problem:**
- Counter wraps from 0xFFFFFFFF to 0x00000000 after ~916 seconds (15.3 min)
- No interrupt handler attached
- Time calculation breaks: `base + count` wraps backwards

**Example Failure:**
```
Time: 915 seconds (just before wrap)
  base = 0
  count = 0xFFFFFFF0 = 4,294,967,280 ticks
  time = (4,294,967,280 * 1,000,000) / 4,687,500 = 916,363,600 µs ✓

Time: 916 seconds (after wrap)
  base = 0  (NOT UPDATED!)
  count = 0x00000010 = 16 ticks (wrapped to zero!)
  time = (16 * 1,000,000) / 4,687,500 = 3 µs ✗ WRONG!

Expected: 916,000,000 µs
Got:      3 µs
ERROR:    Time jumped backwards by 916 seconds!
```

---

### Fixed Code:

**Change 1: Update hrt_init() - Add interrupt attachment**

**Location:** Lines 146-205

```c
void hrt_init(void)
{
    syslog(LOG_ERR, "[hrt] hrt_init starting\n");
    sq_init(&callout_queue);

    // Enable peripheral clock for TC0
    uint32_t regval = getreg32(SAM_PMC_PCER0);
    regval |= HRT_TIMER_PCER;
    putreg32(regval, SAM_PMC_PCER0);

    // Disable TC clock
    putreg32(TC_CCR_CLKDIS, rCCR);

    // Configure TC channel mode
    uint32_t cmr = TC_CMR_WAVE | TC_CMR_WAVSEL_UP;

    // Select prescaler
    if (HRT_TIMER_CLOCK / 8 > 1000000) {
        cmr |= TC_CMR_TCCLKS_MCK32;  // MCK/32 = 4.6875 MHz
    } else {
        cmr |= TC_CMR_TCCLKS_MCK8;   // MCK/8 = 18.75 MHz
    }

    putreg32(cmr, rCMR);

    // Set RC to maximum value for free-running mode
    putreg32(0xFFFFFFFF, rRC);

    // ====== FIX: Attach interrupt handler ======
    irq_attach(HRT_TIMER_VECTOR, hrt_tim_isr, NULL);

    // ====== FIX: Enable RC compare interrupt (overflow) ======
    putreg32(TC_INT_CPCS, rIER);

    // Clear status
    (void)getreg32(rSR);

    // Enable TC clock and trigger
    putreg32(TC_CCR_CLKEN | TC_CCR_SWTRG, rCCR);

    // ====== FIX: Enable IRQ at NVIC level ======
    up_enable_irq(HRT_TIMER_VECTOR);

    // Initialize absolute time base
    hrt_absolute_time_base = 0;
    hrt_counter_wrap_count = 0;

    syslog(LOG_ERR, "[hrt] HRT initialized with overflow interrupt\n");
}
```

---

**Change 2: Update hrt_tim_isr() - Handle overflow**

**Location:** Lines 258-277

```c
/**
 * HRT interrupt handler
 * Handles two interrupts:
 * 1. RC compare (overflow at 0xFFFFFFFF)
 * 2. RA compare (scheduled callbacks - added in Fix 3)
 */
static int hrt_tim_isr(int irq, void *context, void *arg)
{
    uint32_t status;

    // Read and clear status (reading SR clears interrupt flags)
    status = getreg32(rSR);

    // ====== FIX: Handle counter overflow/wrap ======
    if (status & TC_INT_CPCS) {  // RC compare match (counter hit 0xFFFFFFFF)
        // Add full 32-bit range to base time (in TICKS, not microseconds)
        // Counter will wrap to 0, so we track the overflow in the base
        hrt_absolute_time_base += 0x100000000ULL;  // Add 2^32 ticks
        hrt_counter_wrap_count++;

        // Note: hrt_absolute_time() will convert total ticks to microseconds

        hrtinfo("HRT counter wrapped (count=%u)\n", hrt_counter_wrap_count);
    }

    // Handle scheduled callbacks (will be added in Fix 3)
    hrt_call_invoke();
    hrt_call_reschedule();

    return OK;
}
```

---

### Why This Works:

**Time Calculation After Overflow:**

```
Before wrap (t = 915s):
  base = 0
  count = 0xFFFFFFF0
  total_ticks = 0 + 0xFFFFFFF0 = 4,294,967,280
  time_usec = (4,294,967,280 * 1,000,000) / 4,687,500 = 916,363,600 µs ✓

Overflow interrupt fires:
  RC compare match (counter hit 0xFFFFFFFF)
  base += 0x100000000 = 4,294,967,296 ticks

After wrap (t = 916s):
  base = 4,294,967,296  (updated by ISR!)
  count = 0x00000010 = 16  (wrapped to near-zero)
  total_ticks = 4,294,967,296 + 16 = 4,294,967,312
  time_usec = (4,294,967,312 * 1,000,000) / 4,687,500 = 916,000,003 µs ✓

Time continues increasing monotonically!
```

---

### Testing Fix 2:

**Test overflow (requires 16 minutes):**

```bash
nsh> date
# Record current time (e.g., 14:00:00)

nsh> hrt_absolute_time
# Record value (e.g., 60,000,000 = 60 seconds uptime)

# Wait 16 minutes... (go get coffee ☕)

nsh> date
# Should be 14:16:00

nsh> hrt_absolute_time
# Should be 1,020,000,000 (60s + 16min*60s = 1020s)
# Must NOT jump backwards to small value!

nsh> dmesg | grep wrapped
# Should see: "HRT counter wrapped (count=1)"
```

**Success Criteria:**
- ✅ Time continues increasing after 16 minutes
- ✅ No backwards time jumps
- ✅ `hrt_counter_wrap_count` increments each wrap
- ✅ No system crashes or hangs

---

## Fix 3: Callback Interrupts 🟠 HIGH

### Current Code (WRONG):

```c
// Lines 240-252: hrt_call_reschedule() - NO HARDWARE INTERRUPT!
static void hrt_call_reschedule(void)
{
    hrt_abstime now = hrt_absolute_time();
    struct hrt_call *next = (struct hrt_call *)sq_peek(&callout_queue);

    if (next != NULL) {
        hrt_abstime deadline = next->deadline;

        if (deadline < now) {
            deadline = now + HRT_INTERVAL_MIN;
        }

        // BUG: No hardware compare register set!
        // Callbacks rely on polling in main loop!
    }
}
```

**Problem:**
- No RA register programmed
- No compare interrupt enabled
- Callbacks checked via polling (high latency, high CPU)

**Impact:**
```
Callback scheduled for time T:
  STM32: Hardware interrupt at T (latency: 2-5 µs)
  SAMV71 current: Polled check (latency: 50-500 µs, 100x worse!)
```

---

### Fixed Code:

**Change 1: Update hrt_call_reschedule()**

**Location:** Lines 240-252

```c
/**
 * Reschedule next callback using hardware compare interrupt
 */
static void hrt_call_reschedule(void)
{
    hrt_abstime now_usec = hrt_absolute_time();
    struct hrt_call *next = (struct hrt_call *)sq_peek(&callout_queue);

    if (next != NULL) {
        hrt_abstime deadline_usec = next->deadline;

        // Ensure deadline is in the future
        if (deadline_usec < now_usec) {
            deadline_usec = now_usec + HRT_INTERVAL_MIN;
        }

        // ====== FIX: Convert deadline from microseconds to timer ticks ======
        uint64_t deadline_ticks = (deadline_usec * HRT_ACTUAL_FREQ) / 1000000ULL;

        // Calculate compare value relative to current base
        // RA interrupt will fire when CV matches RA
        uint32_t compare = (uint32_t)(deadline_ticks - hrt_absolute_time_base);

        // ====== FIX: Set RA register for compare interrupt ======
        putreg32(compare, rRA);

        // ====== FIX: Enable RA compare interrupt ======
        putreg32(TC_INT_CPAS, rIER);  // Enable CPAS (RA compare) interrupt

        hrtinfo("Scheduled callback at %llu µs (RA=0x%08x)\n",
                deadline_usec, compare);
    } else {
        // No callbacks pending, disable RA interrupt
        putreg32(TC_INT_CPAS, rIDR);  // Disable CPAS interrupt
    }
}
```

---

**Change 2: Update hrt_tim_isr() - Handle both interrupts**

**Location:** Lines 258-285

```c
/**
 * HRT interrupt handler
 * Handles two interrupt sources:
 * 1. TC_INT_CPCS: RC compare (overflow at 0xFFFFFFFF)
 * 2. TC_INT_CPAS: RA compare (scheduled callback deadline)
 */
static int hrt_tim_isr(int irq, void *context, void *arg)
{
    uint32_t status;

    // Read and clear status (reading SR clears interrupt flags)
    status = getreg32(rSR);

    // Handle counter overflow (RC compare)
    if (status & TC_INT_CPCS) {
        // Add full 32-bit range to base time
        hrt_absolute_time_base += 0x100000000ULL;
        hrt_counter_wrap_count++;
        hrtinfo("HRT counter wrapped (count=%u)\n", hrt_counter_wrap_count);
    }

    // ====== FIX: Handle callback deadline (RA compare) ======
    if (status & TC_INT_CPAS) {
        // Disable RA interrupt until next callback scheduled
        putreg32(TC_INT_CPAS, rIDR);

        // Invoke scheduled callbacks
        hrt_call_invoke();

        // Schedule next callback (will re-enable RA interrupt if needed)
        hrt_call_reschedule();
    }

    return OK;
}
```

---

### Why This Works:

**Hardware Compare Mechanism:**

```
TC0 continuously compares CV (counter value) with RA register:

Setup:
  Current time: 1,000,000 µs
  Callback deadline: 1,002,000 µs (2ms from now)

  Convert to ticks: (1,002,000 * 4,687,500) / 1,000,000 = 4,696,875 ticks
  Base ticks: 0
  RA = 4,696,875

Timer operation:
  CV counts: 4,696,870, 4,696,871, 4,696,872...
  CV == RA: Hardware sets TC_INT_CPAS flag
  Interrupt fires (latency: 2-3 µs)
  ISR calls callback function

Total latency: 2-5 µs (vs 50-500 µs with polling!)
```

---

### Testing Fix 3:

**Test callback timing:**

```bash
nsh> mc_rate_control start
nsh> mc_rate_control status

# Expected output:
# mc_rate_control: cycle: 500 events, 1000000us elapsed, 2000.0us avg
#   min 1995us max 2005us 2.5us rms
#
# This shows:
# - Running at 500 Hz (2000µs period) ✓
# - Low jitter (2.5µs rms) ✓
# - Precise timing (min/max within 5µs) ✓

nsh> top
# Check CPU usage - HRT should be <1%
```

**Success Criteria:**
- ✅ Control loops run at exact frequencies (500Hz, 250Hz, 50Hz)
- ✅ Low timing jitter (<5 µs RMS)
- ✅ Low CPU overhead (<1% for HRT)
- ✅ Callbacks fire with <10 µs latency

---

## Implementation Steps

### Step 1: Apply Fix 1 (Time Units)

```bash
# 1. Edit file
nano platforms/nuttx/src/px4/microchip/samv7/hrt/hrt.c

# 2. Modify hrt_absolute_time() function (lines 124-141)
#    Add conversion: (ticks * 1000000) / HRT_ACTUAL_FREQ

# 3. Build
make microchip_samv71-xult-clickboards_default

# 4. Flash
make microchip_samv71-xult-clickboards_default upload

# 5. Test (see Fix 1 Testing section above)
```

---

### Step 2: Apply Fix 2 (Overflow Interrupt)

```bash
# 1. Edit same file
nano platforms/nuttx/src/px4/microchip/samv7/hrt/hrt.c

# 2. Modify hrt_init() (lines 146-205)
#    Add: irq_attach(), enable TC_INT_CPCS, up_enable_irq()

# 3. Modify hrt_tim_isr() (lines 258-277)
#    Add: overflow handling (base += 0x100000000ULL)

# 4. Build and flash
make microchip_samv71-xult-clickboards_default
make microchip_samv71-xult-clickboards_default upload

# 5. Test (wait 16 minutes, see Fix 2 Testing section)
```

---

### Step 3: Apply Fix 3 (Callback Interrupts)

```bash
# 1. Edit same file
nano platforms/nuttx/src/px4/microchip/samv7/hrt/hrt.c

# 2. Modify hrt_call_reschedule() (lines 240-252)
#    Add: RA register programming, TC_INT_CPAS enable

# 3. Update hrt_tim_isr() (lines 258-285)
#    Add: RA compare handling

# 4. Build and flash
make microchip_samv71-xult-clickboards_default
make microchip_samv71-xult-clickboards_default upload

# 5. Test (see Fix 3 Testing section)
```

---

## Verification Tests

### After All Fixes Applied:

**1. Time Units Test:**
```bash
nsh> hrt_absolute_time
# Record value
nsh> sleep 5
nsh> hrt_absolute_time
# Should increase by ~5,000,000 (5 seconds in microseconds)
```

**2. Overflow Test:**
```bash
# Run system for 16+ minutes
nsh> dmesg | grep wrapped
# Should see wrap messages every ~15 minutes
```

**3. Callback Test:**
```bash
nsh> mc_rate_control start
nsh> mc_rate_control status
# Check: cycle rate should be exactly 500 Hz (2000 µs period)
```

**4. EKF2 Integration Test:**
```bash
nsh> ekf2 start
nsh> ekf2 status
# Check: dt values should be ~0.008-0.010 s
# Check: No "time went backwards" errors
```

**5. Full System Test:**
```bash
# Start all modules
nsh> commander start
nsh> sensors start
nsh> mc_att_control start
nsh> mc_pos_control start
nsh> logger start

# Run for 30 minutes
# Monitor: dmesg for errors
# Check: top for CPU usage
# Verify: No crashes or timing anomalies
```

---

## Risk Mitigation

### Potential Issues:

**1. Integer Overflow in Conversion**
```c
// Bad:
uint32_t usec = (ticks * 1000000) / freq;  // May overflow at 2^32!

// Good:
uint64_t usec = (ticks * 1000000ULL) / freq;  // ULL = unsigned long long
```

**2. Division by Zero**
```c
// Already safe: HRT_ACTUAL_FREQ is compile-time constant
// Compiler will error if zero
```

**3. Interrupt Nesting**
```c
// Use enter_critical_section() in hrt_absolute_time()
// Already present in current code
```

**4. Race Condition on Base Update**
```c
// ISR updates base, hrt_absolute_time() reads it
// Solution: Use atomic 64-bit read or critical section (already used)
```

---

## Rollback Plan

If fixes cause issues:

```bash
# Revert changes
git checkout platforms/nuttx/src/px4/microchip/samv7/hrt/hrt.c

# Rebuild
make microchip_samv71-xult-clickboards_default
make microchip_samv71-xult-clickboards_default upload
```

Or revert individual fixes by commenting out specific changes.

---

## Expected Results

**After all 3 fixes:**

- ✅ Time in microseconds (not ticks)
- ✅ No time jumps after 15 minutes
- ✅ Precise callback timing (2-5 µs latency)
- ✅ Low CPU overhead (<1%)
- ✅ EKF2 timing correct
- ✅ Control loops stable
- ✅ Ready for HIL testing

**Performance Comparison:**

| Metric | Before Fixes | After Fixes | STM32 Reference |
|--------|--------------|-------------|-----------------|
| Time units | 4.7 MHz ticks | 1 MHz µs ✓ | 1 MHz µs |
| Wraparound | 15 min crash | Never ✓ | Never |
| Callback latency | 50-500 µs | 2-5 µs ✓ | 2-5 µs |
| CPU overhead | 0.3-6% | <1% ✓ | <1% |

---

## Timeline Estimate

**Fix 1 (Time Units):** 30-60 minutes
- Edit: 15 min
- Build/flash: 5 min
- Test: 10-30 min

**Fix 2 (Overflow):** 1-2 hours
- Edit: 20 min
- Build/flash: 5 min
- Test: 30-60 min (need to wait 16 min)

**Fix 3 (Callbacks):** 1-2 hours
- Edit: 30 min
- Build/flash: 5 min
- Test: 30-60 min

**Total: 3-5 hours** for all three fixes including testing.

---

**Document Version:** 1.0
**Date:** 2025-11-13
**Status:** Ready for implementation
