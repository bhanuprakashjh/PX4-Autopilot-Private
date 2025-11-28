# Click Board Sensor Validation Guide

**Purpose:** Step-by-step guide for validating MIKROE Click board sensors on SAMV71-XULT
**Created:** November 28, 2025
**Firmware:** PX4 v1.17.0 (samv7-custom branch)

---

## ⚠️ IMPORTANT: Known Limitations

### Logger is DISABLED
- **Issue:** SD card continuous writes cause system hang (PIO mode limitation)
- **Impact:** Flight logs cannot be recorded
- **Workaround:** None currently - logger must remain disabled
- **Status:** Separate task to fix HSMCI write path
- **Tracking:** See `SAMV71_IMPLEMENTATION_TRACKER.md` → "Logger/SD Card Continuous Write Hang"

### No Motor Output
- PWM output not yet implemented (Phase 3)
- Board cannot fly until PWM is working

---

## Supported Click Boards

| MIKROE PID | Board Name | Sensor | Interface | Default |
|------------|------------|--------|-----------|---------|
| MIKROE-4044 | 6DOF IMU 6 Click | ICM-20689 | SPI0 | Enabled |
| MIKROE-6514 | 6DOF IMU 27 Click | ICM-45686 | SPI0 | Commented |
| MIKROE-4231 | Compass 4 Click | AK09915/16 | I2C0 | Enabled |
| MIKROE-2293 | Pressure 3 Click | DPS310 | I2C0 | Enabled |
| MIKROE-3566 | Pressure 5 Click | BMP388 | I2C0 | Commented |
| MIKROE-2935 | GeoMagnetic Click | BMM150 | I2C0 | Commented |
| MIKROE-3775 | 13DOF Click | BMI088+BMM150 | I2C0 | Commented |

**Note:** "Commented" means the driver is compiled but not started by default.

---

## Hardware Setup

### mikroBUS Socket Pinout (SAMV71-XULT)

```
        Socket 1                    Socket 2
    ┌─────────────┐             ┌─────────────┐
    │ AN    PWM   │             │ AN    PWM   │
    │ RST   INT   │             │ RST   INT   │
    │ CS    RX    │             │ CS    RX    │
    │ SCK   TX    │             │ SCK   TX    │
    │ MISO  SCL   │             │ MISO  SCL   │
    │ MOSI  SDA   │             │ MOSI  SDA   │
    │ 3V3   5V    │             │ 3V3   5V    │
    │ GND   GND   │             │ GND   GND   │
    └─────────────┘             └─────────────┘
```

### Recommended Click Board Placement

| Socket | Recommended Board | Reason |
|--------|-------------------|--------|
| Socket 1 | ICM-20689 (SPI IMU) | SPI0 directly routed |
| Socket 2 | DPS310 (I2C Baro) | I2C0 shared bus |
| Arduino Headers | AK09916 (I2C Mag) | I2C0 shared bus |

### I2C Bus Sharing
All I2C sensors share I2C0 (TWIHS0). Multiple I2C Click boards can be connected simultaneously as long as addresses don't conflict:

| Sensor | Default I2C Address |
|--------|---------------------|
| AK09916 | 0x0C |
| DPS310 | 0x77 |
| BMP388 | 0x76 (or 0x77) |
| BMM150 | 0x10 (configurable) |
| BMI088 Accel | 0x18/0x19 |
| BMI088 Gyro | 0x68/0x69 |

---

## Validation Procedure

### Step 1: Connect Hardware

1. Power off SAMV71-XULT
2. Insert Click board(s) into appropriate socket(s)
3. Connect USB cable
4. Power on and wait for boot

### Step 2: Check Boot Log

Expected output for enabled sensors without hardware:
```
ERROR [SPI_I2C] icm20689: no instance started (no device on bus?)
WARN  [SPI_I2C] ak09916: no instance started (no device on bus?)
WARN  [SPI_I2C] dps310: no instance started (no device on bus?)
```

With hardware connected, these should NOT appear.

### Step 3: Verify Sensor Detection

#### ICM-20689 (SPI IMU)
```bash
nsh> icm20689 status
```
**Expected:** Driver info with sample rate, FIFO status

#### AK09916 (I2C Magnetometer)
```bash
nsh> ak09916 status
```
**Expected:** Driver info showing I2C bus 0, address 0x0C

#### DPS310 (I2C Barometer)
```bash
nsh> dps310 status
```
**Expected:** Driver info showing I2C bus 0, address 0x77

### Step 4: Check Sensor Data

#### Accelerometer
```bash
nsh> listener sensor_accel
```
**Expected:** ~400Hz updates, values near [0, 0, -9.81] when stationary

#### Gyroscope
```bash
nsh> listener sensor_gyro
```
**Expected:** ~400Hz updates, values near [0, 0, 0] when stationary

