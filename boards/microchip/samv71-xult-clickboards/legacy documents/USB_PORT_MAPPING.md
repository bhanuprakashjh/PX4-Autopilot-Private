# USB Port Mapping for SAMV71-XULT

**Hardware:** SAMV71-XULT has TWO USB ports
**Date:** November 24, 2025

---

## Hardware Configuration

### Physical USB Ports

The SAMV71-XULT board has **two separate USB ports**:

#### 1. Debug/Programmer USB (J20)
```
Connector: J20 - "DEBUG USB"
Chip: EDBG (Embedded Debugger)
Function: Programming + Serial Console
Appears as: /dev/ttyACM0 (Linux) or COMx (Windows)
Purpose: NSH console, debugging, firmware upload
```

#### 2. Target USB (J19)
```
Connector: J19 - "TARGET USB"
Chip: SAMV71 native USB
Function: MAVLink communication
Appears as: /dev/ttyACM1 (Linux) or COMy (Windows)
Purpose: QGroundControl connection
```

---

## Linux Device Mapping

When both USB cables are connected:

```bash
$ ls -l /dev/ttyACM*
crw-rw----+ 1 root plugdev 166, 0 Nov 24 12:27 /dev/ttyACM0
crw-rw----+ 1 root dialout 166, 1 Nov 24 12:26 /dev/ttyACM1
```

**Port Assignment:**
```
/dev/ttyACM0 → Programmer USB (J20) → NSH console
/dev/ttyACM1 → Target USB (J19)     → MAVLink for QGC
```

---

## PX4 Configuration

### Updated Port Mapping

**File:** `boards/microchip/samv71-xult-clickboards/default.px4board`

```cmake
CONFIG_BOARD_SERIAL_GPS1="/dev/ttyS2"     # GPS UART
CONFIG_BOARD_SERIAL_TEL1="/dev/ttyACM1"   # ← MAVLink on Target USB
CONFIG_BOARD_SERIAL_TEL2="/dev/ttyS1"     # UART telemetry
CONFIG_BOARD_SERIAL_RC="/dev/ttyS4"       # RC input
```

**File:** `boards/microchip/samv71-xult-clickboards/init/rc.board_defaults`

```bash
param set-default SYS_USB_AUTO 1        # Enable USB auto-start
param set-default MAV_0_CONFIG 101      # Use TEL1 port
param set-default MAV_0_RATE 57600      # Display baud rate
```

**Result:**
- MAV_0_CONFIG 101 → TEL1 → /dev/ttyACM1 (Target USB)
- MAVLink starts automatically on target USB at boot
- QGC connects to /dev/ttyACM1 (or COMy on Windows)

---

## Why This Configuration?

### Previous (Wrong) Configuration:

```
CONFIG_BOARD_SERIAL_TEL1="/dev/ttyACM0"  ← Wrong!

Problem:
- MAVLink tried to start on /dev/ttyACM0
- But /dev/ttyACM0 is the programmer USB
- Not connected to target USB where QGC looks
- QGC submenu showed connection, main window disconnected
```

### Current (Correct) Configuration:

```
CONFIG_BOARD_SERIAL_TEL1="/dev/ttyACM1"  ← Correct!

Result:
- MAVLink starts on /dev/ttyACM1
- /dev/ttyACM1 is the target USB
- QGC connects successfully
- Console remains on /dev/ttyACM0 (no conflict)
```

---

## Physical Connection Guide

### For Development (Both USBs connected):

```
[PC] ←─── USB cable ───→ [J20 Debug USB]   → /dev/ttyACM0 → NSH console
                          [SAMV71-XULT]
[PC] ←─── USB cable ───→ [J19 Target USB]  → /dev/ttyACM1 → QGroundControl
```

**Two USB cables required:**
1. Debug USB (J20) for terminal/debugging
2. Target USB (J19) for QGroundControl

### For Flight (Single USB):

```
[GCS PC] ←─── USB cable ───→ [J19 Target USB]  → QGroundControl
                              [SAMV71-XULT]
                              [J20 Debug USB] - Not connected
```

**Only target USB needed for QGC:**
- Connect J19 (Target USB) to ground station PC
- J20 (Debug USB) can remain disconnected

---

## Software Access

### Opening Console (NSH):

```bash
# Linux - Programmer USB (J20)
screen /dev/ttyACM0 115200
# or
minicom -D /dev/ttyACM0 -b 115200

# Windows - Programmer USB
PuTTY: COMx at 115200 baud
```

### Connecting QGroundControl:

**Linux:**
```
Port: /dev/ttyACM1
Baud: 57600 (or any standard value)
```

**Windows:**
```
Port: COMy (check Device Manager)
Baud: 57600 (or any standard value)
```

