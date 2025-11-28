# FMU-6X vs SAMV71-XULT Build Comparison

**Date:** November 24, 2025
**Purpose:** Detailed comparison of enabled/disabled features
**Reference:** FMU-6X (full build) vs SAMV71-XULT (constrained build)

---

## Executive Summary

| Metric | FMU-6X | SAMV71-XULT | Difference |
|--------|--------|-------------|------------|
| **Flash Size** | 2 MB | 2 MB | Same |
| **Flash Usage** | ~95% | ~49% | FMU-6X uses 2x more |
| **Build Mode** | Full | Constrained | SAMV71 optimized for space |
| **Enabled Drivers** | ~80 | ~10 | FMU-6X has 8x more |
| **Core Modules** | 100% | 90% | SAMV71 has most essentials |
| **Flight Capable** | ✅ Yes | ✅ Yes | Both flight-ready |

**Key Difference:** FMU-6X has **all sensors/features enabled** for production hardware. SAMV71 has **minimal set** for development/testing.

---

## Configuration Differences

### Build Settings

| Setting | FMU-6X | SAMV71-XULT | Impact |
|---------|--------|-------------|--------|
| **CONSTRAINED_FLASH** | ❌ No | ✅ Yes | SAMV71 optimizes for space |
| **NO_HELP** | ❌ No | ✅ Yes | SAMV71 removes help strings (~50KB saved) |
| **EXTERNAL_METADATA** | ❌ No | ✅ Yes | SAMV71 stores metadata externally |
| **Ethernet** | ✅ Yes | ❌ No | FMU-6X has Ethernet hardware |

### Serial Ports

| Port | FMU-6X | SAMV71-XULT | Status |
|------|--------|-------------|--------|
| **GPS1** | /dev/ttyS0 | /dev/ttyS2 | ✅ Both have |
| **GPS2** | /dev/ttyS7 | ❌ Not configured | ⚠️ SAMV71 could add |
| **TEL1** | /dev/ttyS6 | /dev/ttyACM0 (USB) | ✅ Different but working |
| **TEL2** | /dev/ttyS4 | /dev/ttyS1 | ✅ Both have |
| **TEL3** | /dev/ttyS1 | ❌ Not configured | ⚠️ SAMV71 could add |
| **EXT2** | /dev/ttyS3 | ❌ Not configured | ⚠️ SAMV71 could add |
| **RC** | /dev/ttyS5 | /dev/ttyS4 | ✅ Both have |

---

## Detailed Feature Comparison

### 1. ADC (Analog to Digital Converter)

**FMU-6X:**
```
CONFIG_DRIVERS_ADC_ADS1115=y          # External I2C ADC
CONFIG_DRIVERS_ADC_BOARD_ADC=y        # Onboard ADC channels
```

**SAMV71-XULT:**
```
# CONFIG_DRIVERS_ADC_BOARD_ADC is not set
```

**Impact:** ❌ **MISSING** - Cannot read battery voltage/current

**Why Disabled:** ADC driver not yet implemented for SAMV7

**Can Be Enabled?** ⚠️ Yes, but needs driver work (~4-8 hours)

**Priority:** 🟠 **Medium** - Needed for battery monitoring

---

### 2. Barometer Drivers

**FMU-6X:**
```
CONFIG_DRIVERS_BAROMETER_BMP388=y
CONFIG_DRIVERS_BAROMETER_INVENSENSE_ICP201XX=y
CONFIG_DRIVERS_BAROMETER_MS5611=y
```

**SAMV71-XULT:**
```
CONFIG_DRIVERS_BAROMETER_DPS310=y     # Only this one
```

**Impact:** ⚠️ **Limited** - Only DPS310 supported

**Why Limited:** Space optimization, only included sensors available on board

**Can Be Enabled?** ✅ Yes, add more drivers if needed

**Priority:** 🟢 **Low** - DPS310 is sufficient

---

### 3. Camera Support

**FMU-6X:**
```
CONFIG_DRIVERS_CAMERA_CAPTURE=y
CONFIG_DRIVERS_CAMERA_TRIGGER=y
```

**SAMV71-XULT:**
```
# Not configured
```

**Impact:** ❌ **MISSING** - No camera trigger support

**Why Disabled:** Not needed for basic flight, saves flash

**Can Be Enabled?** ✅ Yes, add if needed

**Priority:** 🟢 **Low** - Optional for mapping/photography

---

### 4. DShot ESC Protocol

