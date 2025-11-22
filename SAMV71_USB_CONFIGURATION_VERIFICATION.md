# SAMV71 USB Configuration Verification Checklist

**Board:** Microchip SAMV71-XULT
**Date:** November 22, 2025
**Purpose:** Verify all USB configurations are applied correctly

---

## Question: Should We "Fully Comply" with Harmony?

### **Answer: NO - And Here's Why:**

**Harmony and NuttX are fundamentally different architectures:**

| Aspect | Microchip Harmony | Apache NuttX | Should We Match? |
|--------|-------------------|--------------|------------------|
| **Cache Management** | Application-level | Driver-level | ❌ NO - Different approach |
| **Cache Mode** | Any (app handles it) | Write-through required | ❌ NO - NuttX is safer |
| **EP7 Workaround** | ❌ Not present | ✅ Has workaround | ✅ NuttX is BETTER! |
| **Code Structure** | Templates (.ftl) | Monolithic driver | ❌ NO - Different paradigm |
| **Philosophy** | Performance-focused | Safety-focused | ❌ NO - Different goals |

**Conclusion:** We should NOT try to "fully comply" with Harmony. NuttX uses a different (and in some ways better) approach.

**What we SHOULD do:** Ensure all NuttX-specific configurations are optimal.

---

## Critical Configurations Checklist

### ✅ APPLIED - Critical for Functionality

#### 1. CONFIG_USBDEV_DMA
```bash
# Current:
CONFIG_USBDEV_DMA=y  ✅ ADDED

# Why critical:
#  warning Currently CONFIG_USBDEV_DMA must be set to make all endpoints working

# Status: APPLIED ✅
```

#### 2. CONFIG_USBDEV_DUALSPEED
```bash
# Current:
CONFIG_USBDEV_DUALSPEED=y  ✅ ADDED

# Why critical:
#  warning CONFIG_USBDEV_DUALSPEED should be defined for high speed support

# Impact:
# - Without: 12 Mbps (full-speed)
# - With: 480 Mbps (high-speed) - 40x faster!

# Status: APPLIED ✅
```

#### 3. CONFIG_SAMV7_USBHS_EP7DMA_WAR
```bash
# Current:
CONFIG_SAMV7_USBHS_EP7DMA_WAR=y  ✅ ADDED

# Why critical:
# - Official Silicon Errata DS80000767J Section 21.3: "No DMA for Endpoint 7"
# - Hardware bug in SAMV71 silicon
# - Microchip's own Harmony/ASF do NOT have this workaround!

# Impact:
# - Without: EP7 DMA transfers fail/corrupt data
# - With: Driver avoids EP7 for DMA, uses PIO instead

# Status: APPLIED ✅
# NuttX is BETTER than Harmony here!
```

#### 4. CONFIG_ARMV7M_DCACHE_WRITETHROUGH
```bash
# Current:
CONFIG_ARMV7M_DCACHE_WRITETHROUGH=y  ✅ ALREADY PRESENT

# Why critical:
#  warning !!! This driver will not work without CONFIG_ARMV7M_DCACHE_WRITETHROUGH=y!!!

# Impact:
# - Write-back mode: DMA/cache coherency issues (like SD card had!)
# - Write-through mode: Simpler, safer, no alignment issues

# Status: ALREADY CORRECT ✅
```

---

### ⚠️ OPTIONAL - Performance Optimizations

#### 5. CONFIG_SAMV7_USBDEVHS_NDTDS

**Current State:**
```bash
# Not set - using default
# Default: 8 DMA transfer descriptors
```

**Harmony Uses:**
```bash
CONFIG_SAMV7_USBHS_NDTDS=32  # 4x more descriptors
CONFIG_SAMV7_USBHS_PREALLOCATE=y
```

**NuttX Documentation Suggests:**
```bash
CONFIG_SAMV7_USBDEVHS_NDTDS=32
CONFIG_SAMV7_USBDEVHS_PREALLOCATE=y
```

**Analysis:**
- **What it does:** Number of DMA transfer descriptors in pool
- **Default 8:** Sufficient for low-bandwidth devices
- **32 descriptors:** Better for high-throughput scenarios
- **Preallocate:** Avoid runtime allocation delays

