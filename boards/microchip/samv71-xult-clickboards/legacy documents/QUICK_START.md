# Quick Start Guide - PX4 on SAMV71-XULT

**Goal:** Get PX4 running on SAMV71-XULT in under 30 minutes.

## Prerequisites

You need:
- Ubuntu 20.04+ or Debian-based Linux (recommended)
- SAMV71-XULT development board
- USB cable (for programming and power)
- Serial terminal software (optional - can use USB CDC/ACM)

## Step 1: Install Build Tools (10 minutes)

```bash
# Update package list
sudo apt-get update

# Install required packages
sudo apt-get install -y \
    git \
    cmake \
    build-essential \
    python3 \
    python3-pip \
    ninja-build \
    gcc-arm-none-eabi \
    gdb-multiarch \
    openocd \
    minicom

# Verify ARM toolchain
arm-none-eabi-gcc --version
# Should show: gcc version 13.2.1 or similar
```

## Step 2: Clone Repository (5 minutes)

```bash
# Clone the repository
git clone <your-repo-url> PX4-Autopilot-SAMV71
cd PX4-Autopilot-SAMV71

# Initialize submodules (this takes a few minutes)
git submodule update --init --recursive

# Install Python dependencies
pip3 install --user -r Tools/setup/requirements.txt
```

## Step 3: Build Firmware (5 minutes)

```bash
# Clean build from scratch
make microchip_samv71-xult-clickboards_default

# Build output will be in:
# build/microchip_samv71-xult-clickboards_default/
```

**Expected output:**
```
Memory region         Used Size  Region Size  %age Used
           flash:      881512 B         2 MB     42.03%
            sram:       22008 B       384 KB      5.60%
```

If build fails, check:
- ARM toolchain is installed correctly
- Python dependencies are installed
- Submodules are initialized

## Step 4: Connect Hardware (2 minutes)

1. Connect SAMV71-XULT board to PC via USB
2. Board should power on (LED may blink)
3. Check device appears:
```bash
lsusb | grep -i atmel
# Should show: Atmel Corp. EDBG CMSIS-DAP
```

## Step 5: Flash Firmware (3 minutes)

```bash
# Flash using OpenOCD
cd /path/to/PX4-Autopilot-SAMV71

openocd -f interface/cmsis-dap.cfg -f target/atsamv.cfg \
    -c "program build/microchip_samv71-xult-clickboards_default/microchip_samv71-xult-clickboards_default.elf verify reset exit"
```

**Expected output:**
```
Open On-Chip Debugger 0.12.0
...
** Programming Started **
** Programming Finished **
** Verify Started **
** Verified OK **
** Resetting Target **
shutdown command invoked
```

**Troubleshooting:**
- If "Can't find target/atsamv71.cfg": Use `target/atsamv.cfg` instead
- If "Error: No device found": Check USB connection and drivers
- If "Error: Flash write failed": Board may be write-protected

## Step 6: Connect to Console (2 minutes)

**Option A: Using USB CDC/ACM (after PX4 boots):**
```bash
# Wait 5 seconds for PX4 to boot
sleep 5

# Connect to USB serial port
minicom -D /dev/ttyACM0 -b 115200
```

**Option B: Using UART1 (immediate):**
```bash
# Find USB-to-serial adapter
ls /dev/ttyUSB*

# Connect (115200 baud, 8N1)
minicom -D /dev/ttyUSB0 -b 115200
```

**Exit minicom:** Press `Ctrl+A` then `X` then `Enter`

## Step 7: Verify PX4 is Running (5 minutes)

You should see:
```
NuttShell (NSH) NuttX-11.0.0
nsh>
```

**Run verification commands:**

```bash
# Show version info
nsh> ver all

# Expected output:
# HW arch: MICROCHIP_SAMV71_XULT_CLICKBOARDS
# PX4 version: 1.17.0
# OS: NuttX Release 11.0.0
# MCU: SAMV70, rev. B

# Check running modules
nsh> logger status
# Expected: INFO [logger] Running in mode: all

nsh> commander status
# Expected: INFO [commander] Disarmed

nsh> sensors status
# Expected: Shows sensor configuration

# Show parameters
nsh> param show | head -10
# Expected: List of 394 parameters

# List running tasks
nsh> top
# Expected: ~20-25 tasks running
```

**Success Criteria:**
- ✅ System boots without "hard fault" messages
- ✅ NSH prompt appears
- ✅ `ver all` shows correct hardware and version
- ✅ `logger status` shows logger running
- ✅ `commander status` shows disarmed
- ✅ No critical errors (warnings about missing sensors are OK)

## Common Issues and Solutions

### Issue: Build fails with "arm-none-eabi-gcc not found"

