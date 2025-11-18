# Hardware-in-the-Loop (HIL) Testing Guide for SAMV71-XULT-Clickboards

## Overview

This guide explains how to perform Hardware-in-the-Loop simulation testing with the Microchip SAMV71-XULT-Clickboards running PX4 Autopilot firmware. HIL allows you to test the real flight controller hardware with simulated sensors and physics from Gazebo, validating control algorithms without physical flight risks.

## What is HIL?

Hardware-in-the-Loop simulation runs:
- **Real PX4 firmware** on the SAMV71 board
- **Real control algorithms** (EKF2, attitude control, position control)
- **Simulated sensors** from Gazebo (IMU, GPS, barometer, magnetometer)
- **Simulated physics** in Gazebo (vehicle dynamics, aerodynamics)

The SAMV71 board receives simulated sensor data via MAVLink and sends back actuator commands, which Gazebo uses to update the simulated vehicle.

## Architecture

```
┌─────────────────────────────────────┐         ┌──────────────────────────────┐
│  PC Running Gazebo                  │         │  SAMV71 Board                │
│                                     │         │                              │
│  ┌───────────────────────────┐     │         │  ┌────────────────────┐      │
│  │  Gazebo Classic           │     │         │  │  PX4 Firmware      │      │
│  │  - Physics simulation     │     │         │  │  - Real HRT        │      │
│  │  - Iris quadcopter model  │     │         │  │  - Real EKF2       │      │
│  │  - Sensor simulation      │     │  USB    │  │  - Real Controllers│      │
│  │  - Environment            │◄────┼─────────┼──┤  - MAVLink         │      │
│  └───────────────────────────┘     │         │  └────────────────────┘      │
│              │                      │         │           │                  │
│              ▼                      │         │           │                  │
│  ┌───────────────────────────┐     │         │  Physical sensor drivers     │
│  │  QGroundControl           │     │         │  bypassed in HIL mode        │
│  │  - Ground station         │◄────┼─────────┼──► MAVLink over USB         │
│  │  - Parameter config       │     │         │                              │
│  │  - Mission planning       │     │         │                              │
│  └───────────────────────────┘     │         │                              │
└─────────────────────────────────────┘         └──────────────────────────────┘

Data Flow:
1. Gazebo → SAMV71: HIL_SENSOR, HIL_GPS (simulated sensor data)
2. SAMV71 → Gazebo: HIL_ACTUATOR_CONTROLS (motor commands)
3. QGC ↔ SAMV71: Parameters, commands, telemetry
```

## Prerequisites

### Hardware Requirements

- **SAMV71-XULT development board** with PX4 firmware flashed
- **USB cable** (micro-USB to PC)
- **PC running Linux** (Ubuntu 20.04/22.04 recommended)

### Software Requirements

**On your development PC**:

1. **PX4 SITL (Software-in-the-Loop)** for Gazebo simulation
2. **Gazebo Classic** (version 11)
3. **QGroundControl** (latest stable)
4. **MAVLink router** (optional, for multiple connections)

### SAMV71 Firmware Requirements

Your firmware must have:
- ✅ HRT (High Resolution Timer) - **REQUIRED**
- ✅ MAVLink module - **REQUIRED**
- ✅ USB CDC/ACM - **REQUIRED**
- ✅ EKF2 - **REQUIRED**
- ✅ MC control modules (mc_att_control, mc_pos_control, mc_rate_control) - **REQUIRED**
- ✅ Commander module - **REQUIRED**
- ✅ Sensors module - **REQUIRED**
- ⚠️ Physical sensor drivers (SPI, I2C sensors) - **BYPASSED in HIL mode**

**Your current firmware meets all requirements!**

## Installation

### 1. Install PX4 SITL Environment

```bash
# Clone PX4 repository (if not already done)
cd ~/
git clone https://github.com/PX4/PX4-Autopilot.git --recursive
cd PX4-Autopilot

# Install dependencies
bash ./Tools/setup/ubuntu.sh

# Build SITL with Gazebo (this also installs Gazebo)
make px4_sitl gazebo-classic
```

### 2. Install QGroundControl

```bash
# Download latest AppImage
cd ~/Downloads
wget https://d176tv9ibo4jno.cloudfront.net/latest/QGroundControl.AppImage

# Make executable
chmod +x QGroundControl.AppImage

# Run
./QGroundControl.AppImage
```

### 3. Verify Gazebo Installation

