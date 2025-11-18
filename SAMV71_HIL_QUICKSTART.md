# SAMV71-XULT HIL (Hardware-In-the-Loop) Quick Start Guide

## Overview

This guide shows how to test HIL simulation on the SAMV71-XULT board despite the parameter storage issue. HIL allows testing PX4 firmware with simulated sensors while running on real hardware.

**Current Limitation:** Flash parameter storage is not working (error 262144), and there's no SD card. However, we can still test MAVLink connectivity and potentially run HIL using workarounds.

---

## Status Check

Based on `ps` output, the following are running:
- **MAVLink** (PID 507) on `/dev/ttyS0` (USB serial)
- **Commander** (PID 425) - system state management
- **Navigator** (PID 558) - mission/flight mode logic
- **Logger** (PID 648) - data logging
- **EKF2** (PID 619) - sensor fusion (currently running with no sensors)

---

## Step 1: Test MAVLink Connection

### 1.1 Check USB Serial Connection

On your PC, verify the USB serial device:

```bash
# Linux
ls -l /dev/ttyACM*
# Should show /dev/ttyACM0

# Check permissions
sudo chmod 666 /dev/ttyACM0
```

### 1.2 Test with QGroundControl

1. **Download QGroundControl:** https://qgroundcontrol.com/
2. **Connect via USB:**
   - QGC should auto-detect the SAMV71 board on `/dev/ttyACM0`
   - Default baud rate: 57600 (check parameter `SER_TEL1_BAUD`)
   - Protocol: MAVLink v2

3. **Verify Connection:**
   - QGC toolbar should show "Connected"
   - System ID should appear (default: 1)
   - Vehicle type should show (may show "Generic" without sensors)

### 1.3 Test with mavproxy (Alternative)

If you prefer command-line testing:

```bash
# Install mavproxy
pip3 install mavproxy

# Connect to SAMV71
mavproxy.py --master=/dev/ttyACM0 --baudrate=57600

# In MAVProxy console, test:
MAV> param show SYS_HITL
MAV> param set SYS_HITL 1
```

**Note:** Setting parameters via MAVLink might bypass the local storage issue since MAVLink protocol includes its own parameter set messages.

---

## Step 2: Workaround for Parameter Storage Issue

### Option A: Set SYS_HITL via MAVLink

If QGroundControl or MAVProxy can set parameters, they might persist in RAM long enough for testing:

1. Connect with QGC
2. Go to **Vehicle Setup > Parameters**
3. Search for `SYS_HITL`
4. Set value to `1` (Enabled)
5. Reboot and verify (it may not persist after power cycle, but should work for current session)

### Option B: Modify Default Parameters in Firmware

If MAVLink parameter set doesn't work, we can modify the default parameter value in the firmware:

**File:** `src/modules/systemlib/parameters.c` or board-specific param defaults

**Change:**
```c
PARAM_DEFINE_INT32(SYS_HITL, 1);  // Default to HIL mode
```

Then rebuild and upload firmware.

### Option C: Use RAM-based Testing (Current Session Only)