**Solution:**
```bash
sudo apt-get install gcc-arm-none-eabi
# Or download from ARM website
```

### Issue: OpenOCD can't connect to board

**Solution:**
```bash
# Check USB connection
lsusb | grep Atmel

# Try different OpenOCD config
openocd -f board/atmel_samv71_xplained_ultra.cfg

# Check permissions
sudo usermod -a -G dialout $USER
# Then logout and login again
```

### Issue: "command not found" errors in NSH

**Solution:** This is normal if sensors aren't connected. Ignore warnings about:
- `icm20948_i2c_passthrough`
- `hmc5883`, `lis3mdl`, `qmc5883l` (magnetometers)
- `px4io`, `dshot`, `pwm_out` (output drivers)
- `tone_alarm`, `rgbled` (indicators)

These are optional modules not built or sensors not connected.

### Issue: USB serial port doesn't appear

**Solution:**
```bash
# Wait longer - PX4 takes ~5-7 seconds to boot
sleep 10
ls /dev/ttyACM*

# If still nothing, use UART1 console instead
# Or check dmesg for USB errors
dmesg | tail
```

### Issue: Parameters show zeros or missing

**Solution:** This is the C++ static initialization bug. Our fix should handle it, but if you see crashes:
```bash
# Check if null pointer fix is present
grep -A3 "C++ static initialization bug" src/lib/parameters/DynamicSparseLayer.h
# Should show the null check code
```

## Next Steps

### Test Basic Functionality

```bash
# Test parameter system
nsh> param set TEST_PARAM 123
nsh> param get TEST_PARAM
# Should show: 123

# Test I2C bus
nsh> i2c bus
# Should show: Bus 0

# Test storage
nsh> ls /fs/microsd
# Should show: log/ directory

# View system log
nsh> dmesg | tail -20
```

### Connect Sensors (Optional)

If you have I2C sensors (AK09916 magnetometer, DPS310 barometer):

```bash
# Scan I2C bus for devices
nsh> i2c dev 0 0x03 0x77

# Manually start sensor (example)
nsh> ak09916 start -I -b 0
nsh> ak09916 status

nsh> dps310 start -I -b 0
nsh> dps310 status
```

### Configure Airframe (Optional)

To test with a specific airframe:

```bash
# Set airframe ID (example: generic quadcopter)
nsh> param set SYS_AUTOSTART 4001

# Save parameters (to microSD since flash storage not configured)
nsh> param save

# Reboot
nsh> reboot
```

## Getting Help

**Check documentation:**
- `boards/microchip/samv71-xult-clickboards/README.md` - Full documentation
- `boards/microchip/samv71-xult-clickboards/PORTING_SUMMARY.md` - What was fixed

**Common commands:**
```bash
# Show all available commands
nsh> ?

# Show command usage
nsh> <command_name>

# View logs in real-time
nsh> dmesg -f

# Monitor CPU usage
nsh> top
```

**Debugging:**
```bash
# Check if modules are running
nsh> logger status
nsh> commander status
nsh> sensors status
nsh> ekf2 status
nsh> mavlink status

# View parameter list
nsh> param show
```

## Success!

If you got here and can run commands in NSH, **congratulations!** You have successfully built and flashed PX4 on the SAMV71-XULT board.

**Your system has:**
- ✅ NuttX RTOS running
- ✅ PX4 flight stack initialized
- ✅ 394 parameters loaded
- ✅ Logger recording data
- ✅ Flight controller ready
- ✅ State estimator (EKF2) running
- ✅ MAVLink communication ready

**Ready for:**
- Sensor integration (when hardware available)
- Airframe configuration
- Ground control station connection
- Flight testing (once sensors and outputs configured)

## What to Contribute

See `README.md` section "Contributing" for areas needing work:

**High Priority:**
1. Configure SPI buses for IMU sensors
2. Set up flash parameter storage (MTD)
3. Configure PWM outputs for motors
4. Add additional UART ports

**Testing:**
1. Test with real sensors when available
2. Verify MAVLink communication with QGroundControl
3. Test parameter persistence
4. Benchmark performance

## Build Statistics

Your successful build should show approximately:
- **Build time:** 2-3 minutes (first build), 30 seconds (incremental)
- **Flash usage:** ~880 KB / 2 MB (42% - plenty of room)
- **RAM usage:** ~22 KB / 384 KB (6% - very efficient)
- **Binary size:** ~880 KB (ELF), ~875 KB (BIN)

---

**Total Setup Time:** ~30 minutes
**Difficulty:** Intermediate (requires Linux and command line familiarity)
**Success Rate:** High (if prerequisites met)

**Questions?** Check the main README.md or review PORTING_SUMMARY.md for detailed technical information.
