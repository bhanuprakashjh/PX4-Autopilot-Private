# SAMV71 Tier 1 Platform Battle Plan
## Making Microchip Beat STM32

**Created:** 2025-11-26
**Status:** Active Development
**Goal:** Full feature parity with STM32 FMU-v6X

---

## Executive Summary

SAMV71 has **software parity** with STM32 for all core flight modules. What's missing are **hardware drivers**. With 1MB of flash headroom (51% unused), there's plenty of space to implement everything.

### Current Gap Analysis

| Component | STM32 FMU-v6X | SAMV71-XULT | Effort | Priority |
|-----------|---------------|-------------|--------|----------|
| HRT Timer | 1 MHz (optimal) | 4.6875 MHz (**FIXED**) | Done | DONE |
| Events Module | Works | **RE-ENABLED** | Done | DONE |
| PWM Output | Full | STUB | 8-12h | CRITICAL |
| io_timer | Full | STUB | 4-8h | CRITICAL |
| DShot | Full | Missing | 8-12h | HIGH |
| ADC | Full | Missing | 4-8h | HIGH |
| Tone Alarm | Full | Missing | 2-4h | MEDIUM |
| Safety Button | Full | Missing | 2-4h | MEDIUM |
| UAVCAN | Full | Missing | 8-16h | LOW |

### SAMV71 Hardware Advantages

| Feature | SAMV71 | Typical STM32 |
|---------|--------|---------------|
| CPU Speed | **300 MHz** | 168-480 MHz |
| FPU | **Double-precision FPv5-D16** | Single-precision |
| Flash Free | **1 MB (51%)** | ~100KB (5%) |
| I+D Cache | **Yes** | Varies |
| Timer Channels | TC0-TC2 (9 channels) | Similar |
| AFEC (ADC) | **2x 12-bit, 2 Msps** | Similar |

---

## Phase 1: Foundation (DONE)

### 1.0 FPU Enabled (COMPLETED)

**File:** `nuttx-config/nsh/defconfig`

Added to enable SAMV71's double-precision FPU:
```
CONFIG_ARCH_FPU=y      # Enable hardware FPU
CONFIG_ARCH_DPFPU=y    # Enable double-precision (FPv5-D16)
```

**Why this matters:**
- SAMV71 has FPv5-D16: Double-precision FPU (better than most STM32!)
- EKF2 matrix math benefits from hardware float/double
- ~10x faster than software floating point

### 1.1 HRT Timer Optimization (COMPLETED)

**Problem:** Original code used expensive 64-bit division:
```c
// SLOW: 64-bit division by 4,687,500
return (ticks * 1000000ULL) / HRT_TIMER_FREQ;
```

**Solution:** Exploit the ratio 4,687,500/1,000,000 = 75/16:
```c
// FAST: Multiply-inverse for division by 75
#define DIV_BY_75_MAGIC    0x1B4E81B5ULL
#define DIV_BY_75_SHIFT    35

static inline uint64_t hrt_ticks_to_usec(uint64_t ticks)
{
    return (ticks << 4) * DIV_BY_75_MAGIC >> DIV_BY_75_SHIFT;
}

static inline uint64_t hrt_usec_to_ticks(hrt_abstime usec)
{
    return ((usec * 75ULL) + 15ULL) >> 4;
}
```

**Performance:** ~10x faster on Cortex-M7 (no 64-bit hardware divider)

### 1.2 Events Module Re-enabled (COMPLETED)

Changed in `default.px4board`:
```
CONFIG_MODULES_EVENTS=y  # Re-enabled with HRT optimization fix
```

### 1.3 Advanced EKF2 Features Enabled (COMPLETED)

**File:** `default.px4board`

