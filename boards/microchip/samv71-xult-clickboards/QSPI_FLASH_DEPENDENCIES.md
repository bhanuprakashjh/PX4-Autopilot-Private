# QSPI Flash Dependencies - What Functions Need It

**Hardware:** SST26VF064B 8MB QSPI Flash on SAMV71-XULT
**Status:** Not yet enabled
**Purpose:** Understanding what PX4 features depend on MTD flash storage

---

## Overview

The QSPI flash (when enabled) provides **MTD (Memory Technology Device) partitions** that PX4 uses for persistent, high-reliability storage. This document maps which PX4 features depend on MTD flash.

## MTD Partition Layout (Typical)

When QSPI flash is configured, it's divided into partitions:

```
/dev/qspiflash0 (8MB total)
├── /fs/mtd_params      (64KB)  - Parameter storage
├── /fs/mtd_caldata     (64KB)  - Factory calibration data
├── /fs/mtd_waypoints   (2MB)   - Mission waypoints/geofence
├── /fs/mtd_net         (8KB)   - Network config (optional)
├── /fs/mtd_mft_ver     (8KB)   - Manifest version (optional)
└── [remaining space available for future use]
```

---

## Core Dependencies

### 1. ⭐ Parameter System (Critical)

**Module:** `src/lib/parameters/`
**MTD Partition:** `/fs/mtd_params`

**What it does:**
- Stores all PX4 configuration parameters persistently
- Falls back to SD card (`/fs/microsd/params`) if MTD not available

**Parameters stored:**
- ~400 system parameters (SYS_*, MAV_*, SDLOG_*, etc.)
- Flight mode settings
- Tuning parameters (PID gains, etc.)
- Radio calibration
- Sensor configurations

**Impact if MTD not available:**
- ⚠️ **Minor** - Parameters stored on SD card instead
- ✅ Fully functional with SD card fallback
- ⚠️ SD card must be present
- ⚠️ Slower access than MTD flash

**Current Status on SAMV71-XULT:**
- ✅ **Working** - Using SD card (`/fs/microsd/params`)
- No MTD needed for basic operation

---

### 2. ⭐ Factory Calibration Storage (High Priority)

**Module:** `src/modules/commander/factory_calibration_storage.cpp`
**MTD Partition:** `/fs/mtd_caldata`

**What it does:**
- Stores **factory-set sensor calibrations** that should never be overwritten by user
- Protects calibration data from accidental resets
- Separate from user-adjustable parameters

**Calibration data stored:**
- **Accelerometer offsets/scales** (CAL_ACC*)
- **Gyroscope offsets/scales** (CAL_GYRO*)
- **Magnetometer offsets/scales** (CAL_MAG*)
- **Thermal compensation** (TC_*)
- Board-specific trim values

**Impact if MTD not available:**
- ⚠️ **Medium** - Factory calibrations stored in regular params instead
- ⚠️ User can accidentally wipe factory calibration with `param reset`
- ⚠️ Calibration lost if SD card removed/formatted
- ⚠️ Production units should have separate factory storage

**Controlled by parameter:**
```bash
SYS_FAC_CAL_MODE = 0  # Disabled (default)
SYS_FAC_CAL_MODE = 1  # All sensors
SYS_FAC_CAL_MODE = 2  # All except magnetometer
```

**Current Status on SAMV71-XULT:**
- ⚠️ **Not configured** - Factory cal mode disabled
- Calibrations stored in regular parameters
- Acceptable for development, problematic for production

**Production Recommendation:**
- ✅ Enable QSPI flash for separate factory calibration storage
- Set `SYS_FAC_CAL_MODE = 1` after factory calibration
- Prevents end-users from losing factory calibration

---

### 3. ⭐ Mission/Waypoint Storage (High Priority for Autonomy)

**Module:** `src/modules/navigator/` + `src/modules/dataman/`
**MTD Partition:** `/fs/mtd_waypoints` (or `/fs/mtd`)

**What it does:**
- Stores autonomous mission waypoints
- Stores geofence boundaries
- Stores rally points (safe return points)
- Stores safe points

**Data stored:**
- **Mission waypoints** - GPS coordinates, altitude, actions
- **Geofence** - Boundary polygons, max/min altitude
- **Rally points** - Emergency landing sites
- **Safe points** - Pre-defined safe locations

**Used by:**
- **Navigator** module - Mission execution
- **Commander** module - Geofence monitoring
- **RTL (Return to Launch)** - Rally point selection
- **Mission upload/download** via QGroundControl

**Impact if MTD not available:**
- ⚠️ **Medium-High** - Missions may fallback to RAM storage
- ⚠️ Mission lost on reboot
- ⚠️ Limited mission size (RAM vs flash capacity)
- ❌ Not suitable for long-range autonomous missions