**Note:** The "y" in COMy will be different from the debug port COMx

---

## Boot Sequence

### Expected Boot Messages:

On NSH console (/dev/ttyACM0):
```
NuttShell (NSH) NuttX-12.0.0
nsh>
INFO  [init] Launching task: /etc/init.d/rcS
...
sercon: Registering CDC/ACM serial driver
sercon: Successfully registered the CDC/ACM serial driver
Starting MAVLink on /dev/ttyACM1         ← Note: ACM1!
INFO  [mavlink] mode: Onboard, data rate: 4000 B/s on /dev/ttyACM1
WARN  [mavlink] offboard mission init failed   ← Expected
...
INFO  [commander] Ready for takeoff!
```

**Key line:**
```
Starting MAVLink on /dev/ttyACM1  ← Must be ACM1, not ACM0!
```

---

## Troubleshooting

### Issue: QGC submenu connects but main window shows disconnected

**Cause:** MAVLink on wrong port (ttyACM0 instead of ttyACM1)

**Check:**
```bash
nsh> mavlink status
instance #0:
        mavlink chan: #0
        type:           USB CDC-ACM
        mode:           Onboard
        device:         /dev/ttyACM1    ← Must be ACM1!
```

**Fix:**
- Verify CONFIG_BOARD_SERIAL_TEL1="/dev/ttyACM1"
- Rebuild and reflash firmware

---

### Issue: Only one /dev/ttyACM* device appears

**Cause:** Only one USB cable connected

**Solution:**
- Connect BOTH USB cables:
  - J20 (Debug) for console
  - J19 (Target) for MAVLink
- Or connect only J19 for QGC (no console)

---

### Issue: Wrong device number assignment

**Cause:** USB enumeration order changes based on plug order

**Check which is which:**
```bash
# Method 1: Check dmesg
dmesg | grep -i "cdc_acm\|ttyACM"

# Method 2: Unplug one cable and check
ls /dev/ttyACM*
# Plug in J20 (Debug) → appears as ttyACM0
# Plug in J19 (Target) → appears as ttyACM1

# Method 3: Check USB IDs
ls -l /dev/serial/by-id/
```

**General rule:**
- First plugged = ttyACM0 (usually debug)
- Second plugged = ttyACM1 (usually target)

---

### Issue: Permission denied accessing /dev/ttyACM1

**Cause:** User not in correct group

**Fix (Linux):**
```bash
# Add user to dialout group
sudo usermod -a -G dialout $USER

# Add user to plugdev group (if needed)
sudo usermod -a -G plugdev $USER

# Log out and back in for changes to take effect
```

**Temporary fix:**
```bash
sudo chmod 666 /dev/ttyACM1
```

---

## Device Verification

### Verify Port Assignment:

Run this on the board:
```bash
nsh> mavlink status
# Should show: /dev/ttyACM1

nsh> param show MAV_0_CONFIG
MAV_0_CONFIG: 101       # TEL1

nsh> param show SYS_USB_AUTO
SYS_USB_AUTO: 1         # Enabled
```

### Verify on PC (Linux):

```bash
# Show all USB serial devices
ls -l /dev/ttyACM*

# Show USB device details
lsusb
# Should show:
# Bus XXX Device XXX: ID 26ac:1371 Microchip PX4 SAMV71XULT (Target USB)
# Bus XXX Device XXX: ID 03eb:2111 Atmel EDBG (Debug USB)

# Show recent USB messages
dmesg | grep -i "cdc_acm\|ttyACM" | tail -20
```

---

## Summary

### Hardware:
```
2 physical USB ports on SAMV71-XULT:
- J20 (Debug/Programmer) → /dev/ttyACM0 → Console
- J19 (Target USB)       → /dev/ttyACM1 → MAVLink/QGC
```

### Configuration:
```
MAV_0_CONFIG = 101              → TEL1 port
TEL1 = /dev/ttyACM1            → Target USB
SYS_USB_AUTO = 1               → Auto-start enabled
```

### Expected Behavior:
```
Boot → USB auto-start → MAVLink on /dev/ttyACM1 → QGC connects
Console remains on /dev/ttyACM0 → No conflict
```

### Critical Fix:
```diff
- CONFIG_BOARD_SERIAL_TEL1="/dev/ttyACM0"  # Wrong (debug port)
+ CONFIG_BOARD_SERIAL_TEL1="/dev/ttyACM1"  # Correct (target port)
```

---

**Document Created:** November 24, 2025
**Issue:** MAVLink on wrong USB port
**Root Cause:** TEL1 mapped to ttyACM0 instead of ttyACM1
**Status:** ✅ FIXED - Rebuild and flash to apply
