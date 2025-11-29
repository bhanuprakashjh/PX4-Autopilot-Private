# SAMV71-XULT PX4 Port - Master Implementation Plan

**Document ID:** MASTER-PLAN-20251129
**Created:** November 29, 2025
**Version:** 1.2
**Status:** ACTIVE

---

## Executive Summary

This master plan synthesizes the comprehensive gap analysis from `FMU6X_SAMV71_DETAILED_COMPARISON.md` and maps implementation tasks to a phased roadmap. The SAMV71-XULT PX4 port has completed core platform bring-up and now requires focused effort on flight-critical subsystems.

### Current State Summary
| Category | Status | Completion |
|----------|--------|------------|
| Core Platform | Complete | 95% |
| Sensor Drivers | Compiled, Untested | 70% |
| Motor Output | Not Implemented | 0% |
| Storage | Read-Only | 50% |
| Flight Testing | Blocked | 0% |

### Critical Path to Flight
```
                              ┌──────────────┐
                         ┌───►│  HITL Test   │───┐
                         │    │  (Simulated) │   │
┌──────────────┐    ┌────┴───────┐           │   │    ┌──────────────┐
│   Sensors    │───►│  Parallel  │           │   ├───►│ Real Flight  │
│  Validated   │    │   Tracks   │           │   │    │   Ready      │
└──────────────┘    └────┬───────┘           │   │    └──────────────┘
                         │    ┌──────────────┐   │
                         └───►│ PWM/io_timer │───┘
                              │ (For motors) │
                              └──────────────┘
```
**Note:** HITL testing can proceed in parallel with PWM development once sensors are validated.
PWM is only required for real motor output, not for HITL simulation.

---

## Gap Analysis Summary

### From FMU6X Comparison Document

The detailed comparison identified these priority gaps:

#### P0 - Critical for Flight (Must Have)
| Gap | FMU6X Has | SAMV71 Status | Task Document |
|-----|-----------|---------------|---------------|
| PWM Output | Full io_timer + DShot | Stubs only | `TASK_DSHOT_PWM_IMPLEMENTATION.md` |
| IMU Sensor | ICM-42688P | Untested ICM-20689 | `TASK_CLICK_BOARD_SENSOR_TESTING.md` |
| Barometer | ICP-20100 | Untested DPS310 | `TASK_CLICK_BOARD_SENSOR_TESTING.md` |
| Magnetometer | BMM150 | Untested AK09916 | `TASK_CLICK_BOARD_SENSOR_TESTING.md` |

#### P1 - High Priority (Should Have)
| Gap | FMU6X Has | SAMV71 Status | Task Document |
|-----|-----------|---------------|---------------|
| Flight Logger | SD card logging | Writes disabled | `TASK_LOGGER_DEBUG_ENABLE.md` |
| ADC Monitoring | Battery, RSSI, etc | Not configured | **NEW: TASK_ADC_BATTERY_MONITOR.md** |
| Parameter Storage | FRAM/Flash | EEPROM MTD only | **NEW: TASK_QSPI_FLASH_STORAGE.md** |
| Console Buffer | dmesg support | Disabled (static init) | `TASK_CONSOLE_BUFFER_DEBUG.md` |

#### P2 - Medium Priority (Nice to Have)
| Gap | FMU6X Has | SAMV71 Status | Task Document |
|-----|-----------|---------------|---------------|
| HITL Simulation | Full support | Needs validation | `TASK_HITL_TESTING.md` |
| Safety Switch | Hardware switch | Not implemented | Future |
| RC Input | SBUS/PPM/CRSF | Basic UART only | Future |
| CAN Bus | UAVCAN | Not configured | Future |

#### P3 - Low Priority (Future)
| Gap | FMU6X Has | SAMV71 Status | Notes |
|-----|-----------|---------------|-------|
| Ethernet | Native | Available unused | Low priority |
| USB OTG | Host mode | Device only | Not needed |
| Crypto | Hardware AES | Available unused | Future security |
| Secondary IMU | Redundant | Single IMU | Phase 2 |

---

## Implementation Phases

### Phase 1: Flight-Critical Foundation
**Timeline:** Immediate Priority
**Parallel Tracks:** 3