```bash
# Check Gazebo version (should be 11.x)
gazebo --version

# Test Gazebo
gazebo --verbose
# Should open Gazebo window - close it after verification
```

## SAMV71 Board Setup

### 1. Flash Firmware

Ensure your SAMV71 board has the latest firmware with HRT fixes:

```bash
cd /media/bhanu1234/Development/PX4-Autopilot-Private

# Build firmware
make microchip_samv71-xult-clickboards_default

# Flash to board
make microchip_samv71-xult-clickboards_default upload
```

### 2. Connect Board to PC

1. Connect SAMV71 board to PC via USB (use the micro-USB port labeled "USB Device")
2. Board should enumerate as `/dev/ttyACM0` (or similar)
3. Verify connection:

```bash
# Check USB device
lsusb | grep -i "microchip\|atmel"

# Check serial port
ls -l /dev/ttyACM*

# Check permissions (add user to dialout group if needed)
sudo usermod -a -G dialout $USER
# Log out and back in for group change to take effect
```

### 3. Connect to NSH Console

Use your preferred serial terminal:

```bash
# Using screen
screen /dev/ttyACM0 57600

# Using minicom
minicom -D /dev/ttyACM0 -b 57600

# Using pyserial-miniterm
python3 -m serial.tools.miniterm /dev/ttyACM0 57600
```

You should see the `nsh>` prompt after pressing Enter.

## HIL Configuration

### Step 1: Enable HIL Mode on SAMV71

In the NSH console:

```bash
nsh> param set SYS_HITL 1
nsh> param save
nsh> reboot
```

After reboot, verify HIL is enabled:

```bash
nsh> param show SYS_HITL
SYS_HITL [1/1]: 1
```

### Step 2: Configure MAVLink

Start MAVLink on USB with high baud rate for low latency:

```bash
nsh> mavlink start -d /dev/ttyACM0 -b 921600 -m onboard -r 4000000
```

Parameters explained:
- `-d /dev/ttyACM0` - USB serial device
- `-b 921600` - Baud rate (921600 for USB is good)
- `-m onboard` - Onboard mode (high-rate telemetry)
- `-r 4000000` - Max data rate in bytes/sec

Verify MAVLink is running:

```bash
nsh> mavlink status
instance #0:
        GCS heartbeat:  0 (0 Hz)
        mavlink chan: #0
        type:           USB serial
        flow control:   AUTO
        tx: 0.000 kB/s
        rx: 0.000 kB/s
        mode:           Onboard
```

### Step 3: Set Vehicle Type

Configure for multicopter:

```bash
nsh> param set MAV_TYPE 2        # Quadrotor
nsh> param set SYS_AUTOSTART 4001  # Generic Quadcopter
nsh> param save
```

### Step 4: Disconnect Serial Console

**Important**: Close your serial terminal (screen/minicom) to free the USB port for QGroundControl and Gazebo.

```bash
# In screen: Ctrl-A then K then Y
# In minicom: Ctrl-A then X
```

## Gazebo Simulation Setup

### Option A: Using PX4 SITL Gazebo (Recommended)

This method uses PX4's built-in Gazebo integration with HIL adapter.

1. **Start Gazebo with Iris model**:

```bash
cd ~/PX4-Autopilot

# Start Gazebo without SITL (we're using real hardware)
make px4_sitl gazebo-classic_iris__hitl

# Alternative: Start Gazebo manually
gazebo ~/PX4-Autopilot/Tools/simulation/gazebo-classic/worlds/iris_hitl.world
```

2. **Verify Gazebo is running**:
   - Gazebo window should open
   - You should see an Iris quadcopter model
   - Check terminal for MAVLink connection attempts

### Option B: Using Standalone Gazebo

If you want more control over the simulation:

1. **Start Gazebo**:

```bash
gazebo --verbose
```

2. **Insert Iris model**:
   - In Gazebo, click "Insert" tab
   - Select "Iris" quadcopter
   - Click in the world to place it

3. **Configure MAVLink plugin** (model must have mavlink_interface plugin):

Check that the Iris model SDF includes:

```xml
<plugin name='mavlink_interface' filename='libgazebo_mavlink_interface.so'>
  <robotNamespace></robotNamespace>
  <imuSubTopic>/imu</imuSubTopic>
  <motorSpeedCommandPubTopic>/gazebo/command/motor_speed</motorSpeedCommandPubTopic>
  <mavlink_addr>INADDR_ANY</mavlink_addr>
  <mavlink_udp_port>14560</mavlink_udp_port>
  <serialEnabled>1</serialEnabled>
  <serialDevice>/dev/ttyACM0</serialDevice>
  <baudRate>921600</baudRate>
  <hil_mode>1</hil_mode>
</plugin>
```