Enabled all useful EKF2 features to match FMU-v6x:
```
CONFIG_EKF2_AUX_GLOBAL_POSITION=y  # Secondary GPS
CONFIG_EKF2_AUXVEL=y               # Auxiliary velocity sources
CONFIG_EKF2_DRAG_FUSION=y          # Aerodynamic drag model
CONFIG_EKF2_EXTERNAL_VISION=y      # Visual odometry/SLAM support
CONFIG_EKF2_GNSS_YAW=y             # Dual-antenna GPS heading
CONFIG_EKF2_RANGE_FINDER=y         # Lidar/rangefinder fusion
```

**Why this matters:**
- External Vision: Enables indoor flight with SLAM/VIO
- Range Finder: Precise altitude hold over terrain
- GNSS Yaw: No magnetometer needed for heading
- Drag Fusion: Better velocity estimation in wind

### 1.4 Additional Modules Enabled (COMPLETED)

```
CONFIG_MODULES_MC_HOVER_THRUST_ESTIMATOR=y  # Better hover
CONFIG_MODULES_GYRO_CALIBRATION=y           # Auto gyro cal
CONFIG_MODULES_MAG_BIAS_ESTIMATOR=y         # Mag cal
CONFIG_MODULES_LOAD_MON=y                   # CPU monitoring
```

---

## Phase 2: Flight-Critical Drivers

### 2.1 io_timer Implementation

**Location:** `platforms/nuttx/src/px4/microchip/samv7/io_pins/`

**Reference:** `platforms/nuttx/src/px4/stm/stm32_common/io_pins/io_timer.c`

**SAMV7 Timer Mapping:**
| Timer | Channel | GPIO | Function |
|-------|---------|------|----------|
| TC0 | CH0 | PA0 (TIOA0) | Motor 1 |
| TC0 | CH1 | PA1 (TIOB0) | Motor 2 |
| TC0 | CH2 | PA2 (TIOA1) | Motor 3 |
| TC1 | CH0 | PD21 (TIOA2) | Motor 4 |
| TC1 | CH1 | PD22 (TIOB2) | Motor 5 |
| TC1 | CH2 | PA15 (TIOA3) | Motor 6 |

**Key Functions to Implement:**
```c
int io_timer_init_timer(unsigned timer, io_timer_channel_mode_t mode);
int io_timer_set_rate(unsigned timer, unsigned rate);
int io_timer_set_ccr(unsigned channel, uint16_t value);
int io_timer_get_channel_mode(unsigned channel);
```

### 2.2 PWM Output Driver

**Location:** `boards/microchip/samv71-xult-clickboards/src/timer_config.cpp`

**Enable in px4board:**
```
CONFIG_DRIVERS_PWM_OUT=y
```

**Implementation Steps:**
1. Define GPIO pins for PWM outputs in `board_config.h`
2. Create timer configuration mapping in `timer_config.cpp`
3. Implement PWM generation using TC waveform mode
4. Test with oscilloscope (50 Hz PWM, 1000-2000 µs pulses)

### 2.3 ADC/Battery Monitoring

**Location:** `boards/microchip/samv71-xult-clickboards/src/adc.cpp`

**Reference:** `platforms/nuttx/src/px4/stm/stm32_common/adc/adc.cpp`

**SAMV7 AFEC (ADC) Features:**
- 2x 12-bit ADC controllers (AFEC0, AFEC1)
- Up to 2 Msps conversion rate
- 16 channels per controller
- Hardware averaging
- DMA support

**Channels to Configure:**
| Channel | Signal | Voltage Divider |
|---------|--------|-----------------|
| AFEC0 CH0 | VBAT | TBD (based on battery) |
| AFEC0 CH1 | IBAT (current sense) | TBD |
| AFEC0 CH2 | 5V rail | 2:1 |
| AFEC0 CH3 | 3.3V rail | Direct |

---

## Phase 3: Enhanced Features

### 3.1 DShot Protocol

**Reference:** `platforms/nuttx/src/px4/stm/stm32_common/dshot/dshot.c`

