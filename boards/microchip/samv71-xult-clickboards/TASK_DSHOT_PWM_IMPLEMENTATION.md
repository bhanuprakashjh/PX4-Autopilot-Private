# Engineering Task: DShot/PWM Motor Output Implementation

**Assigned To:** Engineer 5
**Priority:** HIGH (Critical for flight capability)
**Estimated Effort:** 5-10 days
**Prerequisites:** Deep understanding of timers, DMA, embedded systems, SAMV71 datasheet

---

## Executive Summary

The SAMV71-XULT board currently has **NO motor output capability**. This task involves implementing the io_timer layer and potentially DShot protocol support for the SAMV71 platform. This is the critical path to making the board flight-capable.

---

## Current State

### What Exists
```
platforms/nuttx/src/px4/microchip/samv7/
├── io_pins/
│   └── io_timer_stub.c      # Only stubs - NOT IMPLEMENTED
├── include/px4_arch/
│   ├── io_timer.h           # Header with function declarations
│   └── io_timer_hw_description.h
└── (no dshot.c)             # DShot not implemented
```

### What's Needed
1. **Phase A: Basic PWM** - Standard servo/ESC PWM output (1000-2000µs)
2. **Phase B: DShot** - Digital protocol for modern ESCs (optional but preferred)

---

## Research References