```
                    ┌─────────────────────────────────────┐
                    │     PHASE 1: Foundation             │
                    │     (Can be done in parallel)       │
                    └─────────────────────────────────────┘
                                    │
           ┌────────────────────────┼────────────────────────┐
           ▼                        ▼                        ▼
    ┌──────────────┐        ┌──────────────┐        ┌──────────────┐
    │   Track 1A   │        │   Track 1B   │        │   Track 1C   │
    │  PWM/Timer   │        │   Sensors    │        │   Storage    │
    │  (Engineer 5)│        │ (Engineer 2) │        │ (Engineer 1) │
    └──────────────┘        └──────────────┘        └──────────────┘
```

#### Track 1A: PWM/Timer Implementation (BLOCKING)
**Assignee:** Engineer 5
**Task Doc:** `TASK_DSHOT_PWM_IMPLEMENTATION.md`
**Dependencies:** None
**Effort:** 5-10 days

**Deliverables:**
1. [ ] `io_timer.c` - Full TC-based PWM implementation
2. [ ] `timer_config.cpp` - Board-specific pin mapping
3. [ ] 4+ PWM channels verified with oscilloscope
4. [ ] ESC responds to PWM signals
5. [ ] Optional: DShot600 implementation

**Technical Requirements:**
- Use SAMV71 Timer Counter TC1 and TC2 (current arch layer has `MAX_IO_TIMERS=3`)
- TC0-CH0 reserved for HRT (high-resolution timer)
- Each TC channel has TIOA/TIOB outputs = 4 channels from TC1+TC2
- To get more channels: either extend arch layer for TC3, or use PWM peripheral
- Target frequency: 50-400 Hz PWM, DShot600 optional
- DMA support investigation for DShot (TC may not have direct DMA; PWM peripheral does)

**Timer Resource Reality:**
| Timer | Channels | Available | PWM Outputs |
|-------|----------|-----------|-------------|
| TC0 | CH0,CH1,CH2 | CH1,CH2 only | 4 (TIOA/B x2) - CH0=HRT |
| TC1 | CH0,CH1,CH2 | All | 6 (TIOA/B x3) |
| TC2 | CH0,CH1,CH2 | All | 6 (TIOA/B x3) |
| **Total** | | | **16 possible, 6 with current MAX_IO_TIMERS=3** |

**Decision Required:** Either work within TC1/TC2 (6 channels) or extend arch layer for more.

#### Track 1B: Sensor Validation
**Assignee:** Engineer 2
**Task Doc:** `TASK_CLICK_BOARD_SENSOR_TESTING.md`
**Dependencies:** Hardware (Click boards)
**Effort:** 2-3 days

**Deliverables:**
1. [ ] ICM-20689 IMU validated (SPI)
2. [ ] AK09916 magnetometer validated (I2C)
3. [ ] DPS310 barometer validated (I2C)
4. [ ] EKF2 running with fused data
5. [ ] Test report with data samples

**Sensors Priority:**
| Priority | Sensor | Interface | Status |
|----------|--------|-----------|--------|
| P1 | ICM-20689 | SPI (Socket 1) | Test first |
| P1 | AK09916 | I2C (0x0C) | Test second |
| P1 | DPS310 | I2C (0x77) | Test third |
| P2 | BMP388 | I2C (0x76) | Optional |
| P2 | BMM150 | I2C (0x10) | Optional |

#### Track 1C: Logger/Storage Debug
**Assignee:** Engineer 1
**Task Doc:** `TASK_LOGGER_DEBUG_ENABLE.md`
**Dependencies:** SD card, logic analyzer
**Effort:** 3-5 days

**Deliverables:**
1. [ ] Root cause identified for SD write hang
2. [ ] Fix implemented (DMA or PIO mode)
3. [ ] Logger writes .ulg files successfully
4. [ ] 10-minute logging without corruption
5. [ ] Technical report on fix

**Investigation Focus:**
- HSMCI DMA write path
- D-Cache coherency with DMA
- NuttX FATFS write buffering
- PIO fallback implementation

---

### Phase 2: System Integration
**Timeline:** After Phase 1 completion
**Dependencies:** Track 1A complete (PWM working)

