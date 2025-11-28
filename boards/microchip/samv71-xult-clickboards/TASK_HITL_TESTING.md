# Engineering Task: HITL (Hardware-in-the-Loop) Testing

**Assigned To:** Engineer 3
**Priority:** MEDIUM
**Estimated Effort:** 2-3 days
**Prerequisites:** Linux development machine, QGroundControl, basic PX4 knowledge

---

## Executive Summary

Hardware-in-the-Loop (HITL) simulation allows testing the complete PX4 flight stack running on real SAMV71 hardware, with a simulator (jMAVSim or Gazebo) providing virtual sensor data and receiving actuator commands. This validates the firmware without requiring physical sensors or motors.

---

## What is HITL?

### Architecture
```
┌─────────────────┐     USB/UART      ┌─────────────────┐
│   SAMV71-XULT   │◄────────────────►│  Development PC  │
│   (Real HW)     │                   │                  │
│                 │                   │  ┌───────────┐   │
│  PX4 Firmware   │                   │  │  jMAVSim  │   │
│  - EKF2         │    MAVLink/UDP    │  │    or     │   │
│  - Commander    │◄────────────────►│  │  Gazebo   │   │
│  - Controllers  │                   │  └───────────┘   │
│  - pwm_out_sim  │                   │        │         │
└─────────────────┘                   │        ▼         │
                                      │  ┌───────────┐   │
                                      │  │    QGC    │   │
                                      │  └───────────┘   │
                                      └─────────────────┘
```

### How It Works
1. **Simulator** generates virtual sensor data (IMU, GPS, baro, mag)
2. **Sensor data** sent to SAMV71 via MAVLink HIL messages
3. **PX4 firmware** processes data as if from real sensors
4. **Actuator commands** sent back to simulator
5. **Simulator** updates vehicle physics model
6. **QGroundControl** monitors the system via UDP

---

## References

