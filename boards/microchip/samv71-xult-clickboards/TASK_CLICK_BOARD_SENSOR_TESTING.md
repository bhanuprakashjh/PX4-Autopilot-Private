# Engineering Task: Click Board Sensor Testing

**Assigned To:** Engineer 2
**Priority:** HIGH
**Estimated Effort:** 2-3 days
**Prerequisites:** MIKROE Click boards, basic sensor knowledge, oscilloscope (optional)

---

## Executive Summary

The SAMV71-XULT board has been configured with drivers for 7 MIKROE Click board sensors. All drivers are compiled but have only been tested without hardware (graceful failure mode). This task involves connecting actual Click boards and validating that each sensor works correctly with PX4.

---

## Hardware Required

### Click Boards to Test

| Priority | MIKROE PID | Board Name | Sensor IC | Interface | I2C Addr |
|----------|------------|------------|-----------|-----------|----------|
| **P1** | MIKROE-4044 | 6DOF IMU 6 Click | ICM-20689 | SPI | N/A |
| **P1** | MIKROE-4231 | Compass 4 Click | AK09915/16 | I2C | 0x0C |
| **P1** | MIKROE-2293 | Pressure 3 Click | DPS310 | I2C | 0x77 |
| P2 | MIKROE-6514 | 6DOF IMU 27 Click | ICM-45686 | SPI | N/A |
| P2 | MIKROE-3566 | Pressure 5 Click | BMP388 | I2C | 0x76 |
| P2 | MIKROE-2935 | GeoMagnetic Click | BMM150 | I2C | 0x10 |
| P3 | MIKROE-3775 | 13DOF Click | BMI088+BMM150 | I2C | Multiple |

### Test Equipment
- SAMV71-XULT board with current firmware
- USB cable for power and console
- SD card (formatted FAT32)
- Oscilloscope (optional, for SPI/I2C debugging)
- Logic analyzer (optional, for protocol debugging)

---

## Sensor Datasheets & References