```
                    ┌─────────────────────────────────────┐
                    │     PHASE 2: Integration            │
                    │     (Sequential after Phase 1)      │
                    └─────────────────────────────────────┘
                                    │
                    ┌───────────────┼───────────────┐
                    ▼               ▼               ▼
             ┌──────────────┐ ┌──────────────┐ ┌──────────────┐
             │   Task 2A    │ │   Task 2B    │ │   Task 2C    │
             │ HITL Testing │ │ ADC Monitor  │ │ QSPI Storage │
             │ (Engineer 3) │ │   (New)      │ │   (New)      │
             └──────────────┘ └──────────────┘ └──────────────┘
```

#### Task 2A: HITL Simulation Testing
**Assignee:** Engineer 3
**Task Doc:** `TASK_HITL_TESTING.md`
**Dependencies:** PWM working (Track 1A)
**Effort:** 2-3 days

**Deliverables:**
1. [ ] jMAVSim connection established
2. [ ] QGroundControl telemetry verified
3. [ ] Simulated takeoff/hover stable
4. [ ] Mission flight executed
5. [ ] Failsafe behavior verified

#### Task 2B: ADC Battery Monitoring (NEW TASK NEEDED)
**Assignee:** TBD
**Task Doc:** `TASK_ADC_BATTERY_MONITOR.md` (to be created)
**Dependencies:** None
**Effort:** 1-2 days

**Scope:**
- Configure SAMV71 ADC channels for battery voltage
- Implement voltage divider calibration
- Enable battery failsafe
- Map ADC pins from SAMV71-XULT schematic

**FMU6X Reference:**
```cpp
// boards/px4/fmu-v6x/src/board_config.h
#define ADC_BATTERY1_VOLTAGE_CHANNEL    0
#define ADC_BATTERY1_CURRENT_CHANNEL    1
#define ADC_BATTERY2_VOLTAGE_CHANNEL    2
```

#### Task 2C: QSPI Flash Storage (NEW TASK NEEDED)
**Assignee:** TBD
**Task Doc:** `TASK_QSPI_FLASH_STORAGE.md` (to be created)
**Dependencies:** None
**Effort:** 2-3 days

**Scope:**
- Enable SST26VF064B QSPI flash (8MB on-board)
- Configure as MTD partition for parameters
- Verify wear-leveling (LittleFS or SmartFS)
- Test parameter save/load persistence

**Technical Details:**
- SAMV71-XULT has SST26VF064B on QSPI
- NuttX has SST26 driver in `drivers/mtd/sst26.c`
- Need to configure QSPI pins and clocks

---

### Phase 3: Production Hardening
**Timeline:** After successful HITL testing
**Dependencies:** Phase 2 complete

```
                    ┌─────────────────────────────────────┐
                    │     PHASE 3: Hardening              │
                    │     (Production readiness)          │
                    └─────────────────────────────────────┘
                                    │
              ┌─────────────────────┼─────────────────────┐
              ▼                     ▼                     ▼
       ┌──────────────┐      ┌──────────────┐      ┌──────────────┐
       │   Task 3A    │      │   Task 3B    │      │   Task 3C    │
       │Console Buffer│      │ BlockingList │      │   Safety     │
       │ (Engineer 4) │      │     Fix      │      │   Features   │
       └──────────────┘      └──────────────┘      └──────────────┘
```

#### Task 3A: Console Buffer Enable
**Assignee:** Engineer 4
**Task Doc:** `TASK_CONSOLE_BUFFER_DEBUG.md`
**Dependencies:** None (can be done anytime)
**Effort:** 1-2 days

**Deliverables:**
1. [ ] Lazy initialization implemented
2. [ ] `BOARD_ENABLE_CONSOLE_BUFFER` enabled
3. [ ] `dmesg` command working
4. [ ] Boot log captured and viewable
5. [ ] Thread-safe operation verified

#### Task 3B: PWMSim Re-entrancy Fix (LOW PRIORITY)
**Assignee:** TBD
**Task Doc:** N/A (already documented in code)
**Dependencies:** None
**Effort:** 1-2 days

**Status:** WORKAROUND IN PLACE - NOT BLOCKING

