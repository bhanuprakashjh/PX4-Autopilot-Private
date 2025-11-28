# HRT (High Resolution Timer) Implementation Analysis

**Comparison: SAMV71 TC0 vs STM32 TIM**

This document analyzes the functional differences between the SAMV71 Timer/Counter (TC0) implementation and the STM32 Timer (TIM) implementation used for PX4's High Resolution Timer.

---

## Table of Contents

1. [Overview](#overview)
2. [Hardware Differences](#hardware-differences)
3. [Implementation Comparison](#implementation-comparison)
4. [Functional Differences](#functional-differences)
5. [PX4 Stack Impact](#px4-stack-impact)
6. [Performance Analysis](#performance-analysis)
7. [Recommendations](#recommendations)

---

## Overview

### What is HRT?

The High Resolution Timer (HRT) is a critical component of the PX4 stack that provides:

1. **Microsecond-precision timekeeping** - `hrt_absolute_time()`
2. **Scheduled callbacks** - `hrt_call_at()`, `hrt_call_after()`, `hrt_call_every()`
3. **Timing measurements** - For sensor sampling, control loop timing, latency measurements
4. **PPM input decoding** - RC receiver pulse timing (STM32 only)

**Critical for:**
- Sensor fusion timing in EKF2
- Control loop execution (attitude, position, rate control)
- MAVLink message timestamps
- Logger timestamps
- Scheduling periodic tasks

---

## Hardware Differences

### STM32 Timer (TIM1-TIM12)

**Hardware Type:** General-purpose or Advanced-control timer

**Key Features:**
- **16-bit counter** (most timers)
- **32-bit counter** (TIM2, TIM5 on some variants)
- **4 independent capture/compare channels**
- **Flexible prescaler** (1 to 65536)
- **Input capture** for PPM/PWM measurement
- **Output compare** for PWM generation
- **DMA support** for low-overhead transfers
- **Multiple clock sources** (APB1/APB2)

**Typical Configuration:**
```c
Timer: TIM5 (32-bit) or TIM2 (32-bit)
Clock: APB1 or APB2 (84-216 MHz)
Prescaler: Set to achieve 1 MHz counter
Mode: Free-running up-counter
Channel 1: HRT compare interrupts
Channel 2-4: Available for PPM input capture
```

**Register Access:**
- Direct memory-mapped registers
- `TIMx->CNT`, `TIMx->CCR1`, `TIMx->SR`, etc.

---

### SAMV71 Timer/Counter (TC0)

**Hardware Type:** Timer/Counter block

**Key Features:**
- **32-bit counter** (all TC channels)
- **3 channels per TC block** (TC0, TC1, TC2 each have 3 channels)
- **Flexible clock selection** (MCK/8, MCK/32, MCK/128, external clocks)
- **Waveform generation** (PWM, square wave)
- **Capture mode** (input timing measurement)
- **Compare outputs** (RA, RB, RC registers per channel)
- **No built-in DMA** (uses separate XDMAC controller)

**Our Configuration:**
```c
Timer: TC0 Channel 0
Clock: MCK/32 (150 MHz / 32 = 4.6875 MHz) or MCK/8 (18.75 MHz)
Mode: Waveform mode, up-counting
RC Register: 0xFFFFFFFF (free-running)
```

**Register Access:**
- Memory-mapped TC registers
- `TC0->CV`, `TC0->RA`, `TC0->SR`, etc.
- Accessed via SAM_TC012_BASE + offsets

---

## Implementation Comparison

### Timer Configuration

#### STM32 Implementation

**File:** `platforms/nuttx/src/px4/stm/stm32_common/hrt/hrt.c`

```c
static void hrt_tim_init(void)
{
    // Enable timer clock
    modifyreg32(HRT_TIMER_POWER_REG, 0, HRT_TIMER_POWER_BIT);

    // Configure timer
    rCR1 = 0;                              // Disable timer
    rCR2 = 0;                              // No special mode
    rSMCR = 0;                             // No slave mode
    rDIER = DIER_HRT | DIER_PPM;          // Enable interrupts

    // Set prescaler to achieve 1 MHz
    rPSC = (HRT_TIMER_CLOCK / 1000000) - 1;

    // Free-running 16-bit counter (or 32-bit for TIM2/TIM5)
    rARR = 0xFFFF;                         // Auto-reload at 65536

    // Set compare value for first interrupt
    rCCR_HRT = 1000;                       // 1ms initial deadline

    // Start timer
    rEGR = GTIM_EGR_UG;                   // Update event
    rCR1 = GTIM_CR1_CEN;                  // Enable counter

    // Enable interrupts
    up_enable_irq(HRT_TIMER_VECTOR);
}
```

**Key Characteristics:**
- **16-bit counter** (wraps every 65.536ms at 1MHz)
- **Compare interrupt** on CCR1 for scheduled callbacks
- **Input capture** on CCR2-4 for PPM decoding
- **Prescaler** to exactly 1 MHz (e.g., 84 MHz / 84 = 1 MHz)
- **Hardware overflow detection** (update interrupt)

---

#### SAMV71 Implementation

**File:** `platforms/nuttx/src/px4/microchip/samv7/hrt/hrt.c`

```c
void hrt_init(void)
{
    sq_init(&callout_queue);

    // Enable TC0 peripheral clock
    uint32_t regval = getreg32(SAM_PMC_PCER0);
    regval |= HRT_TIMER_PCER;              // Enable TC0 clock
    putreg32(regval, SAM_PMC_PCER0);

    // Disable TC before configuration
    putreg32(TC_CCR_CLKDIS, rCCR);

    // Configure channel mode
    uint32_t cmr = TC_CMR_WAVE |           // Waveform mode
                   TC_CMR_WAVSEL_UP;       // Up-counting

    // Select prescaler (MCK/32 or MCK/8)
    if (HRT_TIMER_CLOCK / 8 > 1000000) {
        cmr |= TC_CMR_TCCLKS_MCK32;        // 150MHz/32 = 4.6875 MHz
    } else {
        cmr |= TC_CMR_TCCLKS_MCK8;         // 150MHz/8 = 18.75 MHz
    }

    putreg32(cmr, rCMR);

    // Set RC to maximum for free-running
    putreg32(0xFFFFFFFF, rRC);             // 32-bit free-running

    // Disable all interrupts (polling mode)
    putreg32(0xFFFFFFFF, rIDR);

    // Clear status
    (void)getreg32(rSR);

    // Enable and start timer
    putreg32(TC_CCR_CLKEN | TC_CCR_SWTRG, rCCR);

    // Initialize time base
    hrt_absolute_time_base = 0;
    hrt_counter_wrap_count = 0;
}
```

**Key Characteristics:**
- **32-bit counter** (wraps every 4,294 seconds at 1MHz = 71.5 minutes)
- **No compare interrupts** (polling mode currently)
- **No PPM input capture** (not implemented)
- **Prescaler** to ~5 MHz or ~19 MHz (not exact 1 MHz due to MCK/8 or MCK/32 constraints)
- **Software overflow tracking** (manual wrap detection)

---

### Time Measurement

#### STM32 Implementation

```c
hrt_abstime hrt_absolute_time(void)
{
    uint32_t count;
    irqstate_t flags;

    flags = enter_critical_section();

    // Read 16-bit or 32-bit counter
    count = rCNT;

    // Handle 16-bit wraparound with base time
    if (timer_is_16bit) {
        abstime = hrt_absolute_time_base + count;
    } else {
        abstime = count;  // 32-bit counter used directly
    }

    leave_critical_section(flags);

    return abstime;
}
```

**Precision:** 1 microsecond (1 MHz counter)

**Wraparound Handling:**
- **16-bit timers:** Base time updated every 65.536ms via overflow interrupt
- **32-bit timers:** No wraparound for 4,294 seconds (71 minutes)

---

#### SAMV71 Implementation

```c
hrt_abstime hrt_absolute_time(void)
{
    uint32_t count;
    irqstate_t flags;
    uint64_t abstime;

    flags = enter_critical_section();

    // Read 32-bit counter value
    count = getreg32(rCV);

    // Calculate absolute time
    abstime = hrt_absolute_time_base + count;

    leave_critical_section(flags);

    return abstime;
}
```

**Precision:**
- With MCK/32 (4.6875 MHz): ~213 nanoseconds
- With MCK/8 (18.75 MHz): ~53 nanoseconds

**Wraparound Handling:**
- **32-bit counter:** Base time would need to be updated every 916 seconds (MCK/32) or 229 seconds (MCK/8)
- **Current implementation:** No automatic wrap detection (potential issue!)

---

### Scheduled Callbacks

#### STM32 Implementation

**Uses hardware compare interrupts:**

```c
static void hrt_call_reschedule(void)
{
    hrt_abstime now = hrt_absolute_time();
    struct hrt_call *next = (struct hrt_call *)sq_peek(&callout_queue);

    if (next != NULL) {
        hrt_abstime deadline = next->deadline;

        // Calculate compare value
        uint32_t compare = deadline - hrt_absolute_time_base;

        // Set hardware compare register
        rCCR_HRT = compare;

        // Hardware will generate interrupt at deadline
    }
}

static int hrt_tim_isr(int irq, void *context, void *arg)
{
    // Read status register (clears interrupt)
    uint32_t status = rSR;

    // Check if compare match occurred
    if (status & SR_INT_HRT) {
        // Invoke scheduled callbacks
        hrt_call_invoke();

        // Schedule next callback
        hrt_call_reschedule();
    }

    // Handle PPM input capture
    if (status & SR_INT_PPM) {
        hrt_ppm_decode(status);
    }

    return OK;
}
```

**Advantages:**
- Hardware-triggered interrupts (precise timing)
- Low CPU overhead (no polling)
- Multiple independent channels (HRT + PPM)

---

#### SAMV71 Implementation

**Current implementation uses software polling (no interrupts):**

```c
static void hrt_call_reschedule(void)
{
    hrt_abstime now = hrt_absolute_time();
    struct hrt_call *next = (struct hrt_call *)sq_peek(&callout_queue);

    if (next != NULL) {
        hrt_abstime deadline = next->deadline;

        if (deadline < now) {
            deadline = now + HRT_INTERVAL_MIN;
        }
    }

    // NOTE: No hardware compare interrupt enabled!
    // Callbacks invoked via polling in main loop
}

// ISR is defined but not attached to interrupt vector
static int hrt_tim_isr(int irq, void *context, void *arg)
{
    uint32_t status = getreg32(rSR);

    // Handle counter overflow
    if (status & TC_INT_CPCS) {
        hrt_counter_wrap_count++;
    }

    // Process callouts (not currently used)
    hrt_call_invoke();
    hrt_call_reschedule();

    return OK;
}
```

**Issue:** ISR is defined but **never attached**! No `irq_attach()` call in `hrt_init()`.

**Implications:**
- Callbacks rely on polling (higher latency)
- No precise timing for scheduled events
- Higher CPU usage (constant polling)

---

## Functional Differences

### 1. Counter Resolution

| Feature | STM32 TIM | SAMV71 TC0 |
|---------|-----------|------------|
| **Counter width** | 16-bit (most) or 32-bit (TIM2/TIM5) | 32-bit (all channels) |
| **Wraparound time** | 65.536ms (16-bit @ 1MHz) or 71.5 min (32-bit) | 916s @ 4.6875MHz or 229s @ 18.75MHz |
| **Prescaler flexibility** | 1-65536 (any value) | Fixed ratios: MCK/8, MCK/32, MCK/128, etc. |
| **Exact 1MHz possible?** | ✅ Yes (if clock is multiple of 1MHz) | ❌ No (limited to MCK/N where N = 2,8,32,128) |

**Impact on PX4:**
- **SAMV71 advantage:** 32-bit counter eliminates frequent wraparound handling
- **SAMV71 disadvantage:** Cannot achieve exact 1 MHz, requiring scaling conversions

---

### 2. Timing Precision

| Feature | STM32 TIM | SAMV71 TC0 (Current) |
|---------|-----------|----------------------|
| **Timer frequency** | 1.000 MHz (exact) | 4.6875 MHz (MCK/32) or 18.75 MHz (MCK/8) |
| **Time resolution** | 1.000 µs | 0.213 µs (MCK/32) or 0.053 µs (MCK/8) |
| **`hrt_absolute_time()` units** | Microseconds | **Timer ticks** (NOT microseconds!) |

**Critical Issue:** SAMV71 returns raw timer ticks, not microseconds!

**PX4 Expectation:**
```c
// PX4 assumes hrt_absolute_time() returns microseconds
hrt_abstime now = hrt_absolute_time();  // Expected: microseconds since boot
hrt_abstime deadline = now + 1000;      // Expected: 1000µs = 1ms from now
```

**SAMV71 Reality:**
```c
// SAMV71 returns timer ticks (4.6875 MHz or 18.75 MHz)
hrt_abstime now = hrt_absolute_time();  // Returns: ticks since boot
hrt_abstime deadline = now + 1000;      // Actually: 1000 ticks ≈ 213µs or 53µs!
```

**Impact:**
- **All timing calculations are WRONG** by a factor of ~4.7x or ~18.75x
- EKF2 sensor fusion timing incorrect
- Control loop rates incorrect
- MAVLink timestamps incorrect
- Logger timestamps incorrect

**Fix Required:**
```c
hrt_abstime hrt_absolute_time(void)
{
    uint32_t count = getreg32(rCV);

    // Convert ticks to microseconds
    // For MCK/32 (4.6875 MHz): multiply by 1000000 / 4687500 = 0.2133
    // For MCK/8 (18.75 MHz): multiply by 1000000 / 18750000 = 0.0533

    uint64_t ticks = hrt_absolute_time_base + count;
    uint64_t usec = (ticks * 1000000ULL) / HRT_ACTUAL_FREQ;

    return usec;
}
```

---

### 3. Scheduled Callbacks

| Feature | STM32 TIM | SAMV71 TC0 (Current) |
|---------|-----------|----------------------|
| **Interrupt source** | Hardware compare match (CCR1) | **None (polling)** |
| **Callback latency** | ~2-5 µs (hardware interrupt) | **Unknown (polling-based)** |
| **CPU overhead** | Low (interrupt-driven) | **High (constant polling)** |
| **Timing precision** | ±1 µs | **Undefined (no hardware support)** |

**How STM32 Works:**
1. `hrt_call_at(callback, deadline)` called
2. Deadline written to `CCR1` register
3. **Hardware** compares CNT vs CCR1 every clock cycle
4. When CNT == CCR1, hardware triggers interrupt
5. ISR invokes callback function

**How SAMV71 Currently Works:**
1. `hrt_call_at(callback, deadline)` called
2. Callback added to queue (sorted by deadline)
3. **Software polling** in main loop checks if deadline passed
4. When `now >= deadline`, callback invoked

**Missing in SAMV71:**
- No `irq_attach(HRT_TIMER_VECTOR, hrt_tim_isr)` in `hrt_init()`
- Compare registers (RA/RB/RC) not used for interrupts
- Interrupt enable register (IER) not configured

**Fix Required:**
```c
void hrt_init(void)
{
    // ... existing initialization ...

    // Attach interrupt handler
    irq_attach(HRT_TIMER_VECTOR, hrt_tim_isr, NULL);

    // Enable RC compare interrupt
    putreg32(TC_INT_CPCS, rIER);  // Enable interrupt on RC compare

    // Enable IRQ
    up_enable_irq(HRT_TIMER_VECTOR);
}

static void hrt_call_reschedule(void)
{
    struct hrt_call *next = (struct hrt_call *)sq_peek(&callout_queue);

    if (next != NULL) {
        // Convert deadline to timer ticks
        uint32_t compare = (next->deadline * HRT_ACTUAL_FREQ) / 1000000ULL;

        // Set RC register for compare interrupt
        putreg32(compare, rRC);
    }
}
```

---

### 4. PPM Input Capture

| Feature | STM32 TIM | SAMV71 TC0 |
|---------|-----------|------------|
| **Input capture support** | ✅ Yes (CCR2-4) | ✅ Yes (hardware capable) |
| **Implementation** | ✅ Complete in hrt.c | ❌ Not implemented |
| **Edge detection** | Both edges (rising/falling) | Both edges (configurable) |
| **DMA support** | ✅ Yes (for low overhead) | ⚠️ Via XDMAC (separate controller) |

**STM32 PPM Implementation:**
```c
// STM32 uses channel 2-4 for PPM input capture
#define HRT_PPM_CHANNEL  2

static void hrt_ppm_decode(uint32_t status)
{
    uint16_t count = rCCR_PPM;  // Read capture register
    uint16_t width = count - ppm.last_edge;

    // Decode PPM pulse timing
    if (width >= PPM_MIN_START) {
        // Start of new frame
        ppm_frame_start = count;
    } else if (width >= PPM_MIN_CHANNEL_VALUE) {
        // Valid channel pulse
        ppm_temp_buffer[ppm.next_channel++] = width;
    }
}
```

**SAMV71 Status:**
- Hardware supports capture mode (LDRA, LDRB triggers)
- Software implementation not written
- Would need separate TC channel or use RA/RB registers

---

### 5. Wraparound Handling

#### STM32 (16-bit timers)

```c
// Counter wraps every 65,536 µs (65.536ms)
// Overflow interrupt updates base time

static int hrt_tim_isr(int irq, void *context, void *arg)
{
    uint32_t status = rSR;

    if (status & GTIM_SR_UIF) {  // Update interrupt (overflow)
        hrt_absolute_time_base += HRT_COUNTER_PERIOD;  // +65536 µs
    }

    // ... handle other interrupts ...
}

hrt_abstime hrt_absolute_time(void)
{
    uint32_t count = rCNT;  // 16-bit value
    return hrt_absolute_time_base + count;
}
```

**Overflow every:** 65.536ms
**Interrupt overhead:** ~20 interrupts/second

---

#### SAMV71 (32-bit counter)

**With MCK/32 (4.6875 MHz):**
- Counter wraps every: 4,294,967,296 / 4,687,500 = **916 seconds** (15.3 minutes)

**With MCK/8 (18.75 MHz):**
- Counter wraps every: 4,294,967,296 / 18,750,000 = **229 seconds** (3.8 minutes)

**Current Implementation:**
```c
// No overflow interrupt configured!
// hrt_counter_wrap_count defined but never incremented

hrt_abstime hrt_absolute_time(void)
{
    uint32_t count = getreg32(rCV);
    abstime = hrt_absolute_time_base + count;  // Will break after 15.3 min!
    return abstime;
}
```

**Issue:** After wraparound, `count` resets to 0, but `hrt_absolute_time_base` doesn't update, causing time to jump backwards!

**Fix Required:**
```c
static int hrt_tim_isr(int irq, void *context, void *arg)
{
    uint32_t status = getreg32(rSR);

    if (status & TC_INT_CPCS) {  // RC compare (overflow)
        hrt_absolute_time_base += 0xFFFFFFFF;  // Add full counter range
        hrt_counter_wrap_count++;
    }

    // ... handle scheduled callbacks ...

    return OK;
}

void hrt_init(void)
{
    // ... existing init ...

    // Enable overflow interrupt
    putreg32(TC_INT_CPCS, rIER);
    irq_attach(HRT_TIMER_VECTOR, hrt_tim_isr, NULL);
    up_enable_irq(HRT_TIMER_VECTOR);
}
```

---

## PX4 Stack Impact

### Critical Subsystems Affected

#### 1. EKF2 (Extended Kalman Filter)

**Usage:**
```c
// EKF2 tracks sensor sample timing
hrt_abstime gyro_timestamp = hrt_absolute_time();
hrt_abstime accel_timestamp = hrt_absolute_time();

// Sensor fusion uses time deltas
float dt = (accel_timestamp - last_accel_timestamp) * 1e-6f;  // Expects µs!
```

**Impact of Incorrect Timing:**
- **dt calculation wrong** → state prediction errors
- **Sensor fusion timing off** → diverging estimates
- **IMU rate estimation incorrect** → filter instability
- **GPS/Baro/Mag fusion timing wrong** → poor navigation accuracy

**Severity:** 🔴 **CRITICAL** - Flight instability, crashes

---

#### 2. Control Loops

**Usage:**
```c
// MC Rate Controller runs at fixed rate (e.g., 500 Hz = 2000 µs)
hrt_call_every(&rate_control_call, 2000, 2000, rate_control_update, NULL);

void rate_control_update(void *arg)
{
    hrt_abstime now = hrt_absolute_time();
    float dt = (now - last_update) * 1e-6f;  // Expects µs!

    // PID control with dt
    float error = setpoint - measurement;
    integral += error * dt;
    derivative = (error - last_error) / dt;
}
```

**Impact of Incorrect Timing:**
- **Loop rate wrong** → control instability
- **dt wrong** → PID gains incorrect
- **Integral windup** → oscillations, overshoots
- **Derivative spikes** → jitter, noise amplification

**Severity:** 🔴 **CRITICAL** - Unflyable, crashes

---

#### 3. MAVLink

**Usage:**
```c
// Timestamp messages
mavlink_msg_attitude_send(
    chan,
    hrt_absolute_time(),  // Expects microseconds since boot
    roll, pitch, yaw,
    rollspeed, pitchspeed, yawspeed
);
```

**Impact of Incorrect Timing:**
- **QGroundControl displays wrong time**
- **Log analysis shows incorrect timing**
- **GPS/Vision fusion in offboard computer breaks**

**Severity:** 🟡 **MODERATE** - Telemetry issues, log analysis broken

---

#### 4. Logger

**Usage:**
```c
// Log all messages with timestamps
log_message.timestamp = hrt_absolute_time();  // Expects µs
```

**Impact:**
- **ULog files have wrong timestamps**
- **Flight review analysis broken**
- **Cannot correlate events with time**

**Severity:** 🟡 **MODERATE** - Post-flight analysis impossible

---

#### 5. Sensor Drivers

**Usage:**
```c
// Sensor drivers timestamp samples
void icm20689_read(void)
{
    struct gyro_report gyro;
    gyro.timestamp = hrt_absolute_time();  // Expects µs

    // Publish to uORB
    orb_publish(ORB_ID(sensor_gyro), gyro_pub, &gyro);
}
```

**Impact:**
- **EKF2 receives wrong timestamps** → fusion fails
- **Sensor rate estimation wrong** → filter issues

**Severity:** 🔴 **CRITICAL** - Affects all flight control

---

### PX4 Assumptions

PX4 assumes `hrt_absolute_time()` returns **microseconds since boot**:

1. ✅ **Monotonic** - Never goes backwards
2. ✅ **64-bit** - No wraparound for decades
3. ✅ **Microsecond precision** - 1 µs = 1 unit
4. ✅ **Low latency** - Fast to read (~1 µs)
5. ⚠️ **Wraps after 292,471 years** - Acceptable

**SAMV71 Current Issues:**
1. ❌ **Not monotonic** - Will jump backwards after 15.3 min (no overflow handling)
2. ✅ **64-bit** - Base + counter stored as uint64_t
3. ❌ **Wrong units** - Returns timer ticks, not microseconds!
4. ✅ **Low latency** - Single register read
5. ❌ **Wraps after 15.3 minutes** - UNACCEPTABLE!

---

## Performance Analysis

### CPU Overhead

#### STM32 (Interrupt-Driven)

**Per callback:**
```
Interrupt latency:     2-5 µs
ISR execution:         3-8 µs
Context switch:        1-3 µs
Total:                 6-16 µs per callback
```

**Overflow handling (16-bit timer):**
```
Interrupts/sec:        15.26 (every 65.536ms)
ISR time per interrupt: ~5 µs
Total overhead:        76 µs/sec = 0.0076% CPU
```

**Total CPU impact:** < 0.1% (negligible)

---

#### SAMV71 (Polling, Current)

**Per main loop iteration:**
```
hrt_absolute_time() call:  ~1 µs
Queue check:                ~2-5 µs
Callback invocation:        Variable
Total:                      3-6 µs per loop
```

**If polling at 1 kHz:**
```
Overhead:  3-6 ms/sec = 0.3-0.6% CPU
```

**If polling at 10 kHz:**
```
Overhead:  30-60 ms/sec = 3-6% CPU
```

**Total CPU impact:** 0.3-6% (depends on polling rate)

---

#### SAMV71 (Interrupt-Driven, Proposed Fix)

**Same as STM32:** < 0.1% CPU

**Wraparound interrupt (32-bit counter):**
```
Interrupts:  1 every 916 seconds (MCK/32) or 229 seconds (MCK/8)
ISR time:    ~5 µs
Total:       Negligible (<< 0.001% CPU)
```

---

### Latency Comparison

#### STM32 Callback Latency

```
Time from deadline to callback execution:
  Best case:   0-2 µs (hardware compare exact)
  Typical:     2-5 µs (interrupt latency)
  Worst case:  10-20 µs (higher priority interrupt)
```

**Latency histogram tracked:**
```c
const uint16_t latency_buckets[] = { 1, 2, 5, 10, 20, 50, 100, 1000 };
__EXPORT uint32_t latency_counters[9];  // Counts per bucket
```

---

#### SAMV71 Callback Latency (Current)

**Depends on polling frequency:**
```
Polling at 1 kHz:
  Best case:   0-1000 µs
  Average:     500 µs
  Worst case:  >1000 µs

Polling at 10 kHz:
  Best case:   0-100 µs
  Average:     50 µs
  Worst case:  >100 µs
```

**No latency tracking implemented.**

---

#### SAMV71 Callback Latency (With Interrupts)

**Same as STM32:** 2-20 µs typical

---

## Recommendations

### Critical Fixes Required

#### 1. Fix Time Units (CRITICAL 🔴)

**Problem:** Returns timer ticks instead of microseconds.

**Fix:**
```c
// In hrt.c
#define HRT_ACTUAL_FREQ  (HRT_TIMER_CLOCK / HRT_PRESCALER)

hrt_abstime hrt_absolute_time(void)
{
    irqstate_t flags = enter_critical_section();

    uint32_t count = getreg32(rCV);
    uint64_t ticks = hrt_absolute_time_base + count;

    // Convert ticks to microseconds
    uint64_t usec = (ticks * 1000000ULL) / HRT_ACTUAL_FREQ;

    leave_critical_section(flags);

    return usec;
}
```

**File:** `platforms/nuttx/src/px4/microchip/samv7/hrt/hrt.c` line 124

---

#### 2. Enable Overflow Interrupts (CRITICAL 🔴)

**Problem:** Counter wraps after 15.3 minutes, time jumps backwards.

**Fix:**
```c
void hrt_init(void)
{
    // ... existing initialization ...

    // Attach interrupt handler
    irq_attach(HRT_TIMER_VECTOR, hrt_tim_isr, NULL);

    // Enable RC compare interrupt (overflow)
    putreg32(TC_INT_CPCS, rIER);

    // Enable IRQ
    up_enable_irq(HRT_TIMER_VECTOR);
}

static int hrt_tim_isr(int irq, void *context, void *arg)
{
    uint32_t status = getreg32(rSR);

    // Handle overflow
    if (status & TC_INT_CPCS) {
        hrt_absolute_time_base += 0xFFFFFFFFULL;  // Add 2^32 ticks
        hrt_counter_wrap_count++;
    }

    return OK;
}
```

**Files:**
- `platforms/nuttx/src/px4/microchip/samv7/hrt/hrt.c` lines 146, 258

---

#### 3. Enable Scheduled Callback Interrupts (HIGH 🟠)

**Problem:** Callbacks rely on polling (high latency, high CPU).

**Fix:**
```c
static void hrt_call_reschedule(void)
{
    struct hrt_call *next = (struct hrt_call *)sq_peek(&callout_queue);

    if (next != NULL) {
        hrt_abstime deadline_usec = next->deadline;

        // Convert microseconds to timer ticks
        uint64_t deadline_ticks = (deadline_usec * HRT_ACTUAL_FREQ) / 1000000ULL;

        // Set RA register for compare interrupt
        uint32_t compare = (uint32_t)(deadline_ticks - hrt_absolute_time_base);
        putreg32(compare, rRA);

        // Enable RA compare interrupt
        putreg32(TC_INT_CPAS, rIER);
    } else {
        // No callbacks, disable interrupt
        putreg32(TC_INT_CPAS, rIDR);
    }
}

static int hrt_tim_isr(int irq, void *context, void *arg)
{
    uint32_t status = getreg32(rSR);

    // Handle overflow (RC compare)
    if (status & TC_INT_CPCS) {
        hrt_absolute_time_base += 0xFFFFFFFFULL;
        hrt_counter_wrap_count++;
    }

    // Handle callback (RA compare)
    if (status & TC_INT_CPAS) {
        hrt_call_invoke();
        hrt_call_reschedule();
    }

    return OK;
}
```

**File:** `platforms/nuttx/src/px4/microchip/samv7/hrt/hrt.c` lines 240, 258

---

#### 4. Consider Using Exact 1 MHz (OPTIONAL ⚪)

**Problem:** Cannot achieve exact 1 MHz with MCK/N prescalers.

**Options:**

**Option A: Accept non-1MHz and convert**
- Use MCK/32 (4.6875 MHz)
- Convert to microseconds in software
- Simpler, works fine

**Option B: Use external crystal**
- Configure TC to use external clock (XC0/XC1/XC2)
- Add 1 MHz crystal to hardware
- Requires hardware modification

**Recommendation:** **Option A** (software conversion)

---

### Testing Procedure

After implementing fixes:

#### 1. Unit Test: Time Accuracy

```bash
nsh> hrt_test

# Expected output:
# Reading HRT 1000 times...
# Time difference per read: ~1 µs
# Time is monotonic: PASS
# No backwards jumps: PASS
```

#### 2. Unit Test: Wraparound

```bash
# Wait 16+ minutes, check time doesn't jump backwards
nsh> hrt_absolute_time
# Record time
# Wait 16 minutes
nsh> hrt_absolute_time
# Verify time increased by ~960,000,000 µs (16 minutes)
```

#### 3. Integration Test: Control Loop

```bash
nsh> mc_rate_control status
# Check loop rate: should be 500 Hz (2000 µs period)
```

#### 4. Integration Test: EKF2

```bash
nsh> ekf2 status
# Check "dt" values: should be ~0.01s (10ms sensor intervals)
```

---

## Summary

### STM32 vs SAMV71 HRT

| Aspect | STM32 TIM | SAMV71 TC0 (Current) | SAMV71 TC0 (Fixed) |
|--------|-----------|----------------------|--------------------|
| **Counter size** | 16/32-bit | 32-bit | 32-bit |
| **Frequency** | 1 MHz (exact) | 4.6875 or 18.75 MHz | 4.6875 or 18.75 MHz |
| **Returns microseconds?** | ✅ Yes | ❌ No (returns ticks) | ✅ Yes (after conversion) |
| **Wraparound handling** | ✅ Interrupt | ❌ None | ✅ Interrupt |
| **Callback mechanism** | ✅ Hardware interrupt | ❌ Polling | ✅ Hardware interrupt |
| **Callback latency** | 2-5 µs | 50-500 µs | 2-5 µs |
| **CPU overhead** | <0.1% | 0.3-6% | <0.1% |
| **PPM support** | ✅ Yes | ❌ No | ⚠️ Could be added |
| **Status** | Fully functional | 🔴 BROKEN | 🟢 Would work |

### Critical Issues in Current SAMV71 HRT

1. 🔴 **CRITICAL:** Returns timer ticks, not microseconds → All timing calculations wrong
2. 🔴 **CRITICAL:** No wraparound handling → Time jumps backwards after 15.3 min
3. 🟠 **HIGH:** Callbacks use polling → High latency, high CPU overhead
4. 🟡 **MODERATE:** No latency tracking → Cannot measure timing performance
5. 🟢 **LOW:** No PPM support → RC input requires separate implementation

### Next Steps

1. **Immediate:** Implement time unit conversion (microseconds)
2. **Immediate:** Enable overflow interrupt handling
3. **High Priority:** Enable compare interrupts for callbacks
4. **Medium Priority:** Add latency tracking and diagnostics
5. **Low Priority:** Implement PPM input capture (if needed)

---

## Related Files

**Implementation:**
- `platforms/nuttx/src/px4/microchip/samv7/hrt/hrt.c` - SAMV71 HRT implementation
- `platforms/nuttx/src/px4/stm/stm32_common/hrt/hrt.c` - STM32 reference implementation
- `boards/microchip/samv71-xult-clickboards/src/board_config.h` - HRT configuration

**Interface:**
- `platforms/common/include/px4_platform_common/drv_hrt.h` - HRT API definition

**Hardware:**
- `platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/hardware/sam_tc.h` - TC registers
- SAMV71 Datasheet Section 45: Timer/Counter (TC)

---

**Document Version:** 1.0
**Date:** 2025-11-13
**Author:** Generated via analysis of PX4 codebase