#### Magnetometer
```bash
nsh> listener sensor_mag
```
**Expected:** Updates at configured rate, values depend on local magnetic field

#### Barometer
```bash
nsh> listener sensor_baro
```
**Expected:** Pressure ~101325 Pa at sea level, temperature reading

### Step 5: Verify EKF2 Operation

```bash
nsh> ekf2 status
```
**Expected:** No "missing data" errors, attitude estimates updating

```bash
nsh> listener vehicle_attitude
```
**Expected:** Quaternion/euler angles updating, reasonable values

### Step 6: Check Health Status

```bash
nsh> commander status
```
Or check preflight warnings:
```bash
nsh> listener vehicle_status
```

**With valid IMU:** Should NOT see "Accel Sensor 0 missing" or "Gyro Sensor 0 missing"

---

## Enabling Additional Sensors

### To enable a commented-out sensor:

1. Edit `boards/microchip/samv71-xult-clickboards/init/rc.board_sensors`
2. Uncomment the desired sensor line:
   ```bash
   # Before:
   # bmp388 -I -b 0 start

   # After:
   bmp388 -I -b 0 start
   ```
3. Rebuild and flash firmware

### Alternative: Manual start at runtime
```bash
nsh> bmp388 -I -b 0 start
nsh> bmm150 -I -b 0 start
nsh> bmi088_i2c -b 0 start
```

---

## Troubleshooting

### "no instance started (no device on bus?)"

**Causes:**
1. Click board not properly seated
2. Wrong socket (SPI vs I2C mismatch)
3. I2C address conflict
4. Hardware defect

**Debug:**
```bash
# Scan I2C bus for devices
nsh> i2cdetect -b 0
```

### Sensor detected but no data

**Check uORB topics:**
```bash
nsh> uorb status
nsh> listener sensor_accel 1
```

### EKF2 not starting

**Check for required sensors:**
```bash
nsh> ekf2 status
```
EKF2 requires at least one IMU (accel + gyro) to start.

### SPI communication errors

**Verify SPI DMA is enabled:**
```bash
# In defconfig, ensure:
CONFIG_SAMV7_SPI_DMA=y
```

---

## Validation Checklist

### Per-Sensor Checklist

#### ICM-20689 (SPI IMU)
- [ ] Driver starts without errors
- [ ] `icm20689 status` shows valid info
- [ ] `listener sensor_accel` shows data
- [ ] `listener sensor_gyro` shows data
- [ ] Data rates match expected (~400Hz)
- [ ] Values reasonable when stationary

#### AK09916 (I2C Magnetometer)
- [ ] Driver starts without errors
- [ ] `ak09916 status` shows valid info
- [ ] `listener sensor_mag` shows data
- [ ] Values change when board rotated

#### DPS310 (I2C Barometer)
- [ ] Driver starts without errors
- [ ] `dps310 status` shows valid info
- [ ] `listener sensor_baro` shows data
- [ ] Pressure reading reasonable (~101kPa)
- [ ] Temperature reading reasonable

### System-Level Checklist
- [ ] All enabled sensors detected at boot
- [ ] EKF2 starts without "missing data" errors
- [ ] `commander status` shows no sensor preflight failures
- [ ] System stable under continuous operation (10+ minutes)
- [ ] No bus errors in sensor status

---

## Test Results Template

```
Date: _______________
Tester: _______________
Firmware Version: _______________

Click Boards Tested:
[ ] ICM-20689 (MIKROE-4044) - Socket: ___
[ ] AK09916 (MIKROE-4231) - Socket: ___
[ ] DPS310 (MIKROE-2293) - Socket: ___
[ ] Other: _______________ - Socket: ___

Results:
┌─────────────┬────────────┬───────────────┬─────────────────┐
│ Sensor      │ Detected?  │ Data Valid?   │ Notes           │
├─────────────┼────────────┼───────────────┼─────────────────┤
│ ICM-20689   │ Yes / No   │ Yes / No      │                 │
│ AK09916     │ Yes / No   │ Yes / No      │                 │
│ DPS310      │ Yes / No   │ Yes / No      │                 │
│ EKF2        │ Running?   │ Attitude OK?  │                 │
└─────────────┴────────────┴───────────────┴─────────────────┘

Issues Found:
1.
2.
3.

Overall Status: PASS / FAIL
```

---

## References

- `SAMV71_IMPLEMENTATION_TRACKER.md` - Implementation progress and known issues
- `SAMV71_MASTER_PORTING_GUIDE.md` - Complete porting reference
- `rc.board_sensors` - Sensor startup script
- `default.px4board` - Build configuration

---

**Document Version:** 1.0
**Last Updated:** November 28, 2025
