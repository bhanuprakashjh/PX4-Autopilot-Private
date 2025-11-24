# QGroundControl USB Connection Guide

**Issue:** QGroundControl doesn't have 2000000 baud in dropdown
**Solution:** Use any standard baud rate - USB ignores it
**Date:** November 24, 2025

---

## Understanding USB CDC/ACM Baud Rates

### Key Fact: USB Baud Rate is Meaningless

When using **USB CDC/ACM** (MAV_0_CONFIG 101), the baud rate is **virtual**:

```
USB Protocol ≠ Traditional Serial UART
- USB has its own flow control and timing
- The "baud rate" setting is completely ignored
- USB operates at USB 2.0 High Speed (480 Mbps)
- The virtual COM port can run at any "baud rate"
```

**Result:** You can select ANY baud rate in QGroundControl and it will work.

---

## Quick Solution

### For QGroundControl Connection:

1. **Connect USB cable** from SAMV71-XULT to PC
2. **Open QGroundControl**
3. **Select Connection:**
   - **Port:** COMx (Windows) or /dev/ttyACM0 (Linux/Mac)
   - **Baud Rate:** Select **57600** or **115200** from dropdown
4. **Click Connect**

**Important:** The baud rate you select doesn't matter for USB. Pick any standard value (57600, 115200, 921600, etc.).

---

## Current Configuration

### SAMV71-XULT MAVLink Settings:

```bash
# File: boards/microchip/samv71-xult-clickboards/init/rc.board_defaults

param set-default MAV_0_CONFIG 101      # USB CDC/ACM
param set-default MAV_0_RATE 57600      # Display rate (cosmetic for USB)
```

**MAV_0_CONFIG 101 means:**
- Device: /dev/ttyACM0 (USB virtual serial port)
- Protocol: MAVLink v2
- Connection: USB CDC/ACM (not UART)

**MAV_0_RATE 57600 means:**
- This is the "advertised" baud rate
- For USB, this is ignored by the hardware
- Set to standard value so QGC dropdown matches

---

## Connection Methods

### Method 1: USB CDC/ACM (Current - Recommended)

```
✅ Advantages:
- No UART wiring needed
- High speed (USB 2.0)
- Stable connection
- Built-in power (can power board via USB)
- Baud rate irrelevant

⚠️ Disadvantages:
- Requires USB cable
- Cannot disconnect USB while operating

Configuration:
- MAV_0_CONFIG: 101
- Device: /dev/ttyACM0
- Baud: Any (57600 recommended for QGC dropdown)
```

### Method 2: UART Telemetry (Alternative)

If you prefer traditional UART connection:

```bash
# Change to UART TELEM1 (if you have it wired)
param set MAV_0_CONFIG 102      # TELEM1 on /dev/ttyS1
param set SER_TEL1_BAUD 57600   # MUST match QGC setting
param save
reboot
```

```
✅ Advantages:
- Can disconnect USB
- Standard telemetry radio compatible
- True hardware UART

⚠️ Disadvantages:
- Requires UART wiring
- Baud rate MUST match exactly
- Slower than USB (max 921600 typical)

Configuration:
- MAV_0_CONFIG: 102 (TELEM1)
- Device: /dev/ttyS1
- Baud: 57600 or 115200 (must match QGC)
```

---

## QGroundControl Connection Steps

### Windows:

1. **Plug in USB cable**
2. **Windows will detect** "USB Serial Device" or "PX4 FMU"
3. **Check Device Manager** → Ports (COM & LPT) → Note COM port number (e.g., COM3)
4. **Open QGroundControl**
5. **Top-left** → Comm Links → Add
6. **Settings:**
   ```
   Name: SAMV71
   Type: Serial
   Port: COM3 (or whatever your port is)
   Baud: 57600 (or 115200, doesn't matter for USB)
   ```
7. **Click Connect**

### Linux:

1. **Plug in USB cable**
2. **Check device:**
   ```bash
   ls /dev/ttyACM*
   # Should show: /dev/ttyACM0
   ```
3. **Give permissions** (if needed):
   ```bash
   sudo usermod -a -G dialout $USER
   # Log out and back in
   ```
4. **Open QGroundControl**
5. **Top-left** → Comm Links → Add
6. **Settings:**
   ```
   Name: SAMV71
   Type: Serial
   Port: /dev/ttyACM0
   Baud: 57600 (or any standard value)
   ```
7. **Click Connect**

### macOS:

1. **Plug in USB cable**
2. **Check device:**
   ```bash
   ls /dev/tty.usbmodem*
   # Should show: /dev/tty.usbmodem14201 (or similar)
   ```
3. **Open QGroundControl**
4. **Settings:**
   ```
   Port: /dev/tty.usbmodem14201
   Baud: 57600 (any standard value works)
   ```
5. **Click Connect**

---

## Troubleshooting

### Issue: QGC shows "2000000 baud" error

**Cause:** PX4 was advertising 2000000 baud (non-standard)

**Solution:** ✅ FIXED - Updated to 57600 in rc.board_defaults

**Verification:**
```bash
# After flashing new firmware, check:
nsh> param show MAV_0_RATE
MAV_0_RATE: 57600
```

---

### Issue: QGC doesn't connect

**Checklist:**

1. ✅ **Check USB cable** - Try a different cable (some are power-only)
2. ✅ **Check port** - Verify correct COM port / /dev/ttyACM0
3. ✅ **Check drivers** - Windows may need USB serial driver
4. ✅ **Check permissions** - Linux needs dialout group membership
5. ✅ **Check QGC version** - Use QGroundControl v4.0+
6. ✅ **Try any baud** - 57600, 115200, 921600 all work for USB

