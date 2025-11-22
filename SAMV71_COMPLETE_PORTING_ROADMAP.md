# SAMV71-XULT Complete PX4 Porting Roadmap

**Board:** Microchip ATSAMV71-XULT Development Board
**Target:** Full flight-ready PX4 autopilot platform
**Current Status:** Safe-mode with SD card bugs identified
**Last Updated:** November 22, 2025

---

## Table of Contents

1. [Current State and Immediate Fixes](#1-current-state-and-immediate-fixes)
2. [Phase 0: Foundation - Full Build Expansion](#phase-0-foundation---full-build-expansion)
3. [Phase 1: Simulation Ready (HIL/Gazebo)](#phase-1-simulation-ready-hilgazebo)
4. [Phase 2: Sensor Integration and Validation](#phase-2-sensor-integration-and-validation)
5. [Phase 3: Full System Integration](#phase-3-full-system-integration)
6. [Phase 4: Flight Readiness](#phase-4-flight-readiness)
7. [Phase 5: Flight Testing and Validation](#phase-5-flight-testing-and-validation)
8. [Phase 6: Production Ready](#phase-6-production-ready)
9. [Appendix A: Testing Matrix](#appendix-a-testing-matrix)
10. [Appendix B: Hardware Requirements](#appendix-b-hardware-requirements)
11. [Appendix C: PX4 Module Dependencies](#appendix-c-px4-module-dependencies)

---

## 1. Current State and Immediate Fixes

### 1.1 Current Configuration

**Build:** `microchip_samv71-xult-clickboards_default`
**Mode:** Safe-mode with early exit in rcS
**Working:**
- ✅ NuttX boot (clean, <2 seconds)
- ✅ NSH shell (responsive)
- ✅ Parameter storage (manual construction fix applied)
- ✅ SD card mount (manual file I/O works)
- ✅ HRT (high-resolution timer verified)
- ✅ USB console (/dev/ttyACM0)
- ✅ I2C bus available (/dev/i2c0)
- ✅ Debug tools (dmesg, hrt_test, top, free)

**Known Issues:**
- ❌ SD card logger fails (HSMCI driver bugs - **5 bugs identified**)
- ⚠️ MAVLink disabled (safe-mode configuration)
- ⚠️ Autopilot stack not running (safe-mode)
- ⚠️ Sensors not initialized (safe-mode)

### 1.2 Immediate Fix: SD Card HSMCI Driver

**Document Reference:** `SAMV7_HSMCI_SD_CARD_DEBUG_INVESTIGATION.md`

**Critical Bugs to Fix (in order):**

#### Fix #1: Add D-Cache Invalidation (HIGHEST PRIORITY)
**Impact:** Data corruption on all SD card reads
**File:** `platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/sam_hsmci.c:3106`
**Change:** Add `up_invalidate_dcache((uintptr_t)buffer, (uintptr_t)buffer + buflen);`

**Verification:**
```bash
nsh> echo "test" > /fs/microsd/test.txt
nsh> cat /fs/microsd/test.txt  # Should match exactly
```

#### Fix #2: Remove BLKR Clear
**Impact:** Prevents timeout errors
**File:** `sam_hsmci.c:1496`
**Change:** Delete line `sam_putreg(priv, 0, SAM_HSMCI_BLKR_OFFSET);`

**Verification:**
```bash
nsh> mount -t vfat /dev/mmcsd0 /fs/microsd  # Should succeed
```

#### Fix #3: Remove Duplicate BLKR Writes
**Impact:** Eliminates zero-write bug trigger
**File:** `sam_hsmci.c:2146-2163`
**Change:** Delete both BLKR write blocks in `sam_sendcmd()`

#### Fix #4: DMA Setup Use Cache Only
**Impact:** Removes fragile fallback logic
**File:** `sam_hsmci.c:3112-3154`
**Change:** Use cached `priv->blocklen/nblocks` instead of reading BLKR

#### Fix #5: Add DTIP Check (Optional)
**Impact:** Defensive programming
**File:** `sam_hsmci.c:2209`
**Change:** Add `DEBUGASSERT((sr & HSMCI_SR_DTIP_Msk) == 0);`

**Timeline:** 1-2 days (implementation + testing)

**Success Criteria:**
- ✅ `mount -t vfat /dev/mmcsd0 /fs/microsd` succeeds
- ✅ `logger start` works without timeout
- ✅ Sustained SD writes complete successfully
- ✅ No data corruption on reads

---

## Phase 0: Foundation - Full Build Expansion

**Goal:** Enable full PX4 stack while maintaining safe-mode baseline

**Duration:** 1-2 weeks

**Prerequisites:**
- ✅ SD card fixes applied and verified
- ✅ Current safe-mode configuration documented

---

### 0.1 Remove Safe-Mode Restrictions

#### Step 0.1.1: Re-enable PX4 Startup Script

**File:** `ROMFS/px4fmu_common/init.d/rcS`

**Current (Safe-mode):**
```bash
if [ $STORAGE_AVAILABLE = yes ]
then
    param select-backup $PARAM_BACKUP_FILE
fi

#
# SAFE MODE: skipping PX4 startup (SD debug)
#
exit 0  # ← REMOVE THIS LINE
```

**Change to:**
```bash
if [ $STORAGE_AVAILABLE = yes ]
then
    param select-backup $PARAM_BACKUP_FILE
fi

# Continue with normal PX4 startup
# Source the vehicle-specific configuration
if [ -f /etc/init.d/rc.vehicle_setup ]
then
    . /etc/init.d/rc.vehicle_setup
fi

# Start the flight mode selector
if [ -f /etc/init.d/rc.autostart ]
then
    . /etc/init.d/rc.autostart
fi
```

**Verification:**
```bash
# After boot, check processes:
nsh> ps
# Should show commander, sensors, navigator, etc.

nsh> top
# CPU should be <50% idle (services running)
```

---

### 0.2 Enable Core PX4 Modules

#### Step 0.2.1: Update Board Configuration

**File:** `boards/microchip/samv71-xult-clickboards/default.px4board`

**Modules to Enable (Incremental):**

**Stage 1 - Core Services:**
```cmake
CONFIG_MODULES_COMMANDER=y          # Flight mode manager
CONFIG_MODULES_DATAMAN=y            # Mission storage
CONFIG_MODULES_NAVIGATOR=y          # Mission execution
CONFIG_MODULES_LOAD_MON=y           # CPU monitoring
CONFIG_MODULES_BATTERY_STATUS=y     # Battery management
```

**Stage 2 - Essential Drivers:**
```cmake
CONFIG_DRIVERS_PWM_OUT=y            # PWM output (motors/servos)
CONFIG_DRIVERS_RC_INPUT=y           # RC receiver input
CONFIG_COMMON_RC=y                  # RC processing
```

**Stage 3 - Sensors (placeholders until hardware available):**
```cmake
CONFIG_DRIVERS_DIFFERENTIAL_PRESSURE=y  # Airspeed (future)
CONFIG_DRIVERS_DISTANCE_SENSOR=y        # Rangefinder (future)
CONFIG_DRIVERS_GPS=y                    # GPS module
CONFIG_DRIVERS_MAGNETOMETER=y           # Compass (I2C/SPI)
CONFIG_DRIVERS_BAROMETER=y              # Baro (I2C/SPI)
```

**Stage 4 - Estimators:**
```cmake
CONFIG_MODULES_EKF2=y               # Extended Kalman Filter
CONFIG_MODULES_ATTITUDE_ESTIMATOR_Q=y  # Quaternion estimator (backup)
```

**Stage 5 - Controllers:**
```cmake
CONFIG_MODULES_MC_ATT_CONTROL=y     # Multicopter attitude
CONFIG_MODULES_MC_POS_CONTROL=y     # Multicopter position
CONFIG_MODULES_FW_ATT_CONTROL=y     # Fixed-wing attitude
CONFIG_MODULES_FW_POS_CONTROL=y     # Fixed-wing position
```

#### Step 0.2.2: Incremental Build and Test

**Process:**
1. Enable Stage 1 modules
2. Build and test
3. Verify boot, check dmesg for errors
4. Enable Stage 2, repeat
5. Continue through Stage 5

**Build Command:**
```bash
# Clean build after config changes
make clean
make microchip_samv71-xult-clickboards_default

# Check binary size:
ls -lh build/microchip_samv71-xult-clickboards_default/*.bin
# Should stay under 2MB flash limit
```

**Testing After Each Stage:**
```bash
# Boot and check:
nsh> dmesg
# Look for module initialization messages

nsh> top
# Verify new modules are running

nsh> free
# Monitor RAM usage (should stay under 384KB)

# Test specific module:
nsh> commander status
nsh> sensors status
nsh> ekf2 status
```

**Expected Flash/RAM Usage:**
```
Stage 0 (Safe-mode):  920 KB flash,  18 KB RAM
Stage 1 (Core):      1100 KB flash,  50 KB RAM
Stage 2 (Drivers):   1300 KB flash,  80 KB RAM
Stage 3 (Sensors):   1450 KB flash, 100 KB RAM
Stage 4 (EKF2):      1650 KB flash, 150 KB RAM
Stage 5 (Control):   1800 KB flash, 180 KB RAM

LIMIT:               2048 KB flash, 384 KB RAM
```

**If Approaching Limits:**
- Disable unused airframe types (keep only multicopter OR fixed-wing)
- Remove unused drivers
- Disable debug symbols: `CONFIG_DEBUG_SYMBOLS=n`
- Enable size optimization: `CONFIG_OPTIMIZATION_SIZE=y`

---

### 0.3 Configure Default Airframe

**File:** `boards/microchip/samv71-xult-clickboards/init/rc.board_defaults`

**Current (Safe-mode):**
```bash
#!/bin/sh
# board defaults
param set-default MAV_0_CONFIG 0  # MAVLink disabled
```

**Change to (Simulation Ready):**
```bash
#!/bin/sh
# SAMV71-XULT Board Defaults

# Set default airframe (quadcopter for testing)
param set-default SYS_AUTOSTART 4001  # Generic Quadcopter

# MAVLink configuration (will configure in Phase 1)
param set-default MAV_0_CONFIG 0      # Disabled for now

# System identification
param set-default SYS_AUTOCONFIG 1    # Allow autoconfig
param set-default SYS_MC_EST_GROUP 2  # Use EKF2

# Safety (no physical safety switch on dev board)
param set-default CBRK_IO_SAFETY 22027    # Bypass I/O safety
param set-default COM_PREARM_MODE 0       # No preflight checks yet

# Battery (dummy values for bench testing)
param set-default BAT_N_CELLS 3           # 3S LiPo
param set-default BAT_V_EMPTY 3.3         # Empty voltage
param set-default BAT_V_CHARGED 4.2       # Full voltage

# RC input (to be configured when hardware connected)
param set-default RC_MAP_THROTTLE 3
param set-default RC_MAP_ROLL 1
param set-default RC_MAP_PITCH 2
param set-default RC_MAP_YAW 4

# Telemetry (will configure UART in Phase 1)
param set-default MAV_0_RATE 57600        # Baud rate for TELEM1
param set-default MAV_0_FORWARD 1         # Enable forwarding
```

**Verification:**
```bash
nsh> param show SYS_AUTOSTART
# Should show 4001

nsh> commander status
# Should show airframe type
```

---

### 0.4 Memory Optimization

**If flash/RAM usage is too high:**

#### Flash Optimization:
```cmake
# In boards/microchip/samv71-xult-clickboards/default.px4board:

# Disable unused features:
CONFIG_SYSTEMCMDS_SERIAL_TEST=n
CONFIG_SYSTEMCMDS_I2CDETECT=n  # Only if not debugging I2C
CONFIG_SYSTEMCMDS_NSHTERM=n

# Reduce logging verbosity:
CONFIG_SYSTEMCMDS_DMESG=n  # Only after debugging complete

# Use smaller MAVLink dialect:
CONFIG_MAVLINK_DIALECT_minimal=y  # Instead of common

# Disable unused vehicle types:
# Keep only what you need (e.g., multicopter only)
CONFIG_MODULES_ROVER_POS_CONTROL=n
CONFIG_MODULES_VTOL_ATT_CONTROL=n
```

#### RAM Optimization:
```cmake
# Reduce buffer sizes:
CONFIG_SDLOG_BUFFER_SIZE=8192      # Default is 12288
CONFIG_MAVLINK_MAX_INSTANCES=1     # Default is 3

# Reduce uORB queue depths:
# (Advanced - only if necessary)
```

**Monitor with:**
```bash
nsh> free
              total       used       free    largest
Mem:         383680      62304     321376     321376

# RAM usage breakdown:
nsh> top
# Shows per-process memory
```

---

## Phase 1: Simulation Ready (HIL/Gazebo)

**Goal:** Enable Hardware-In-the-Loop (HIL) simulation without physical sensors

**Duration:** 2-3 weeks

**Prerequisites:**
- ✅ Full PX4 stack running
- ✅ SD card logging working
- ✅ All modules starting successfully

---

### 1.1 MAVLink on Hardware UART (TELEM1)

#### Hardware Setup

**SAMV71-XULT UART Pinout (USART0 - TELEM1):**
```
Pin PA9  → USART0_RX (connect to FTDI TX)
Pin PA10 → USART0_TX (connect to FTDI RX)
GND      → FTDI GND
```

**FTDI Adapter:** 3.3V UART (e.g., FT232RL)

**Wiring:**
```
SAMV71-XULT    FTDI Adapter
-----------    ------------
PA9  (RX)  →   TXD
PA10 (TX)  →   RXD
GND        →   GND
VCC        →   (Not connected - board powered via USB)
```

#### Software Configuration

**File:** `boards/microchip/samv71-xult-clickboards/init/rc.board_defaults`

```bash
# Enable MAVLink on TELEM1 (USART0)
param set-default MAV_0_CONFIG 101     # TELEM1
param set-default MAV_0_RATE 57600     # Baud rate
param set-default MAV_0_MODE 0         # Normal mode
param set-default MAV_0_FORWARD 1      # Enable forwarding

# Keep USB console for NSH:
param set-default SYS_USB_AUTO 2       # USB CDC (not MAVLink)
```

**File:** `boards/microchip/samv71-xult-clickboards/src/board_config.h`

**Verify USART0 is configured:**
```c
/* USART0: TELEM1 - MAVLink telemetry */
#define GPIO_USART0_RX   GPIO_USART0_RX_1  /* PA9 */
#define GPIO_USART0_TX   GPIO_USART0_TX_1  /* PA10 */
```

**File:** `boards/microchip/samv71-xult-clickboards/nuttx-config/nsh/defconfig`

```bash
# Enable USART0
CONFIG_SAMV7_USART0=y
CONFIG_USART0_SERIAL_CONSOLE=n  # Console stays on USB
CONFIG_USART0_BAUD=57600
CONFIG_USART0_BITS=8
CONFIG_USART0_PARITY=0
CONFIG_USART0_2STOP=0
```

#### Testing MAVLink UART

**On Development PC:**
```bash
# Install MAVProxy
pip3 install MAVProxy

# Connect to FTDI adapter (usually /dev/ttyUSB0)
mavproxy.py --master=/dev/ttyUSB0 --baudrate=57600 --aircraft=SAMV71_test

# Expected output:
# Received 1024 heartbeat(s) from...
# link 1 OK

# Test commands:
MAVLINK> mode stabilize
MAVLINK> arm throttle
MAVLINK> status
```

**On SAMV71-XULT NSH (via USB console):**
```bash
nsh> mavlink status
instance #0:
  transport protocol: serial (/dev/ttyS1 @ 57600)
  mode: Normal
  MAVLink version: 2
  messages received: 152
  messages sent: 3421
  messages lost: 0
```

**Success Criteria:**
- ✅ MAVProxy connects and receives heartbeats
- ✅ Commands sent from PC are received
- ✅ Telemetry data visible in MAVProxy
- ✅ NSH console still accessible via USB

---

### 1.2 USB CDC MAVLink Emulation (Second USB Port)

**Goal:** Use second USB port for MAVLink (simulation/development)

**Note:** SAMV71-XULT has dual USB:
1. **USB-A (Debug):** Currently used for NSH console
2. **USB-B (Device):** Can be configured as USB CDC for MAVLink

#### Hardware Configuration

**Jumpers on SAMV71-XULT:**
- J300: Configure for USB Device mode
- Check schematic for exact jumper positions

#### NuttX USB CDC Configuration

**File:** `boards/microchip/samv71-xult-clickboards/nuttx-config/nsh/defconfig`

```bash
# Enable dual USB CDC
CONFIG_USBDEV=y
CONFIG_USBDEV_DUALSPEED=y
CONFIG_CDCACM=y
CONFIG_CDCACM_COMPOSITE=y
CONFIG_CDCACM_NDEVICES=2  # Two CDC devices

# First CDC device: NSH console (existing)
CONFIG_CDCACM_CONSOLE=y

# Second CDC device: MAVLink
# (Configuration in PX4 layer)
```

**File:** `boards/microchip/samv71-xult-clickboards/src/sam_usbdev.c`

**Implement dual CDC enumeration** (if not already present)

#### PX4 Configuration for USB MAVLink

**File:** `rc.board_defaults`

```bash
# Option 1: MAVLink on second USB CDC
param set-default MAV_1_CONFIG 102     # USB CDC #2
param set-default MAV_1_RATE 921600    # High speed USB
param set-default MAV_1_MODE 0         # Normal

# Option 2: Keep UART for MAVLink, use USB for shell only
# (Use this if dual USB CDC is complex)
param set-default MAV_0_CONFIG 101     # TELEM1 (UART)
param set-default SYS_USB_AUTO 2       # USB CDC shell only
```

#### Testing USB CDC MAVLink

**On Development PC:**
```bash
# Check for second CDC device
ls -l /dev/ttyACM*
# Should show /dev/ttyACM0 (NSH) and /dev/ttyACM1 (MAVLink)

# Test with MAVProxy on second device
mavproxy.py --master=/dev/ttyACM1 --baudrate=921600

# Or use QGroundControl:
# Settings → Comm Links → Add → USB
# Select /dev/ttyACM1
```

**Fallback if Dual USB is Complex:**
Use USB-to-Serial adapter on TELEM1, keep USB for NSH only.

---

### 1.3 USB Bootloader (Future Upgrade Path)

**Goal:** Enable USB DFU bootloader like STM32 boards

**Note:** This is **future work** - implement after core functionality stable

#### Bootloader Requirements

**Reference Implementation:** STM32 PX4_FMU boards

**Key Components:**
1. **Bootloader binary** in first flash sector
2. **Application** in main flash
3. **Boot decision logic** (button press, magic value, etc.)
4. **DFU protocol** implementation

#### High-Level Implementation Plan

**Stage 1: Bootloader Development**
```
1. Port PX4 bootloader to SAMV7
   - Based on platforms/nuttx/src/bootloader/
   - Adapt for SAMV7 flash layout

2. Implement boot decision:
   - Check for magic value in RAM
   - Check for button press (SW0 on SAMV71-XULT)
   - Timeout and boot application if no DFU request

3. USB DFU stack:
   - Use NuttX USB DFU class
   - Or port STM32 DFU implementation
```

**Stage 2: Flash Layout**
```
SAMV7 Flash (2MB):
0x00400000 - 0x00408000:  Bootloader (32 KB)
0x00408000 - 0x00410000:  Reserved (32 KB)
0x00410000 - 0x00600000:  Application (1984 KB)
```

**Stage 3: Build System Integration**
```cmake
# In boards/microchip/samv71-xult-clickboards/:
# Create bootloader target:
make microchip_samv71-xult-clickboards_bootloader

# Create combined binary:
# bootloader + app with correct offsets
```

**Stage 4: Upload Tool**
```bash
# Use dfu-util or custom Python script
dfu-util -a 0 -s 0x00410000 -D firmware.bin
```

**Timeline:** 3-4 weeks (after Phase 1-2 complete)

**Priority:** Medium (nice-to-have, not critical for flight)

**Benefits:**
- No need for JTAG/SWD programmer
- Field updates via USB
- Matches STM32 workflow
- Easier for non-developers

**Defer Until:**
- Core functionality proven
- Flight tests successful
- Upstream contribution planned

---

### 1.4 HIL/Gazebo Simulation Setup

#### Gazebo Prerequisites

**On Development PC:**
```bash
# Install Gazebo (Ubuntu 22.04)
sudo apt-get install gazebo11 libgazebo11-dev

# Install PX4 Gazebo plugin
cd ~/src
git clone https://github.com/PX4/PX4-SITL_gazebo-classic.git
cd PX4-SITL_gazebo-classic
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install
```

#### HIL Configuration on SAMV71

**File:** `rc.board_defaults`

```bash
# HIL mode (Hardware-In-Loop)
param set-default SYS_HITL 1              # Enable HIL mode
param set-default MAV_0_CONFIG 101        # MAVLink on TELEM1
param set-default MAV_0_RATE 921600       # High baud for HIL
param set-default MAV_0_MODE 0            # Normal mode

# Disable real sensors (use simulation)
param set-default SENS_EN_GPSSIM 1        # GPS from simulation
param set-default SYS_HAS_MAG 0           # No real magnetometer yet
param set-default SYS_HAS_BARO 0          # No real barometer yet
```

#### Running HIL Simulation

**On Development PC:**
```bash
# Terminal 1: Start Gazebo with quadcopter model
cd ~/src/PX4-Autopilot
DONT_RUN_PX4=1 make px4_sitl_default gazebo_iris

# Terminal 2: Connect MAVProxy to SAMV71 hardware
mavproxy.py --master=/dev/ttyUSB0 --baudrate=921600 \
            --out=udp:127.0.0.1:14550 \
            --out=udp:127.0.0.1:14551

# Terminal 3: Connect QGroundControl or simulation viewer
# (Connects to UDP ports from MAVProxy)
```

**On SAMV71 NSH Console:**
```bash
nsh> commander status
# Should show HIL mode active

nsh> sensors status
# Should show simulated sensor data

nsh> ekf2 status
# Should show EKF fusing simulated data
```

**Expected Behavior:**
- Gazebo shows quadcopter model
- SAMV71 receives simulated IMU, GPS, baro data
- EKF2 converges (check `ekf2 status`)
- Can arm and control via MAVLink
- Motors/servos respond to attitude commands

**Success Criteria:**
- ✅ HIL mode starts without errors
- ✅ Simulated sensors provide data
- ✅ EKF2 achieves good health
- ✅ Can arm in simulation
- ✅ Attitude/position control responds

---

## Phase 2: Sensor Integration and Validation

**Goal:** Integrate physical sensors when click boards arrive

**Duration:** 3-4 weeks

**Prerequisites:**
- ✅ HIL simulation working
- ✅ I2C and SPI drivers functional
- ✅ Click boards and sensors acquired

---

### 2.1 Hardware Preparation

#### Recommended Click Board Sensors

**MikroElektronika Click Boards for SAMV71-XULT:**

**IMU (I2C/SPI):**
- **ICM-20689 Click** (I2C/SPI, 6-axis IMU)
- Or **MPU-9250 Click** (I2C/SPI, 9-axis with magnetometer)

**Barometer (I2C/SPI):**
- **BMP280 Click** (I2C/SPI, barometer + temperature)
- Or **MS5611 Click** (I2C/SPI, high precision baro)

**Magnetometer (I2C):**
- **MAG3110 Click** (I2C, 3-axis compass)
- Or integrated in MPU-9250

**GPS (UART):**
- **GPS Click** (UART, NMEA/UBX)
- Or external GPS module on UART2

**Optional:**
- **Air Quality Click** (I2C, differential pressure for airspeed)
- **Proximity Click** (I2C, distance sensor)
- **Laser Distance Click** (I2C, rangefinder)

#### I2C Bus Configuration

**SAMV71-XULT I2C Availability:**
```
I2C0: Available on Arduino headers
  SCL: PA4
  SDA: PA3

I2C1: Available on click board headers
  SCL: PB1
  SDA: PB0
```

**File:** `boards/microchip/samv71-xult-clickboards/src/board_config.h`

```c
/* I2C Configuration */
#define PX4_I2C_BUS_EXPANSION    0  /* I2C0 for Arduino/Click sensors */
#define PX4_I2C_BUS_ONBOARD      1  /* I2C1 for onboard sensors (if any) */
```

#### SPI Bus Configuration

**SAMV71-XULT SPI Availability:**
```
SPI0: Available for click boards
  SCK:  PA14
  MISO: PA12
  MOSI: PA13
  CS:   PA11 (or GPIO)
```

**File:** `board_config.h`

```c
/* SPI Configuration */
#define PX4_SPI_BUS_SENSORS      0  /* SPI0 for click board sensors */

/* Chip select GPIO assignments */
#define GPIO_SPI_CS_ICM20689     (GPIO_OUTPUT | GPIO_CFG_PULLUP | GPIO_PORT_PIOA | GPIO_PIN11)
#define GPIO_SPI_CS_BMP280       (GPIO_OUTPUT | GPIO_CFG_PULLUP | GPIO_PORT_PIOA | GPIO_PIN12)
```

---

### 2.2 I2C Sensor Integration

#### Step 2.2.1: Verify I2C Bus

**Without sensors connected:**
```bash
nsh> i2cdetect 0
     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
00:          -- -- -- -- -- -- -- -- -- -- -- -- --
10: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
...
# Should show empty bus (all --)
```

**With sensors connected (example ICM-20689 at 0x68):**
```bash
nsh> i2cdetect 0
     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
00:          -- -- -- -- -- -- -- -- -- -- -- -- --
10: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
...
60: -- -- -- -- -- -- -- -- 68 -- -- -- -- -- -- --
# Shows device at 0x68
```

#### Step 2.2.2: Enable ICM-20689 Driver (I2C Example)

**File:** `boards/microchip/samv71-xult-clickboards/default.px4board`

```cmake
CONFIG_DRIVERS_IMU_INVENSENSE_ICM20689=y
CONFIG_COMMON_INV_MPU=y  # Common InvenSense driver
```

**File:** `boards/microchip/samv71-xult-clickboards/init/rc.board_sensors`

```bash
#!/bin/sh
# SAMV71-XULT Sensor Initialization

# Start ICM-20689 on I2C0
icm20689 -I -b 0 -a 0x68 start

# Start magnetometer if separate (e.g., MAG3110)
# mag3110 -I -b 0 -a 0x0e start

# Start barometer (e.g., BMP280)
# bmp280 -I -b 0 -a 0x76 start
```

**File:** `ROMFS/px4fmu_common/init.d/rcS` (after parameter load)

```bash
# Load board-specific sensors
if [ -f /etc/init.d/rc.board_sensors ]
then
    . /etc/init.d/rc.board_sensors
fi
```

#### Step 2.2.3: Verify Sensor Operation

**Build and flash:**
```bash
make clean
make microchip_samv71-xult-clickboards_default
# Flash firmware
```

**Test on board:**
```bash
nsh> icm20689 status
# Should show device found, initialized

nsh> listener sensor_accel
# Should show accelerometer data streaming
# Example output:
# TOPIC: sensor_accel
# timestamp: 1234567890
# x: 0.12, y: -0.05, z: -9.81  (m/s^2)

nsh> listener sensor_gyro
# Should show gyroscope data

nsh> sensors status
# Should show:
# accel0: ICM20689 (I2C)
#   status: OK
#   temperature: 25.3°C
#   samples: 1523451
```

**Data Validation:**
```bash
# Accelerometer should show ~9.81 m/s^2 in Z when stationary
# Gyroscope should show ~0 when stationary
# Rotate board → gyro values change
# Tilt board → accel values change
```

---

### 2.3 SPI Sensor Integration

**If using SPI variant of ICM-20689:**

#### Enable SPI Driver

**File:** `nuttx-config/nsh/defconfig`

```bash
CONFIG_SAMV7_SPI0=y
CONFIG_SPI=y
CONFIG_SPI_EXCHANGE=y
```

**File:** `src/board_config.h`

```c
#define PX4_SPI_BUS_SENSORS      0
#define PX4_SPIDEV_ICM_20689     PX4_MK_SPI_SEL(0,0)  /* SPI0, CS0 */
```

**File:** `rc.board_sensors`

```bash
# Start ICM-20689 on SPI0
icm20689 -S -b 0 -c 0 start  # -S = SPI, -b = bus, -c = CS
```

**Advantages of SPI over I2C:**
- Higher bandwidth (faster sensor rates)
- Lower latency
- More reliable for high-rate IMU data

---

### 2.4 GPS Integration (UART)

#### Hardware Connection

**SAMV71-XULT UART2 for GPS:**
```
Pin PD25 → UART2_RX (connect to GPS TX)
Pin PD26 → UART2_TX (connect to GPS RX)
GND      → GPS GND
5V       → GPS VCC (from external if needed)
```

**Recommended GPS Module:**
- **u-blox NEO-M8N** (common, well-supported)
- **u-blox ZED-F9P** (RTK-capable, high precision)

#### NuttX UART Configuration

**File:** `nuttx-config/nsh/defconfig`

```bash
CONFIG_SAMV7_UART2=y
CONFIG_UART2_BAUD=38400  # u-blox default
CONFIG_UART2_BITS=8
CONFIG_UART2_PARITY=0
CONFIG_UART2_2STOP=0
```

**File:** `src/board_config.h`

```c
/* UART2: GPS */
#define GPIO_UART2_RX   GPIO_UART2_RX_1  /* PD25 */
#define GPIO_UART2_TX   GPIO_UART2_TX_1  /* PD26 */
```

#### PX4 GPS Driver

**File:** `rc.board_sensors`

```bash
# Start GPS on UART2
gps start -d /dev/ttyS2 -p ubx  # u-blox protocol
```

**File:** `rc.board_defaults`

```bash
# GPS configuration
param set-default GPS_1_CONFIG 202    # UART2 (GPS1)
param set-default GPS_1_PROTOCOL 1    # u-blox UBX
param set-default GPS_UBX_DYNMODEL 7  # Airborne <2g
```

#### Verify GPS Operation

```bash
nsh> listener sensor_gps
# Should show GPS data (may take time to get fix)
# Example:
# lat: 0 (no fix yet)
# lon: 0
# alt: 0
# satellites_used: 0

# After ~30 seconds (outdoors with clear sky):
# lat: 40.7128° N
# lon: -74.0060° W
# alt: 123.4 m
# satellites_used: 12
# fix_type: 3 (3D fix)

nsh> gps status
# Should show:
# GPS status: OK
# Fix type: 3D
# Satellites: 12
# HDOP: 0.8
```

---

### 2.5 EKF2 Integration and Tuning

**Goal:** Get Extended Kalman Filter to fuse all sensor data

#### Prerequisites
- ✅ IMU providing accel + gyro
- ✅ Magnetometer providing compass (or GPS heading)
- ✅ Barometer providing altitude
- ✅ GPS providing position

#### Enable EKF2

**File:** `default.px4board`

```cmake
CONFIG_MODULES_EKF2=y
```

**File:** `rc.board_defaults`

```bash
# EKF2 configuration
param set-default EKF2_ENABLE 1
param set-default SYS_MC_EST_GROUP 2  # Use EKF2

# Sensor selection
param set-default EKF2_AID_MASK 1     # Use GPS
param set-default EKF2_HGT_MODE 0     # Baro for height
param set-default EKF2_MAG_TYPE 0     # Auto mag mode

# Sensor noise (may need tuning)
param set-default EKF2_ACC_NOISE 0.35    # Accel noise
param set-default EKF2_GYR_NOISE 0.015   # Gyro noise
param set-default EKF2_MAG_NOISE 0.05    # Mag noise
param set-default EKF2_BARO_NOISE 2.0    # Baro noise (m)
param set-default EKF2_GPS_P_NOISE 0.5   # GPS position noise (m)
param set-default EKF2_GPS_V_NOISE 0.3   # GPS velocity noise (m/s)
```

#### Monitor EKF2 Health

```bash
nsh> ekf2 status
# Should show:
# EKF2 IMU0: OK
# EKF2 IMU1: not available
# GPS: OK
# Baro: OK
# Mag: OK
#
# Innovation test ratios:
# vel_test_ratio: 0.12  (should be <1.0)
# pos_test_ratio: 0.08  (should be <1.0)
# hgt_test_ratio: 0.15  (should be <1.0)
# mag_test_ratio: 0.22  (should be <1.0)
# tas_test_ratio: 0.00  (airspeed, not used yet)
#
# Flags:
# tilt_align: YES
# yaw_align: YES
# gps: YES

# Listen to estimated vehicle state:
nsh> listener vehicle_local_position
# Shows EKF estimate:
# x: 1.23 m (relative position)
# y: -0.45 m
# z: -0.12 m (negative = up)
# vx: 0.01 m/s (velocity)
# vy: 0.00 m/s
# vz: -0.02 m/s
```

#### EKF2 Tuning Process

**If innovation test ratios are high (>0.5):**

1. **Check sensor data quality:**
```bash
nsh> listener sensor_accel
nsh> listener sensor_gyro
# Look for noise, spikes, offsets
```

2. **Calibrate sensors:**
```bash
nsh> commander calibrate accel
# Follow prompts, place board in each orientation

nsh> commander calibrate gyro
# Keep board still during calibration

nsh> commander calibrate mag
# Rotate board in all axes

nsh> commander calibrate baro
# Keep board at reference altitude
```

3. **Adjust noise parameters:**
If test ratios high for specific sensor, increase its noise parameter:
```bash
# Example: GPS position test ratio high
param set EKF2_GPS_P_NOISE 1.0  # Increase from 0.5
```

4. **Check alignment:**
```bash
# Verify IMU is aligned with board frame:
param show SENS_BOARD_ROT  # Should be 0 (no rotation)
```

**Success Criteria:**
- ✅ All innovation test ratios < 0.5
- ✅ Position estimate stable (no jumps)
- ✅ Velocity estimate smooth
- ✅ Attitude estimate matches physical orientation
- ✅ `commander check` shows all sensors healthy

---

### 2.6 Sensor Validation Testing

#### Test 2.6.1: Static Ground Tests

**Accelerometer Test:**
```bash
# Board flat on table
nsh> listener sensor_accel
# Z-axis: ~-9.81 m/s^2 (pointing down)
# X, Y axes: ~0 m/s^2

# Rotate 90° (nose up)
# X-axis: ~-9.81 m/s^2
# Y-axis: ~0 m/s^2
# Z-axis: ~0 m/s^2
```

**Gyroscope Test:**
```bash
# Board stationary
nsh> listener sensor_gyro
# All axes: ~0 rad/s (some bias OK, should be <0.01 rad/s)

# Rotate board around Z-axis (yaw)
# Z-axis: should show rotation rate (e.g., 0.5 rad/s = ~28°/s)
```

**Magnetometer Test:**
```bash
nsh> listener sensor_mag
# Note values for each axis

# Rotate board 360° in yaw
# Values should vary smoothly in sine wave pattern
# Max/min values should be similar for X and Y axes

# Check for interference:
# Keep away from metal, magnets, motors
# Values should be stable when stationary
```

**Barometer Test:**
```bash
nsh> listener sensor_baro
# Note altitude

# Raise board 1 meter
# Altitude should change by ~1 meter

# Temperature should be reasonable (20-30°C indoors)
```

**GPS Test:**
```bash
# Take board outdoors with clear sky view

nsh> listener sensor_gps
# Wait for fix (may take 30-60 seconds on first boot)

# Fix type: 3 (3D fix)
# Satellites: >8 for good accuracy
# HDOP: <1.5 for good accuracy

# Walk 10 meters north
# Latitude should increase
# Verify position change matches distance
```

#### Test 2.6.2: Sensor Fusion Validation

**Attitude Estimation:**
```bash
nsh> listener vehicle_attitude
# Quaternion: q[0], q[1], q[2], q[3]

# Or view Euler angles:
nsh> listener vehicle_attitude | grep -A3 "roll\|pitch\|yaw"

# Tilt board 45° in roll
# Roll value should show ~0.785 rad (45°)

# Pitch board 45° forward
# Pitch value should show ~0.785 rad

# Rotate board 90° in yaw
# Yaw value should change by ~1.57 rad (90°)
```

**Position Estimation:**
```bash
nsh> listener vehicle_local_position
# With GPS fix and stationary:
# x, y, z should be stable (within 0.5m)
# vx, vy, vz should be near zero (<0.1 m/s)

# Walk 10 meters east
# y-value should increase by ~10 meters
```

**Velocity Estimation:**
```bash
# Shake board gently
nsh> listener vehicle_local_position
# Velocity should change during motion
# Return to zero when stopped
# Should be smooth (no jumps)
```

#### Test 2.6.3: External Instrumentation

**Logging for Analysis:**
```bash
# Start logger with high rate
nsh> logger on
nsh> logger start -t  # Topic-based logging

# Perform test sequence:
# - Keep still for 30 seconds
# - Rotate in roll 360°
# - Rotate in pitch 360°
# - Rotate in yaw 360°
# - Walk in square pattern (10m each side)

# Stop logging
nsh> logger stop

# Download log
# On PC: logs are in /fs/microsd/log/
# Use pyulog or FlightPlot to analyze
```

**Analysis Tools:**

1. **PlotJuggler** (recommended):
```bash
# Install
sudo apt-get install plotjuggler

# Convert ULG to CSV
ulog2csv 2025-11-22_log_001.ulg

# Import CSV into PlotJuggler
plotjuggler
```

2. **Flight Review** (web-based):
```bash
# Upload log to https://logs.px4.io
# Automatic analysis of:
# - Sensor health
# - EKF innovation
# - Vibration levels
# - Estimation quality
```

3. **pyulog** (Python):
```python
from pyulog import ULog

log = ULog('log_file.ulg')

# Access sensor data
accel = log.get_dataset('sensor_accel')
gyro = log.get_dataset('sensor_gyro')

# Plot in matplotlib
import matplotlib.pyplot as plt
plt.plot(accel.data['timestamp'], accel.data['x'])
plt.show()
```

**Validation Metrics:**

| Metric | Good | Acceptable | Poor |
|--------|------|------------|------|
| **Accel Noise** | <0.3 m/s² | <0.5 m/s² | >0.5 m/s² |
| **Gyro Noise** | <0.01 rad/s | <0.02 rad/s | >0.02 rad/s |
| **Gyro Bias Stability** | <0.001 rad/s | <0.005 rad/s | >0.005 rad/s |
| **Mag Noise** | <0.05 Gauss | <0.1 Gauss | >0.1 Gauss |
| **Baro Noise** | <1 m | <2 m | >2 m |
| **GPS Accuracy (HDOP)** | <1.0 | <2.0 | >2.0 |
| **EKF Position Drift** | <1 m/min | <2 m/min | >2 m/min |
| **Attitude Error** | <1° | <2° | >2° |

---

## Phase 3: Full System Integration

**Goal:** Enable all PX4 subsystems for complete autopilot functionality

**Duration:** 2-3 weeks

**Prerequisites:**
- ✅ All sensors working
- ✅ EKF2 converging
- ✅ MAVLink functional

---

### 3.1 PWM Output Configuration (Motors/Servos)

#### Hardware: PWM Output on SAMV71

**Timer/Channel Availability:**
```
TC0_CH0 (PA0):  PWM1
TC0_CH1 (PA15): PWM2
TC1_CH0 (PA26): PWM3
TC1_CH1 (PA27): PWM4
TC2_CH0 (PA5):  PWM5
TC2_CH1 (PA6):  PWM6
```

**File:** `src/board_config.h`

```c
/* PWM Outputs */
#define DIRECT_PWM_OUTPUT_CHANNELS  6

#define GPIO_TIM_CH1   GPIO_TC0_TIOA0  /* PA0  - PWM1 */
#define GPIO_TIM_CH2   GPIO_TC0_TIOA1  /* PA15 - PWM2 */
#define GPIO_TIM_CH3   GPIO_TC1_TIOA0  /* PA26 - PWM3 */
#define GPIO_TIM_CH4   GPIO_TC1_TIOA1  /* PA27 - PWM4 */
#define GPIO_TIM_CH5   GPIO_TC2_TIOA0  /* PA5  - PWM5 */
#define GPIO_TIM_CH6   GPIO_TC2_TIOA1  /* PA6  - PWM6 */
```

#### Implement PWM Driver

**File:** `src/drivers/samv7_pwm_out.cpp` (new file - to be created)

**Reference:** `platforms/nuttx/src/px4/common/stm32_common/io_timer.c`

**Key Functions:**
```cpp
int io_timer_init(void);
int io_timer_set_rate(unsigned timer, unsigned rate);
int io_timer_set_enable(bool enable, enum Mode mode, unsigned channel_mask);
int io_timer_channel_init(unsigned channel, io_timer_channel_mode_t mode);
int up_pwm_servo_set(unsigned channel, servo_position_t value);
```

**Timer Configuration:**
```c
/* PWM frequency: 50 Hz (20ms period for standard RC servos)
 * Or 400 Hz for ESCs
 */
#define PWM_FREQUENCY_DEFAULT  400  /* Hz */
#define PWM_PERIOD_US         2500  /* microseconds (1/400) */

/* Pulse width range: 1000-2000 us typical */
#define PWM_PULSE_MIN         1000  /* us */
#define PWM_PULSE_MAX         2000  /* us */
```

#### Enable PWM in Build

**File:** `default.px4board`

```cmake
CONFIG_DRIVERS_PWM_OUT=y
CONFIG_BOARD_HAS_PWM=y
```

**File:** `src/CMakeLists.txt`

```cmake
px4_add_board_module(
    MODULE drivers__samv7_pwm_out
    SRCS
        samv7_pwm_out.cpp
    DEPENDS
        drivers_pwm_out
)
```

#### Test PWM Output

**Without motors (safety):**
```bash
nsh> pwm test -c 1 -p 1500
# Channel 1 should output 1500 us pulse
# Measure with oscilloscope or servo tester

# Test all channels:
nsh> pwm test -c 12345678 -p 1500
```

**With servo connected:**
```bash
# Center position
nsh> pwm test -c 1 -p 1500

# Full deflection one way
nsh> pwm test -c 1 -p 2000

# Full deflection other way
nsh> pwm test -c 1 -p 1000
```

**Mixer configuration:**
```bash
# Set mixer for quadcopter
nsh> mixer load /dev/pwm_output0 /etc/mixers/quad_x.main.mix

# Test motor output (BE CAREFUL - REMOVE PROPS!)
nsh> motor_test test -m 1 -p 10
# Motor 1 at 10% throttle
```

---

### 3.2 RC Input Configuration

#### Hardware: RC Receiver Input

**Options for RC input:**

**Option 1: PPM Input (Preferred)**
```
Timer input capture:
  TC0_TIOA0 → PA0 (can be configured as input capture)
```

**Option 2: SBUS Input (UART)**
```
UART3_RX → PD28
Configure as SBUS (inverted serial, 100kbaud)
```

**Option 3: Spektrum/DSM (UART)**
```
UART3_RX → PD28
Configure for Spektrum protocol (115200 baud)
```

#### Implement RC Input Driver

**File:** `src/drivers/samv7_rc_input.cpp` (new file)

**Reference:** STM32 implementations:
- `platforms/nuttx/src/px4/stm/stm32_common/dshot/dshot.cpp` (timer input capture)
- `src/drivers/rc_input/rc_input.cpp` (protocol parsing)

**For PPM:**
```cpp
/* Configure timer for input capture on rising/falling edges
 * PPM frame: 20ms period, 8 channels
 * Each pulse: 1000-2000 us width
 * Frame sync: >3ms gap
 */
```

**For SBUS:**
```c
/* Configure UART for SBUS:
 * - 100000 baud
 * - 8 data bits, even parity, 2 stop bits
 * - Inverted (requires hardware inverter or GPIO trick)
 */
CONFIG_UART3_BAUD=100000
CONFIG_UART3_PARITY=2  /* Even */
CONFIG_UART3_2STOP=1
```

#### Test RC Input

**With RC receiver connected:**
```bash
# Check raw RC values
nsh> listener input_rc
# Should show:
# channel_count: 8
# values[0]: 1500 (roll)
# values[1]: 1500 (pitch)
# values[2]: 1000 (throttle)
# values[3]: 1500 (yaw)
# ...

# Move RC sticks - values should change
# Typical ranges: 1000-2000 us
```

**RC calibration:**
```bash
# In QGroundControl:
# Vehicle Setup → Radio → Calibrate

# Or manually:
nsh> commander calibrate rc
# Follow prompts
```

**Parameter mapping:**
```bash
nsh> param set RC_MAP_ROLL 1      # Channel 1 = Roll
nsh> param set RC_MAP_PITCH 2     # Channel 2 = Pitch
nsh> param set RC_MAP_THROTTLE 3  # Channel 3 = Throttle
nsh> param set RC_MAP_YAW 4       # Channel 4 = Yaw
nsh> param set RC_MAP_MODE_SW 5   # Channel 5 = Flight mode

nsh> param save
```

---

### 3.3 Full Module Testing

**Enable all modules incrementally:**

#### Commander (Flight Mode Manager)

```bash
nsh> commander status
# State: DISARMED
# Main state: MANUAL
# Navigation state: MANUAL
# Arming state: STANDBY

nsh> commander arm
# Should arm if safety checks pass

nsh> commander disarm
```

**Parameters:**
```bash
param set COM_RC_LOSS_T 1.0      # RC loss timeout (seconds)
param set COM_RC_IN_MODE 0       # RC input mode (0=PPM/SBUS)
param set COM_DISARM_PRFLT 10    # Preflight disarm timeout
param set COM_PREARM_MODE 0      # Prearm checks (0=disabled for testing)
```

#### Sensors Module

```bash
nsh> sensors status
# IMU: OK
# Mag: OK
# Baro: OK
# GPS: OK

# Check publication rates:
nsh> listener sensor_combined
# Should show fused sensor data at 100+ Hz
```

#### Navigator (Mission/Position Control)

```bash
nsh> navigator status
# Should show current flight mode
# Mission state if mission loaded
```

#### Attitude Controller

```bash
nsh> mc_att_control status  # For multicopter
# or
nsh> fw_att_control status  # For fixed-wing

# Should show:
# Controller active
# Roll/pitch/yaw rates
```

#### Position Controller

```bash
nsh> mc_pos_control status  # For multicopter
# Should show position control active
# Setpoints and current position
```

---

### 3.4 Module-by-Module Validation

**Create test script:**

**File:** `/fs/microsd/etc/extras/test_modules.sh`

```bash
#!/bin/sh
# Module validation script

echo "=== Testing Core Modules ==="

# Test 1: Commander
echo "Testing commander..."
commander status
if [ $? -eq 0 ]; then
    echo "[PASS] Commander running"
else
    echo "[FAIL] Commander not running"
fi

# Test 2: Sensors
echo "Testing sensors..."
sensors status
if [ $? -eq 0 ]; then
    echo "[PASS] Sensors running"
else
    echo "[FAIL] Sensors not running"
fi

# Test 3: EKF2
echo "Testing EKF2..."
ekf2 status
if [ $? -eq 0 ]; then
    echo "[PASS] EKF2 running"
else
    echo "[FAIL] EKF2 not running"
fi

# Test 4: MAVLink
echo "Testing MAVLink..."
mavlink status
if [ $? -eq 0 ]; then
    echo "[PASS] MAVLink running"
else
    echo "[FAIL] MAVLink not running"
fi

# Test 5: RC Input
echo "Testing RC input..."
rc_input check
if [ $? -eq 0 ]; then
    echo "[PASS] RC input detected"
else
    echo "[FAIL] No RC input"
fi

# Test 6: PWM Output
echo "Testing PWM output..."
pwm info
if [ $? -eq 0 ]; then
    echo "[PASS] PWM output available"
else
    echo "[FAIL] PWM output not available"
fi

echo "=== Module Test Complete ==="
```

**Run on board:**
```bash
nsh> sh /fs/microsd/etc/extras/test_modules.sh
```

---

## Phase 4: Flight Readiness

**Goal:** Prepare for first flight test

**Duration:** 1-2 weeks

**Prerequisites:**
- ✅ All sensors validated
- ✅ RC input working
- ✅ PWM outputs working
- ✅ Controllers running

---

### 4.1 Safety Configuration

**Critical safety parameters:**

```bash
# Failsafe configuration
param set COM_RC_LOSS_T 1.0          # RC loss triggers failsafe after 1 second
param set NAV_RCL_ACT 2              # RC loss action: Land
param set NAV_DLL_ACT 2              # Data link loss: Land

# Geofence (optional but recommended)
param set GF_ACTION 2                # Geofence breach: Land
param set GF_MAX_HOR_DIST 100        # Max horizontal distance (meters)
param set GF_MAX_VER_DIST 50         # Max altitude (meters)

# Low battery handling
param set BAT_CRIT_THR 0.15          # Critical at 15% (land immediately)
param set BAT_LOW_THR 0.25           # Low at 25% (RTL)
param set COM_LOW_BAT_ACT 3          # Low battery: Land

# Kill switch
param set COM_KILL_DISARM 5.0        # Auto-disarm after kill switch for 5s
param set RC_MAP_KILL_SW 7           # Channel 7 = kill switch

# Return to launch
param set RTL_RETURN_ALT 30          # RTL altitude (meters)
param set RTL_DESCEND_ALT 10         # Descend altitude
param set RTL_LAND_DELAY 5.0         # Hover before landing (seconds)
```

---

### 4.2 Airframe Configuration

**For Quadcopter Testing:**

```bash
# Select airframe
param set SYS_AUTOSTART 4001         # Generic Quadcopter X

# Motor mapping (verify with your frame)
# Motor layout (top view, X configuration):
#     Front
#   3     1
#     \ /
#     / \
#   2     4
#     Rear

# Verify mixer: /etc/mixers/quad_x.main.mix

# PWM parameters
param set PWM_MAIN_MIN 1000          # Min pulse width
param set PWM_MAIN_MAX 2000          # Max pulse width
param set PWM_MAIN_DISARM 900        # Disarmed pulse (ESC off)
param set PWM_MAIN_FAIL1 0           # Failsafe pulse (0 = no output)

# ESC calibration
param set PWM_MAIN_RATE 400          # PWM frequency (Hz)
```

---

### 4.3 PID Tuning (Initial Values)

**Attitude (rate) controller:**

```bash
# Roll rate controller
param set MC_ROLLRATE_P 0.15
param set MC_ROLLRATE_I 0.2
param set MC_ROLLRATE_D 0.003
param set MC_ROLLRATE_MAX 220        # deg/s

# Pitch rate controller
param set MC_PITCHRATE_P 0.15
param set MC_PITCHRATE_I 0.2
param set MC_PITCHRATE_D 0.003
param set MC_PITCHRATE_MAX 220       # deg/s

# Yaw rate controller
param set MC_YAWRATE_P 0.2
param set MC_YAWRATE_I 0.1
param set MC_YAWRATE_D 0.0
param set MC_YAWRATE_MAX 200         # deg/s
```

**Attitude (angle) controller:**

```bash
param set MC_ROLL_P 6.5
param set MC_PITCH_P 6.5
param set MC_YAW_P 2.8
```

**Altitude controller:**

```bash
param set MPC_Z_P 1.0
param set MPC_Z_VEL_P 0.4
param set MPC_Z_VEL_I 0.15
param set MPC_Z_VEL_D 0.05
```

**Position controller:**

```bash
param set MPC_XY_P 0.95
param set MPC_XY_VEL_P 0.09
param set MPC_XY_VEL_I 0.02
param set MPC_XY_VEL_D 0.01
```

**Note:** These are starting values. Tuning required based on actual airframe.

---

### 4.4 Pre-Flight Checklist

**Hardware:**
- [ ] All sensors connected and verified
- [ ] RC receiver bound and tested
- [ ] Motors spinning in correct direction
- [ ] Props installed with correct orientation (or REMOVED for initial tests)
- [ ] Battery at 100%
- [ ] Kill switch mapped and tested
- [ ] Telemetry link active

**Software:**
```bash
nsh> commander check
# All preflight checks should pass:
# - IMU OK
# - Mag OK
# - Baro OK
# - GPS OK (if required)
# - RC OK
# - Battery OK
# - Safety OK
```

**Parameter Review:**
```bash
# Verify critical parameters
nsh> param show SYS_AUTOSTART
nsh> param show COM_RC_LOSS_T
nsh> param show NAV_RCL_ACT
nsh> param show RC_MAP_*
nsh> param show PWM_MAIN_*
```

**Log Verification:**
```bash
# Ensure logging is active
nsh> logger status
# Should show: logging active

# Check SD card space
nsh> df /fs/microsd
# Should have >1GB free
```

---

## Phase 5: Flight Testing and Validation

**Goal:** Validate autopilot in actual flight

**Duration:** Ongoing (iterative)

**Prerequisites:**
- ✅ All Phase 4 tasks complete
- ✅ Safety review passed
- ✅ Test pilot briefed

---

### 5.1 Ground Testing (Props OFF)

**Test 1: Arming Sequence**

```bash
# Connect to QGroundControl
# Arm vehicle from RC (throttle down + yaw right)
# Or from QGC

# Verify:
# - Motors initialize correctly
# - No error messages
# - All status indicators green in QGC
```

**Test 2: Throttle Response**

```bash
# With props REMOVED:
# Arm vehicle
# Gradually increase throttle

# Verify:
# - All motors spin up together
# - Motor speed increases smoothly
# - No stuttering or stopping
```

**Test 3: Attitude Response**

```bash
# Arm vehicle
# Give roll/pitch/yaw inputs on RC

# Verify:
# - Motors respond to inputs
# - Correct motors increase/decrease speed
# - Response is smooth (no oscillation)

# Example: Roll right
# - Motor 1 (front right) should speed up
# - Motor 2 (rear left) should speed up
# - Motor 3 (front left) should slow down
# - Motor 4 (rear right) should slow down
```

---

### 5.2 Tethered Hover Test

**Setup:**
- Secure drone with tether (bungee cord or rope)
- Tether should allow 1m vertical movement
- Clear area, soft ground

**Test Procedure:**
```
1. Pre-flight checks in QGroundControl
2. Arm vehicle
3. Gradually increase throttle to 50%
4. Observe behavior:
   - Should lift off ground slightly
   - Should remain stable (no wild oscillations)
   - Should respond to stick inputs
5. Reduce throttle to land
6. Disarm
```

**Data Collection:**
```bash
# Log is automatically recorded
# After test, review log:

# Download from SD card
# Use Flight Review (https://logs.px4.io)

# Check for:
# - Vibration levels (<30 m/s^2 for accel)
# - Attitude tracking (setpoint vs actual)
# - Motor saturation (should not hit limits)
# - EKF health (innovation ratios <0.5)
```

---

### 5.3 First Free Flight (Stabilize Mode)

**Location Requirements:**
- Open area, minimum 50m x 50m
- No obstacles or people within 30m
- Good GPS reception (>8 satellites, HDOP <1.5)
- Light wind (<5 m/s)

**Flight Plan:**
```
1. Arm in Stabilize/Manual mode
2. Increase throttle to 70%
3. Lift to 1-2m altitude (hover)
4. Hold for 10 seconds
5. Small roll/pitch inputs to verify control
6. Land gently
7. Disarm
```

**Success Criteria:**
- [ ] Stable hover (no drift >1m)
- [ ] Responsive to RC inputs
- [ ] No oscillations in attitude
- [ ] Battery voltage stable
- [ ] No warnings/errors

**If Issues:**

| Issue | Possible Cause | Solution |
|-------|---------------|----------|
| Oscillations in roll/pitch | P gain too high | Reduce MC_ROLLRATE_P, MC_PITCHRATE_P |
| Sluggish response | P gain too low | Increase P gains |
| Drifts in one direction | Accelerometer calibration | Recalibrate accel |
| Altitude bounces | Z controller tuning | Adjust MPC_Z_P, MPC_Z_VEL_P |
| Yaw drift | Magnetometer cal or interference | Recalibrate mag, check for sources |

---

### 5.4 Position Hold Test (GPS Required)

**After successful stabilize flight:**

**Test Procedure:**
```
1. Arm in Position mode
2. Takeoff to 5m altitude
3. Release all sticks (hover in place)
4. Observe for 30 seconds
5. Gentle stick inputs to move 5m in each direction
6. Release sticks - should return to position
7. Land
```

**Success Criteria:**
- [ ] Holds position within 1m (horizontal)
- [ ] Holds altitude within 0.5m
- [ ] Returns to position when sticks released
- [ ] GPS quality good (>8 sats, HDOP <1.0)

---

### 5.5 Mission Mode Test (Waypoint Navigation)

**Create simple mission in QGroundControl:**

```
Waypoint 1: Takeoff to 10m
Waypoint 2: Fly to position 20m North
Waypoint 3: Fly to position 20m East
Waypoint 4: Fly to position 20m South
Waypoint 5: Fly to home position
Waypoint 6: Land
```

**Upload mission, switch to Mission mode, arm**

**Success Criteria:**
- [ ] Follows waypoints within 2m
- [ ] Smooth transitions between waypoints
- [ ] Completes mission autonomously
- [ ] Lands at designated point

---

### 5.6 Endurance Test

**Goal:** Verify system stability over extended flight

**Test:**
```
1. Fully charged battery
2. Takeoff in Position mode
3. Hover at 5m for maximum flight time
4. Monitor battery voltage
5. Land when battery reaches 20%
6. Record flight time
```

**Data to collect:**
- Total flight time
- Battery consumption
- Temperature of components (ESCs, battery)
- EKF health over time
- Any degradation in performance

---

### 5.7 Stress Testing

**Advanced tests after basic flight proven:**

**Test 1: Aggressive Maneuvers**
```
- Rapid roll/pitch inputs
- Quick altitude changes
- Fast yaw rotations
- Check for:
  - Controller saturation
  - EKF stability
  - Motor response
```

**Test 2: Failsafe Testing**
```
- RC loss: Turn off transmitter mid-flight
  - Should trigger RTL or Land
- GPS loss: (difficult to test - use HIL simulation)
- Battery failsafe: Drain to low battery threshold
  - Should trigger RTL/Land
```

**Test 3: Wind Testing**
```
- Fly in moderate wind (5-10 m/s)
- Check position hold accuracy
- Check return-to-launch accuracy
- Monitor controller effort
```

---

## Phase 6: Production Ready

**Goal:** Prepare for community release/upstream contribution

**Duration:** Ongoing

---

### 6.1 Documentation

**Create complete documentation package:**

1. **Hardware Setup Guide**
   - Board pinout
   - Sensor connections
   - Wiring diagrams
   - BOM (Bill of Materials)

2. **Build Instructions**
   - Toolchain setup
   - Build commands
   - Flash procedures

3. **Configuration Guide**
   - Parameter reference
   - Tuning procedures
   - Troubleshooting

4. **Flight Testing Guide**
   - Pre-flight checks
   - First flight procedure
   - Tuning guide

---

### 6.2 Code Cleanup

**Prepare for upstream submission:**

1. **Code Style**
   - Follow PX4 coding standards
   - Run `make format` to auto-format

2. **Remove Debug Code**
   - Remove excessive logging
   - Clean up commented-out code
   - Remove development-only features

3. **Documentation**
   - Add comments to complex sections
   - Document any SAMV7-specific workarounds
   - Update board README

---

### 6.3 Upstream Contribution

**Submit to PX4 main repository:**

1. **Fork PX4-Autopilot**
```bash
# On GitHub: Fork PX4/PX4-Autopilot

# Clone your fork
git clone https://github.com/YOUR_USERNAME/PX4-Autopilot
cd PX4-Autopilot
git remote add upstream https://github.com/PX4/PX4-Autopilot

# Create branch for SAMV71
git checkout -b samv71-xult-support
```

2. **Organize Commits**
```bash
# Separate logical commits:
# 1. Add SAMV7 architecture support
# 2. Add SAMV71-XULT board
# 3. Add HSMCI driver fixes
# 4. Add sensor drivers
# 5. Documentation

git rebase -i HEAD~20  # Interactive rebase to organize
```

3. **Create Pull Request**
```
Title: "Add support for Microchip SAMV71-XULT development board"

Description:
- Hardware details
- Features implemented
- Testing performed
- Known limitations
```

4. **CI/CD Compliance**
```bash
# Ensure builds pass
make microchip_samv71-xult-clickboards_default

# Run PX4 tests
make tests

# Check code style
make checkformat
```

---

## Appendix A: Testing Matrix

### Sensor Testing Matrix

| Sensor | Static Test | Dynamic Test | Integration Test | Status |
|--------|-------------|--------------|------------------|--------|
| **ICM-20689 (I2C)** | ✅ Read WHO_AM_I | ✅ Rotate board | ✅ EKF fusion | ⏸️ Pending hardware |
| **ICM-20689 (SPI)** | ✅ Read WHO_AM_I | ✅ Rotate board | ✅ EKF fusion | ⏸️ Pending hardware |
| **MAG3110** | ✅ Read values | ✅ Rotate 360° | ✅ EKF heading | ⏸️ Pending hardware |
| **BMP280** | ✅ Read pressure | ✅ Raise 1m | ✅ EKF altitude | ⏸️ Pending hardware |
| **GPS (u-blox)** | ✅ Get fix | ✅ Walk 10m | ✅ EKF position | ⏸️ Pending hardware |

### Module Testing Matrix

| Module | Unit Test | Integration | Flight Test | Status |
|--------|-----------|-------------|-------------|--------|
| **Commander** | ✅ Arm/disarm | ✅ Mode switching | ✅ In-flight modes | 🔄 In progress |
| **Sensors** | ✅ Data output | ✅ Fusion | ✅ Flight data | 🔄 In progress |
| **EKF2** | ✅ Convergence | ✅ Static accuracy | ✅ Flight accuracy | 🔄 In progress |
| **MC Att Control** | ✅ Setpoint tracking | ✅ Bench test | ✅ Flight | ⏸️ Pending |
| **MC Pos Control** | ✅ Setpoint gen | ✅ Simulated | ✅ Flight | ⏸️ Pending |
| **Navigator** | ✅ Waypoint parsing | ✅ Mission planning | ✅ Mission flight | ⏸️ Pending |
| **MAVLink** | ✅ Heartbeat | ✅ Command/response | ✅ Telemetry | 🔄 In progress |

### Flight Test Matrix

| Test | Conditions | Success Criteria | Status |
|------|-----------|------------------|--------|
| **Tethered Hover** | Props on, tethered | Stable hover, responsive | ⏸️ Not started |
| **Free Hover** | Stabilize mode, 2m | No oscillation, controlled | ⏸️ Not started |
| **Position Hold** | GPS lock, 5m altitude | Drift <1m, alt ±0.5m | ⏸️ Not started |
| **Waypoint Nav** | 4-point square mission | Track <2m error | ⏸️ Not started |
| **RTL Test** | Trigger from 50m away | Returns within 2m | ⏸️ Not started |
| **Failsafe** | RC loss simulation | Triggers RTL/Land | ⏸️ Not started |
| **Endurance** | Full battery | >10 min flight | ⏸️ Not started |

---

## Appendix B: Hardware Requirements

### Development Hardware

**Minimum Required:**
- ✅ SAMV71-XULT development board
- ✅ USB cable (Type-A to Micro-B)
- ✅ SD card (8GB+ recommended)
- ✅ JTAG/SWD programmer (if not using USB bootloader)

**Recommended for Full Testing:**
- ⏸️ FTDI USB-to-Serial adapter (3.3V)
- ⏸️ Click boards with sensors (ICM-20689, BMP280, etc.)
- ⏸️ GPS module (u-blox NEO-M8N or better)
- ⏸️ RC transmitter + receiver (PPM or SBUS output)
- ⏸️ ESCs (4x for quadcopter)
- ⏸️ Motors and propellers
- ⏸️ Battery (3S or 4S LiPo)
- ⏸️ Quadcopter frame

**Testing Equipment:**
- Oscilloscope (for PWM verification)
- Multimeter
- Servo tester
- Logic analyzer (optional, for debugging protocols)

### Software Requirements

**Development PC:**
- Ubuntu 22.04 LTS (or compatible)
- ARM GCC toolchain (version 13.2.1)
- CMake 3.28.3+
- Python 3.10+
- Git

**Ground Control:**
- QGroundControl (latest stable)
- MAVProxy (optional, for CLI control)

**Analysis Tools:**
- PlotJuggler or FlightPlot
- pyulog for log analysis
- Flight Review (web-based: logs.px4.io)

---

## Appendix C: PX4 Module Dependencies

### Core Dependencies

```
commander
├── sensors (requires IMU, mag, baro, GPS)
├── ekf2 (requires sensor_combined)
├── navigator (requires vehicle_status)
└── mavlink (communication)

sensors
├── sensor drivers (IMU, mag, baro, GPS)
└── sensor_calibration

ekf2
├── sensor_combined (from sensors)
└── vehicle_gps_position

mc_att_control (multicopter attitude)
├── vehicle_attitude_setpoint
├── vehicle_rates_setpoint
└── actuator_controls

mc_pos_control (multicopter position)
├── vehicle_local_position (from EKF)
└── vehicle_attitude

navigator
├── mission (dataman for storage)
└── vehicle_global_position (from EKF)
```

### Optional Modules

```
logger (recommended)
├── SD card support
└── All topics (for flight review)

mavlink (telemetry)
├── Serial port (UART or USB)
└── All topics to stream

rc_input
└── Timer or UART hardware

pwm_out
└── Timer hardware
```

---

## Timeline Summary

| Phase | Duration | Cumulative | Key Milestone |
|-------|----------|------------|---------------|
| **Immediate Fixes** | 1-2 days | 2 days | SD card working |
| **Phase 0** | 1-2 weeks | 2 weeks | Full PX4 stack |
| **Phase 1** | 2-3 weeks | 5 weeks | Simulation ready |
| **Phase 2** | 3-4 weeks | 9 weeks | Sensors integrated |
| **Phase 3** | 2-3 weeks | 12 weeks | Full system working |
| **Phase 4** | 1-2 weeks | 14 weeks | Flight ready |
| **Phase 5** | Ongoing | - | Flight validated |
| **Phase 6** | Ongoing | - | Production ready |

**Estimated Time to First Flight:** 12-14 weeks (assuming no major blockers)

---

**Document Version:** 1.0
**Last Updated:** November 22, 2025
**Status:** Planning document - execution to begin after SD card fixes
