# SAMV71 Disabled Features - Safe Mode Configuration

**Date:** November 22, 2025
**Purpose:** Document what was disabled during SD card DMA debugging
**Status:** ✅ SD card now working - Features can be re-enabled

---

## Executive Summary

During the SD card DMA debugging process, several features were **intentionally disabled** to create a "safe mode" environment. This was necessary to:
1. Eliminate interference from multiple services accessing SD card simultaneously
2. Prevent garbage values and crashes from buggy services
3. Create a clean, minimal boot for debugging
4. Reduce console noise to see actual errors

**Now that SD card DMA and parameters are working, most of these can be re-enabled.**

---

## What Was Disabled and Why

### 1. Logger Service (SD Card Writing)

**Disabled in:** `boards/microchip/samv71-xult-clickboards/init/rc.logging`

**Current State:**
```bash
#!/bin/sh
# Disable logging for SD debugging
echo "INFO [logger] logging disabled for SD card debugging"
```

**Why it was disabled:**
- Logger does **continuous high-speed SD card writes**
- Before DMA fix, this caused:
  - Timeout errors (errno 116)
  - Console freezing
  - Garbage data
  - System instability requiring power cycle
- Boot log showed: `SDLOG_MODE -1` (logging disabled)

**Impact:**
- ❌ No flight logs saved
- ❌ No sensor data recording
- ❌ No performance monitoring

**Can it be re-enabled now?**
- ✅ **YES!** SD card DMA now working
- Need to test continuous logging under load
- May reveal any remaining DMA issues (OVRE/UNRE/DTOE)

**How to re-enable:**
```bash
# Remove or comment out the rc.logging file
# Or change SDLOG_MODE parameter
param set SDLOG_MODE 0    # Log from boot
param set SDLOG_MODE 1    # Log when armed
```

---

### 2. MAVLink Communication

**Disabled in:** `boards/microchip/samv71-xult-clickboards/init/rc.board_mavlink`

**Current State:**
```bash
#!/bin/sh
echo "INFO: Board mavlink config - disabling all mavlink for SD debugging"
set SERIAL_DEV none
```

**Also in:** `boards/microchip/samv71-xult-clickboards/init/rc.board_defaults`
```bash
param set-default MAV_0_CONFIG 0   # MAVLink instance 0 disabled
```

**Why it was disabled:**
- MAVLink creates **background tasks** that can interfere with debugging
- Uses serial ports (USB/UART) which could conflict with console
- Generates continuous messages that clutter console output
- Parameter issues caused MAVLink to spam errors

**Impact:**
- ❌ No ground station communication
- ❌ No telemetry data
- ❌ No QGroundControl connection
- ❌ No mission upload/download
- ✅ Clean console for debugging
- ✅ No USB port conflicts

**Can it be re-enabled now?**
- ✅ **YES!** Parameters working, SD card stable
- Should work normally now

**How to re-enable:**
```bash
# For USB MAVLink (ttyACM0)
param set MAV_0_CONFIG 101
param set MAV_0_MODE 0      # Normal mode

# For UART MAVLink (TELEM1)
param set MAV_1_CONFIG 102
param set MAV_1_MODE 0

# Restart to apply
reboot
```

---

### 3. Dataman Service (Mission Storage)