**Debug commands:**
```bash
# On board (via serial console):
nsh> mavlink status
# Should show:
# instance #0: ... on /dev/ttyACM0 ... streaming

nsh> param show MAV_0_CONFIG
MAV_0_CONFIG: 101      # USB CDC/ACM

nsh> param show MAV_0_RATE
MAV_0_RATE: 57600      # Display rate
```

---

### Issue: Device not showing up on PC

**Windows:**
```powershell
# Check Device Manager
devmgmt.msc

# Look for:
- "Ports (COM & LPT)" → USB Serial Device (COMx)
- Or "Other devices" → Unknown device (needs driver)

# Install driver if needed:
- Use Windows Update
- Or install PX4 USBSER driver
```

**Linux:**
```bash
# Check if device detected
lsusb
# Should show: Atmel Corp. (or similar)

# Check ttyACM device
ls -l /dev/ttyACM0
# If permission denied:
sudo chmod 666 /dev/ttyACM0
# Or permanently:
sudo usermod -a -G dialout $USER
```

**macOS:**
```bash
# Check device
ls /dev/tty.usbmodem*

# If not showing:
# - Try different USB port
# - Check cable (needs data pins)
# - Check System Preferences → Security
```

---

## Advanced: Custom Baud Rates (If Needed)

If you want to use a UART and need a specific non-standard baud rate:

### For USB CDC/ACM (Current Setup):
```bash
# MAV_0_RATE is just cosmetic for USB
param set MAV_0_RATE 57600     # Standard for QGC dropdown
param set MAV_0_RATE 115200    # Also common
param set MAV_0_RATE 921600    # High-speed option
# Any of these work identically on USB
```

### For UART (If you switch to TELEM):
```bash
# Switch to UART
param set MAV_0_CONFIG 102           # TELEM1 on /dev/ttyS1
param set SER_TEL1_BAUD 57600       # MUST match QGC exactly
# Available: 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600

# Or use GPS port:
param set MAV_0_CONFIG 103           # GPS1 on /dev/ttyS2
param set SER_GPS1_BAUD 115200      # MUST match QGC exactly
```

---

## Why USB Baud Rate Doesn't Matter

### Technical Explanation:

**Traditional UART:**
```
UART timing = bit period = 1 / baud_rate
Example: 57600 baud = 17.36 µs per bit
- Both sides MUST agree on timing
- Mismatched baud = garbled data
```

**USB CDC/ACM:**
```
USB packet-based protocol
- No fixed bit timing
- Hardware handles clock recovery
- "Baud rate" is just a parameter in control messages
- Actual data rate = USB speed (480 Mbps for HS)
```

**Result:** The "baud rate" you set in software is **completely ignored** by USB hardware.

---

## Recommended Settings

### For QGroundControl:

**Baud Rate Selection:**
```
1st Choice: 57600    ← Most compatible
2nd Choice: 115200   ← Also very common
3rd Choice: 921600   ← High-speed option

All work identically on USB!
```

**Why 57600 is recommended:**
- Standard PX4 default
- Appears in all QGC versions
- Matches most telemetry radios (if you switch later)
- Historical PX4 default

---

## Summary

### What You Need to Know:

1. **USB baud rate is meaningless** - Pick any standard value in QGC
2. **Current config is correct** - MAV_0_CONFIG 101 = USB CDC/ACM
3. **Updated to 57600** - Matches QGC dropdown options
4. **Connection will work** - Use any baud in QGC (57600 recommended)

### What Changed:

```diff
# Before (not in rc.board_defaults):
param set-default MAV_0_CONFIG 101
# MAV_0_RATE not set → defaults to high value

# After (updated):
param set-default MAV_0_CONFIG 101
+ param set-default MAV_0_RATE 57600   ← Added this
```

### To Apply Fix:

```bash
# 1. Flash new firmware (already built)
openocd -f interface/cmsis-dap.cfg -c "adapter speed 500" \
  -f target/atsamv.cfg \
  -c "program build/microchip_samv71-xult-clickboards_default/microchip_samv71-xult-clickboards_default.elf verify reset exit"

# 2. Connect to QGC
# Port: COM3 (Windows) or /dev/ttyACM0 (Linux)
# Baud: 57600 (or any standard value)

# 3. Verify
# Should connect successfully!
```

---

## Still Having Issues?

If QGC still won't connect after trying different baud rates:

1. **Check MAVLink is running:**
   ```bash
   nsh> mavlink status
   # Should show streaming on /dev/ttyACM0
   ```

2. **Check USB device:**
   ```bash
   # Linux:
   dmesg | grep -i cdc
   # Should show: cdc_acm registered
   ```

3. **Try MAVProxy first:**
   ```bash
   # Install MAVProxy:
   pip3 install mavproxy

   # Connect (any baud works):
   mavproxy.py --master=/dev/ttyACM0 --baudrate=57600
   # Or on Windows:
   mavproxy.py --master=COM3 --baudrate=57600
   ```

4. **Enable MAVLink debug:**
   ```bash
   nsh> mavlink stop
   nsh> mavlink start -d /dev/ttyACM0 -m onboard -r 4000 -x
   # -x enables debug output
   ```

---

**Document Created:** November 24, 2025
**Status:** USB CDC/ACM baud rate set to 57600 (standard)
**Result:** QGroundControl can connect using any baud from dropdown
**Recommendation:** Use 57600 or 115200 in QGC settings