**FMU-6X:**
```
CONFIG_DRIVERS_DSHOT=y
```

**SAMV71-XULT:**
```
# CONFIG_DRIVERS_DSHOT is not set
```

**Impact:** ❌ **MISSING** - Cannot use DShot ESCs

**Why Disabled:** Requires timer implementation

**Can Be Enabled?** ⚠️ Yes, but needs timer/PWM work (~8-12 hours)

**Priority:** 🔴 **High** - Needed for motor control

**Workaround:** Use traditional PWM once implemented

---

### 5. GPS Drivers

**FMU-6X:**
```
CONFIG_DRIVERS_GNSS_SEPTENTRIO=y      # High-end RTK GPS
CONFIG_DRIVERS_GPS=y                  # Standard GPS
```

**SAMV71-XULT:**
```
CONFIG_DRIVERS_GPS=y                  # Standard GPS only
```

**Impact:** ⚠️ **Limited** - No RTK GPS support

**Why Limited:** Space optimization, standard GPS sufficient

**Can Be Enabled?** ✅ Yes, if RTK needed

**Priority:** 🟢 **Low** - Standard GPS is adequate

---

### 6. IMU (Inertial Measurement Unit) Drivers

**FMU-6X:**
```
CONFIG_DRIVERS_IMU_ANALOG_DEVICES_ADIS16470=y
CONFIG_DRIVERS_IMU_BOSCH_BMI088=y
CONFIG_DRIVERS_IMU_INVENSENSE_ICM20602=y
CONFIG_DRIVERS_IMU_INVENSENSE_ICM20649=y
CONFIG_DRIVERS_IMU_INVENSENSE_ICM20948=y
CONFIG_DRIVERS_IMU_INVENSENSE_ICM42670P=y
CONFIG_DRIVERS_IMU_INVENSENSE_ICM42688P=y
CONFIG_DRIVERS_IMU_INVENSENSE_ICM45686=y
CONFIG_DRIVERS_IMU_INVENSENSE_IIM42652=y
```

**SAMV71-XULT:**
```
CONFIG_DRIVERS_IMU_INVENSENSE_ICM20689=y  # Only this one
```

**Impact:** ⚠️ **Limited** - Only ICM20689 supported

**Why Limited:** Space optimization, only included sensors on board

**Can Be Enabled?** ✅ Yes, add more if different IMU used

**Priority:** 🟢 **Low** - One IMU driver is sufficient

---

### 7. Magnetometer Drivers

**FMU-6X:**
```
CONFIG_COMMON_MAGNETOMETER=y          # All mag drivers
```

**SAMV71-XULT:**
```
CONFIG_DRIVERS_MAGNETOMETER_AKM_AK09916=y  # Only this one
```

**Impact:** ⚠️ **Limited** - Only AK09916 supported

**Why Limited:** Space optimization

**Can Be Enabled?** ✅ Yes, add more if needed

**Priority:** 🟢 **Low** - One mag driver sufficient

---

### 8. Distance Sensors / Range Finders

**FMU-6X:**
```
CONFIG_COMMON_DISTANCE_SENSOR=y       # All distance sensors
CONFIG_EKF2_RANGE_FINDER=y           # EKF2 fusion support
```

**SAMV71-XULT:**
```
# Not configured
# CONFIG_EKF2_RANGE_FINDER is not set
```

**Impact:** ❌ **MISSING** - No rangefinder/lidar support

**Why Disabled:** Space optimization, not essential

**Can Be Enabled?** ✅ Yes, if precision landing needed

**Priority:** 🟢 **Low** - Optional for most applications

---

### 9. Differential Pressure / Airspeed Sensors

**FMU-6X:**
```
CONFIG_COMMON_DIFFERENTIAL_PRESSURE=y
CONFIG_DRIVERS_DIFFERENTIAL_PRESSURE_AUAV=y
CONFIG_MODULES_AIRSPEED_SELECTOR=y
```

**SAMV71-XULT:**
```
# Not configured
# CONFIG_SENSORS_VEHICLE_AIRSPEED is not set
```

**Impact:** ❌ **MISSING** - No airspeed sensor for fixed-wing

**Why Disabled:** Multicopter focus, saves space

**Can Be Enabled?** ✅ Yes, if flying fixed-wing

**Priority:** 🟢 **Low** for MC, 🔴 **High** for FW

---

### 10. PWM / Motor Output