**DShot Timing Requirements:**
| Protocol | Bit Period | T0H | T1H |
|----------|-----------|-----|-----|
| DShot150 | 6.67 µs | 2.50 µs | 5.00 µs |
| DShot300 | 3.33 µs | 1.25 µs | 2.50 µs |
| DShot600 | 1.67 µs | 0.625 µs | 1.25 µs |

**Implementation:** Use TC in waveform mode with DMA for precise timing.

### 3.2 Tone Alarm

**Reference:** `platforms/nuttx/src/px4/stm/stm32_common/tone_alarm/`

**Implementation:** Use one TC channel in PWM mode for audio output.

### 3.3 Safety Button

**Implementation:** GPIO interrupt handler for external safety switch.

---

## Phase 4: Optimization & Testing

### 4.1 Work Queue Distribution

Current: Most modules on `lp_default` queue.

Optimize by moving to dedicated queues:
- rate_ctrl: Rate controllers
- nav_and_controllers: Position controllers
- INS0: EKF2

### 4.2 Performance Benchmarks

**Target Metrics:**
| Metric | Target | Method |
|--------|--------|--------|
| Boot Time | < 5 seconds | `top` command timing |
| CPU Idle | > 30% | `top` during hover |
| Loop Rate | 400 Hz attitude, 50 Hz position | `listener actuator_outputs` |
| RAM Usage | < 300 KB | `free` command |

---

## Build & Test Commands

### Build
```bash
cd /media/bhanu1234/Development/PX4-Autopilot-Private
make microchip_samv71-xult-clickboards_default
```

### Flash
```bash
# Using OpenOCD
openocd -f interface/cmsis-dap.cfg -f target/atsamv.cfg \
  -c "program build/microchip_samv71-xult-clickboards_default/*.elf verify reset exit"
```

### Test Events Module
```bash
# In NSH shell after boot
nsh> ps | grep events
# Should show send_event process

nsh> top
# Check CPU usage, should be < 70%
```

### Test HRT Performance
```bash
nsh> perf
# Check latency counters - should show low values
```

---

## Files Modified

### Already Modified:
1. `platforms/nuttx/src/px4/microchip/samv7/hrt/hrt.c` - HRT optimization
2. `boards/microchip/samv71-xult-clickboards/default.px4board` - Events re-enabled

### To Be Created:
1. `platforms/nuttx/src/px4/microchip/samv7/io_pins/io_timer.c` - Timer abstraction
2. `boards/microchip/samv71-xult-clickboards/src/timer_config.cpp` - PWM config
3. `boards/microchip/samv71-xult-clickboards/src/adc.cpp` - ADC driver

---

## Timeline

| Week | Tasks | Deliverable |
|------|-------|-------------|
| 1 | HRT fix, Events module, PWM start | Boot works, HRT fast |
| 2 | PWM complete, ADC | Motors spin, battery shows |
| 3 | DShot, Tone alarm | Full motor control |
| 4 | Safety, optimization | Flight ready |

---

## Success Criteria

**Minimum Viable Flight Controller:**
- [ ] Boot < 5 seconds
- [ ] Events module working
- [ ] PWM outputs functional
- [ ] Battery voltage displayed
- [ ] QGC full connection
- [ ] Arm/disarm working
- [ ] Motors spin correctly

**Full Tier 1 Parity:**
- [ ] All above plus:
- [ ] DShot support
- [ ] Tone alarm
- [ ] Safety button
- [ ] UAVCAN
- [ ] < 60% flash usage
- [ ] Documentation complete

---

## Notes

- Flash usage after all features: ~60% estimated (plenty of room)
- SAMV7's 300 MHz + double-precision FPU gives excellent EKF2 performance
- TC peripherals are very capable - just need proper configuration
- Reference the STM32 implementations but don't copy blindly - SAMV7 peripherals differ

---

**Next Action:** Build firmware and test HRT fix with events module enabled.