**Background:**
The `BlockingList.hpp` static init issue is **ALREADY FIXED** with proper constructor
initialization using `pthread_mutex_init()`. The remaining SAMV7 guard in `pwm_out_sim`
is a **different issue** - a work queue re-entrancy race condition.

**Current Workaround (in PWMSim.cpp line 618-624):**
```cpp
// SAMV7: Skip updateSubscriptions due to work queue switch re-entrancy issue.
// ScheduleNow() immediately triggers Run() on rate_ctrl before the first
// updateSubscriptions() completes, causing a race condition / crash.
// This is NOT a mutex init issue - the mutex fixes are working.
#if !defined(CONFIG_ARCH_CHIP_SAMV7)
    _mixing_output.updateSubscriptions(true);
#endif
```

**Root Cause:** When `MixingOutput::updateSubscriptions()` calls `ScheduleNow()`, it
immediately triggers `Run()` on rate_ctrl work queue before the first call completes,
causing a re-entrancy crash.

**Potential Fix (Future):**
- Investigate NuttX work queue scheduling behavior on SAMV7
- Add re-entrancy guard in updateSubscriptions()
- Or defer subscription updates to next cycle

**Impact:** Minimal - pwm_out_sim works for HITL with current workaround.

#### Task 3C: Safety Features
**Assignee:** TBD
**Dependencies:** HITL testing complete
**Effort:** 2-3 days

**Scope:**
- RC input configuration (SBUS/PPM)
- Safety switch implementation
- Geofence testing
- Return-to-Launch verification
- Kill switch functionality

---

### Phase 4: Advanced Features (Future)
**Timeline:** Post-flight validation
**Status:** Planning only

| Feature | Effort | Priority | Notes |
|---------|--------|----------|-------|
| DShot Bidirectional | 2-3 days | P2 | ESC telemetry |
| CAN/UAVCAN | 3-5 days | P2 | GPS, sensors |
| Ethernet MAVLink | 2 days | P3 | High bandwidth |
| Secondary IMU | 1-2 days | P2 | Redundancy |
| Hardware Crypto | 2 days | P3 | Secure boot |

---

## Resource Allocation

### Engineer Assignments

| Engineer | Primary Task | Secondary Task | Phase |
|----------|--------------|----------------|-------|
| Engineer 1 | Logger/SD Debug | QSPI Storage | 1C, 2C |
| Engineer 2 | Click Board Sensors | ADC Battery | 1B, 2B |
| Engineer 3 | HITL Testing | Safety Features | 2A, 3C |
| Engineer 4 | Console Buffer | - | 3A |
| Engineer 5 | PWM/DShot (CRITICAL) | - | 1A |

**Note:** BlockingList static init is already fixed. PWMSim re-entrancy has workaround in place.

### Hardware Requirements

| Item | Quantity | Purpose | Assigned To |
|------|----------|---------|-------------|
| SAMV71-XULT | 2+ | Development boards | All |
| ICM-20689 Click | 1 | IMU testing | Engineer 2 |
| Compass 4 Click | 1 | Mag testing | Engineer 2 |
| Pressure 3 Click | 1 | Baro testing | Engineer 2 |
| SD Cards (32GB) | 3 | Logger testing | Engineer 1 |
| Oscilloscope | 1 | PWM/signal debug | Engineer 5 |
| Logic Analyzer | 1 | Protocol debug | Shared |
| ESC + Motor | 4 | PWM validation | Engineer 5 |
| LiPo Battery | 1 | Power testing | Engineer 2 |

---

## Risk Assessment

### High Risk Items

| Risk | Impact | Likelihood | Mitigation |
|------|--------|------------|------------|
| PWM/Timer complexity | Flight blocked | Medium | Start immediately, allocate extra time |
| SD write hang unfixable | No logging | Low | Use MAVLink log streaming |
| Sensor incompatibility | Flight blocked | Low | Multiple sensor options available |
| DMA cache issues | Data corruption | Medium | Use write-through, explicit flush |

### Medium Risk Items

| Risk | Impact | Likelihood | Mitigation |
|------|--------|------------|------------|
| Click board availability | Testing delayed | Medium | Order early |
| Static init issues | Runtime crashes | Medium | Lazy init pattern |
| EKF2 tuning needed | Poor flight | Medium | Use HITL for tuning |

