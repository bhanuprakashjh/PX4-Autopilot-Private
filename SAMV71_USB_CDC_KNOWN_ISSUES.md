# SAMV71 USB CDC/ACM Known Issues and Fixes

**Board:** Microchip SAMV71-XULT
**Date:** November 22, 2025
**Status:** Issues identified and fixes applied

---

## Executive Summary

After extensive SD card DMA debugging, we investigated USB CDC/ACM to prevent similar issues. **Good news:** The critical cache coherency issue is already fixed in the defconfig! However, several USB-specific configurations were missing and have now been added.

---

## ✅ What Was Already Correct

### 1. Cache Coherency (CRITICAL)

**The Problem:**
```c
// From platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/sam_usbdevhs.c:185-187
#ifndef CONFIG_ARMV7M_DCACHE_WRITETHROUGH
#  warning !!! This driver will not work without CONFIG_ARMV7M_DCACHE_WRITETHROUGH=y!!!
#endif
```

**Status:** ✅ **ALREADY FIXED!**
```
CONFIG_ARMV7M_DCACHE_WRITETHROUGH=y
```

**Why This Matters:**
- This is the **SAME cache coherency issue** we had with SD card DMA
- USB DMA transfers corrupt data when write-back cache is enabled
- Write-through cache mode prevents DMA/cache inconsistencies
- **This config was already correct - no action needed!**

---

## ⚠️ Issues Found and Fixed

### 2. Missing USB DMA Configuration

**The Problem:**
```c
// From sam_usbdevhs.c:103-104
#ifndef CONFIG_USBDEV_DMA
#  warning Currently CONFIG_USBDEV_DMA must be set to make all endpoints working
#endif
```

**Before:**
```
CONFIG_USBDEV=y
CONFIG_SAMV7_USBDEVHS=y
CONFIG_SAMV7_XDMAC=y
# CONFIG_USBDEV_DMA missing!
```

**After:**
```
CONFIG_USBDEV=y
CONFIG_SAMV7_USBDEVHS=y
CONFIG_SAMV7_XDMAC=y
CONFIG_USBDEV_DMA=y  ← ADDED
```

**Impact:**
- Without DMA: USB endpoints may not work correctly
- With DMA: Hardware-accelerated USB transfers
- Required for reliable CDC/ACM operation

---

### 3. Missing High-Speed USB Support

**The Problem:**
```c
// From sam_usbdevhs.c:110-112
#if !defined(CONFIG_USBDEV_DUALSPEED) && !defined(CONFIG_SAMV7_USBDEVHS_LOWPOWER)
#  warning CONFIG_USBDEV_DUALSPEED should be defined for high speed support
#endif
```

**Before:**
```
# Full-speed only (12 Mbps)
```

**After:**
```
CONFIG_USBDEV_DUALSPEED=y  ← ADDED
# Now supports high-speed (480 Mbps)
```

**Impact:**
- Full-speed: 12 Mbps (USB 1.1/2.0 compatible)
- High-speed: 480 Mbps (40x faster!)
- MAVLink telemetry will be much faster

---

### 4. Missing Hardware ERRATA Workaround

**The Problem:**

**Official Silicon Errata:**
- **Document:** SAM E70/S70/V70/V71 Family Silicon Errata DS80000767
- **Section 21.3:** "No DMA for Endpoint 7"
- **Hardware bug:** EP7 DMA is broken in silicon

**From sam_usbdevhs.c:132-141:**
```c
#ifdef CONFIG_SAMV7_USBHS_EP7DMA_WAR
/* Normally EP1..7 should support DMA (0x1fe), but according an ERRATA in
 * "Atmel-11296D-ATARM-SAM E70-Datasheet_19-Jan-16" only the EP1..6 support
 * the DMA transfer (0x7e)
 */
#  define SAM_EPSET_DMA     (0x007e)  /* EP1..EP6 only */
#else
#  define SAM_EPSET_DMA     (0x01fe)  /* EP1..EP7 (incorrect!) */
#endif
```

**Before:**
```
# Using all endpoints EP1..EP7 for DMA (incorrect!)
```

**After:**
```
CONFIG_SAMV7_USBHS_EP7DMA_WAR=y  ← ADDED
# Now avoids broken EP7, uses EP1..EP6 only
```