## QGroundControl Setup

### 1. Start QGroundControl

```bash
cd ~/Downloads
./QGroundControl.AppImage
```

### 2. Configure Connection

QGroundControl should **automatically detect** the SAMV71 board on `/dev/ttyACM0`.

If not detected:
1. Go to **Application Settings → Comm Links**
2. Add new link:
   - **Name**: SAMV71 HIL
   - **Type**: Serial
   - **Serial Port**: `/dev/ttyACM0`
   - **Baud Rate**: 921600
3. Click **OK** and **Connect**

### 3. Verify Connection

Check QGC toolbar:
- Vehicle should show as **Connected**
- GPS icon should show **No GPS** (expected before Gazebo sends data)
- Battery icon should show battery status

### 4. Configure HIL Parameters

In QGC, go to **Vehicle Setup → Parameters**:

**Critical HIL Parameters**:
```
SYS_HITL          = 1     # Enable HIL mode
MAV_TYPE          = 2     # Quadrotor
SYS_AUTOSTART     = 4001  # Generic Quadcopter
SYS_AUTOCONFIG    = 1     # Auto-apply defaults

# Sensor Configuration (HIL will override these)
SENS_EN_BARO      = 1     # Enable barometer
SENS_EN_GPSSIM    = 0     # Disable GPS sim (use HIL_GPS)

# EKF2 Configuration
EKF2_ENABLED      = 1     # Enable EKF2
EKF2_HGT_REF      = 1     # Barometric altitude reference
EKF2_GPS_CHECK    = 21    # Less strict GPS checks for HIL
```

Click **Save** after changing parameters.

## Running HIL Simulation

### Full Startup Sequence

**1. Power on SAMV71 board** (USB connected to PC)

**2. Wait for boot** (check with `dmesg` on PC):
```bash
dmesg | tail
# Should show: usb 1-x: new full-speed USB device
```

**3. Start Gazebo**:
```bash
cd ~/PX4-Autopilot
make px4_sitl gazebo-classic_iris__hitl
```

**4. Start QGroundControl**:
```bash
~/Downloads/QGroundControl.AppImage
```

**5. Wait for connection**:
   - QGC should show vehicle connected
   - Gazebo should show MAVLink heartbeat in terminal
   - SAMV71 should be receiving HIL_SENSOR messages

**6. Verify sensor data flow**:

Open QGC → **Analyze Tools → MAVLink Inspector**:

Check for these messages:
- `HEARTBEAT` - From SAMV71 (every 1 Hz)
- `HIL_SENSOR` - From Gazebo to SAMV71 (high rate)
- `HIL_GPS` - From Gazebo to SAMV71 (1-5 Hz)
- `HIL_ACTUATOR_CONTROLS` - From SAMV71 to Gazebo (high rate)
- `ATTITUDE` - From SAMV71 (EKF2 estimate)
- `LOCAL_POSITION_NED` - From SAMV71 (EKF2 position)

### Arming and Testing

**1. Check Preflight Status**:

In QGC main screen, check for preflight errors. Common issues:
- **GPS not found** - Wait for Gazebo to send HIL_GPS
- **EKF not initialized** - Wait 10-30 seconds for EKF to converge
- **Mode not set** - Switch to a valid mode

**2. Set Flight Mode**:

In QGC, switch to **Stabilized** or **Altitude** mode.

**3. Arm the Vehicle**:

Click **Disarmed** button in QGC (or use safety switch if enabled).

**4. Test Throttle**:

In QGC, use the virtual joystick or RC transmitter to:
- Increase throttle slowly
- Vehicle should take off in Gazebo
- Verify SAMV71 is sending motor commands
- Check attitude control response

**5. Test Position Control**:

Switch to **Position** mode:
- Vehicle should hold position
- Try moving with sticks
- Check position hold performance

### Monitoring HIL Performance

**Check Control Loop Timing** (via NSH):

```bash
# In another terminal, connect to NSH
screen /dev/ttyACM0 57600

nsh> top
# Check that mc_att_control, mc_pos_control are running at expected rates

nsh> listener vehicle_attitude 5
# Should show stable attitude estimates

nsh> listener vehicle_local_position 5
# Should show position tracking
```