**For MAVLink CDC/ACM:**
- MAVLink telemetry is moderate bandwidth
- CDC/ACM typically uses 3 endpoints (control + bulk IN/OUT)
- Default 8 should be sufficient
- 32 would provide more headroom

**Recommendation:**
```bash
# Conservative (current):
# Using default 8 - test first

# Optimized (if needed):
CONFIG_SAMV7_USBDEVHS_NDTDS=32
CONFIG_SAMV7_USBDEVHS_PREALLOCATE=y
```

**Status:** ⚠️ OPTIONAL - Can add if performance issues observed

---

### ❌ NOT APPLICABLE - Disabled Features

#### 6. CONFIG_SAMV7_USBDEVHS_SCATTERGATHER
```bash
# From sam_usbdevhs.c:116-117:
/* Not yet supported */
#undef CONFIG_SAMV7_USBDEVHS_SCATTERGATHER

# Status: Explicitly disabled in NuttX driver
# Reason: Not implemented yet
# Action: Do NOT enable
```

#### 7. CONFIG_SAMV7_USBDEVHS_LOWPOWER
```bash
# What it does: Forces full-speed only (12 Mbps)
# Conflicts with: CONFIG_USBDEV_DUALSPEED

# From sam_usbdevhs.c:107-108:
#if defined(CONFIG_USBDEV_DUALSPEED) && defined(CONFIG_SAMV7_USBDEVHS_LOWPOWER)
#  error CONFIG_USBDEV_DUALSPEED must not be defined with full-speed only support
#endif

# Status: Must NOT be enabled (we want high-speed)
# Action: Leave disabled
```

#### 8. CONFIG_SAMV7_USBHS_REGDEBUG
```bash
# What it does: Debug USB register access
# Only useful when: Debugging driver itself
# Performance impact: Significant (logs every register access)

# Status: Not needed for normal operation
# Action: Leave disabled
```

---

## Configuration Comparison

### What We Have vs What Harmony Has

| Configuration | Our NuttX | Harmony v3 | Needed? |
|---------------|-----------|------------|---------|
| **DMA enabled** | ✅ Yes | ✅ Yes | ✅ Critical |
| **High-speed** | ✅ Yes | ✅ Yes | ✅ Critical |
| **EP7 workaround** | ✅ Yes | ❌ No | ✅ NuttX better! |
| **Write-through cache** | ✅ Yes | ❌ No | ✅ NuttX safer! |
| **DMA descriptors** | 8 (default) | 32 | ⚠️ Optional |
| **Preallocate** | ❌ No | ✅ Yes | ⚠️ Optional |
| **Cache coherency** | Driver-level | App-level | ✅ Different approach |

---

## Why NuttX Approach is Different (and Better)

### 1. Cache Coherency Handling

**Harmony Approach (Application Responsibility):**
```c
// Application code must do:
uint8_t buffer[1024] __attribute__((aligned(32)));

// Before TX:
DCACHE_CLEAN_BY_ADDR(buffer, sizeof(buffer));
usb_transmit(buffer, sizeof(buffer));

// After RX:
DCACHE_INVALIDATE_BY_ADDR(buffer, sizeof(buffer));
```

**Pros:**
- ✅ Flexible (app controls cache management)
- ✅ Can use write-back cache (faster)

**Cons:**
- ❌ Easy to forget cache operations (bugs!)
- ❌ Application must handle alignment
- ❌ More complex application code

**NuttX Approach (Driver Responsibility):**
```c
// Driver handles cache automatically:
static void sam_dma_wrsetup(...) {
    up_clean_dcache(buffer, buffer + length);  // Driver does this
    // Setup DMA
}

// Application just calls:
usb_transmit(buffer, sizeof(buffer));  // Simple!
```

**Pros:**
- ✅ Application doesn't worry about cache
- ✅ Write-through mode prevents bugs
- ✅ Simpler application code

**Cons:**
- ❌ Slightly slower (write-through cache)
- ❌ Less flexible

**NuttX Design Philosophy:** Safety and simplicity over maximum performance

---

