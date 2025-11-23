# SAMV71-XULT PX4 Flight Controller Project Plan
## UPDATED - Based on Current 48% Completion Status

**Last Updated:** November 23, 2025
**Current Status:** 48% Complete - Core platform working, sensors/flight control pending
**Team:** Team B (3 software engineers, task-based)
**Timeline Remaining:** 8-12 weeks to flight-ready system

---

## 📊 Current Status Summary

```
COMPLETED (Sprint 0-2 Equivalent):
├── Core System:        ████████████████████░░ 90% ✅
├── Communication:      ███████████████░░░░░░░ 75% ✅
├── Storage:            ████████████████████░░ 90% ✅ (HSMCI fixed!)
└── PX4 Infrastructure: ████████████████░░░░░░ 80% ✅

REMAINING WORK:
├── Sensors:            ░░░░░░░░░░░░░░░░░░░░░░  0% ← NEXT
├── Motor Outputs:      ░░░░░░░░░░░░░░░░░░░░░░  0%
├── Navigation:         ██████░░░░░░░░░░░░░░░░ 30% (dataman/navigator disabled)
├── Logging:            ░░░░░░░░░░░░░░░░░░░░░░  0% (disabled)
└── Simulation:         ░░░░░░░░░░░░░░░░░░░░░░  0%
```

---

## ✅ What You've Already Accomplished

### Sprint 0-2 Equivalent: Foundation Complete

**Core System (90% Complete):**
- ✅ NuttX 11.0.0 boots successfully
- ✅ NSH shell working instantly
- ✅ 300 MHz Cortex-M7 running
- ✅ Cache enabled (I-Cache, D-Cache write-through for DMA)
- ✅ HRT (High-Resolution Timer) working and validated
- ✅ Work queues functional (hpwork, lpwork)
- ✅ Memory: 384KB SRAM (27KB used, 6.9%), 2MB Flash (1020KB used, 48.6%)