### Official Documentation
- [PX4 HITL Guide (main)](https://docs.px4.io/main/en/simulation/hitl)
- [PX4 HITL Guide (v1.16)](https://docs.px4.io/v1.16/en/simulation/hitl)

### Tutorials
- [RIIS: Hardware in the Loop Testing for PX4](https://www.riis.com/blog/harware-in-the-loop-testing-for-px4)
- [MathWorks: PX4 HITL System Architecture](https://www.mathworks.com/help/uav/px4/ug/px4-hitl-system-architecture.html)

---

## Prerequisites

### Software Requirements

| Software | Version | Purpose |
|----------|---------|---------|
| Ubuntu | 20.04/22.04 | Development OS |
| QGroundControl | Latest | Ground station |
| Java | JDK 11+ | Required for jMAVSim |
| jMAVSim | Latest | Simulator (simpler) |
| Gazebo Classic | 11.x | Simulator (advanced) |

### Hardware Requirements
- SAMV71-XULT board with current firmware
- USB cable (data, not charge-only)
- SD card (optional, logger disabled anyway)

---

## Setup Instructions

### Step 1: Verify SAMV71 Firmware

#### 1.1 Check pwm_out_sim Module
```bash
# Connect via USB and open serial console
nsh> pwm_out_sim -h
```

**Expected:** Help text showing available options
**If "command not found":** Module missing, verify `CONFIG_MODULES_SIMULATION_PWM_OUT_SIM=y` in px4board

#### 1.2 Verify Current Configuration
```bash
nsh> param show SYS_AUTOSTART
# Should be 4001 (Quadcopter x) or similar

nsh> param show SYS_HITL
# 0 = disabled, 1 = enabled
```

### Step 2: Configure PX4 Parameters

#### 2.1 Via NSH Console
```bash
# Enable HITL mode
nsh> param set SYS_HITL 1

# Use HIL Quadcopter airframe
nsh> param set SYS_AUTOSTART 1001

# Disable RC failsafe (using joystick/simulator)
nsh> param set COM_RC_IN_MODE 1
nsh> param set NAV_RCL_ACT 0

# Save and reboot
nsh> param save
nsh> reboot
```

#### 2.2 Via QGroundControl
1. Connect SAMV71-XULT via USB
2. Open QGroundControl
3. Go to **Vehicle Setup** → **Safety**
4. Set **HITL Enabled** to "Enabled"
5. Go to **Vehicle Setup** → **Airframe**
6. Select **HIL Quadcopter X**
7. Apply and Reboot

### Step 3: Install jMAVSim

#### 3.1 Install Java
```bash
sudo apt update
sudo apt install default-jdk ant
java -version  # Should show JDK 11+
```

#### 3.2 Build jMAVSim
```bash
cd /media/bhanu1234/Development/PX4-Autopilot-Private

# Build jMAVSim tools
make px4_sitl jmavsim

# This builds jMAVSim in Tools/simulation/jmavsim/
```

### Step 4: Configure QGroundControl

#### 4.1 Disable Auto-Connect
1. Open QGroundControl
2. Go to **Settings** (gear icon) → **Comm Links**
3. **Uncheck** all auto-connect options EXCEPT **UDP**
4. This prevents QGC from connecting directly to USB

#### 4.2 Verify UDP Settings
- Default UDP port: 14550
- jMAVSim bridges MAVLink between SAMV71 and QGC

---

## Running HITL Simulation

### Method 1: jMAVSim (Recommended for Initial Testing)

#### 1.1 Find Serial Port
```bash
# On Linux, find the SAMV71 USB port
dmesg | grep tty
# Look for: ttyACM0 or similar

ls -l /dev/ttyACM*
# Should show /dev/ttyACM0
```

#### 1.2 Launch jMAVSim
```bash
cd /media/bhanu1234/Development/PX4-Autopilot-Private

# Launch jMAVSim connected to SAMV71
./Tools/simulation/jmavsim/jmavsim_run.sh -q -d /dev/ttyACM0 -b 921600 -r 250
```

**Parameters:**
- `-q` : Quiet mode
- `-d /dev/ttyACM0` : Serial port to SAMV71
- `-b 921600` : Baud rate
- `-r 250` : Sensor update rate (Hz)

#### 1.3 Start QGroundControl
```bash
# In a separate terminal
./QGroundControl.AppImage
```

QGC should auto-connect via UDP and show the simulated vehicle.

#### 1.4 Verify Connection
In jMAVSim window:
- Vehicle should be visible (quadcopter model)
- Attitude should respond to SAMV71 commands

In QGroundControl:
- Vehicle connected (green)
- Telemetry updating
- Map showing vehicle position

### Method 2: Gazebo Classic (Advanced)

#### 2.1 Install Gazebo
```bash
sudo apt install gazebo11 libgazebo11-dev
```

#### 2.2 Configure Gazebo Model
Edit the HITL model configuration:
```bash
code Tools/simulation/gazebo-classic/sitl_gazebo-classic/models/iris_hitl/iris_hitl.sdf
```

Find and update the serial device:
```xml
<plugin name='mavlink_interface' filename='libgazebo_mavlink_interface.so'>
  <serialDevice>/dev/ttyACM0</serialDevice>
  <baudRate>921600</baudRate>
  ...
</plugin>
```

#### 2.3 Launch Gazebo HITL
```bash
cd /media/bhanu1234/Development/PX4-Autopilot-Private

# Source Gazebo setup
source Tools/simulation/gazebo-classic/setup_gazebo.bash

# Launch Gazebo with HITL model
gazebo Tools/simulation/gazebo-classic/worlds/hitl_iris.world
```

---

## Test Procedures

### Test 1: Basic Connection

#### Objective
Verify SAMV71 communicates with simulator and QGC.

#### Steps
1. Start jMAVSim with SAMV71 connected
2. Start QGroundControl
3. Verify connection established

#### Expected Results
- [ ] jMAVSim shows vehicle model
- [ ] QGC shows connected vehicle
- [ ] Telemetry (attitude, position) updating
- [ ] No MAVLink errors in console

---

### Test 2: Sensor Data Flow

#### Objective
Verify simulator sensor data reaches PX4.

#### Steps
```bash
# On SAMV71 console (via separate USB-UART or before starting sim)
nsh> listener sensor_accel
nsh> listener sensor_gyro
nsh> listener sensor_baro
nsh> listener sensor_mag
```

#### Expected Results
- [ ] Sensor topics show simulated data
- [ ] Update rates reasonable (100+ Hz for IMU)
- [ ] Values change when simulator vehicle moves

---

### Test 3: EKF2 Operation

#### Objective
Verify state estimation with simulated sensors.

#### Steps
```bash
nsh> ekf2 status
nsh> listener vehicle_attitude
nsh> listener vehicle_local_position
nsh> listener vehicle_global_position
```

#### Expected Results
- [ ] EKF2 running without errors
- [ ] Attitude estimates valid
- [ ] Position estimates valid
- [ ] GPS fusion working (if simulated)

---

### Test 4: Arming Test

#### Objective
Verify vehicle can arm in HITL mode.

#### Steps
1. In QGroundControl, click **Arm** button
2. Or use joystick arm gesture
3. Or via console: `nsh> commander arm`

#### Expected Results
- [ ] Vehicle arms successfully
- [ ] Motors show armed (in simulator)
- [ ] No preflight failures blocking arm

#### Troubleshooting Arming
```bash
# Check why arming fails
nsh> commander status
nsh> commander check

# Common fixes
nsh> param set CBRK_SUPPLY_CHK 894281  # Disable power check
nsh> param set COM_ARM_WO_GPS 1        # Allow arm without GPS
```

---

### Test 5: Takeoff and Hover

#### Objective
Verify basic flight capability.

#### Steps
1. Arm vehicle
2. In QGroundControl: **Takeoff** button
3. Or: Use joystick to increase throttle
4. Observe vehicle in simulator

#### Expected Results
- [ ] Vehicle takes off smoothly
- [ ] Hovers at target altitude
- [ ] Attitude stable (no oscillations)
- [ ] Position hold working

---

### Test 6: Mission Flight

#### Objective
Verify autonomous navigation.

#### Steps
1. In QGroundControl, create simple mission:
   - Takeoff to 10m
   - Fly to waypoint 50m away
   - Return to launch
   - Land
2. Upload mission
3. Start mission

#### Expected Results
- [ ] Mission uploads successfully
- [ ] Vehicle follows waypoints
- [ ] RTL works correctly
- [ ] Landing smooth

---

### Test 7: Failsafe Testing

#### Objective
Verify failsafe behavior.

#### Steps
1. During flight, simulate RC loss
2. Simulate GPS loss
3. Simulate low battery

#### Expected Results
- [ ] RC loss triggers RTL (or configured action)
- [ ] GPS loss handled gracefully
- [ ] Low battery warning shown

---

## Troubleshooting

### Connection Issues

| Symptom | Cause | Solution |
|---------|-------|----------|
| "No device" | Wrong port | Check `dmesg | grep tty` |
| "Permission denied" | No serial access | `sudo usermod -a -G dialout $USER` |
| jMAVSim no response | Wrong baud rate | Try 57600, 115200, 921600 |
| QGC won't connect | Auto-connect issue | Disable USB auto-connect |

### HITL-Specific Issues

| Symptom | Cause | Solution |
|---------|-------|----------|
| No sensor data | SYS_HITL not set | `param set SYS_HITL 1` |
| Can't arm | Preflight failures | Check `commander status` |
| Attitude wrong | Sensor mismatch | Verify HIL airframe |
| Motors don't spin | pwm_out_sim issue | Verify module running |

### Performance Issues

| Symptom | Cause | Solution |
|---------|-------|----------|
| Laggy response | USB latency | Use higher baud rate |
| Jittery attitude | Update rate low | Increase `-r` parameter |
| Simulator slow | PC performance | Close other applications |

---

## Test Results Template

```
=============================================================
SAMV71 HITL Test Report
=============================================================
Date: _______________
Tester: _______________
Firmware: _______________
Simulator: jMAVSim / Gazebo

CONFIGURATION
-------------
SYS_HITL: ___
SYS_AUTOSTART: ___
Serial Port: ___
Baud Rate: ___

TEST RESULTS
------------
┌─────────────────────┬────────┬─────────────────────────┐
│ Test                │ Result │ Notes                   │
├─────────────────────┼────────┼─────────────────────────┤
│ Basic Connection    │ P / F  │                         │
│ Sensor Data Flow    │ P / F  │                         │
│ EKF2 Operation      │ P / F  │                         │
│ Arming              │ P / F  │                         │
│ Takeoff/Hover       │ P / F  │                         │
│ Mission Flight      │ P / F  │                         │
│ Failsafe Testing    │ P / F  │                         │
└─────────────────────┴────────┴─────────────────────────┘

PERFORMANCE METRICS
-------------------
MAVLink latency: ___ ms
Sensor update rate: ___ Hz
Control loop rate: ___ Hz

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
1. **Test report** - Completed template
2. **Configuration guide** - Exact steps that worked
3. **Issue list** - Any problems encountered
4. **Screenshots/Video** - QGC and simulator running

### Optional
1. **Performance analysis** - Latency measurements
2. **Gazebo testing** - If jMAVSim works, try Gazebo
3. **Advanced scenarios** - Failsafe, missions, etc.

---

## Success Criteria

- [ ] jMAVSim connects to SAMV71 successfully
- [ ] QGroundControl shows telemetry
- [ ] Vehicle can arm in HITL mode
- [ ] Basic hover flight stable
- [ ] 10-minute flight without crashes
- [ ] Mission execution works

---

## Known Limitations

### Logger Disabled
**WARNING:** Flight logs cannot be recorded due to SD card write issues. Use MAVLink log streaming if logging is needed:
```bash
# In QGC: Enable MAVLink logging
# Or: Use mavlink_shell to stream logs
```

### No Real Sensors
In HITL mode, real sensors are disabled. The simulator provides all sensor data. This is by design.

### Community Supported
Per PX4 documentation: "HITL is community supported and maintained. It may or may not work with current versions of PX4."

---

## Advanced Topics

### Using Joystick
```bash
# Configure joystick in QGC
# Or set parameters:
nsh> param set COM_RC_IN_MODE 1   # Joystick mode
nsh> param set NAV_RCL_ACT 0      # Disable RC failsafe
```

### Custom Airframes
If using custom vehicle configuration, create a custom HITL airframe based on existing HIL airframes.

### Multiple Vehicles
Gazebo supports multiple vehicles in HITL. Requires separate serial connections and vehicle IDs.

---

## References

- `SAMV71_IMPLEMENTATION_TRACKER.md` - Project status
- `default.px4board` - Shows pwm_out_sim enabled
- `rc.board_defaults` - Current parameter defaults
- [PX4 HITL Documentation](https://docs.px4.io/main/en/simulation/hitl)

---

**Document Version:** 1.0
**Created:** November 28, 2025
**Status:** Ready for Assignment