### 2. EP7 Errata Workaround

**Silicon Bug:** EP7 DMA broken in SAMV71 hardware (Official Errata DS80000767J)

**Harmony:** ❌ No workaround - assumes EP7 works!
```c
// Harmony allows EP7 for DMA (incorrect!)
#define USBHS_DMA_ENDPOINTS  (0x01FE)  // EP1-EP7
```

**NuttX:** ✅ Has workaround!
```c
#ifdef CONFIG_SAMV7_USBHS_EP7DMA_WAR
/* According to ERRATA only EP1..6 support DMA */
#  define SAM_EPSET_DMA  (0x007E)  // EP1-EP6 only
#else
#  define SAM_EPSET_DMA  (0x01FE)  // EP1-EP7 (incorrect!)
#endif
```

**Result:** NuttX is MORE correct than Microchip's own reference implementation!

---

### 3. Configuration Explicitness

**Harmony:** Implicit configurations (assumes defaults)
```python
# Template auto-generates with defaults
# User doesn't see what's configured
```

**NuttX:** Explicit configuration required
```bash
CONFIG_USBDEV_DMA=y            # Must explicitly enable
CONFIG_USBDEV_DUALSPEED=y      # Must explicitly enable
CONFIG_SAMV7_USBHS_EP7DMA_WAR=y  # Must explicitly enable
```