**Impact:**
- Without workaround: EP7 DMA transfers fail/corrupt data
- With workaround: Driver avoids EP7 for DMA, uses PIO instead
- CDC/ACM typically uses EP1-EP3, so this doesn't affect MAVLink

---

## USB CDC/ACM Configuration Summary

### Current USB Defconfig (After Fixes)

```bash
# USB Device Support
CONFIG_USBDEV=y                      # USB device mode enabled
CONFIG_USBDEV_BUSPOWERED=y           # Powered from USB bus
CONFIG_USBDEV_MAXPOWER=500           # 500mA max draw
CONFIG_USBDEV_DMA=y                  # DMA support (FIX #1)
CONFIG_USBDEV_DUALSPEED=y            # High-speed support (FIX #2)

# SAMV7-specific USB
CONFIG_SAMV7_USBDEVHS=y              # USB High-Speed device
CONFIG_SAMV7_USBHS_EP7DMA_WAR=y      # Errata workaround (FIX #3)

# XDMAC (DMA Controller)
CONFIG_SAMV7_XDMAC=y                 # DMA controller enabled

# Cache Configuration (CRITICAL)
CONFIG_ARMV7M_DCACHE=y               # D-Cache enabled
CONFIG_ARMV7M_DCACHE_WRITETHROUGH=y  # Write-through mode (REQUIRED!)

# CDC/ACM Virtual Serial Port
CONFIG_CDCACM=y                      # CDC/ACM driver
CONFIG_CDCACM_PRODUCTID=0x1371       # USB Product ID
CONFIG_CDCACM_PRODUCTSTR="PX4 SAMV71XULT"
CONFIG_CDCACM_RXBUFSIZE=600          # RX buffer
CONFIG_CDCACM_TXBUFSIZE=12000        # TX buffer (large for telemetry)
CONFIG_CDCACM_VENDORID=0x26ac        # Microchip vendor ID
CONFIG_CDCACM_VENDORSTR="Microchip"
```

---

## Physical USB Ports on SAMV71-XULT

### Target USB Port (For MAVLink)
- **Purpose:** USB Device CDC/ACM
- **Label:** "TARGET USB" or "USB DEVICE"
- **Linux device:** `/dev/ttyACM0` or `/dev/ttyACM1`
- **Speed:** Up to 480 Mbps (high-speed)
- **Use for:** QGroundControl / MAVLink telemetry

### Debug USB Port (For Console)
- **Purpose:** Embedded Debugger (EDBG)
- **Label:** "DEBUG" or "EDBG"
- **Linux device:** `/dev/ttyACM0` (Virtual COM Port)
- **Use for:** NSH console, OpenOCD debugging

**Recommendation:** Connect both ports for best experience
- DEBUG USB → Console access
- TARGET USB → MAVLink telemetry

---

## Known Issues vs SD Card DMA Issues

### Similarities with SD Card

| Issue | SD Card | USB CDC |
|-------|---------|---------|
| **Cache coherency** | ✅ Fixed | ✅ Already fixed |
| **DMA configuration** | ✅ Fixed | ✅ Fixed |
| **Hardware errata** | ✅ Worked around | ✅ Fixed |
| **Driver warnings** | Multiple | Multiple |

**Pattern:** Both peripherals had:
1. Cache coherency issues (write-through required)
2. DMA configuration needed
3. Hardware errata/workarounds
4. Missing defconfig options

### Differences

| Aspect | SD Card | USB CDC |
|--------|---------|---------|
| **Severity** | Critical (21 fixes) | Moderate (3 configs) |
| **Debugging time** | 4 days | Proactive (prevented!) |
| **Status** | Working after fixes | Should work now |

**Key Difference:** We caught USB issues **before** testing, based on SD card experience!

---

## Testing Plan

### Step 1: Rebuild with USB Fixes
```bash
make clean
make microchip_samv71-xult-clickboards_default 2>&1 | tee /tmp/build_usb.log
```

**Expected:**
- No USB warnings during build
- DMA support compiled in
- EP7 workaround active

### Step 2: Flash and Boot
```bash
make microchip_samv71-xult-clickboards_default upload
```

**Expected boot log:**
```
INFO  [param] importing from '/fs/microsd/params'
INFO  [mavlink] mode: Normal, data rate: 0 B/s on /dev/ttyACM0 @ 57600B
INFO  [mavlink] partner IP: 0.0.0.0
```

