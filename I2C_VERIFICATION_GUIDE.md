# I2C Verification Guide for SAMV71-XULT-Clickboards

## I2C Configuration Summary

**Hardware**: SAMV71-XULT Development Board
**I2C Controller**: TWIHS0 (Two-Wire Interface High Speed 0)
**Pins**:
- **SDA (Data)**: PA3 (TWD0)
- **SCL (Clock)**: PA4 (TWCK0)

**Connected Sensors**:
- **AK09916** - Magnetometer (I2C address: 0x0C)
- **DPS310** - Barometric pressure sensor (I2C address: 0x77 or 0x76)

**PX4 Configuration**:
- I2C bus count: 1 (I2C_BUS_MAX_BUS_ITEMS = 1)
- Bus type: External (mikroBUS and Arduino headers)

## How to Verify I2C is Working

### Method 1: Check Boot Log for Sensor Detection

When PX4 boots, each I2C sensor driver attempts to initialize and prints status messages.

**Steps**:

1. Connect to NSH console:
```bash
screen /dev/ttyACM0 57600
```

2. Look for sensor initialization messages in boot log:
```
[sensor_mag] AK09916 on bus 0 (external) found
[sensor_baro] DPS310 on bus 0 (external) found
```

**What to look for**:
- ✅ **Success**: "found" or "detected" messages
- ❌ **Failure**: "not found", "failed to initialize", "I2C error"

### Method 2: Check Sensor Status Commands

PX4 sensor drivers provide status commands to check their state.

**Check Magnetometer (AK09916)**:
```bash
nsh> ak09916 status
```

Expected output if working:
```
Running on I2C bus 0
Device ID: 0x48090
Measurement rate: 100 Hz
Status: OK
```

**Check Barometer (DPS310)**:
```bash
nsh> dps310 status
```

Expected output if working:
```
Running on I2C bus 0
Device ID: 0x10DF10
Measurement rate: 50 Hz
Temperature: 25.3°C
Pressure: 1013.25 hPa
Status: OK
```

**If sensors not found**:
```
ERROR: not found
```
or
```
ERROR: no instance running
```

### Method 3: Listen to uORB Sensor Topics

If sensors are working, they publish data to uORB topics.

**Check Magnetometer Data**:
```bash
nsh> listener sensor_mag 10
```

Expected output (10 samples):
```
TOPIC: sensor_mag
    timestamp: 123456789
    device_id: 4689168
    x: 0.234 (Gauss)
    y: -0.125 (Gauss)
    z: 0.456 (Gauss)
    temperature: 25.3
    error_count: 0
```

**Check Barometer Data**:
```bash
nsh> listener sensor_baro 10
```

Expected output:
```
TOPIC: sensor_baro
    timestamp: 123456790
    device_id: 1105168
    pressure: 101325.0 (Pa)
    temperature: 25.3 (°C)
    error_count: 0
```

**If no data**:
```
ERROR: never published
```

### Method 4: Use I2C Detection Tools (if available)

Some PX4 builds include I2C scanning utilities.

**Scan I2C bus for devices**:
```bash
nsh> i2cdetect 0
```

Expected output:
```
     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
00:          -- -- -- -- -- -- -- -- -- 0c -- -- --
10:          -- -- -- -- -- -- -- -- -- -- -- -- --
20:          -- -- -- -- -- -- -- -- -- -- -- -- --
30:          -- -- -- -- -- -- -- -- -- -- -- -- --
40:          -- -- -- -- -- -- -- -- -- -- -- -- --
50:          -- -- -- -- -- -- -- -- -- -- -- -- --
60:          -- -- -- -- -- -- -- -- -- -- -- -- --
70:          -- -- -- -- -- -- -- 77
```

Devices found:
- `0c` - AK09916 magnetometer
- `77` - DPS310 barometer

**Note**: `i2cdetect` command may not be available in constrained flash builds.

### Method 5: Check Sensor Module Status

The central sensors module aggregates all sensor data.

```bash
nsh> sensors status
```

Expected output:
```
Magnetometer:
  device_id: 4689168 (AK09916)
  status: OK

Barometer:
  device_id: 1105168 (DPS310)
  status: OK
```

### Method 6: Monitor for I2C Errors

Check system logs for I2C communication errors.

```bash
nsh> logger status
```

If I2C has errors, you'll see:
```
ERROR [sensor_mag] I2C transfer failed: -5
ERROR [sensor_baro] read failed: timeout
```

### Method 7: Verify NuttX I2C Driver Initialization

Check if NuttX I2C driver was initialized during boot.

Look in boot log for:
```
[INFO] sam_i2c_initialize: Initializing TWIHS0
[INFO] sam_i2c_initialize: TWIHS0 frequency: 100000 Hz
```

## Expected Sensor Behavior

### AK09916 Magnetometer

**I2C Address**: 0x0C (7-bit)
**Communication**: Read WHO_AM_I register (0x01) should return 0x48
**Update Rate**: Configurable (10 Hz - 100 Hz)
**Data Range**: ±4912 µT (±49.12 Gauss)