---

## Dependencies Graph

```
                              ┌─────────────┐
                              │   START     │
                              └──────┬──────┘
                                     │
           ┌─────────────────────────┼─────────────────────────┐
           │                         │                         │
           ▼                         ▼                         ▼
    ┌──────────────┐          ┌──────────────┐          ┌──────────────┐
    │ Track 1A:    │          │ Track 1B:    │          │ Track 1C:    │
    │ PWM/Timer    │          │ Sensors      │          │ Logger       │
    │ (BLOCKING)   │          │              │          │              │
    └──────┬───────┘          └──────┬───────┘          └──────┬───────┘
           │                         │                         │
           │                         │                         │
           └─────────────┬───────────┘                         │
                         │                                     │
                         ▼                                     │
                  ┌──────────────┐                             │
                  │ Task 2A:     │                             │
                  │ HITL Testing │◄────────────────────────────┘
                  └──────┬───────┘          (logger helps but not required)
                         │
                         │
           ┌─────────────┼─────────────┐
           │             │             │
           ▼             ▼             ▼
    ┌──────────────┐ ┌──────────────┐ ┌──────────────┐
    │ Task 2B:     │ │ Task 2C:     │ │ Task 3A-C:   │
    │ ADC Battery  │ │ QSPI Storage │ │ Hardening    │
    └──────────────┘ └──────────────┘ └──────────────┘
                         │
                         ▼
                  ┌──────────────┐
                  │ FLIGHT READY │
                  └──────────────┘
```

---

## Success Metrics

### Phase 1 Exit Criteria
- [ ] PWM output verified on oscilloscope (1000-2000µs pulses)
- [ ] At least one IMU providing valid accel/gyro data
- [ ] At least one barometer providing valid pressure data
- [ ] At least one magnetometer providing valid field data
- [ ] EKF2 running without sensor errors

### Phase 2 Exit Criteria
- [ ] HITL takeoff and hover stable for 5+ minutes
- [ ] Mission flight (waypoint navigation) successful
- [ ] Battery voltage monitoring functional
- [ ] Parameters persist across reboots (QSPI or EEPROM)

### Phase 3 Exit Criteria
- [ ] Console buffer (`dmesg`) working
- [ ] No static initialization crashes
- [ ] RC input functional
- [ ] Safety features verified

### Final Acceptance
- [ ] 30-minute stable HITL flight
- [ ] All P0 and P1 gaps closed
- [ ] Documentation complete
- [ ] Code ready for upstream PR consideration

---

## Document Cross-References

### Existing Task Documents
| Document | Engineer | Phase | Priority |
|----------|----------|-------|----------|
| `TASK_DSHOT_PWM_IMPLEMENTATION.md` | 5 | 1A | CRITICAL |
| `TASK_CLICK_BOARD_SENSOR_TESTING.md` | 2 | 1B | HIGH |
| `TASK_LOGGER_DEBUG_ENABLE.md` | 1 | 1C | HIGH |
| `TASK_HITL_TESTING.md` | 3 | 2A | MEDIUM |
| `TASK_CONSOLE_BUFFER_DEBUG.md` | 4 | 3A | LOW |

### Documents To Be Created
| Document | Scope | Priority |
|----------|-------|----------|
| `TASK_ADC_BATTERY_MONITOR.md` | ADC configuration, battery monitoring | HIGH |
| `TASK_QSPI_FLASH_STORAGE.md` | SST26 flash, parameter storage | MEDIUM |

**Note:** `TASK_BLOCKINGLIST_FIX.md` not needed - issue already fixed in `BlockingList.hpp`.

### Reference Documents
| Document | Purpose |
|----------|---------|
| `FMU6X_SAMV71_DETAILED_COMPARISON.md` | Gap analysis source |
| `SAMV71_IMPLEMENTATION_TRACKER.md` | Project status tracking |
| `CLICK_BOARD_VALIDATION_GUIDE.md` | Quick sensor test reference |

---

## Appendix A: Code Files to Create/Modify

### Phase 1A: PWM/Timer