**Storage (90% Complete):**
- ✅ **HSMCI driver working!** (SD card fully functional)
- ✅ FAT32 filesystem mounted at /fs/microsd
- ✅ DMA support with XDMAC
- ✅ Read/Write/Append/Delete operations all working
- ✅ Tested with 16GB card
- ⚠️ Known cosmetic issues: CMD1/CMD55 errors during init (don't affect functionality)
- ⚠️ Sustained writes can bottleneck (will affect logger)

**Communication (75% Complete):**
- ✅ USB CDC/ACM working (/dev/ttyACM0)
- ✅ USB High-Speed (480 Mbps) with DMA
- ✅ MAVLink v2 protocol working
- ✅ Heartbeat streaming (1 Hz)
- ✅ Bidirectional TX/RX confirmed
- ✅ MAVProxy tested: 13,784+ packets, 0 errors
- ✅ QGroundControl connects successfully
- ✅ Parameter protocol via MAVLink working
- ✅ DEBUG console via USART1/EDBG

**PX4 Infrastructure (80% Complete):**
- ✅ Parameter system working (376/1081 defaults loaded)
- ✅ Parameters save to /fs/microsd/params
- ✅ Parameters persist across reboot
- ✅ uORB messaging system functional
- ✅ Module framework working
- ✅ Event system operational
- ✅ System commands: top, ps, free, dmesg, ver, param, etc.

**This is EXCELLENT progress!** You've essentially completed my original Sprint 0-2.

---

## 🎯 Remaining Work: Sprints 3-10

### Your Current Phase Mapping

| Your Phase | My Sprint | Status | Description |
|------------|-----------|--------|-------------|
| ~~Phase 1: Re-enable Services~~ | Sprint 3 | **NEXT** | dataman, navigator, logger |
| ~~Phase 2: Sensor Integration~~ | Sprint 4-5 | Pending | IMU, Baro, Mag, GPS |
| ~~Phase 3: QGC Integration~~ | Sprint 5 | Pending | Full QGC testing |
| ~~Phase 4: Motor Outputs~~ | Sprint 6 | Pending | PWM/DShot |
| ~~Phase 5: Simulation~~ | Sprint 3 | Optional | SITL/Gazebo |
| ~~Phase 6: HITL~~ | Sprint 3 | Optional | Hardware-in-loop |
| ~~Phase 7: Real Hardware~~ | Sprint 7-8 | Pending | Flight testing |

---

## Sprint 3: Re-enable Services & HIL Validation (2 weeks)

**Objective:** Re-enable disabled features, validate computational capacity

### Track 1: Re-enable Disabled Services

| Task ID | Task Name | Hours | Skills | Hardware | Priority | Assignable To |
|---------|-----------|-------|--------|----------|----------|---------------|
| T1-S3-01 | Re-enable dataman in rcS | 2 | PX4 config | SAMV71 | HIGH | Any engineer |
| T1-S3-02 | Test dataman startup | 4 | Testing, debugging | SAMV71, SD card | HIGH | Same as T1-S3-01 |
| T1-S3-03 | Test mission upload via QGC | 4 | QGC, testing | SAMV71, QGC | HIGH | Any engineer |
| T1-S3-04 | Test mission download | 2 | QGC, testing | SAMV71, QGC | HIGH | Same as T1-S3-03 |
| T1-S3-05 | Re-enable navigator in rcS | 2 | PX4 config | SAMV71 | HIGH | Any engineer |
| T1-S3-06 | Test navigator startup | 4 | Testing, debugging | SAMV71 | HIGH | Same as T1-S3-05 |
| T1-S3-07 | Re-enable logger (MAVLink-only first) | 4 | PX4 config | SAMV71 | MEDIUM | Any engineer |
| T1-S3-08 | Test logger MAVLink streaming | 8 | Testing, log analysis | SAMV71, MAVProxy | MEDIUM | Any engineer |
| T1-S3-09 | Enable logger SD card logging | 4 | PX4 config | SAMV71, SD card | MEDIUM | Any engineer |
| T1-S3-10 | Test SD logging stability (1 hour) | 8 | Testing | SAMV71, SD card | MEDIUM | Any engineer |
| T1-S3-11 | Monitor for SD write bottlenecks | 4 | Performance analysis | SAMV71 | MEDIUM | Any engineer |
| T1-S3-12 | Document logger configuration | 4 | Documentation | None | LOW | Any engineer |

**Subtotal Track 1: 50 hours**

**Parallel Work:**
- Engineer A: T1-S3-01 → T1-S3-04 (dataman, 12h)
- Engineer B: T1-S3-05 → T1-S3-06 (navigator, 6h)
- Engineer C: T1-S3-07 → T1-S3-12 (logger, 32h)

### Track 2: Optional HIL Validation (Computational Gate)

*Note: This is OPTIONAL but recommended to validate CPU/RAM capacity before sensor integration*

| Task ID | Task Name | Hours | Skills | Hardware | Priority | Assignable To |
|---------|-----------|-------|--------|----------|----------|---------------|
| T2-S3-01 | Install Gazebo Classic on dev PC | 4 | Linux, ROS | Dev PC | OPTIONAL | Any engineer |
| T2-S3-02 | Setup PX4 SITL | 8 | PX4, Gazebo | Dev PC | OPTIONAL | Same as T2-S3-01 |
| T2-S3-03 | Test SITL simulation | 4 | Gazebo | Dev PC | OPTIONAL | Same as T2-S3-01 |
| T2-S3-04 | Configure SAMV71 for HITL mode | 8 | PX4, UART | SAMV71 | OPTIONAL | Any engineer |
| T2-S3-05 | Setup serial link: Gazebo → SAMV71 | 8 | Serial, debugging | SAMV71, USB-Serial | OPTIONAL | Same as T2-S3-04 |
| T2-S3-06 | Test sensor data flow in HITL | 8 | Debugging | SAMV71, Gazebo | OPTIONAL | Same as T2-S3-04 |
| T2-S3-07 | Add CPU/RAM monitoring | 8 | Profiling | SAMV71 | OPTIONAL | Any engineer |
| T2-S3-08 | Run 10-min hover test in HITL | 8 | Testing | SAMV71, Gazebo | OPTIONAL | Any engineer |
| T2-S3-09 | Analyze CPU/RAM usage | 8 | Analysis | None | OPTIONAL | Any engineer |
| T2-S3-10 | Document HITL results | 4 | Documentation | None | OPTIONAL | Any engineer |

**Subtotal Track 2: 68 hours**

**Decision:** Skip if confident in hardware, OR run in parallel with sensor work

### Sprint 3 Exit Criteria

**MUST HAVE:**
- ✅ dataman starts successfully
- ✅ navigator starts successfully
- ✅ Missions can be uploaded/downloaded via QGC
- ✅ logger starts (MAVLink mode minimum)

**NICE TO HAVE:**
- ✅ SD logging working without bottlenecks
- ✅ HITL validation complete (if attempted)
- ✅ CPU <70%, RAM <300KB confirmed

**Estimated Duration:** 1-2 weeks

---

## Sprint 4: First Sensor Integration (IMU) (2 weeks)

**Objective:** Get real sensor data flowing, validate EKF2 with IMU

### Your Existing Hardware

From your document, you need to determine which IMU you have:
- ICM-20689 (mentioned in your doc)
- Or other IMU on SAMV71-XULT-ClickBoards

### Track 1: IMU Driver Integration

| Task ID | Task Name | Hours | Skills | Hardware | Dependencies | Assignable To |
|---------|-----------|-------|--------|----------|--------------|---------------|
| T1-S4-01 | Identify IMU hardware on board | 2 | Hardware inspection | SAMV71 board | None | Any engineer |
| T1-S4-02 | Create rc.board_sensors script | 4 | Bash, PX4 | None | None | Any engineer |
| T1-S4-03 | Verify I2C/SPI bus working | 4 | I2C/SPI tools | SAMV71 | None | Any engineer |
| T1-S4-04 | Scan for I2C devices | 2 | i2cdetect | SAMV71 | T1-S4-03 | Same |
| T1-S4-05 | Enable ICM-20689 driver (or equivalent) | 8 | PX4 config, C | SAMV71 | T1-S4-01 | Any engineer |
| T1-S4-06 | Configure SPI peripheral | 8 | SPI, NuttX | SAMV71 | T1-S4-05 | Same as T1-S4-05 |
| T1-S4-07 | Test WHO_AM_I register | 4 | SPI debugging | SAMV71, IMU | T1-S4-06 | Same |
| T1-S4-08 | Verify sensor_accel uORB publishes | 4 | PX4 uORB, listener | SAMV71 | T1-S4-07 | Any engineer |
| T1-S4-09 | Verify sensor_gyro uORB publishes | 4 | PX4 uORB, listener | SAMV71 | T1-S4-08 | Same |
| T1-S4-10 | Static test: Check noise levels | 4 | Data analysis | SAMV71 | T1-S4-09 | Any engineer |
| T1-S4-11 | Dynamic test: Rotate board | 4 | Testing | SAMV71 | T1-S4-10 | Same |
| T1-S4-12 | Verify SPI timing with scope | 8 | Oscilloscope | SAMV71, scope | T1-S4-07 | Engineer w/ HW |

**Subtotal: 56 hours**

### Track 2: EKF2 Integration

| Task ID | Task Name | Hours | Skills | Hardware | Dependencies | Assignable To |
|---------|-----------|-------|--------|----------|--------------|---------------|
| T2-S4-01 | Configure EKF2 for IMU-only | 8 | PX4 EKF2 | SAMV71 | T1-S4-09 | Any engineer |
| T2-S4-02 | Verify sensor_combined topic | 4 | listener, uORB | SAMV71 | T2-S4-01 | Same |
| T2-S4-03 | Check EKF2 innovations | 4 | PX4 EKF2 | SAMV71 | T2-S4-02 | Same |
| T2-S4-04 | Test EKF2 convergence (IMU only) | 8 | Testing, analysis | SAMV71 | T2-S4-03 | Same |
| T2-S4-05 | Implement IMU calibration | 12 | PX4 calibration | SAMV71 | T1-S4-11 | Any engineer |
| T2-S4-06 | Test calibration procedure | 4 | Testing | SAMV71 | T2-S4-05 | Same |
| T2-S4-07 | Verify calibration params saved | 4 | PX4 params | SAMV71 | T2-S4-06 | Same |
| T2-S4-08 | Verify attitude estimate | 4 | listener, testing | SAMV71 | T2-S4-04 | Any engineer |
| T2-S4-09 | Document IMU configuration | 4 | Documentation | None | T2-S4-08 | Any engineer |

**Subtotal: 52 hours**

### Sprint 4 Exit Criteria

- ✅ IMU driver publishes sensor_accel at expected rate
- ✅ IMU driver publishes sensor_gyro at expected rate
- ✅ Data looks correct (static and dynamic tests)
- ✅ EKF2 converges with IMU-only
- ✅ Attitude estimate stable (±1 degree static)
- ✅ Calibration procedure working
- ✅ `listener vehicle_attitude` shows valid data

**Test Commands:**
```bash
nsh> listener sensor_accel -n 10
nsh> listener sensor_gyro -n 10
nsh> listener vehicle_attitude -n 10
nsh> commander status  # Should show "sensors OK"
```

**Estimated Duration:** 2 weeks

---

## Sprint 5: Full Sensor Suite (2 weeks)

**Objective:** Add Baro, Mag, GPS - Complete sensor integration

### Track 1: Remaining Sensors

| Task ID | Task Name | Hours | Skills | Hardware | Dependencies | Assignable To |
|---------|-----------|-------|--------|----------|--------------|---------------|
| T1-S5-01 | Enable DPS310 barometer driver | 8 | I2C, PX4 | SAMV71 | Sprint 4 | Any engineer |
| T1-S5-02 | Test CHIP_ID read | 2 | I2C debugging | SAMV71 | T1-S5-01 | Same |
| T1-S5-03 | Verify sensor_baro publishes | 4 | listener | SAMV71 | T1-S5-02 | Same |
| T1-S5-04 | Test altitude estimate | 4 | Testing | SAMV71 | T1-S5-03 | Same |
| T1-S5-05 | Enable AK09916 magnetometer driver | 8 | I2C, PX4 | SAMV71 | Sprint 4 | Any engineer |
| T1-S5-06 | Test WHO_AM_I read | 2 | I2C debugging | SAMV71 | T1-S5-05 | Same |
| T1-S5-07 | Verify sensor_mag publishes | 4 | listener | SAMV71 | T1-S5-06 | Same |
| T1-S5-08 | Implement mag calibration (360°) | 8 | PX4 calibration | SAMV71 | T1-S5-07 | Any engineer |
| T1-S5-09 | Test mag calibration | 4 | Testing | SAMV71 | T1-S5-08 | Same |
| T1-S5-10 | Enable GPS on USART2 | 8 | UART, GPS | SAMV71 | Sprint 4 | Any engineer |
| T1-S5-11 | Configure GPS baud rate | 4 | Serial config | SAMV71 | T1-S5-10 | Same |
| T1-S5-12 | Test GPS UART communication | 4 | Serial debugging | SAMV71, GPS | T1-S5-11 | Same |
| T1-S5-13 | Verify vehicle_gps_position publishes | 4 | listener | SAMV71, GPS | T1-S5-12 | Same |
| T1-S5-14 | Test GPS fix outdoors | 4 | Testing, outdoor | SAMV71, GPS outdoor | T1-S5-13 | Any engineer |
| T1-S5-15 | Test GPS HDOP, satellite count | 4 | GPS testing | SAMV71, GPS | T1-S5-14 | Same |

**Subtotal: 72 hours**

**Parallel Work:**
- Engineer A: Baro (T1-S5-01 → T1-S5-04, 18h)
- Engineer B: Mag (T1-S5-05 → T1-S5-09, 26h)
- Engineer C: GPS (T1-S5-10 → T1-S5-15, 28h)

### Track 2: Full Sensor Fusion

| Task ID | Task Name | Hours | Skills | Hardware | Dependencies | Assignable To |
|---------|-----------|-------|--------|----------|--------------|---------------|
| T2-S5-01 | Configure EKF2 baro fusion | 4 | PX4 EKF2 | SAMV71 | T1-S5-04 | Any engineer |
| T2-S5-02 | Configure EKF2 mag fusion | 4 | PX4 EKF2 | SAMV71 | T1-S5-09 | Any engineer |
| T2-S5-03 | Configure EKF2 GPS fusion | 8 | PX4 EKF2 | SAMV71 | T1-S5-15 | Any engineer |
| T2-S5-04 | Test all sensors simultaneously | 8 | Multi-sensor testing | SAMV71, all sensors | T2-S5-03 | Any engineer |
| T2-S5-05 | Monitor CPU with all sensors | 4 | Performance | SAMV71 | T2-S5-04 | Same |
| T2-S5-06 | Monitor RAM with all sensors | 4 | Performance | SAMV71 | T2-S5-04 | Same |
| T2-S5-07 | Verify EKF2 full fusion (outdoor) | 8 | Testing, outdoor | SAMV71, all sensors | T2-S5-04 | Any engineer |
| T2-S5-08 | Test EKF convergence outdoors | 4 | Testing, outdoor | SAMV71, outdoor | T2-S5-07 | Same |
| T2-S5-09 | Tune EKF2 parameters | 16 | PX4 EKF2 expert | SAMV71, outdoor | T2-S5-08 | Engineer w/ EKF2 |
| T2-S5-10 | Test fusion in various conditions | 12 | Testing | SAMV71, various | T2-S5-09 | Any engineer |
| T2-S5-11 | Document sensor configuration | 8 | Documentation | None | T2-S5-10 | Any engineer |

**Subtotal: 80 hours**

### Sprint 5 Exit Criteria

- ✅ All 4 sensors publishing reliably:
  - `listener sensor_accel`
  - `listener sensor_gyro`
  - `listener sensor_baro`
  - `listener sensor_mag`
  - `listener vehicle_gps_position`
- ✅ EKF2 full fusion working
- ✅ `listener vehicle_attitude` stable
- ✅ `listener vehicle_local_position` working
- ✅ `commander status` shows all sensors healthy
- ✅ GPS fix achieved outdoors
- ✅ Position hold <0.5m (outdoor, GPS)
- ✅ Heading accuracy <5 degrees
- ✅ CPU <75% with all sensors
- ✅ RAM <320KB

**Estimated Duration:** 2 weeks

---

## Sprint 6: Motor Outputs & Frame Integration (2-3 weeks)

**Objective:** Enable PWM outputs, integrate onto Holybro 500 frame

### Track 1: PWM Driver Development

| Task ID | Task Name | Hours | Skills | Hardware | Dependencies | Assignable To |
|---------|-----------|-------|--------|----------|--------------|---------------|
| T1-S6-01 | Research SAMV71 timer config | 8 | NuttX, timers, docs | Documentation | Sprint 5 | Any engineer |
| T1-S6-02 | Enable CONFIG_DRIVERS_PWM_OUT | 2 | Build config | None | T1-S6-01 | Any engineer |
| T1-S6-03 | Configure timer peripheral (TC0/1/2) | 12 | Timers, NuttX | SAMV71 | T1-S6-02 | Same as T1-S6-01 |
| T1-S6-04 | Define PWM output channels | 4 | board_config.h | None | T1-S6-03 | Same |
| T1-S6-05 | Implement 50Hz PWM (servos) | 12 | PWM, embedded | SAMV71 | T1-S6-04 | Same |
| T1-S6-06 | Map to 4 motor outputs | 4 | GPIO config | SAMV71 | T1-S6-05 | Same |
| T1-S6-07 | Test with servo tester | 4 | Hardware testing | SAMV71, servo tester | T1-S6-06 | Any engineer |
| T1-S6-08 | Verify timing with oscilloscope | 4 | Oscilloscope | SAMV71, scope | T1-S6-07 | Engineer w/ HW |
| T1-S6-09 | Test pwm_out command | 4 | Testing | SAMV71 | T1-S6-08 | Any engineer |
| T1-S6-10 | (Optional) Implement DShot | 16 | DMA, timers | SAMV71 | T1-S6-09 | Any engineer |

**Subtotal: 70 hours (54h without DShot)**

### Track 2: RC Input

| Task ID | Task Name | Hours | Skills | Hardware | Dependencies | Assignable To |
|---------|-----------|-------|--------|----------|--------------|---------------|
| T2-S6-01 | Configure UART for SBUS (100k inverted) | 8 | UART, SBUS | SAMV71 | Sprint 5 | Any engineer |
| T2-S6-02 | Implement SBUS parser | 12 | Protocol, C | None | T2-S6-01 | Same |
| T2-S6-03 | Publish input_rc uORB | 4 | PX4 uORB | SAMV71, RC RX | T2-S6-02 | Same |
| T2-S6-04 | Test with RC transmitter | 4 | RC equipment | SAMV71, RC TX/RX | T2-S6-03 | Any engineer w/ RC |
| T2-S6-05 | Verify channel mapping (AETR) | 4 | RC testing | SAMV71, RC | T2-S6-04 | Same |
| T2-S6-06 | Test failsafe (RC loss) | 4 | Safety testing | SAMV71, RC | T2-S6-05 | Same |
| T2-S6-07 | Document RC configuration | 4 | Documentation | None | T2-S6-06 | Any engineer |

**Subtotal: 40 hours**

### Track 3: Flight Controller Configuration

| Task ID | Task Name | Hours | Skills | Hardware | Dependencies | Assignable To |
|---------|-----------|-------|--------|----------|--------------|---------------|
| T3-S6-01 | Configure mixer (quad X) | 4 | PX4 mixers | None | T1-S6-09 | Any engineer |
| T3-S6-02 | Test motor mapping (props off!) | 8 | Safety, testing | SAMV71, ESCs, motors | T1-S6-09, T3-S6-01 | Any engineer |
| T3-S6-03 | Verify motor directions | 4 | Testing | SAMV71, motors | T3-S6-02 | Same |
| T3-S6-04 | Configure MC controllers | 12 | PX4 controllers | SAMV71 | Sprint 5 | Any engineer w/ PX4 |
| T3-S6-05 | Set conservative gains | 4 | PX4 tuning | None | T3-S6-04 | Same |
| T3-S6-06 | Test controller bench (no props) | 8 | Testing | SAMV71, RC | T2-S6-06, T3-S6-05 | Any engineer |
| T3-S6-07 | Configure safety (arming, failsafe) | 8 | PX4 safety | SAMV71 | T3-S6-06 | Any engineer |

**Subtotal: 48 hours**

### Track 4: Holybro 500 Frame Integration

| Task ID | Task Name | Hours | Skills | Hardware | Dependencies | Assignable To |
|---------|-----------|-------|--------|----------|--------------|---------------|
| T4-S6-01 | Disassemble Holybro 500 | 2 | Mechanical | Holybro frame | None | Any engineer |
| T4-S6-02 | Design SAMV71 mount (CAD) | 8 | CAD | CAD software | T4-S6-01 | Engineer w/ CAD |
| T4-S6-03 | 3D print mount | 4 | 3D printing | 3D printer | T4-S6-02 | Any engineer |
| T4-S6-04 | Mount SAMV71 on frame | 4 | Mechanical | SAMV71, frame | T4-S6-03 | Any engineer |
| T4-S6-05 | Wire 5V BEC | 4 | Soldering | BEC, wires | T4-S6-04 | Any engineer |
| T4-S6-06 | Wire ESCs to battery | 4 | Soldering | ESCs, PDB | T4-S6-05 | Same |
| T4-S6-07 | Connect ESC PWM signals | 4 | Wiring | ESCs, SAMV71 | T4-S6-06, T1-S6-09 | Same |
| T4-S6-08 | Connect RC receiver | 2 | Wiring | RC RX | T4-S6-07, T2-S6-06 | Same |
| T4-S6-09 | Connect all sensors | 4 | Wiring | GPS, etc | T4-S6-08 | Same |
| T4-S6-10 | Verify all connections | 4 | Testing, multimeter | Frame assembly | T4-S6-09 | Any engineer |
| T4-S6-11 | Bench power test | 4 | Safety | Bench PSU | T4-S6-10 | Any engineer |
| T4-S6-12 | Battery power test | 4 | Safety | Battery | T4-S6-11 | Any engineer |
| T4-S6-13 | Full system check | 4 | Testing | Full system | T4-S6-12 | Any engineer |
| T4-S6-14 | Calibrate sensors on frame | 4 | PX4 calibration | Full system | T4-S6-13 | Any engineer |
| T4-S6-15 | Document pre-flight checklist | 4 | Documentation | None | T4-S6-14 | Any engineer |

**Subtotal: 56 hours**

### Sprint 6 Exit Criteria

- ✅ PWM outputs verified with oscilloscope
- ✅ `pwm_out test` command working
- ✅ RC input working (all channels)
- ✅ `listener input_rc` shows all 16 channels
- ✅ Motors spin correctly (props off!)
- ✅ Motor directions verified
- ✅ Arming sequence works
- ✅ SAMV71 mounted on Holybro frame
- ✅ All wiring complete and verified
- ✅ Sensors calibrated on frame
- ✅ `commander status` shows ready to arm

**Test Commands:**
```bash
nsh> pwm_out status
nsh> pwm_out test -c 1 -p 1500
nsh> listener input_rc
nsh> commander status
nsh> commander arm
nsh> commander disarm
```

**Estimated Duration:** 2-3 weeks (longer if DShot required)

---

## Sprint 7: Tethered Flight Testing (2 weeks)

**Objective:** Validate flight readiness with tethered tests

**⚠️ SAFETY CRITICAL - ALL 3 ENGINEERS REQUIRED**

### Pre-Flight Validation

| Task ID | Task Name | Hours | Skills | Team | Dependencies |
|---------|-----------|-------|--------|------|--------------|
| T-S7-01 | Install propellers (correct direction) | 2 | Mechanical | 1 engineer | Sprint 6 |
| T-S7-02 | Balance propellers | 4 | Mechanical | 1 engineer | T-S7-01 |
| T-S7-03 | Motor thrust test (props on, secured) | 4 | Safety | 1 engineer | T-S7-02 |
| T-S7-04 | Verify battery monitoring | 2 | Testing | 1 engineer | T-S7-03 |
| T-S7-05 | Test low-battery warning | 2 | Testing | 1 engineer | T-S7-04 |
| T-S7-06 | Vibration test (ground, props on) | 4 | Testing | 1 engineer | T-S7-03 |
| T-S7-07 | Create tether rig (~1m height) | 4 | Mechanical | 1 engineer | None |
| T-S7-08 | Setup outdoor test area | 2 | Site prep | 1 engineer | None |
| T-S7-09 | Prepare safety equipment | 2 | Safety | 1 engineer | None |
| T-S7-10 | Document emergency procedures | 4 | Safety planning | 1 engineer | None |

**Subtotal: 30 hours**

### Tethered Test Execution

**⚠️ ALL TESTS REQUIRE ALL 3 ENGINEERS: Pilot, Safety, Data**

| Task ID | Task Name | Hours | Team | Dependencies |
|---------|-----------|-------|------|--------------|
| T-S7-11 | **Tethered Test #1: 10 sec hover @ 0.5m** | 4 | ALL 3 | T-S7-10 |
| T-S7-12 | Log analysis #1 | 8 | 1 engineer | T-S7-11 |
| T-S7-13 | Tuning based on Test #1 | 8 | 1 engineer | T-S7-12 |
| T-S7-14 | Hardware fixes from Test #1 | 8 | 1 engineer | T-S7-12 |
| T-S7-15 | **Tethered Test #2: 30 sec @ 1m** | 4 | ALL 3 | T-S7-13, T-S7-14 |
| T-S7-16 | Log analysis #2 | 4 | 1 engineer | T-S7-15 |
| T-S7-17 | **Tethered Test #3: 1 min, altitude changes** | 4 | ALL 3 | T-S7-16 |
| T-S7-18 | Log analysis #3 | 4 | 1 engineer | T-S7-17 |
| T-S7-19 | **Tethered Test #4: 2 min, aggressive inputs** | 4 | ALL 3 | T-S7-18 |
| T-S7-20 | Log analysis #4 | 4 | 1 engineer | T-S7-19 |
| T-S7-21 | Compile all data | 8 | 1 engineer | T-S7-20 |
| T-S7-22 | GO/NO-GO decision for free flight | 2 | ALL 3 | T-S7-21 |

**Subtotal: 62 hours**

### Sprint 7 Exit Criteria

- ✅ Tethered hover stable for 2+ minutes
- ✅ No oscillations (vibration <30 m/s²)
- ✅ Attitude stable (±5 degrees)
- ✅ Failsafe triggers correctly
- ✅ Battery monitoring accurate
- ✅ Board temp <60°C
- ✅ Logs show healthy system
- ✅ Team confident for free flight
- ✅ **GO decision for Sprint 8**

**Estimated Duration:** 2 weeks (weather dependent)

---

## Sprint 8: Free Flight Validation (2 weeks)

**Objective:** Achieve stable flight, validate basic flight modes

**⚠️ CRITICAL SAFETY SPRINT - ALL 3 ENGINEERS REQUIRED**

### Flight Test Execution

**All flights require: Pilot, Safety Observer, Data Logger**

| Task ID | Flight Test | Hours | Team | Dependencies |
|---------|-------------|-------|------|--------------|
| T-S8-01 | Pre-flight review meeting | 4 | ALL 3 | Sprint 7 GO |
| T-S8-02 | **Flight #1: First hover (0.5m, 10 sec)** | 2 | ALL 3 | T-S8-01 |
| T-S8-03 | Emergency log analysis | 2 | 1 engineer | T-S8-02 |
| T-S8-04 | GO/NO-GO for Flight #2 | 1 | ALL 3 | T-S8-03 |
| T-S8-05 | **Flight #2: Extended hover (1m, 30 sec)** | 2 | ALL 3 | T-S8-04 |
| T-S8-06 | Log analysis #2 | 2 | 1 engineer | T-S8-05 |
| T-S8-07 | **Flight #3: Altitude test (2m, 1 min)** | 3 | ALL 3 | T-S8-06 |
| T-S8-08 | Log analysis #3 | 3 | 1 engineer | T-S8-07 |
| T-S8-09 | **Flight #4: Position Hold (3m, POS mode)** | 3 | ALL 3 | T-S8-08 |
| T-S8-10 | Log analysis #4 | 3 | 1 engineer | T-S8-09 |
| T-S8-11 | Create 4-point mission (5m square) | 2 | 1 engineer | T-S8-10 |
| T-S8-12 | **Flight #5: Waypoint following** | 4 | ALL 3 | T-S8-11 |
| T-S8-13 | Waypoint accuracy analysis | 4 | 1 engineer | T-S8-12 |
| T-S8-14 | **Flight #6: Endurance (to low battery)** | 4 | ALL 3 | T-S8-13 |
| T-S8-15 | Endurance analysis | 4 | 1 engineer | T-S8-14 |
| T-S8-16 | **Flight #7: Aggressive maneuvers** | 3 | ALL 3 | T-S8-15 |
| T-S8-17 | Oscillation/vibration analysis | 3 | 1 engineer | T-S8-16 |
| T-S8-18 | **Flight #8: Failsafe test (5m, TX off)** | 3 | ALL 3 | T-S8-17 |
| T-S8-19 | Failsafe analysis | 2 | 1 engineer | T-S8-18 |
| T-S8-20 | Compile all flight data | 8 | 1 engineer | T-S8-19 |
| T-S8-21 | Create performance report | 8 | 1 engineer | T-S8-20 |
| T-S8-22 | Final Sprint 8 review | 4 | ALL 3 | T-S8-21 |

**Total: ~70 hours**

### Sprint 8 Exit Criteria

- ✅ Stable hover achieved (1+ min)
- ✅ Position hold working (<1m drift)
- ✅ Waypoint following (<2m error)
- ✅ Endurance >10 minutes
- ✅ Failsafe functional (RTL/Land)
- ✅ No crashes
- ✅ Team confident in stability

**Estimated Duration:** 2 weeks (weather dependent)

---

## Sprint 9-10: Optimization & Documentation (3-4 weeks)

**Objective:** Production polish, complete documentation

### Sprint 9: Optimization

| Task Category | Tasks | Hours | Assignable To |
|---------------|-------|-------|---------------|
| Performance | Analyze hotspots, optimize loops, tune scheduler | 28 | Any engineer |
| Reliability | 100+ flight stress test, environmental testing | 48 | Team rotation |
| Advanced Flight | Precision tests, fast forward, mission replay | 20 | Pilot + 1 |

### Sprint 10: Documentation

| Task Category | Tasks | Hours | Assignable To |
|---------------|-------|-------|---------------|
| User Docs | Hardware guide, build instructions, calibration | 32 | Any engineer |
| Code Quality | Code review, comments, unit tests | 36 | Any engineer |
| PIC32CZ80 Plan | Migration planning, architecture analysis | 32 | Engineer w/ NuttX |
| Final | Retrospective, handoff, stakeholder demo | 12 | ALL 3 |

**Total Sprint 9-10: ~208 hours over 3-4 weeks**

---

## Timeline Summary

### Completed (Sprint 0-2 Equivalent)
- ✅ **Weeks 1-6:** Foundation complete (48% done)

### Remaining Timeline

| Sprint | Duration | Calendar Weeks | Cumulative |
|--------|----------|----------------|------------|
| Sprint 3: Re-enable Services + Optional HIL | 1-2 weeks | Weeks 7-8 | 50% |
| Sprint 4: First Sensor (IMU) | 2 weeks | Weeks 9-10 | 60% |
| Sprint 5: Full Sensors | 2 weeks | Weeks 11-12 | 75% |
| Sprint 6: Motors & Frame | 2-3 weeks | Weeks 13-15 | 85% |
| Sprint 7: Tethered Tests | 2 weeks | Weeks 16-17 | 92% |
| Sprint 8: Free Flight | 2 weeks | Weeks 18-19 | 97% |
| Sprint 9-10: Polish & Docs | 3-4 weeks | Weeks 20-23 | 100% |

**Total Remaining:** 14-17 weeks (3.5-4.25 months)
**Total Project:** 20-23 weeks (~5-6 months from project start)
**Remaining from Now:** 14-17 weeks to flight-ready

---

## Quick Start: Next Steps (This Week)

### Immediate Actions (Sprint 3 Start)

**Day 1: Re-enable dataman**
```bash
# Edit ROMFS/px4fmu_common/init.d/rcS
# Uncomment lines 277-287 (dataman section)
make microchip_samv71-xult-clickboards_default
# Flash and test
nsh> ps | grep dataman
```

**Day 2: Re-enable navigator**
```bash
# Edit ROMFS/px4fmu_common/init.d/rcS
# Uncomment line 543 (navigator start)
make microchip_samv71-xult-clickboards_default
# Flash and test
nsh> ps | grep navigator
```

**Day 3: Test mission upload**
```bash
# Connect QGC
# Try uploading a simple mission
# Verify: nsh> ls -l /fs/microsd/dataman
```

**Day 4-5: Re-enable logger**
```bash
# Edit boards/microchip/samv71-xult-clickboards/init/rc.logging
# Replace with MAVLink-only mode first
make microchip_samv71-xult-clickboards_default
# Flash and test
nsh> logger status
```

---

## Success Metrics

### Current Status (48% Complete)
- ✅ Core system working
- ✅ Storage working (SD card!)
- ✅ Communication working (MAVLink, QGC)
- ✅ Parameters working

### Minimal Viable System (Target: Sprint 5, ~75%)
- ✅ All above
- [ ] All sensors providing data
- [ ] EKF2 estimating position
- [ ] Motor outputs functional
- [ ] Can arm/disarm safely

### Flight Ready (Target: Sprint 8, ~97%)
- [ ] Stable hover achieved
- [ ] Position hold working
- [ ] Mission execution functional
- [ ] Failsafes validated
- [ ] Safety tested

### Production Ready (Target: Sprint 10, 100%)
- [ ] 100+ successful flights
- [ ] Documentation complete
- [ ] Long-term stability proven
- [ ] Ready for real missions

---

## Risk Management

### Critical Risks

| Risk | Probability | Impact | Sprint | Mitigation |
|------|------------|--------|--------|------------|
| Sensor drivers fail | Medium | High | 4-5 | Use reference drivers from other boards |
| PWM implementation complex | High | High | 6 | Budget extra time, research SAMV71 examples |
| Unstable flight | Medium | Critical | 7-8 | Extensive tethered tests, conservative tuning |
| Weather delays | High | Low | 7-8 | Indoor work backup, flexible schedule |

### Current Advantages

✅ **SD card already working** - Your HSMCI fixes are solid
✅ **MAVLink validated** - Communication proven
✅ **48% complete** - Strong foundation
✅ **Flash/RAM headroom** - 48.6% flash, 6.9% RAM used

---

## Notes for Your Team

### What You've Done Exceptionally Well

1. **HSMCI Debug & Fix** - The fact that SD card is fully working puts you ahead
2. **Parameter System** - 376/1081 defaults working is solid
3. **MAVLink** - 13,784+ packets with 0 errors is production-grade
4. **Memory Management** - Only 6.9% RAM used leaves lots of headroom

### What's Interesting About Your Setup

Your task list shows you have **ClickBoards** which suggests modular sensor boards. This could make sensor integration easier if you have:
- ICM-20689 Click
- DPS310 Click (barometer)
- AK09916 Click (magnetometer)

**Question for your team:** What sensors do you physically have on the ClickBoards?

### Recommended Focus Order

1. **Sprint 3 (This Week):** Re-enable dataman, navigator, logger - Easy wins
2. **Sprint 4 (Weeks 2-3):** IMU first - Hardest sensor, but enables EKF2
3. **Sprint 5 (Weeks 4-5):** Other sensors - Should be easier after IMU
4. **Sprint 6 (Weeks 6-8):** PWM - Most complex remaining task
5. **Sprint 7-8 (Weeks 9-12):** Flight testing - Weather dependent

---

**Next Review:** End of Sprint 3 (2 weeks)
**Document Version:** 2.0 - Updated from Current Status
**Updated:** November 23, 2025