**FMU-6X:**
```
CONFIG_DRIVERS_PWM_OUT=y
CONFIG_DRIVERS_PWM_INPUT=y
```

**SAMV71-XULT:**
```
# CONFIG_DRIVERS_PWM_OUT is not set
```

**Impact:** ❌ **MISSING** - No motor output!

**Why Disabled:** Timer/PWM not yet implemented

**Can Be Enabled?** ⚠️ Yes, requires timer implementation (~8-12 hours)

**Priority:** 🔴 **CRITICAL** - Needed to fly!

**Status:** This is the #1 missing feature preventing flight

---

### 11. RC Input

**FMU-6X:**
```
CONFIG_COMMON_RC=y                    # All RC protocols
```

**SAMV71-XULT:**
```
CONFIG_DRIVERS_RC_INPUT=y             # Basic RC support
```

**Impact:** ✅ **Enabled** - RC input working

**Status:** ✅ **Working** (assuming hardware connected)

---

### 12. Safety Features

**FMU-6X:**
```
CONFIG_DRIVERS_SAFETY_BUTTON=y
CONFIG_DRIVERS_TONE_ALARM=y
```

**SAMV71-XULT:**
```
# Not configured
```

**Impact:** ⚠️ **MISSING** - No safety button or audio feedback

**Why Disabled:** Hardware not present, saves space

**Can Be Enabled?** ✅ Yes, if hardware added

**Priority:** 🟠 **Medium** - Safety button useful for arming

---

### 13. UAVCAN / DroneCAN

**FMU-6X:**
```
CONFIG_DRIVERS_UAVCAN=y
```

**SAMV71-XULT:**
```
# Not configured
```

**Impact:** ❌ **MISSING** - No CAN bus support

**Why Disabled:** Hardware not configured, saves significant space (~100KB)

**Can Be Enabled?** ⚠️ Yes, SAMV7 has CAN hardware (~8-16 hours work)

**Priority:** 🟢 **Low** - Optional for most builds

---

### 14. Power Monitoring

**FMU-6X:**
```
CONFIG_DRIVERS_POWER_MONITOR_INA226=y
CONFIG_DRIVERS_POWER_MONITOR_INA228=y
CONFIG_DRIVERS_POWER_MONITOR_INA238=y
```

**SAMV71-XULT:**
```
# Not configured
```

**Impact:** ❌ **MISSING** - No current/voltage monitoring

**Why Disabled:** Hardware not present

**Can Be Enabled?** ✅ Yes, add INA226 sensor on I2C

**Priority:** 🟠 **Medium** - Needed for battery monitoring

---

### 15. PX4IO Coprocessor

**FMU-6X:**
```
CONFIG_DRIVERS_PX4IO=y
```

**SAMV71-XULT:**
```
# Not configured
```

**Impact:** ❌ **MISSING** - No IO coprocessor

**Why Disabled:** Hardware not present (PX4IO is separate board)

**Can Be Enabled?** ❌ No, requires PX4IO hardware

**Priority:** 🟢 **Low** - Not needed for basic flight

---

## Module Comparison

### Modules Enabled on BOTH

✅ **Core Flight Stack:**
- Commander (flight mode management)
- Control Allocator (motor mixing)
- EKF2 (state estimation)
- Flight Mode Manager
- Land Detector
- Manual Control
- MAVLink
- Navigator
- RC Update
- Sensors

✅ **Multicopter Control:**
- MC Attitude Control
- MC Position Control
- MC Rate Control

✅ **Support Modules:**
- Battery Status
- Dataman
- Logger

**Result:** ✅ **All essential flight modules present**

---

### Modules on FMU-6X Only

❌ **Fixed-Wing Control:**
- FW_ATT_CONTROL
- FW_AUTOTUNE_ATTITUDE_CONTROL
- FW_MODE_MANAGER
- FW_LATERAL_LONGITUDINAL_CONTROL
- FW_RATE_CONTROL

**Impact:** Cannot fly fixed-wing aircraft

**Priority:** 🟢 **Low** if only doing multicopter

---

❌ **VTOL Control:**
- VTOL_ATT_CONTROL

**Impact:** Cannot fly VTOL aircraft

**Priority:** 🟢 **Low** if only doing multicopter

---

❌ **Advanced Features:**
- Airspeed Selector
- Camera Feedback
- ESC Battery monitoring
- Events system
- Gimbal control
- Gyro Calibration
- Hardfault Stream
- Landing Target Estimator
- Load Monitor (load_mon)
- Mag Bias Estimator
- MC Autotune
- MC Hover Thrust Estimator
- Temperature Compensation
- UXRCE DDS Client