### DShot Protocol
- [Oscar Liang: What is DShot](https://oscarliang.com/dshot/) - Excellent protocol overview
- [PX4 DShot Documentation](https://docs.px4.io/main/en/peripherals/dshot.html)
- [ArduPilot DShot ESCs](https://ardupilot.org/copter/docs/common-dshot-escs.html)
- [Betaflight DShot Discussion](https://github.com/betaflight/betaflight/discussions/11425)

### SAMV71 Timer Counter
- [AVR Freaks: SAMV71 Timer Configuration](https://www.avrfreaks.net/forum/configure-timer-1-or-timer-2-sam-v71)
- [AVR Freaks: TIOB Output Configuration](https://www.avrfreaks.net/forum/sam-g55-xplained-pro-timer-counter-waveform-tiob0-pwm)
- SAMV71 Datasheet Chapter 48: Timer Counter (TC)

### PX4 Reference Implementations
- `platforms/nuttx/src/px4/stm/stm32_common/dshot/dshot.c` - STM32 DShot
- `platforms/nuttx/src/px4/stm/stm32_common/io_pins/io_timer.c` - STM32 io_timer
- `platforms/nuttx/src/px4/nxp/imxrt/dshot/dshot.c` - NXP i.MX RT DShot

---

## DShot Protocol Overview

### Timing Specifications

| Protocol | Bit Period | T0H (0 bit) | T1H (1 bit) | Frame Rate |
|----------|------------|-------------|-------------|------------|
| DShot150 | 6.67 µs | 2.50 µs | 5.00 µs | ~8 kHz |
| DShot300 | 3.33 µs | 1.25 µs | 2.50 µs | ~16 kHz |
| DShot600 | 1.67 µs | 0.625 µs | 1.25 µs | ~32 kHz |
| DShot1200 | 0.83 µs | 0.313 µs | 0.625 µs | ~64 kHz |

### Frame Format
```
┌─────────────────────────────────────────────────────────┐
│  11-bit Throttle  │ Telemetry │   4-bit CRC            │
│   (0-2047)        │   (1 bit) │   (XOR checksum)       │
└─────────────────────────────────────────────────────────┘
     Bits 15-5          Bit 4        Bits 3-0
```

### Implementation Approaches

**Approach 1: Timer + DMA Burst (STM32 style)**
- Timer generates base PWM frequency
- DMA bursts update CCR registers for each bit
- Most efficient, minimal CPU usage

**Approach 2: Bitbang with DMA to GPIO**
- Timer triggers DMA transfers
- DMA writes directly to GPIO registers
- Used when timer doesn't support CCR DMA burst

**Approach 3: Bitbang with Timer Interrupts**
- Timer interrupt toggles GPIO
- High CPU usage, not recommended

---

## SAMV71 Timer Counter Analysis

### TC Peripheral Overview

SAMV71 has 4 Timer Counter modules (TC0-TC3), each with 3 channels:
- 12 total TC channels available
- TC0 CH0 reserved for HRT (high-resolution timer)

| TC Module | Channels | Available | Notes |
|-----------|----------|-----------|-------|
| TC0 | CH0, CH1, CH2 | CH1, CH2 | CH0 = HRT |
| TC1 | CH0, CH1, CH2 | All | Free |
| TC2 | CH0, CH1, CH2 | All | Free |
| TC3 | CH0, CH1, CH2 | All | Free |

**Potential PWM Channels:** 11 (more than enough for a hexacopter)

### TC Waveform Mode (PWM)

Each TC channel can output PWM on TIOA and TIOB pins:
- **RA compare** → Controls TIOA transitions
- **RB compare** → Controls TIOB transitions
- **RC compare** → Period/overflow

```
                RC (period)
                    │
    ┌───────────────┴───────────────┐
    │                               │
    │   RA          RB              │
    │    │           │              │
────┘    └───────────┘              └────  TIOA
         ▲           ▲
         │           │
      duty start   duty end
```

### Critical Question: DMA to TC Registers?

**For DShot, we need to update RA/RB 16+ times per frame.**

Check SAMV71 datasheet Section 48.7:
- Does TC have DMA request capability?
- Can XDMAC burst-write to TC_RA/TC_RB?

**Investigation needed:**
```c
// Check if TC has DMA trigger capability
// Look for: TC_QIDR, TC_QIMR (Quadrature Decoder)
// Or: PDC/DMA connection in TC chapter
```

### Alternative: PWM Controller (PWM)

SAMV71 also has a dedicated PWM peripheral (separate from TC):
- 4 channels (PWMH0-3, PWML0-3)
- Built-in DMA support
- May be better suited for DShot

**Check:** SAMV71 Datasheet Chapter 47: Pulse Width Modulation Controller

---

## Phase A: Basic PWM Implementation

### Objective
Implement standard RC servo/ESC PWM (50-400 Hz, 1000-2000 µs pulses)

### Files to Create/Modify

```
platforms/nuttx/src/px4/microchip/samv7/
├── io_pins/
│   ├── io_timer.c           # NEW: Full implementation
│   └── CMakeLists.txt       # Update to include io_timer.c
├── include/px4_arch/
│   └── io_timer_hw_description.h  # Define SAMV7-specific types
```

```
boards/microchip/samv71-xult-clickboards/
├── src/
│   └── timer_config.cpp     # Board-specific timer/pin mapping
```

### Implementation Steps

#### Step 1: Study Reference Implementation
```bash
# Read STM32 io_timer carefully
code platforms/nuttx/src/px4/stm/stm32_common/io_pins/io_timer.c
```

Key functions to implement:
- `io_timer_init_timer()` - Initialize TC peripheral
- `io_timer_set_rate()` - Set PWM frequency
- `io_timer_set_ccr()` - Set duty cycle
- `io_timer_channel_init()` - Configure channel mode

#### Step 2: Map SAMV71-XULT Pins

Identify which TC channels route to accessible headers:

| Header | Pin | TC Channel | TIOA/TIOB |
|--------|-----|------------|-----------|
| mikroBUS1 PWM | PC19 | ? | ? |
| mikroBUS2 PWM | PB1 | ? | ? |
| Arduino PWM | ? | ? | ? |

**Use:** SAMV71-XULT schematic + SAMV71 datasheet pin mux table

#### Step 3: Implement io_timer.c

```c
// platforms/nuttx/src/px4/microchip/samv7/io_pins/io_timer.c

#include <px4_arch/io_timer.h>
#include <sam_tc.h>
#include <sam_gpio.h>

int io_timer_init_timer(unsigned timer)
{
    // 1. Enable TC clock in PMC
    sam_tc_clockenable(timer);

    // 2. Configure TC for waveform mode
    uint32_t cmr = TC_CMR_WAVE |           // Waveform mode
                   TC_CMR_WAVSEL_UP_RC |   // Up count, reset on RC
                   TC_CMR_EEVT_XC0 |       // Make TIOB an output
                   TC_CMR_ACPA_CLEAR |     // Clear TIOA on RA match
                   TC_CMR_ACPC_SET;        // Set TIOA on RC match

    putreg32(cmr, TC_CMR(timer));

    // 3. Set default period (50 Hz = 20ms)
    putreg32(calculate_rc_for_freq(50), TC_RC(timer));

    return OK;
}

int io_timer_set_ccr(unsigned channel, uint16_t value)
{
    // Convert value (0-65535) to RA register value
    uint32_t ra = map_value_to_ra(channel, value);
    putreg32(ra, TC_RA(channel));
    return OK;
}
```

#### Step 4: Create timer_config.cpp

```cpp
// boards/microchip/samv71-xult-clickboards/src/timer_config.cpp

#include <px4_arch/io_timer_hw_description.h>

constexpr io_timers_t io_timers[MAX_IO_TIMERS] = {
    initIOTimer(Timer::TC1),  // PWM channels 1-2
    initIOTimer(Timer::TC2),  // PWM channels 3-4
};

constexpr timer_io_channels_t timer_io_channels[MAX_TIMER_IO_CHANNELS] = {
    initIOTimerChannel(io_timers, {Timer::TC1, Timer::ChannelA},
                       {GPIO::PortC, GPIO::Pin19}),  // mikroBUS1 PWM
    initIOTimerChannel(io_timers, {Timer::TC1, Timer::ChannelB},
                       {GPIO::PortB, GPIO::Pin1}),   // mikroBUS2 PWM
    // Add more channels...
};
```

#### Step 5: Enable in Build

```cmake
# boards/microchip/samv71-xult-clickboards/default.px4board
CONFIG_DRIVERS_PWM_OUT=y
```

#### Step 6: Test

```bash
nsh> pwm_out start
nsh> pwm info
nsh> pwm test -c 1 -p 1500   # Test channel 1 at 1500µs
```

---

## Phase B: DShot Implementation

### Objective
Implement DShot protocol for digital ESC communication

### Feasibility Analysis

**Question 1: Does SAMV71 TC support DMA burst to RA/RB?**

Check datasheet for:
- PDC (Peripheral DMA Controller) support in TC
- XDMAC trigger sources from TC

If NO native DMA support:
- Use PWM Controller instead (has DMA)
- Or use bitbang approach with GPIO DMA

**Question 2: Can we achieve DShot600 timing?**

DShot600 requires:
- Bit period: 1.67 µs
- T0H: 625 ns, T1H: 1250 ns

With MCK = 150 MHz, TC clock = MCK/2 = 75 MHz:
- 1 tick = 13.3 ns
- Bit period = 125 ticks ✅
- T0H = 47 ticks, T1H = 94 ticks ✅

**Timing is achievable!**

### Files to Create

```
platforms/nuttx/src/px4/microchip/samv7/
├── dshot/
│   ├── dshot.c              # NEW: DShot implementation
│   └── CMakeLists.txt
├── include/px4_arch/
│   └── dshot.h              # NEW: DShot header
```

### Implementation Approach

If TC has DMA support:
```c
// Similar to STM32 approach
// DMA transfers buffer of duty cycle values to TC_RA
// TC generates waveform automatically
```

If TC lacks DMA (use PWM Controller):
```c
// Use SAMV71 PWM peripheral instead
// PWM has native DMA support for duty cycle updates
```

If neither works (bitbang fallback):
```c
// Timer interrupt triggers GPIO toggles
// Higher CPU usage but guaranteed to work
```

---

## Hardware Pin Mapping

### SAMV71-XULT Available PWM Pins

Research needed from schematic. Likely candidates:

| Location | Pin | Peripheral | Notes |
|----------|-----|------------|-------|
| mikroBUS1 PWM | PC19 | TC? / PWM? | Check mux |
| mikroBUS2 PWM | PB1 | TC? / PWM? | Check mux |
| Arduino D3 | ? | ? | PWM capable? |
| Arduino D5 | ? | ? | PWM capable? |
| Arduino D6 | ? | ? | PWM capable? |
| EXT1/EXT2 | ? | ? | Check headers |

### Minimum for Quadcopter
Need 4 PWM channels for motors 1-4.

---

## Test Plan

### Phase A Tests (Basic PWM)

| Test | Command | Expected |
|------|---------|----------|
| Driver loads | `pwm_out start` | No errors |
| PWM info | `pwm info` | Shows channels |
| Servo test | `pwm test -c 1 -p 1500` | 1500µs pulse |
| Frequency | Oscilloscope | 50-400 Hz |
| Duty cycle | Oscilloscope | 1000-2000µs |

### Phase B Tests (DShot)

| Test | Command | Expected |
|------|---------|----------|
| Driver loads | `dshot start` | No errors |
| Protocol | Oscilloscope | DShot waveform |
| ESC response | Connect ESC | Beeps/arms |
| Throttle | Arm + throttle | Motor spins |

---

## Deliverables

### Phase A (Required)
1. `io_timer.c` - Full implementation
2. `timer_config.cpp` - Board pin mapping
3. Updated `CMakeLists.txt` files
4. Test report with oscilloscope captures
5. Documentation updates

### Phase B (Optional but Recommended)
1. `dshot.c` - DShot implementation
2. `dshot.h` - Header file
3. Performance benchmarks
4. ESC compatibility testing

---

## Success Criteria

### Phase A
- [ ] `pwm_out` driver starts without errors
- [ ] 4+ PWM channels available
- [ ] Correct pulse width (1000-2000 µs)
- [ ] Correct frequency (50-400 Hz configurable)
- [ ] ESC responds to PWM signals
- [ ] Motors spin with throttle commands

### Phase B
- [ ] `dshot` driver starts without errors
- [ ] DShot600 timing verified on oscilloscope
- [ ] ESC arms with DShot protocol
- [ ] Bidirectional DShot (telemetry) working (bonus)

---

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| TC lacks DMA | Medium | High | Use PWM peripheral or bitbang |
| Pin routing issues | Medium | Medium | Check schematic early |
| Timing precision | Low | High | SAMV71 has fast clocks |
| Cache coherency | Medium | Medium | Use write-through or clean cache |

---

## Timeline Estimate

| Phase | Task | Days |
|-------|------|------|
| A.1 | Research & pin mapping | 1 |
| A.2 | io_timer implementation | 2-3 |
| A.3 | Testing & debug | 1-2 |
| B.1 | DShot feasibility study | 1 |
| B.2 | DShot implementation | 2-3 |
| B.3 | Testing & ESC validation | 1-2 |

**Total: 5-10 days**

---

## Notes

### Why DShot Over PWM?
- Digital protocol (no calibration needed)
- Higher update rates (up to 32 kHz)
- Telemetry support (RPM, temperature)
- Better reliability

### Fallback Plan
If DShot proves too complex, basic PWM is sufficient for initial flights. DShot can be added later.

---

## References

- SAMV71 Datasheet: Chapter 48 (TC), Chapter 47 (PWM)
- `SAMV71_IMPLEMENTATION_TRACKER.md` - Phase 3 section
- STM32 reference: `platforms/nuttx/src/px4/stm/stm32_common/`
- [PX4 PWM Output Documentation](https://docs.px4.io/main/en/peripherals/pwm_escs_and_servo.html)

---

**Document Version:** 1.0
**Created:** November 28, 2025
**Status:** Ready for Assignment