**Typical Values** (depends on Earth's magnetic field at your location):
- X: -0.3 to +0.3 Gauss
- Y: -0.3 to +0.3 Gauss
- Z: 0.3 to 0.6 Gauss (vertical component, Northern hemisphere)

**Health Check**:
```bash
nsh> listener sensor_mag 5
```
Verify:
- ✅ Data updates at expected rate
- ✅ Error count is 0
- ✅ Values are stable when not moving
- ✅ Values change when rotating board

### DPS310 Barometer

**I2C Address**: 0x77 (7-bit) - default, or 0x76 if SDO pulled low
**Communication**: Read PROD_ID register (0x0D) should return 0x10
**Update Rate**: Configurable (1 Hz - 128 Hz)
**Pressure Range**: 300 - 1200 hPa
**Temperature Range**: -40°C to +85°C

**Typical Values**:
- Pressure: 950 - 1050 hPa (at sea level, varies with weather)
- Temperature: Room temperature (~20-25°C)

**Health Check**:
```bash
nsh> listener sensor_baro 5
```
Verify:
- ✅ Data updates at expected rate
- ✅ Error count is 0
- ✅ Pressure is reasonable for your location
- ✅ Temperature matches ambient

## Troubleshooting I2C Issues

### Issue: Sensors Not Found at Boot

**Possible Causes**:
1. I2C pins not configured correctly
2. Sensors not powered
3. Wrong I2C address
4. Hardware not connected
5. Pull-up resistors missing

**Debug Steps**:

1. **Check I2C bus initialization**:
```bash
nsh> ak09916 start -X
# -X forces external bus (bus 0)
```

2. **Check for I2C errors in dmesg** (if enabled):
```bash
nsh> dmesg | grep -i i2c
```

3. **Verify sensor power** - Check if clickboards are properly inserted

4. **Check pin configuration** in `boards/microchip/samv71-xult-clickboards/nuttx-config/include/board.h`:
```c
#define GPIO_I2C0_SDA  GPIO_TWD0    /* PA3 */
#define GPIO_I2C0_SCL  GPIO_TWCK0   /* PA4 */
```

### Issue: Sensor Found But No Data

**Possible Causes**:
1. Sensor not started
2. Sensor in error state
3. I2C communication intermittent

**Debug Steps**:

1. **Restart sensor driver**:
```bash
nsh> ak09916 stop
nsh> ak09916 start -X
nsh> ak09916 status
```

2. **Check error counters**:
```bash
nsh> listener sensor_mag 1
# Look at error_count field
```

3. **Monitor I2C traffic** - If you have logic analyzer, monitor PA3/PA4

### Issue: Intermittent I2C Errors

**Possible Causes**:
1. Electromagnetic interference
2. Long wires / poor connections
3. I2C speed too high
4. Bus contention

**Solutions**:

1. **Reduce I2C speed** - Modify NuttX board config:
```c
/* In board.h */
#define BOARD_I2C0_FREQUENCY 100000  /* 100 kHz instead of 400 kHz */
```

2. **Add delays** - Increase I2C retry timeout in sensor driver

3. **Check hardware** - Ensure good connections, short wires

### Issue: Wrong I2C Address

Some sensors have configurable addresses via hardware pins.

**DPS310**:
- Default: 0x77 (SDO pulled high or floating)
- Alternate: 0x76 (SDO pulled low)

**To change driver address** (if needed):
Edit sensor driver initialization code to use alternate address.

## Hardware Verification Checklist

Before assuming software issues, verify hardware:

- [ ] mikroBUS click boards fully inserted into sockets
- [ ] Power LED on click boards illuminated
- [ ] I2C pull-up resistors present (usually 4.7kΩ or 10kΩ on click boards)
- [ ] No shorts between SDA and SCL
- [ ] SAMV71 board powered on
- [ ] USB connection stable

## NuttX I2C Driver Details

SAMV71 uses **TWIHS (Two-Wire Interface High Speed)** peripheral.

**Key Features**:
- Hardware I2C controller (not bit-banged)
- Supports Standard mode (100 kHz) and Fast mode (400 kHz)
- DMA capable (not yet used in PX4)
- Multi-master capable

**NuttX Driver Location**:
```
NuttX/nuttx/arch/arm/src/samv7/sam_twihs.c
```

**Configuration**:
Defined in NuttX board config:
```
NuttX/nuttx-config/include/board.h
NuttX/nuttx-config/scripts/Make.defs
```

## Summary: Quick I2C Health Check

Run these commands in NSH to verify I2C is working:

```bash
# 1. Check sensor status
nsh> ak09916 status
nsh> dps310 status

# 2. Listen to sensor data
nsh> listener sensor_mag 5
nsh> listener sensor_baro 5

# 3. Check for errors
nsh> sensors status
```

**Expected Results**:
- ✅ Both sensors show "Running on I2C bus 0"
- ✅ Data streams continuously
- ✅ Error counts are 0
- ✅ Values are reasonable

**If any check fails**, I2C may not be working correctly and needs debugging.

---

**Next Steps After I2C Verification**:

1. If I2C works ✅ → Proceed to SPI implementation for ICM-20689
2. If I2C fails ❌ → Debug I2C before moving forward

---

**Document Version**: 1.0
**Last Updated**: 2025-11-16
**Target Platform**: SAMV71-XULT-Clickboards
**Prerequisites**: Firmware flashed, sensors physically connected