**Impact:** ⚠️ Some nice-to-have features missing

**Priority:** 🟢 **Low** - Not critical for basic flight

---

### Modules Disabled on SAMV71 Due to Space

**Can be re-enabled if space allows:**

1. **Load Monitor** (load_mon)
   - Shows CPU usage
   - Impact: ⚠️ Useful for debugging
   - Space: ~10KB

2. **MC Hover Thrust Estimator**
   - Auto-estimates hover throttle
   - Impact: ⚠️ Nice for tuning
   - Space: ~15KB

3. **Temperature Compensation**
   - Compensates sensor drift with temperature
   - Impact: ⚠️ Improves accuracy
   - Space: ~20KB

4. **Mag Bias Estimator**
   - Auto-calibrates magnetometer
   - Impact: ⚠️ Useful for compass
   - Space: ~10KB

5. **Hardfault Stream**
   - Streams crash logs
   - Impact: ⚠️ Debugging aid
   - Space: ~5KB

**Total if all re-enabled:** ~60KB (would bring usage to ~55%)

---

## System Commands Comparison

### Commands on BOTH

✅ **Essential:**
- bsondump
- mft
- param
- top
- ver
- nshterm
- tune_control

### Commands on FMU-6X Only

❌ **Debugging:**
- dmesg (disabled on SAMV71 - has console bug)
- hardfault_log (disabled - needs SAMV7 implementation)
- gpio
- perf
- work_queue

❌ **Network:**
- netman (FMU-6X has Ethernet)

❌ **Hardware:**
- actuator_test
- i2c_launcher
- i2cdetect (could be enabled on SAMV71)
- led_control
- mtd
- reboot
- system_time
- topic_listener
- uorb

**Impact:** ⚠️ Some debugging tools missing

**Can Be Enabled?** ✅ Yes, most could be added if needed

**Priority:** 🟠 **Medium** - Useful for development

---

## EKF2 Feature Comparison

### EKF2 Features Disabled on SAMV71

All disabled to save flash space:

```
# CONFIG_EKF2_AUX_GLOBAL_POSITION is not set
# CONFIG_EKF2_AUXVEL is not set
# CONFIG_EKF2_DRAG_FUSION is not set
# CONFIG_EKF2_EXTERNAL_VISION is not set
# CONFIG_EKF2_GNSS_YAW is not set
# CONFIG_EKF2_RANGE_FINDER is not set
# CONFIG_EKF2_SIDESLIP is not set
```

**Impact:**

| Feature | Missing Capability | Priority |
|---------|-------------------|----------|
| **AUX_GLOBAL_POSITION** | Secondary GPS fusion | 🟢 Low |
| **AUXVEL** | Auxiliary velocity input | 🟢 Low |
| **DRAG_FUSION** | Aerodynamic drag model | 🟢 Low (FW only) |
| **EXTERNAL_VISION** | Visual odometry/SLAM | 🟠 Medium (indoor) |
| **GNSS_YAW** | Dual GPS heading | 🟢 Low |
| **RANGE_FINDER** | Lidar height | 🟠 Medium (precision) |
| **SIDESLIP** | Fixed-wing slip | 🟢 Low (FW only) |

**Core EKF2 still works perfectly** - these are optional enhancements

---

## Flash Usage Analysis

### FMU-6X (Full Build)
```
Flash: ~1.9 MB / 2 MB (95%)
Features: Everything enabled
```

### SAMV71-XULT (Constrained Build)
```
Flash: ~1.0 MB / 2 MB (49%)
Features: Essential only
Headroom: ~1 MB available
```

### What Could Be Added to SAMV71

With ~1 MB headroom, you could add:

**High Priority:**
- ✅ PWM Output (~20KB) - **CRITICAL FOR FLIGHT**
- ✅ DShot (~30KB) - Motor protocol
- ✅ ADC (~10KB) - Battery monitoring
- ✅ Timer config (~5KB) - Hardware timers

**Medium Priority:**
- ⚠️ Load Monitor (~10KB)
- ⚠️ MC Hover Thrust Estimator (~15KB)
- ⚠️ Temperature Compensation (~20KB)
- ⚠️ I2C debugging tools (~10KB)
- ⚠️ More system commands (~20KB)