Since parameters work in RAM (just don't persist), you can:

1. Boot the board
2. Set `SYS_HITL` via NSH or MAVLink
3. Start HIL simulation without rebooting
4. Test for current session only

---

## Step 3: Configure HIL Simulation

### 3.1 Install Gazebo

On your PC (not on SAMV71):

```bash
# Ubuntu 22.04
sudo apt install ros-humble-gazebo-ros-pkgs gazebo

# Or standalone Gazebo
curl -sSL http://get.gazebosim.org | sh
```

### 3.2 Set Up PX4 SITL Integration

Even though we're running hardware (HIL), Gazebo still needs PX4 models:

```bash
cd ~/
git clone https://github.com/PX4/PX4-Autopilot.git
cd PX4-Autopilot
make px4_sitl gazebo
```

This builds the Gazebo models and plugins.

### 3.3 Launch Gazebo HIL

```bash
cd ~/PX4-Autopilot
source Tools/simulation/gazebo/setup_gazebo.bash $(pwd) $(pwd)/build/px4_sitl_default

# Launch Gazebo with quadcopter model
gazebo Tools/simulation/gazebo/worlds/hitl_iris.world
```

---

## Step 4: Connect HIL to SAMV71

### 4.1 MAVLink Routing

Gazebo needs to send simulated sensor data to the SAMV71 via MAVLink. Use `mavlink-router` to route messages:

```bash
# Install mavlink-router
sudo apt install mavlink-router

# Route between Gazebo (UDP 14550) and SAMV71 (serial /dev/ttyACM0)
mavlink-routerd -e 127.0.0.1:14550 -e /dev/ttyACM0:57600
```

### 4.2 Verify Data Flow

On the SAMV71 NSH console:

```bash
nsh> listener sensor_accel
nsh> listener sensor_gyro
nsh> listener sensor_baro
```

You should see simulated sensor data streaming from Gazebo.

---

## Step 5: Test HIL Flight

### 5.1 Arm the Vehicle in QGC

1. Open QGroundControl
2. Connect to SAMV71 (should auto-detect)
3. Go to **Fly** view
4. Check pre-arm status (may need to bypass some checks if sensors aren't perfect)
5. Try to arm (may require force-arming if in HIL mode without full sensor suite)

### 5.2 Manual Flight Test

1. In QGC, switch to **Manual** mode
2. Use virtual joystick or RC transmitter to control
3. Motors in Gazebo should respond to stick inputs
4. Verify attitude control (pitch/roll/yaw)

---

## Troubleshooting

### Issue: QGC Can't Connect

**Check:**
```bash
# On SAMV71 NSH
nsh> mavlink status all
```

If empty output, MAVLink may not be fully initialized. Check boot log for errors.

**Try:**
```bash
nsh> mavlink stop
nsh> mavlink start -d /dev/ttyS0 -b 57600 -m onboard -r 4000
```

### Issue: SYS_HITL Won't Set

**Temporary workaround:**
1. Modify firmware to default `SYS_HITL=1`
2. Rebuild and upload
3. No need to set parameter manually

**Files to modify:**
- `src/modules/commander/Commander.cpp` - check for `SYS_HITL` usage
- Board-specific parameter defaults

### Issue: No Sensor Data in HIL

**Check Gazebo plugin:**
```bash
# In Gazebo, verify PX4 HIL plugin is loaded
gz plugin --list | grep -i mavlink
```

**Check MAVLink routing:**
```bash
# Verify mavlink-router is forwarding HIL_SENSOR messages
# Use Wireshark or tcpdump to inspect MAVLink packets
```

### Issue: Can't Arm in HIL Mode

**Bypass pre-arm checks:**
```bash
nsh> commander takeoff  # Force takeoff (may not work without arming)
nsh> param set COM_ARM_WO_GPS 1  # Allow arming without GPS
nsh> param set NAV_RCL_ACT 0     # Disable RC loss failsafe
```

Or force-arm in QGC by holding the arm button for 3 seconds.

---

## Expected Results

### Successful HIL Setup:

1. **QGC Connected:** Green "Connected" status, system ID visible
2. **Sensor Data Streaming:** `listener sensor_*` shows simulated data from Gazebo
3. **Attitude Estimation Working:** `listener vehicle_attitude` shows roll/pitch/yaw from Gazebo physics
4. **Motor Control:** Gazebo quadcopter responds to RC inputs via QGC
5. **EKF Healthy:** `commander status` shows EKF OK (may take time to converge)

### Known Limitations:

- Parameters don't persist across reboots (flash storage issue)
- May need to reconfigure HIL settings after each power cycle
- Some sensors may not be simulated (e.g., rangefinder, optical flow)

---

## Next Steps After HIL Validation

1. **Fix Parameter Storage:**
   - Debug flash parameter implementation (error 262144)
   - Add SD card for persistent storage
   - Use FRAM or EEPROM as alternative

2. **Add Real Sensors:**
   - Test with physical Click boards (ICM-20689, DPS310, AK09916)
   - Compare real sensor data vs. simulated HIL data
   - Validate sensor fusion (EKF2)

3. **Full Autopilot Testing:**
   - Test autonomous missions in Gazebo
   - Validate waypoint navigation
   - Test failsafe modes (GPS loss, battery low, etc.)

---

## Reference

- **PX4 HIL Documentation:** https://docs.px4.io/main/en/simulation/hitl.html
- **MAVLink Protocol:** https://mavlink.io/en/
- **Gazebo Integration:** https://docs.px4.io/main/en/simulation/gazebo.html
- **QGroundControl:** https://docs.qgroundcontrol.com/

---

## Quick Command Reference

```bash
# On SAMV71 NSH Console:
nsh> param set SYS_HITL 1         # Enable HIL mode (doesn't persist)
nsh> mavlink status all           # Check MAVLink connections
nsh> listener sensor_accel        # View accelerometer data
nsh> listener sensor_gyro         # View gyroscope data
nsh> listener vehicle_attitude    # View attitude estimate
nsh> commander status             # Check system health

# On PC:
mavproxy.py --master=/dev/ttyACM0 --baudrate=57600
mavlink-routerd -e 127.0.0.1:14550 -e /dev/ttyACM0:57600
gazebo Tools/simulation/gazebo/worlds/hitl_iris.world
```

---

**Last Updated:** 2025-11-16
**Status:** Parameter storage broken, but MAVLink and core modules running
**Next Test:** Verify QGroundControl connection to validate MAVLink before proceeding with Gazebo HIL