| File | Action | Description |
|------|--------|-------------|
| `platforms/nuttx/src/px4/microchip/samv7/io_pins/io_timer.c` | CREATE | Full io_timer implementation |
| `boards/microchip/samv71-xult-clickboards/src/timer_config.cpp` | CREATE | Pin/timer mapping |
| `platforms/nuttx/src/px4/microchip/samv7/dshot/dshot.c` | CREATE | DShot protocol (optional) |
| `platforms/nuttx/src/px4/microchip/samv7/io_pins/CMakeLists.txt` | MODIFY | Add io_timer.c |

### Phase 1C: Logger

| File | Action | Description |
|------|--------|-------------|
| `arch/arm/src/samv7/sam_hsmci.c` | INVESTIGATE | DMA write path |
| `boards/microchip/samv71-xult-clickboards/nuttx-config/nsh/defconfig` | MODIFY | HSMCI config |

### Phase 2C: QSPI Storage

| File | Action | Description |
|------|--------|-------------|
| `boards/microchip/samv71-xult-clickboards/src/spi.cpp` | MODIFY | QSPI configuration |
| `boards/microchip/samv71-xult-clickboards/src/init.c` | MODIFY | MTD initialization |

### Phase 3A: Console Buffer

| File | Action | Description |
|------|--------|-------------|
| `platforms/nuttx/src/px4/common/console_buffer.cpp` | MODIFY | Lazy init |
| `boards/microchip/samv71-xult-clickboards/src/board_config.h` | MODIFY | Enable feature |

---

## 2-Day Coding Sprint (Nov 29-30, 2025)

**Goal:** Complete all code changes without hardware, ready for Monday testing.
**Constraint:** No sensor hardware available until Monday.

### Task Assignments

| Task | Assignee | Priority | Dependencies | Est. Hours |
|------|----------|----------|--------------|------------|
| PWM/io_timer arch implementation | Codex | CRITICAL | None | 8-12 |
| PWMSim re-entrancy guard | Codex | HIGH | PWM work | 2-3 |
| Console buffer lazy init | Codex | MEDIUM | None | 2-3 |
| SD logger write-path fix | Claude | HIGH | None | 6-8 |
| QSPI parameter storage scaffolding | Claude | MEDIUM | None | 3-4 |
| ADC pin mapping scaffolding | Claude | LOW | None | 2-3 |
| Hardware test checklist | Bhanu | - | All above | 1-2 |

### Codex Tasks (PWM Focus)

#### 1. PWM/io_timer Implementation
**Files to create/modify:**
- `platforms/nuttx/src/px4/microchip/samv7/io_pins/io_timer.c` - Full implementation
- `boards/microchip/samv71-xult-clickboards/src/timer_config.cpp` - Pin mapping
- `platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/io_timer_hw_description.h` - Update if needed

**Deliverables:**
- [ ] TC1/TC2 timer initialization working
- [ ] `pwm_out start` succeeds
- [ ] `pwm info` shows channels
- [ ] Code compiles with `CONFIG_DRIVERS_PWM_OUT=y`

#### 2. PWMSim Re-entrancy Guard
**File:** `src/modules/simulation/pwm_out_sim/PWMSim.cpp`

**Deliverables:**
- [ ] Add deferred subscription update pattern
- [ ] Remove `#if !defined(CONFIG_ARCH_CHIP_SAMV7)` guard
- [ ] Verify no crash on work queue switch

#### 3. Console Buffer Lazy Init
**Files:**
- `platforms/nuttx/src/px4/common/console_buffer.cpp`
- `platforms/nuttx/src/px4/common/include/px4_platform/console_buffer.h`
- `boards/microchip/samv71-xult-clickboards/src/board_config.h`

**Deliverables:**
- [ ] Lazy init pattern implemented
- [ ] `BOARD_ENABLE_CONSOLE_BUFFER` enabled
- [ ] `dmesg` command available

### Claude Tasks (Storage/Debug Focus)

#### 1. SD Logger Write-Path Fix
**Context:** DMA works for reads only. PIO writes work for small operations but hang under sustained load (logger).

**Files to investigate/modify:**
- `platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/sam_hsmci.c`
- Board defconfig for HSMCI options