### ICM-20689 (SPI IMU)
- [Official Datasheet (TDK InvenSense)](https://invensense.tdk.com/download-pdf/icm-20689-datasheet/)
- [Digi-Key Datasheet](https://www.digikey.com/htmldatasheets/production/3360805/0/0/1/icm-20689-datasheet.html)
- [Arduino Library Reference](https://github.com/finani/ICM20689)

**Key Specifications:**
| Parameter | Value |
|-----------|-------|
| SPI Speed | Up to 8 MHz |
| Accel Range | ±2g, ±4g, ±8g, ±16g |
| Gyro Range | ±250, ±500, ±1000, ±2000 °/s |
| WHO_AM_I Register | 0x75, Value: 0x98 |
| FIFO Size | 4 KB |

### AK09916 (I2C Magnetometer)
- [AK09916 Datasheet](https://www.akm.com/akm/en/file/datasheet/AK09916C.pdf)

**Key Specifications:**
| Parameter | Value |
|-----------|-------|
| I2C Address | 0x0C |
| I2C Speed | Up to 400 kHz |
| Measurement Range | ±4912 µT |
| WHO_AM_I Register | 0x01, Value: 0x09 |

### DPS310 (I2C Barometer)
- [DPS310 Datasheet (Infineon)](https://www.infineon.com/dgdl/Infineon-DPS310-DataSheet-v01_02-EN.pdf)

**Key Specifications:**
| Parameter | Value |
|-----------|-------|
| I2C Address | 0x77 (or 0x76) |
| Pressure Range | 300-1200 hPa |
| Temperature Range | -40 to +85°C |
| Product ID Register | 0x0D, Value: 0x10 |

---

## Pre-Test Setup

### 1. Verify Firmware Version
```bash
nsh> ver all
# Should show: PX4 git-hash with samv7-custom branch
```

### 2. Check Driver Availability
```bash
nsh> icm20689 -h
nsh> ak09916 -h
nsh> dps310 -h
# Each should show help text, not "command not found"
```

### 3. Scan I2C Bus (Before Connecting Sensors)
```bash
nsh> i2cdetect -b 0
# Should show empty bus (no devices)
```

### 4. Understand Current Boot Behavior
Without sensors connected, boot log shows:
```
ERROR [SPI_I2C] icm20689: no instance started (no device on bus?)
WARN  [SPI_I2C] ak09916: no instance started (no device on bus?)
WARN  [SPI_I2C] dps310: no instance started (no device on bus?)
```
This is **expected** - drivers probe and fail gracefully.

---

## Test Procedures

### Test 1: ICM-20689 SPI IMU (MIKROE-4044)

#### Hardware Setup
1. Insert 6DOF IMU 6 Click into **mikroBUS Socket 1**
2. Verify orientation (component side up, notch aligned)
3. Power on SAMV71-XULT

#### Pin Mapping (Socket 1)
| Click Pin | SAMV71 Pin | Function |
|-----------|------------|----------|
| SCK | PD22 | SPI Clock |
| MISO | PD20 | SPI Data Out |
| MOSI | PD21 | SPI Data In |
| CS | PD25 | Chip Select |
| INT | PA0 | Data Ready |

#### Validation Steps

**Step 1: Check Boot Log**
```bash
# Reboot and check for successful detection
nsh> reboot
# Look for: icm20689 driver started successfully
```

**Step 2: Check Driver Status**
```bash
nsh> icm20689 status
```
**Expected Output:**
```
icm20689: Running on SPI bus 0
  sample rate: 1000 Hz
  FIFO: enabled
  temperature: 25.3 C
```

**Step 3: Verify Accelerometer Data**
```bash
nsh> listener sensor_accel 5
```
**Expected Output (stationary, level):**
```
TOPIC: sensor_accel
  timestamp: 12345678
  device_id: 0x...
  x: ~0.0 m/s²
  y: ~0.0 m/s²
  z: ~-9.81 m/s²
  temperature: ~25 C
```

**Step 4: Verify Gyroscope Data**
```bash
nsh> listener sensor_gyro 5
```
**Expected Output (stationary):**
```
TOPIC: sensor_gyro
  timestamp: 12345678
  x: ~0.0 rad/s
  y: ~0.0 rad/s
  z: ~0.0 rad/s
```

**Step 5: Motion Test**
```bash
nsh> listener sensor_accel 10
# Rotate/tilt board - values should change
# X-axis tilt: x value increases
# Y-axis tilt: y value increases
```

**Step 6: Data Rate Verification**
```bash
nsh> uorb status sensor_accel
# Check update rate - should be 400-1000 Hz
```

#### Troubleshooting ICM-20689

| Symptom | Possible Cause | Solution |
|---------|----------------|----------|
| "no device on bus" | Bad connection | Reseat Click board |
| WHO_AM_I mismatch | Wrong sensor | Check Click board model |
| Data stuck at 0 | Sensor in sleep | Check power-up sequence |
| Noisy data | Bad SPI signal | Check SPI clock speed, use scope |
| Wrong orientation | Mount issue | Verify sensor orientation in driver |

---

### Test 2: AK09916 I2C Magnetometer (MIKROE-4231)

#### Hardware Setup
1. Insert Compass 4 Click into **mikroBUS Socket 2** or Arduino I2C headers
2. Both share I2C0 (TWIHS0) bus
3. Power on SAMV71-XULT

#### Validation Steps

**Step 1: Scan I2C Bus**
```bash
nsh> i2cdetect -b 0
```
**Expected Output:**
```
     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
00:          -- -- -- -- -- -- -- -- -- 0c -- -- --
10: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
...
```
Device at 0x0C confirms AK09916 detected.

**Step 2: Start Driver (if not auto-started)**
```bash
nsh> ak09916 -I -b 0 start
```

**Step 3: Check Driver Status**
```bash
nsh> ak09916 status
```

**Step 4: Verify Magnetometer Data**
```bash
nsh> listener sensor_mag 5
```
**Expected Output:**
```
TOPIC: sensor_mag
  timestamp: 12345678
  device_id: 0x...
  x: varies (depends on local field)
  y: varies
  z: varies
  temperature: ~25 C
```

**Step 5: Compass Test**
```bash
nsh> listener sensor_mag 20
# Rotate board 360° - X/Y values should vary sinusoidally
# Total field magnitude should stay roughly constant
```

#### Troubleshooting AK09916

| Symptom | Possible Cause | Solution |
|---------|----------------|----------|
| Not on I2C scan | Wrong address | Check SDO pin state |
| Data stuck | Measurement mode | Verify continuous mode |
| High noise | EMI interference | Move away from motors/power |
| Incorrect values | Hard/soft iron | Requires calibration |

---

### Test 3: DPS310 I2C Barometer (MIKROE-2293)

#### Hardware Setup
1. Insert Pressure 3 Click into **mikroBUS Socket 2** or Arduino I2C headers
2. Can coexist with AK09916 on same I2C bus (different addresses)
3. Power on SAMV71-XULT

#### Validation Steps

**Step 1: Scan I2C Bus**
```bash
nsh> i2cdetect -b 0
```
**Expected:** Device at 0x77

**Step 2: Check Driver Status**
```bash
nsh> dps310 status
```

**Step 3: Verify Barometer Data**
```bash
nsh> listener sensor_baro 5
```
**Expected Output:**
```
TOPIC: sensor_baro
  timestamp: 12345678
  device_id: 0x...
  pressure: ~101325 Pa (at sea level)
  temperature: ~25 C
```

**Step 4: Altitude Change Test**
```bash
nsh> listener sensor_baro 10
# Move board up/down 1 meter
# Pressure should change by ~12 Pa per meter
```

#### Troubleshooting DPS310

| Symptom | Possible Cause | Solution |
|---------|----------------|----------|
| Wrong I2C address | SDO pin state | Try 0x76 instead |
| Pressure reads 0 | Sensor not initialized | Check power sequencing |
| High noise | Temperature compensation | Let sensor warm up |
| Drift over time | Normal behavior | Requires calibration |

---

### Test 4: Multiple Sensors Together

#### Setup
Connect all P1 priority sensors:
- ICM-20689 in Socket 1 (SPI)
- AK09916 on I2C bus
- DPS310 on I2C bus

#### Validation

**Step 1: Full Boot Test**
```bash
nsh> reboot
# All three sensors should initialize without errors
```

**Step 2: EKF2 Operation**
```bash
nsh> ekf2 status
```
**Expected:** No "missing data" errors

**Step 3: Sensor Module Status**
```bash
nsh> sensors status
```
Should show all sensors active.

**Step 4: Vehicle Attitude**
```bash
nsh> listener vehicle_attitude 5
```
**Expected:** Quaternion values updating, reasonable euler angles

**Step 5: Commander Status**
```bash
nsh> commander status
```
**Expected:** No "Sensor missing" preflight failures

---

## Additional Sensors (P2/P3)

### Enabling Commented-Out Sensors

Edit `boards/microchip/samv71-xult-clickboards/init/rc.board_sensors`:

```bash
# Uncomment desired sensors:
icm45686 -s -b 0 start        # ICM-45686 (SPI)
bmp388 -I -b 0 start          # BMP388 (I2C)
bmm150 -I -b 0 start          # BMM150 (I2C)
bmi088_i2c -b 0 start         # BMI088 (I2C)
```

Then rebuild and flash firmware.

### Manual Start at Runtime
```bash
# For quick testing without rebuild:
nsh> bmp388 -I -b 0 start
nsh> bmm150 -I -b 0 start
```

---

## Test Results Template

```
=============================================================
SAMV71 Click Board Sensor Test Report
=============================================================
Date: _______________
Tester: _______________
Firmware: _______________

HARDWARE CONFIGURATION
----------------------
Socket 1: _______________
Socket 2: _______________
Arduino Headers: _______________

TEST RESULTS
------------
┌─────────────┬──────────┬───────────┬──────────┬─────────────┐
│ Sensor      │ Detected │ Data OK   │ Rate OK  │ Notes       │
├─────────────┼──────────┼───────────┼──────────┼─────────────┤
│ ICM-20689   │ Y / N    │ Y / N     │ Y / N    │             │
│ AK09916     │ Y / N    │ Y / N     │ Y / N    │             │
│ DPS310      │ Y / N    │ Y / N     │ Y / N    │             │
│ ICM-45686   │ Y / N    │ Y / N     │ Y / N    │             │
│ BMP388      │ Y / N    │ Y / N     │ Y / N    │             │
│ BMM150      │ Y / N    │ Y / N     │ Y / N    │             │
│ BMI088      │ Y / N    │ Y / N     │ Y / N    │             │
└─────────────┴──────────┴───────────┴──────────┴─────────────┘

SYSTEM INTEGRATION
------------------
[ ] All sensors initialize at boot
[ ] EKF2 runs without errors
[ ] Vehicle attitude valid
[ ] Commander preflight passes
[ ] 10-minute stability test passed

ISSUES FOUND
------------
1.
2.
3.

OVERALL STATUS: PASS / FAIL
=============================================================
```

---

## Deliverables

### Required
1. **Test report** - Completed template for each sensor tested
2. **Issue list** - Any problems found with drivers or hardware
3. **Configuration updates** - Any needed changes to rc.board_sensors
4. **Photos** - Hardware setup documentation

### Optional
1. **Calibration data** - If sensors require calibration
2. **Performance metrics** - Data rates, latency measurements
3. **Comparison data** - If testing multiple similar sensors

---

## Success Criteria

- [ ] At least one IMU (ICM-20689 or ICM-45686) working
- [ ] At least one magnetometer (AK09916 or BMM150) working
- [ ] At least one barometer (DPS310 or BMP388) working
- [ ] EKF2 running with valid attitude output
- [ ] No sensor preflight failures in commander
- [ ] System stable for 30+ minutes with all sensors

---

## Known Limitations

### Logger Disabled
**WARNING:** The logger is currently disabled due to SD card write issues. You cannot record flight logs. See `TASK_LOGGER_DEBUG_ENABLE.md` for details.

### No Motor Output
PWM output is not yet implemented. Actual flight testing is not possible until Phase 3 is complete.

---

## References

- `CLICK_BOARD_VALIDATION_GUIDE.md` - Quick reference guide
- `SAMV71_IMPLEMENTATION_TRACKER.md` - Overall project status
- `rc.board_sensors` - Sensor startup configuration
- `default.px4board` - Build configuration

---

**Document Version:** 1.0
**Created:** November 28, 2025
**Status:** Ready for Assignment