**Check Gazebo Performance**:

In Gazebo, check real-time factor:
- Should be close to 1.0x (real-time)
- If < 0.5x, simulation is too slow (reduce sensor rates)

**Check MAVLink Statistics**:

```bash
nsh> mavlink status
# Look for:
# - tx/rx rates
# - No dropped messages
# - Low latency
```

## Troubleshooting

### Issue: QGC doesn't detect SAMV71

**Symptoms**: QGC shows "Waiting for vehicle..."

**Solutions**:
1. Check USB connection: `ls -l /dev/ttyACM*`
2. Check permissions: `sudo chmod 666 /dev/ttyACM0` (temporary fix)
3. Verify MAVLink running on SAMV71:
   ```bash
   screen /dev/ttyACM0 57600
   nsh> mavlink status
   ```
4. Restart MAVLink:
   ```bash
   nsh> mavlink stop
   nsh> mavlink start -d /dev/ttyACM0 -b 921600
   ```

### Issue: No sensor data in HIL

**Symptoms**: GPS icon shows "No GPS", EKF not initializing

**Solutions**:
1. Verify `SYS_HITL = 1` in parameters
2. Check Gazebo is sending HIL messages:
   - In Gazebo terminal, look for "Sending HIL_SENSOR"
3. Check MAVLink Inspector in QGC for `HIL_SENSOR` and `HIL_GPS` messages
4. Restart Gazebo simulation

### Issue: EKF won't initialize

**Symptoms**: "EKF internal checks" preflight failure

**Solutions**:
1. Wait 30 seconds for convergence
2. Check barometer is working: `nsh> listener sensor_baro`
3. Reduce GPS accuracy requirements:
   ```bash
   nsh> param set EKF2_GPS_CHECK 21
   nsh> param save
   ```
4. Check EKF status: `nsh> commander status`

### Issue: Vehicle won't arm

**Symptoms**: Arm button disabled, preflight checks fail

**Solutions**:
1. Check preflight errors in QGC
2. Bypass RC checks if no RC:
   ```bash
   nsh> param set COM_RC_IN_MODE 1  # RC optional
   nsh> param set NAV_RCL_ACT 0     # No action on RC loss
   ```
3. Check safety switch: `nsh> param set CBRK_IO_SAFETY 22027`
4. Check mode: Switch to Stabilized mode first

### Issue: Poor control performance

**Symptoms**: Oscillations, unstable flight, crashes in simulation

**Solutions**:
1. Check HRT is working properly:
   ```bash
   nsh> listener sensor_accel
   # Should show high-rate data (~100 Hz or more)
   ```
2. Reduce control gains (if very oscillatory):
   ```bash
   nsh> param set MC_ROLL_P 5.0    # Reduce from default 6.5
   nsh> param set MC_PITCH_P 5.0   # Reduce from default 6.5
   ```
3. Check CPU load: `nsh> top` - Should have idle time
4. Check for HRT timing issues: `nsh> hrt_absolute_time` (if available)

### Issue: Gazebo not sending data

**Symptoms**: No HIL messages in MAVLink Inspector

**Solutions**:
1. Check Gazebo MAVLink plugin is loaded:
   - In Gazebo terminal, look for "MAVLink interface initialized"
2. Verify serial port in Gazebo config matches SAMV71:
   - Should be `/dev/ttyACM0` with baud 921600
3. Check no other program is using `/dev/ttyACM0`:
   ```bash
   lsof | grep ttyACM0
   ```
4. Restart Gazebo simulation

### Issue: High latency / slow response

**Symptoms**: Commands delayed, sluggish response

**Solutions**:
1. Increase MAVLink data rate:
   ```bash
   nsh> mavlink stop
   nsh> mavlink start -d /dev/ttyACM0 -b 921600 -r 8000000
   ```
2. Reduce Gazebo sensor update rates (edit model SDF)
3. Check USB cable quality (use short, high-quality cable)
4. Close unnecessary programs on PC

## Advanced Topics

### Using MAVLink Router for Multiple Connections

If you want both QGC and Gazebo to connect simultaneously:

```bash
# Install MAVLink Router
sudo apt install mavlink-router

# Create config file
cat > /tmp/mavlink-router.conf << EOF
[General]
TcpServerPort=5760

[UdpEndpoint QGC]
Mode = Normal
Address = 127.0.0.1
Port = 14550

[UdpEndpoint Gazebo]
Mode = Normal
Address = 127.0.0.1
Port = 14560

[UartEndpoint SAMV71]
Device = /dev/ttyACM0
Baud = 921600
EOF

# Run router
mavlink-routerd -c /tmp/mavlink-router.conf
```

### Custom Gazebo Worlds

Create custom environments:

```bash
cd ~/PX4-Autopilot/Tools/simulation/gazebo-classic/worlds

# Copy existing world
cp iris_hitl.world custom_hil.world

# Edit with Gazebo:
gazebo custom_hil.world

# Add obstacles, lighting, etc.
```

### Recording HIL Sessions

Use `rosbag` (if ROS is installed) or PX4 logger:

```bash
# On SAMV71
nsh> logger on
# Fly in HIL
nsh> logger off

# Download log
# Use QGC: Analyze Tools → Log Download
```

### Testing Different Vehicle Types

Change vehicle type for fixed-wing HIL:

```bash
nsh> param set MAV_TYPE 1        # Fixed wing
nsh> param set SYS_AUTOSTART 4003  # Standard plane
nsh> param save
nsh> reboot

# In Gazebo, load plane model instead of iris
```

## Performance Benchmarks

Expected performance with SAMV71:

| Metric | Target | Notes |
|--------|--------|-------|
| Control Loop Rate | 250-500 Hz | mc_rate_control |
| Attitude Loop Rate | 100-250 Hz | mc_att_control |
| Position Loop Rate | 50-100 Hz | mc_pos_control |
| EKF Update Rate | 100-200 Hz | EKF2 fusion |
| MAVLink Latency | < 10 ms | USB connection |
| CPU Usage | < 80% | Check with `top` |
| HRT Accuracy | ±1 µs | Microsecond timing |

Monitor these with `top` and `listener` commands in NSH.

## Known Limitations

### SAMV71-Specific

1. **No PWM output in HIL** - Actuator commands sent via MAVLink only (no physical PWM)
2. **Limited flash** - Constrained flash build may limit logging
3. **No console in HIL** - USB occupied by MAVLink (use separate UART for debug if needed)

### General HIL Limitations

1. **Sensor noise models** - May not match real-world sensors exactly
2. **Aerodynamics** - Simplified compared to real flight
3. **Latency** - USB/MAVLink adds latency vs real sensor I2C/SPI
4. **No vibration** - Real motors cause significant vibration

## Next Steps After HIL Validation

Once HIL testing is successful:

1. **Implement SPI drivers** - For real ICM-20689 IMU
2. **Implement PWM output** - For real motor control via TC timers
3. **Test with real sensors** - Compare against HIL performance
4. **Bench test with motors** - Verify PWM output drives ESCs
5. **Tethered flight test** - Controlled first flight
6. **Full flight testing** - Complete validation

## References

- **PX4 HIL Documentation**: https://docs.px4.io/main/en/simulation/hitl.html
- **Gazebo Classic**: http://gazebosim.org/tutorials?cat=guided_b&tut=guided_b1
- **MAVLink Protocol**: https://mavlink.io/en/
- **QGroundControl**: http://qgroundcontrol.com/

## Troubleshooting Checklist

Before reporting issues, verify:

- [ ] HRT initialized successfully (check boot log)
- [ ] USB enumerated as `/dev/ttyACM0`
- [ ] MAVLink running on SAMV71 (`mavlink status`)
- [ ] `SYS_HITL = 1` parameter set
- [ ] Gazebo sending HIL messages (check terminal)
- [ ] QGC connected to vehicle
- [ ] No USB permission issues (`dialout` group)
- [ ] No other programs using `/dev/ttyACM0`
- [ ] Sufficient CPU headroom (`top` shows idle time)
- [ ] EKF initialized (wait 30+ seconds)

## Support

For issues specific to SAMV71 HIL:
1. Check boot logs for HRT errors
2. Verify MAVLink connection with `mavlink status`
3. Test basic MAVLink with `mavproxy.py --master=/dev/ttyACM0 --baudrate=921600`
4. Check GitHub issues: https://github.com/PX4/PX4-Autopilot/issues

---

**Document Version**: 1.0
**Last Updated**: 2025-11-16
**Target Platform**: SAMV71-XULT-Clickboards with PX4 Autopilot
**Prerequisites**: HRT implementation completed and verified
**Status**: Ready for HIL testing after driver implementation complete