**Fallback behavior:**
- Dataman tries `/fs/mtd` first
- Falls back to RAM storage if MTD unavailable
- RAM storage = volatile (lost on power cycle)

**Current Status on SAMV71-XULT:**
```
ERROR [bsondump] open '/fs/mtd_caldata' failed (2)
```
- ⚠️ **Not working** - Dataman looking for MTD partition
- Likely using RAM fallback
- Missions not persistent across reboots

**Mission Flight Recommendation:**
- ✅ Enable QSPI flash for reliable autonomous missions
- ⚠️ Can test without MTD using RAM storage (non-persistent)

---

### 4. Network Configuration (Low Priority)

**Module:** Various network modules
**MTD Partition:** `/fs/mtd_net`

**What it does:**
- Stores network settings (IP address, gateway, DNS)
- Ethernet MAC address
- WiFi credentials (if applicable)

**Impact if MTD not available:**
- ⚠️ **Low** - Network config defaults used
- Must reconfigure network on each boot
- Not critical for SAMV71-XULT (no Ethernet/WiFi by default)

**Current Status on SAMV71-XULT:**
- ✅ **Not needed** - No network hardware configured

---

### 5. Bootloader Settings (Very Low Priority)

**Module:** Bootloader
**MTD Partition:** Boot sector or dedicated partition

**What it does:**
- Store bootloader configuration
- Boot flags, fallback settings

**Impact if MTD not available:**
- ✅ **None** - Not used on SAMV71-XULT
- Bootloader configured at compile time

---

## Module-by-Module Analysis

### Modules That **Require** MTD Flash:

**None are strictly required - all have fallbacks**

### Modules That **Benefit** from MTD Flash:

| Module | MTD Usage | Fallback | Impact if No MTD |
|--------|-----------|----------|-----------------|
| **Parameters** | /fs/mtd_params | SD card | ⚠️ Minor - works fine on SD |
| **Factory Calibration** | /fs/mtd_caldata | Mixed with params | ⚠️ Medium - can lose factory cal |
| **Dataman** | /fs/mtd_waypoints | RAM storage | ⚠️ Medium - missions not persistent |
| **Navigator** | via Dataman | RAM missions | ⚠️ Medium - limited autonomy |
| **Commander** | via Dataman | RAM geofence | ⚠️ Medium - geofence not persistent |
| **Sensors** | via Factory Cal | Regular params | ⚠️ Low - calibration less protected |

### Modules That **Don't Care** About MTD Flash:

- ✅ **Flight Control** - Works entirely in RAM
- ✅ **EKF2** - State estimation in RAM
- ✅ **Attitude/Position Control** - Real-time, no storage
- ✅ **Sensors** - Real-time sensor reads
- ✅ **MAVLink** - Communication, no storage
- ✅ **Logger** - Uses SD card, not MTD
- ✅ **Commander** (basic) - Flight modes in RAM
- ✅ **RC Input** - No persistent storage

---

## Flight Capability Matrix

### Without QSPI Flash (Current State):

| Flight Mode | Status | Notes |
|-------------|--------|-------|
| **Manual** | ✅ Full | No storage needed |
| **Stabilized** | ✅ Full | No storage needed |
| **Altitude Hold** | ✅ Full | No storage needed |
| **Position Hold** | ✅ Full | No storage needed |
| **Acro** | ✅ Full | No storage needed |
| **RTL (Return to Launch)** | ✅ Full | Uses takeoff position from RAM |
| **Mission (simple)** | ⚠️ Limited | RAM-only missions, lost on reboot |
| **Mission (complex)** | ❌ Limited | Limited waypoints, not persistent |
| **Geofence** | ⚠️ Limited | RAM-only, lost on reboot |
| **Rally Points** | ❌ Not persistent | Lost on power cycle |

### With QSPI Flash Enabled:

| Flight Mode | Status | Improvement |
|-------------|--------|-------------|
| **Manual** | ✅ Full | No change |
| **Stabilized** | ✅ Full | No change |
| **Altitude Hold** | ✅ Full | No change |
| **Position Hold** | ✅ Full | No change |
| **Acro** | ✅ Full | No change |
| **RTL** | ✅ Full | Rally points available |
| **Mission (simple)** | ✅ Full | Missions persist across reboots |
| **Mission (complex)** | ✅ Full | Thousands of waypoints possible |
| **Geofence** | ✅ Full | Geofence persists |
| **Rally Points** | ✅ Full | Multiple rally points persistent |

---

## When Do You Actually NEED QSPI Flash?

### ✅ **Not Needed** (Current SD Card Storage is Fine):

1. **Development/Testing**
   - Parameter tuning
   - Sensor calibration
   - Manual flight testing
   - Line-of-sight operation

2. **Simple Flight Operations**
   - Stabilized/manual flight
   - Position hold
   - Simple RTL