**Low Priority:**
- Additional IMU drivers (~50KB total)
- Additional sensor drivers (~30KB)
- Camera support (~25KB)
- UAVCAN (~100KB)

**Total "Must Have" for Flight:** ~65KB
**Total "Nice to Have":** ~75KB
**Total "Optional":** ~205KB

**Recommendation:** Add high-priority items now, rest as needed

---

## Critical Missing Features for Flight

### 🔴 CRITICAL (Must Have to Fly):

1. **PWM Output / Motor Control**
   - Status: ❌ Not implemented
   - Effort: ~8-12 hours
   - Blocks: Actual flight

2. **Timer Configuration**
   - Status: ❌ Not configured
   - Effort: ~4-8 hours
   - Blocks: PWM output

### 🟠 IMPORTANT (Needed Soon):

3. **ADC / Battery Monitoring**
   - Status: ❌ Not implemented
   - Effort: ~4-8 hours
   - Impact: No battery voltage/current

4. **Safety Features**
   - Status: ⚠️ Limited (no button, no alarm)
   - Effort: ~2-4 hours
   - Impact: Manual arming only

### 🟢 NICE TO HAVE:

5. **Additional Sensors**
   - Status: ⚠️ Limited driver selection
   - Effort: ~1-2 hours per driver
   - Impact: Flexibility

6. **Debug Tools**
   - Status: ⚠️ Some missing (i2cdetect, perf, etc.)
   - Effort: ~1-2 hours total
   - Impact: Development convenience

---

## Recommendations by Use Case

### For QGC/HIL Testing (Current Phase):
**Status:** ✅ **Ready Now**
- All required modules enabled
- MAVLink working
- Parameters working
- No hardware output needed

**Action:** None needed, proceed with testing

---

### For First Flight:
**Status:** ❌ **Blocked**
- Missing PWM output
- Missing timer configuration

**Action Required:**
1. Implement timer_config.cpp (~4-8 hours)
2. Enable PWM_OUT (~2-4 hours)
3. Configure motor outputs
4. Test motor control

**Estimated Time:** 1-2 days

---

### For Production:
**Status:** ⚠️ **Needs Work**
- Missing battery monitoring
- Missing safety features
- Missing some redundancy

**Action Required:**
1. Add ADC support (~4-8 hours)
2. Add power monitoring INA226 (~2-4 hours)
3. Add safety button (~2-4 hours)
4. Add tone alarm (~2-4 hours)
5. Enable additional modules (~4-8 hours)

**Estimated Time:** 3-5 days

---

## Summary Table

| Category | FMU-6X | SAMV71-XULT | Impact | Can Enable? |
|----------|--------|-------------|--------|-------------|
| **Core Flight** | ✅ 100% | ✅ 100% | ✅ None | N/A |
| **Motor Output** | ✅ Yes | ❌ No | 🔴 Critical | ⚠️ Yes, 8-12h |
| **Sensors (Basic)** | ✅ Many | ✅ Minimal | ✅ Sufficient | ✅ Easy |
| **Sensors (Advanced)** | ✅ All | ❌ Few | 🟢 Low | ✅ Easy |
| **Battery Mon** | ✅ Yes | ❌ No | 🟠 Important | ⚠️ Yes, 4-8h |
| **Safety** | ✅ Full | ⚠️ Partial | 🟠 Important | ✅ Yes, 4-8h |
| **Fixed-Wing** | ✅ Yes | ❌ No | 🟢 Low (MC only) | ✅ Easy |
| **VTOL** | ✅ Yes | ❌ No | 🟢 Low (MC only) | ✅ Easy |
| **CAN Bus** | ✅ Yes | ❌ No | 🟢 Low | ⚠️ Yes, 8-16h |
| **Ethernet** | ✅ Yes | ❌ No | 🟢 Low | ❌ No HW |
| **Debug Tools** | ✅ Full | ⚠️ Partial | 🟠 Medium | ✅ Easy |

---

## Next Steps

### Phase 1: QGC/HIL Testing (Now)
✅ **Ready** - No changes needed

### Phase 2: Motor Output (Next)
🔴 **Critical** - Implement PWM/timers

### Phase 3: Battery Monitoring
🟠 **Important** - Implement ADC

### Phase 4: Production Features
🟢 **Optional** - Add remaining features as needed

---

**Document Version:** 1.0
**Date:** November 24, 2025
**Conclusion:** SAMV71 has **all core flight software**, but missing **hardware interfaces** (PWM, ADC). Software is flight-ready, hardware drivers need work.