**Approach:**
- Option A: Fix DMA write path (cache coherency - flush before TX DMA)
- Option B: Fix PIO write hang (identify blocking point under load)

**Deliverables:**
- [ ] Root cause identified
- [ ] Fix implemented (either DMA or PIO path)
- [ ] Stress test script created
- [ ] Feature flag: `SAMV71_SD_WRITE_FIX`

#### 2. QSPI Parameter Storage Scaffolding
**Files:**
- `boards/microchip/samv71-xult-clickboards/nuttx-config/nsh/defconfig` - Enable SST26
- `boards/microchip/samv71-xult-clickboards/src/init.c` - MTD init
- `boards/microchip/samv71-xult-clickboards/src/board_config.h` - QSPI defines

**Deliverables:**
- [ ] SST26 driver enabled in defconfig
- [ ] QSPI pin configuration added
- [ ] MTD partition init stub
- [ ] Kconfig option for param storage location
- [ ] Default: SD (switch to QSPI after validation)

#### 3. ADC Pin Mapping Scaffolding
**Files:**
- `boards/microchip/samv71-xult-clickboards/src/board_config.h` - ADC channel defines

**Deliverables:**
- [ ] AFEC channels mapped from SAMV71-XULT schematic
- [ ] `ADC_BATTERY_VOLTAGE_CHANNEL` etc. defined
- [ ] Driver enable behind Kconfig flag

### Test Gates for Monday

| Test | Command | Pass Criteria |
|------|---------|---------------|
| PWM channels | `pwm_out start && pwm info` | Shows 4+ channels |
| PWM output | Oscilloscope on TIOA/B pins | 50-400Hz, 1000-2000µs |
| SD write | `logger on` for 10 min | No hang, valid .ulg |
| Console buffer | `dmesg` | Shows boot log |
| QSPI mount | `mount` (if enabled) | MTD partition visible |

### Feature Flags (Safety)

All new code paths behind compile-time flags:
```c
// boards/microchip/samv71-xult-clickboards/src/board_config.h

// Set to 1 when ready to test PWM output
#define SAMV71_ENABLE_PWM_OUT           0

// Set to 1 to use QSPI for parameters instead of SD
#define SAMV71_USE_QSPI_PARAMS          0

// Set to 1 to enable SD write fix (DMA or improved PIO)
#define SAMV71_SD_WRITE_FIX_ENABLED     0

// Set to 1 to enable ADC battery monitoring
#define SAMV71_ENABLE_ADC_BATTERY       0
```

---

## Appendix B: Weekly Milestone Targets

### Week 1
- [ ] PWM: io_timer skeleton compiling
- [ ] Sensors: Click boards ordered/received
- [ ] Logger: Root cause investigation started

### Week 2
- [ ] PWM: Basic 50Hz PWM output verified
- [ ] Sensors: ICM-20689 tested and working
- [ ] Logger: Fix approach determined

### Week 3
- [ ] PWM: 4-channel PWM, ESC testing
- [ ] Sensors: All P1 sensors validated
- [ ] Logger: Fix implemented, testing

### Week 4
- [ ] HITL: jMAVSim connected, basic flight
- [ ] Integration: All Phase 1 complete
- [ ] Documentation: Test reports submitted

### Week 5+
- [ ] HITL: Extended testing, mission flights
- [ ] Hardening: Phase 3 tasks
- [ ] Polish: Code cleanup, upstream prep

---

## Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2025-11-29 | Claude | Initial master plan |
| 1.1 | 2025-11-29 | Claude | Corrections: Timer resource reality (TC1/TC2, MAX_IO_TIMERS=3); BlockingList already fixed; PWMSim issue is re-entrancy not mutex; HITL can run parallel with sensors |
| 1.2 | 2025-11-29 | Team | Added 2-day coding sprint (Nov 29-30): Codex=PWM/io_timer, Claude=SD/QSPI/ADC, Bhanu=test checklist; Feature flags for safety |

---

## Approval

| Role | Name | Date | Signature |
|------|------|------|-----------|
| Technical Lead | | | |
| Project Manager | | | |
| Hardware Lead | | | |

---

**END OF DOCUMENT**