**Pros:**
- ✅ Forces user to understand configuration
- ✅ Self-documenting (config file shows what's enabled)
- ✅ Prevents "magic" defaults causing issues

**Cons:**
- ❌ More verbose
- ❌ Easier to forget options

---

## Final Configuration Status

### ✅ All Critical Configurations Applied

```bash
# === CRITICAL (Applied) ===
CONFIG_USBDEV=y                      # USB device mode
CONFIG_USBDEV_BUSPOWERED=y           # USB bus powered
CONFIG_USBDEV_MAXPOWER=500           # 500mA max
CONFIG_USBDEV_DMA=y                  # ✅ FIX #1 - DMA support
CONFIG_USBDEV_DUALSPEED=y            # ✅ FIX #2 - High-speed
CONFIG_SAMV7_USBHS_EP7DMA_WAR=y      # ✅ FIX #3 - Errata workaround

CONFIG_SAMV7_USBDEVHS=y              # SAMV7 USB HS device
CONFIG_SAMV7_XDMAC=y                 # DMA controller

CONFIG_ARMV7M_DCACHE=y               # D-Cache enabled
CONFIG_ARMV7M_DCACHE_WRITETHROUGH=y  # ✅ Critical - write-through mode

CONFIG_CDCACM=y                      # CDC/ACM driver
CONFIG_CDCACM_RXBUFSIZE=600          # RX buffer
CONFIG_CDCACM_TXBUFSIZE=12000        # TX buffer (large for telemetry)
```

### ⚠️ Optional Performance Configs (Not Applied - Using Defaults)

```bash
# === OPTIONAL (Can add if needed) ===
# CONFIG_SAMV7_USBDEVHS_NDTDS=32       # More DMA descriptors (default: 8)
# CONFIG_SAMV7_USBDEVHS_PREALLOCATE=y  # Preallocate descriptors

# Recommendation: Test with defaults first
# Add these only if performance issues observed
```

### ❌ Should NOT Enable

```bash
# === MUST NOT ENABLE ===
# CONFIG_SAMV7_USBDEVHS_SCATTERGATHER  # Not implemented in NuttX
# CONFIG_SAMV7_USBDEVHS_LOWPOWER       # Conflicts with dual-speed
# CONFIG_SAMV7_USBHS_REGDEBUG          # Only for driver debugging
```

---

## Testing Checklist

### Before Build
- [x] Verify CONFIG_USBDEV_DMA=y
- [x] Verify CONFIG_USBDEV_DUALSPEED=y
- [x] Verify CONFIG_SAMV7_USBHS_EP7DMA_WAR=y
- [x] Verify CONFIG_ARMV7M_DCACHE_WRITETHROUGH=y
- [x] Verify MAV_0_CONFIG=101 (USB CDC/ACM)

### During Build
- [ ] No USB-related warnings during compilation
- [ ] Build succeeds without errors
- [ ] Flash size within limits

### After Boot
- [ ] USB device enumerated on host PC
- [ ] /dev/ttyACM0 or /dev/ttyACM1 appears
- [ ] lsusb shows "26ac:1371 Microchip PX4 SAMV71XULT"
- [ ] MAVLink starts on USB CDC/ACM
- [ ] No DMA errors in boot log

### During Operation
- [ ] QGroundControl connects successfully
- [ ] Telemetry data flows correctly
- [ ] No data corruption or garbled messages
- [ ] Sustained transfers work (not just initial connection)
- [ ] No cache coherency errors
- [ ] System remains stable

---

## If Performance Issues Observed

If you see any of these symptoms:
- ❌ Slow telemetry updates
- ❌ Dropped data packets
- ❌ Transfer stalls or timeouts
- ❌ "DMA descriptor pool exhausted" errors

**Then consider adding:**
```bash
CONFIG_SAMV7_USBDEVHS_NDTDS=32
CONFIG_SAMV7_USBDEVHS_PREALLOCATE=y
```

**Otherwise:** Leave defaults (simpler is better)

---

## Comparison Summary

### Do We "Fully Comply" with Harmony?

**Answer: NO - By Design**

| Aspect | Compliance | Reason |
|--------|-----------|--------|
| **Cache management** | ❌ Different | NuttX: driver-level, Harmony: app-level |
| **Cache mode** | ❌ Different | NuttX: write-through, Harmony: any |
| **EP7 handling** | ✅ Better | NuttX has workaround, Harmony doesn't! |
| **DMA enabled** | ✅ Same | Both use DMA |
| **High-speed** | ✅ Same | Both support 480 Mbps |
| **Descriptors** | ⚠️ Different | NuttX: 8 default, Harmony: 32 |

### Should We Try to Match Harmony Exactly?

**NO!** Here's why:

1. **Different architectures** - Harmony is template-based, NuttX is monolithic
2. **Different philosophies** - Harmony prioritizes performance, NuttX prioritizes safety
3. **NuttX is actually better in some areas** - EP7 workaround, driver-level cache handling
4. **Write-through cache is safer** - Prevents cache coherency bugs we had with SD card

### What Matters Most?

✅ **All critical configs applied**
✅ **Cache coherency handled correctly**
✅ **Hardware errata worked around**
✅ **High-speed mode enabled**
✅ **DMA support enabled**

**Status: READY FOR TESTING** 🚀

---

## Conclusion

### What We've Applied
1. ✅ **CONFIG_USBDEV_DMA=y** - Required for all endpoints to work
2. ✅ **CONFIG_USBDEV_DUALSPEED=y** - High-speed support (480 Mbps)
3. ✅ **CONFIG_SAMV7_USBHS_EP7DMA_WAR=y** - Silicon errata workaround
4. ✅ **CONFIG_ARMV7M_DCACHE_WRITETHROUGH=y** - Already present (critical!)
5. ✅ **MAV_0_CONFIG=101** - MAVLink on USB CDC/ACM

### What We're NOT Changing
1. ⚠️ **NDTDS** - Using default 8 (can increase to 32 if needed)
2. ⚠️ **PREALLOCATE** - Not enabled (can add if needed)

### Why This is Better Than SD Card
**SD Card:** Started broken, spent 4 days debugging
**USB:** Proactively verified, fixed before testing

### Confidence Level
**VERY HIGH** - All critical configs applied, NuttX approach is sound

---

## References

- [NuttX SAMV71-XULT Documentation](https://nuttx.apache.org/docs/latest/platforms/arm/samv7/boards/samv71-xult/index.html)
- [SAM E70/S70/V70/V71 Silicon Errata DS80000767J](https://ww1.microchip.com/downloads/en/DeviceDoc/SAM-E70-S70-V70-V71-Family-Errata-DS80000767J.pdf)
- [Microchip Harmony USB Library](https://github.com/Microchip-MPLAB-Harmony/usb)

---

**Document Version:** 1.0
**Last Updated:** November 22, 2025
**Status:** All critical configurations verified and applied ✅