3. **Learning/Education**
   - Understanding PX4
   - Code development
   - Hardware prototyping

### ⚠️ **Recommended** (QSPI Flash Would Help):

1. **Autonomous Missions**
   - Waypoint missions that survive reboots
   - Persistent geofence
   - Rally point usage

2. **Production Hardware**
   - Factory calibration protection
   - Separate calibration from user params
   - Professional reliability

3. **Long-Range Operations**
   - Pre-programmed missions
   - Failsafe configurations
   - Emergency procedures

### ❌ **Required** (Must Have QSPI Flash):

1. **Commercial Operations**
   - Regulatory compliance may require persistent storage
   - Factory calibration separation required
   - Audit trail of configurations

2. **Safety-Critical Applications**
   - Medical delivery drones
   - Emergency response
   - Infrastructure inspection

3. **Unattended Operations**
   - Missions that span multiple flights
   - Remote operations without SD access
   - Harsh environments where SD might fail

---

## Current SAMV71-XULT Status

### What Works Now (Without QSPI Flash):

✅ **Parameter System**
- Storage: `/fs/microsd/params`
- Status: Fully functional
- Impact: None

✅ **Basic Flight Operations**
- All manual/stabilized modes work
- Position hold works
- RTL works

✅ **Sensor Calibration**
- Stored in regular parameters
- Persists on SD card
- User can calibrate normally

### What Doesn't Work (Without QSPI Flash):

⚠️ **Factory Calibration Separation**
- Error: `/fs/mtd_caldata not found`
- Impact: Factory cal mixed with user params
- Workaround: Disable factory cal mode (default)

⚠️ **Persistent Missions**
- Error: `/fs/mtd` not found
- Impact: Missions stored in RAM (volatile)
- Workaround: Upload mission before each flight

⚠️ **Dataman Storage**
- Error: MTD partitions not found
- Impact: Using RAM fallback
- Workaround: Acceptable for testing

---

## Implementation Priority

### If you're doing **Development/Testing:**
**Priority: LOW** - QSPI flash not needed
- Current SD card storage is adequate
- All basic functionality works
- Can defer QSPI implementation

### If you're doing **Autonomous Missions:**
**Priority: MEDIUM** - QSPI flash helpful but not required
- Implement before long-range missions
- RAM storage works for short-term testing
- Upload missions before each flight as workaround

### If you're building **Production Hardware:**
**Priority: HIGH** - QSPI flash strongly recommended
- Implement before volume production
- Professional reliability
- Factory calibration protection essential

---

## Alternative: Use SD Card for Everything

**Current approach is completely valid:**

| Feature | SD Card | QSPI Flash |
|---------|---------|------------|
| **Parameters** | ✅ Working | ⚠️ Faster access |
| **Calibration** | ⚠️ User can wipe | ✅ Protected |
| **Missions** | ⚠️ Volatile (RAM) | ✅ Persistent |
| **Capacity** | ✅ Gigabytes | ⚠️ Only 8MB |
| **Speed** | ⚠️ ~3 MB/s write | ✅ ~10+ MB/s |
| **Reliability** | ⚠️ Can corrupt | ✅ Very reliable |
| **Cost** | ✅ Already have | ✅ Already on board! |

**Bottom Line:**
- SD card works fine for parameters
- QSPI flash adds reliability and features
- Both can coexist (QSPI for critical data, SD for logs)

---

## Summary

### What Depends on QSPI Flash:

**Critical Dependencies:** None - all have fallbacks
**Nice-to-Have Dependencies:**
1. Factory calibration storage (production)
2. Persistent mission storage (autonomy)
3. Parameter storage (faster, more reliable)

### Current SAMV71-XULT Functionality:

**Without QSPI Flash:**
- ✅ Manual flight: Full
- ✅ Position hold: Full
- ✅ RTL: Full
- ✅ Parameters: Full (SD card)
- ⚠️ Missions: Limited (RAM-based)
- ⚠️ Factory cal: Not separated

**With QSPI Flash Enabled:**
- ✅ Everything above, plus:
- ✅ Persistent missions
- ✅ Protected factory calibration
- ✅ Professional reliability
- ✅ Faster parameter access

### Recommendation:

**For your current stage (development/testing):**
- ⚠️ QSPI flash implementation is **optional**
- ✅ Current SD card storage is **adequate**
- ✅ All core functionality works without QSPI
- 📅 Implement QSPI before moving to production or autonomous operations

**Time investment:**
- ~9 hours to implement QSPI flash
- Immediate benefit: Professional-grade storage
- Long-term benefit: Production-ready platform

---

**Document Version:** 1.0
**Date:** November 24, 2025
**Status:** Comprehensive analysis complete
**Next Action:** Decide when to implement based on project phase