**Status:** Partially disabled (service doesn't start, compiled in)

**Why it was disabled:**
- Uses `/fs/mtd_caldata` partition which doesn't exist
- Caused errors and console spam
- Not needed for basic SD card testing

**Boot log shows:**
```
ERROR [bsondump] open '/fs/mtd_caldata' failed (2)
```

**Impact:**
- ❌ Cannot store missions
- ❌ Cannot store geofence
- ❌ Cannot store rally points

**Can it be re-enabled now?**
- ⚠️ **Partially** - Still needs MTD partition configuration
- Currently uses SD card as fallback (should work)
- Not critical for testing

**How to enable:**
1. Configure MTD partition in NuttX defconfig
2. Or use SD card storage (should work now)
3. Test `dataman start` command

---

### 4. Background Parameter Auto-Save

**Status:** Background save has issues (manual save works)

**What happens:**
```
ERROR [tinybson] killed: failed reading length
ERROR [parameters] parameter export to /fs/microsd/params failed (1) attempt 1
```

**Why:**
- Automatic background saves have timing/threading issues
- Possibly conflicts with other SD card access
- **Manual `param save` works perfectly**

**Impact:**
- ⚠️ Parameters don't auto-save on shutdown
- ✅ Manual save works fine

**Workaround:**
Always use manual `param save` before powering off

**Fix needed:**
- Investigate BSON library thread safety
- Check background task stack size
- Verify SD card access serialization

---

### 5. Early Boot Exit (Safe Mode)

**Disabled in:** `ROMFS/init.d/rcS`

**Added code:**
```bash
#
# SAFE MODE: skipping PX4 startup (SD debug)
#
exit 0
```

**Location:** After parameter import, before service startup

**Why it was added:**
- Stop boot immediately after loading parameters
- Prevent any services from starting
- Gives instant NSH prompt
- Perfect for debugging SD card and parameters

**Impact:**
- ❌ No commander (flight controller)
- ❌ No sensors
- ❌ No EKF2
- ❌ No flight modes
- ✅ Instant NSH prompt
- ✅ Minimal system load (~99% idle)
- ✅ Clean console

**Can it be removed now?**
- ✅ **YES!** Core functionality working
- Safe to proceed to full boot
- Will reveal any remaining issues

**How to disable safe mode:**
```bash
# Comment out or remove the "exit 0" line in rcS
# Then rebuild
make clean
make microchip_samv71-xult-clickboards_default
```

---

## Features Disabled by rcS Early Exit

When safe mode is active (`exit 0` in rcS), these services **never start**:

### Flight Control Stack
- ❌ **Commander** - Flight mode management
- ❌ **Sensors** - IMU, barometer, magnetometer
- ❌ **EKF2** - State estimation
- ❌ **Navigator** - Mission/waypoint following
- ❌ **Position/Attitude Control** - Flight stabilization

### Data Services
- ❌ **Logger** - Flight log recording
- ❌ **Dataman** - Mission storage

### Communication
- ❌ **MAVLink** - Ground station link
- ❌ **RC Input** - Radio control

### Optional Features
- ❌ **PWM Output** - Motor control
- ❌ **RGB LED** - Status indicators
- ❌ **Tone Alarm** - Audio feedback

**All of these can start normally once safe mode is removed.**

---

## What's Currently Running (Minimal Boot)

With safe mode enabled:

```
nsh> ps
  PID GROUP PRI POLICY   TYPE    NPX STATE    EVENT     SIGMASK  STACKBASE  STACKSIZE      USED   FILLED    COMMAND
    0     0   0 FIFO     Kthread N-- Ready              00000000 0x00000000      2048       772    37.6%    Idle Task
    1     1 255 RR       Task    --- Waiting  Semaphore 00000000 0x20429db8      8176      1304    15.9%    init
    2     2 100 RR       Kthread --- Waiting  Semaphore 00000000 0x2042bde0      2032       836    41.1%    lpwork 0x2042bab0
    3     3 200 RR       Kthread --- Waiting  Semaphore 00000000 0x2042c5d8      2032       764    37.5%    hpwork 0x2042c2a8
```

**Only essential NuttX tasks - no PX4 services!**

---

## Summary: What Can Be Re-Enabled

| Feature | Currently | Can Re-enable? | Priority | Notes |
|---------|-----------|----------------|----------|-------|
| **Logger** | Disabled | ✅ YES | P2 - Medium | Test continuous logging |
| **MAVLink** | Disabled | ✅ YES | P2 - Medium | Ground station communication |
| **Full Boot** | Early exit | ✅ YES | P1 - High | Remove `exit 0` from rcS |
| **Dataman** | Errors | ⚠️ Partial | P3 - Low | Needs MTD config or SD fallback |
| **Background Param Save** | Broken | ⚠️ Fix needed | P3 - Low | Manual save works |

**Priority:**
- P1 = High - Needed for flight testing
- P2 = Medium - Needed for full functionality
- P3 = Low - Nice to have

---

## Recommended Re-Enable Sequence

### Phase 1: Remove Safe Mode (Next Step)

1. **Edit** `ROMFS/init.d/rcS`
   - Comment out or remove `exit 0` line

2. **Rebuild and test:**
   ```bash
   make clean
   make microchip_samv71-xult-clickboards_default
   # Flash and test
   ```

3. **Expected result:**
   - Full PX4 boot
   - Commander starts
   - Sensors module starts
   - Parameters still load from SD card
   - System boots with saved configuration

### Phase 2: Re-enable MAVLink

1. **Set MAVLink parameters:**
   ```bash
   param set MAV_0_CONFIG 101   # USB
   param set MAV_0_MODE 0       # Normal
   param save
   reboot
   ```

2. **Test with QGroundControl:**
   - Connect via USB
   - Verify telemetry
   - Check parameters sync

### Phase 3: Re-enable Logger

1. **Set logging parameters:**
   ```bash
   param set SDLOG_MODE 0      # Log from boot
   param set SDLOG_PROFILE 0   # Default profile
   param save
   reboot
   ```

2. **Monitor for issues:**
   - Check for DMA errors
   - Verify log files created
   - Test sustained logging

### Phase 4: Stress Test

1. **Run all services simultaneously:**
   - Logger writing
   - MAVLink streaming
   - Parameter saves
   - Sensor data

2. **Monitor for:**
   - OVRE/UNRE/DTOE errors
   - System stability
   - Console responsiveness

---

## Files to Modify for Re-Enabling

### 1. Remove Safe Mode
**File:** `ROMFS/init.d/rcS`
```bash
# Comment out this line:
# exit 0
```

### 2. Enable MAVLink
**File:** `boards/microchip/samv71-xult-clickboards/init/rc.board_defaults`
```bash
# Change this:
param set-default MAV_0_CONFIG 0

# To this:
param set-default MAV_0_CONFIG 101
```

**Or delete:** `boards/microchip/samv71-xult-clickboards/init/rc.board_mavlink`

### 3. Enable Logger
**Delete or empty:** `boards/microchip/samv71-xult-clickboards/init/rc.logging`

### 4. Set Parameters
```bash
param set MAV_0_CONFIG 101
param set SDLOG_MODE 0
param save
```

---

## Why Safe Mode Was Necessary

### The Problems We Had:

1. **SD Card DMA Not Working:**
   - CUBC stuck at initial value
   - All transfers timing out (errno 116)
   - Logger caused system freeze
   - Required power cycle to recover

2. **Parameter Storage Broken:**
   - `param set` failed
   - `param save` created 5-byte empty files
   - C++ static initialization bug
   - malloc() failing during startup

3. **Multiple Services Conflicting:**
   - Logger trying to write continuously
   - MAVLink trying to communicate
   - Parameters trying to save
   - All failing and spamming console

4. **Impossible to Debug:**
   - Console filled with error messages
   - System unstable
   - Couldn't isolate issues
   - Couldn't test fixes

### The Solution: Safe Mode

By disabling everything except core functionality:
- ✅ Isolated SD card for testing
- ✅ Clean console to see actual errors
- ✅ Stable platform for debugging
- ✅ Could test one thing at a time
- ✅ Fixed SD card DMA (21 fixes)
- ✅ Fixed parameter storage (lazy allocation)

**Now that both are fixed, we can re-enable services!**

---

## Current Configuration Files

### What's Currently Disabled

```bash
# rc.board_defaults
param set-default MAV_0_CONFIG 0     # MAVLink disabled
param set-default SDLOG_MODE -1      # Logging disabled

# rc.board_mavlink
set SERIAL_DEV none                  # No serial devices

# rc.logging
echo "INFO [logger] logging disabled"

# rcS
exit 0                                # Early exit (safe mode)
```

### Default (Normal) Configuration Should Be

```bash
# rc.board_defaults
param set-default MAV_0_CONFIG 101   # MAVLink on USB
param set-default SDLOG_MODE 0       # Logging enabled

# rc.board_mavlink
# (normal mavlink startup code)

# rc.logging
# (normal logger startup code)

# rcS
# (no early exit, full boot)
```

---

## Verification After Re-Enabling

### Full Boot Test

After removing safe mode, verify:

```bash
# 1. Check all services started
ps | grep -E "commander|sensors|ekf2|logger|mavlink"

# 2. Verify parameters loaded
param show SYS_AUTOSTART
param show MAV_0_CONFIG
param show SDLOG_MODE

# 3. Check logger status
logger status

# 4. Check MAVLink status
mavlink status

# 5. Check SD card still working
echo "Full boot test" > /fs/microsd/test.txt
cat /fs/microsd/test.txt

# 6. Check logs being written
ls -l /fs/microsd/log/
```

**Expected:** All services running, parameters loaded, logging active.

---

## Conclusion

**Safe mode was a debugging necessity, not a permanent configuration.**

**What we achieved in safe mode:**
- ✅ Fixed SD card DMA (4 days, 21 fixes)
- ✅ Fixed parameter storage (lazy allocation)
- ✅ Verified file I/O working
- ✅ Achieved parameter persistence

**What we can do now:**
- ✅ Re-enable all services
- ✅ Full PX4 boot
- ✅ MAVLink communication
- ✅ Flight logging
- ✅ Normal operation

**The debugging journey is complete. Time to fly!** 🚀

---

**Document Version:** 1.0
**Last Updated:** November 22, 2025
**Status:** Safe mode no longer needed - Features ready to re-enable