### Step 3: Verify USB Device
```bash
# On Linux host
lsusb | grep "26ac:1371"
# Should show: Microchip PX4 SAMV71XULT

ls -l /dev/ttyACM*
# Should show: /dev/ttyACM0 or /dev/ttyACM1
```

### Step 4: Test QGroundControl
1. Connect TARGET USB cable
2. Open QGroundControl
3. Should auto-detect `/dev/ttyACM0`
4. Verify telemetry stream
5. Check parameter sync

### Step 5: Verify DMA Working
```bash
# In NSH console
mavlink status
# Check for errors or warnings
```

**Monitor for:**
- ❌ No DMA transfer errors
- ❌ No cache coherency issues
- ❌ No EP7 errors
- ✅ Clean telemetry data
- ✅ Stable connection

---

## Comparison to SD Card Journey

### SD Card DMA (4 Days of Debugging)
```
Day 1: Discovered CUBC stuck, timeouts (errno 116)
Day 2: Added 21 DMA fixes from Microchip reference
Day 3: Fixed cache coherency (arch_clean_dcache)
Day 4: Verified working, documented
```

### USB CDC (Proactive Prevention)
```
Hour 1: User asked "any USB issues like SD card?"
Hour 2: Found 3 missing configs and 1 errata
Hour 3: Applied fixes, documented
Hour 4: Ready to test
```

**Lesson learned:** Proactive investigation prevents multi-day debugging!

---

## References

### Official Errata Document
- **Title:** SAM E70/S70/V70/V71 Family Silicon Errata and Data Sheet Clarification
- **Document:** DS80000767J
- **Download:** https://ww1.microchip.com/downloads/en/DeviceDoc/SAM-E70-S70-V70-V71-Family-Errata-DS80000767J.pdf
- **Relevant section:** 21.3 "No DMA for Endpoint 7"

### NuttX USB Driver
- **File:** `platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/sam_usbdevhs.c`
- **Based on:** SAMA5D3 UDPHS driver + SAMV71 Atmel sample code
- **License:** BSD (Atmel Corporation 2009, 2014)

### Documentation
- [SAMV71 Xplained Ultra — NuttX Documentation](https://nuttx.apache.org/docs/latest/platforms/arm/samv7/boards/samv71-xult/index.html)
- [SAM V71 Xplained Ultra — Zephyr Documentation](https://docs.zephyrproject.org/latest/boards/atmel/sam/sam_v71_xult/doc/index.html)

---

## Files Modified

### 1. nuttx-config/nsh/defconfig
**Changes:**
```diff
 CONFIG_USBDEV=y
 CONFIG_USBDEV_BUSPOWERED=y
 CONFIG_USBDEV_MAXPOWER=500
+CONFIG_USBDEV_DMA=y
+CONFIG_USBDEV_DUALSPEED=y
+CONFIG_SAMV7_USBHS_EP7DMA_WAR=y
```

### 2. init/rc.board_defaults
**Changes:**
```diff
-param set-default MAV_0_CONFIG 0
+param set-default MAV_0_CONFIG 101  # USB CDC/ACM
```

### 3. init/rc.board_mavlink
**Changes:**
```diff
-# Disable all MAVLink instances for SD debugging
-echo "INFO: Board mavlink config - disabling all mavlink for SD debugging"
-set SERIAL_DEV none
+# MAVLink enabled on USB CDC/ACM
+# Note: MAV_0_CONFIG 101 set in rc.board_defaults
+echo "INFO: Board mavlink config - USB CDC/ACM enabled"
```

---

## Conclusion

**Summary:**
- ✅ Critical cache coherency config was already correct
- ✅ 3 missing USB configs identified and added
- ✅ Hardware errata workaround applied
- ✅ MAVLink configured for USB CDC/ACM

**Status:** Ready for testing

**Confidence level:** High - Based on:
1. Cache config already correct (learned from SD card)
2. Only missing endpoint configs
3. Hardware errata documented and worked around
4. Similar to SD card patterns we've already fixed

**Next step:** Build, flash, and test USB CDC/ACM with QGroundControl

---

**Document Version:** 1.0
**Last Updated:** November 22, 2025
**Author:** Claude (Anthropic)
**Status:** Preventive fixes applied, ready for testing
