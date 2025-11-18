# PX4 SAMV71-XULT Changes Documentation

**Date:** November 18, 2025
**Purpose:** Document all changes made to get PX4 running on SAMV71-XULT board with LittleFS parameter storage

---

## Summary of Changes

This document tracks all modifications made to the PX4 codebase for SAMV71-XULT board support, including:
1. LittleFS filesystem support for internal flash parameter storage
2. NSH shell compatibility fixes
3. Dataman storage path configuration
4. Build system integration

---

## 1. LittleFS Mount Command (`littlefs_mount`)

### Files Created:
- `src/systemcmds/littlefs_mount/littlefs_mount.c`
- `src/systemcmds/littlefs_mount/Kconfig`
- `src/systemcmds/littlefs_mount/CMakeLists.txt`

### Purpose:
Created a system command to mount and format LittleFS filesystems from NSH shell.

### Key Features:
- Supports mounting existing LittleFS filesystems
- Auto-format capability with `-f` flag
- Creates mount point directories automatically
- Proper error handling and reporting

### Implementation Details:
**File:** `src/systemcmds/littlefs_mount/littlefs_mount.c`
- Added `px4_getopt` for command-line parsing
- Uses `nx_mount()` with "littlefs" filesystem type
- Supports "forceformat" option for initial formatting
- Changed all `nullptr` to `NULL` for C compatibility

**File:** `src/systemcmds/littlefs_mount/Kconfig`
```kconfig
menuconfig SYSTEMCMDS_LITTLEFS_MOUNT
    bool "littlefs_mount"
    default n
    depends on PLATFORM_NUTTX
    select FS_LITTLEFS
```

---

## 2. Board Configuration Changes

### File: `boards/microchip/samv71-xult-clickboards/default.px4board`

#### Added Configuration Options:
```
CONFIG_SYSTEMCMDS_LITTLEFS_MOUNT=y
CONFIG_SYSTEMCMDS_MFT=y
```

**Purpose:** Enable the littlefs_mount command and MFT utility in the build.

---

## 3. MTD Flash Partition Setup

### File: `boards/microchip/samv71-xult-clickboards/src/init.c`

#### Flash Partition Configuration:
- **Partition Size:** 32KB (currently - needs to be 128KB for dataman)
- **Location:** Last 32KB of 2MB flash (0x001F8000 - 0x00200000)
- **MTD Offset Reported:** 0x38000 (224KB from start)
- **Device Nodes Created:**
  - `/dev/mtdblock0` - Block device for LittleFS
  - `/dev/mtdparams` - Character device wrapper

#### Key Code Sections:

**Function:** `samv71_setup_param_mtd()`
```c
#define PARAM_FLASHFS_PARTITION_SIZE   (32 * 1024)  // TODO: Increase to 128KB

static int samv71_setup_param_mtd(void)
{
    // Initialize PROGMEM driver
    sam_progmem_initialize();
    struct mtd_dev_s *progmem = progmem_initialize();

    // Get flash geometry
    struct mtd_geometry_s geo;
    progmem->ioctl(progmem, MTDIOC_GEOMETRY, &geo);

    // Calculate partition at end of flash
    const size_t progmem_size = geo.erasesize * geo.neraseblocks;
    const size_t partition_offset = progmem_size - PARAM_FLASHFS_PARTITION_SIZE;

    // Create MTD partition
    struct mtd_dev_s *param_mtd = mtd_partition(progmem, firstblock, nblocks);

    // Initialize FTL layer
    ftl_initialize(0, param_mtd);

    // Register block device
    bchdev_register("/dev/mtdblock0", "/dev/mtdparams", false);
}
```

**Critical Note:** Physical flash writes go to 0x5f8000-0x5fc000 range, which is correct for 2MB flash.

---

## 4. NuttX Configuration

### File: `boards/microchip/samv71-xult-clickboards/nuttx-config/nsh/defconfig`

#### Added/Modified Options:
```
CONFIG_MTD_PROGMEM=y
CONFIG_FS_LITTLEFS=y
CONFIG_SAMV7_PROGMEM_NSECTORS=2
```

**Purpose:** Enable NuttX MTD PROGMEM and LittleFS filesystem support.

---

## 5. Board Startup Script

### File: `boards/microchip/samv71-xult-clickboards/init/rc.board_defaults`

#### Current Implementation (NSH-compatible):
```bash
#!/bin/sh
#
# SAMV71-XULT board defaults

# Mount LittleFS, format on first boot
littlefs_mount -d /dev/mtdblock0 -m /fs/mtd_params || littlefs_mount -d /dev/mtdblock0 -m /fs/mtd_params -f

# Override dataman storage path to use internal flash instead of SD
set DATAMAN_OPT "-f /fs/mtd_params/dataman"

# Default airframe
param set-default SYS_AUTOSTART 60100
```

#### Key Design Decisions:
1. **No stderr redirection (`2>&1`)** - NSH doesn't support it
2. **No test/[ ] commands** - NSH has `CONFIG_NSH_DISABLE_TEST=y`
3. **No nested if/else** - NSH has limited conditional support
4. **Simple command chaining** - Uses `||` for fallback, `&&` for success paths
5. **Uses `/dev/mtdblock0`** - Block device required for LittleFS (not char device `/dev/mtdparams`)

#### Evolution History:
- **Version 1:** Used bash syntax with `[ ]` tests → Failed (NSH doesn't support)
- **Version 2:** Used `2>&1` stderr redirect → Failed ("too many arguments")
- **Version 3:** Used `/dev/mtdparams` → Failed (ENOTBLK - char device)
- **Version 4 (Current):** Clean NSH syntax with `/dev/mtdblock0` → Works!

---

## 6. Main Startup Script Integration

### File: `ROMFS/px4fmu_common/init.d/rcS`

#### Modified Dataman Start Section:
**Before:**
```bash
if param compare SYS_DM_BACKEND 0
then
    # dataman start default
    dataman start
fi
```

**After:**
```bash
if param compare SYS_DM_BACKEND 0
then
    # dataman start default (with optional board-specific path)
    dataman start $DATAMAN_OPT
fi
```

**Purpose:** Allow boards to override dataman storage path via `DATAMAN_OPT` variable set in `rc.board_defaults`.

---

## 7. Unicode Character Fixes

### File: `boards/microchip/samv71-xult-clickboards/src/etc/extras.d/test_px4_full.sh`

#### Issue:
ROMFS build failed with:
```
UnicodeEncodeError: 'ascii' codec can't encode character '\u2717'
```

#### Fix:
Changed Unicode checkmarks to ASCII equivalents:
- `✗` → `[X]`

**Lines affected:** 206-209

---

## Build Errors Fixed During Development

### Error 1: Unicode in Shell Script
**Error:** `UnicodeEncodeError: 'ascii' codec can't encode character '\u2717'`
**Location:** `test_px4_full.sh:206-209`
**Fix:** Replaced Unicode characters with ASCII `[X]`

### Error 2: Missing Kconfig
**Error:** `littlefs_mount` module not compiled despite being enabled
**Root Cause:** No Kconfig file to register the module
**Fix:** Created `src/systemcmds/littlefs_mount/Kconfig`

### Error 3: nullptr in C File
**Error:** `'nullptr' undeclared` in `littlefs_mount.c`
**Root Cause:** C file using C++ keyword
**Fix:** Replaced all 6 occurrences of `nullptr` with `NULL`

### Error 4: Missing Header
**Error:** `implicit declaration of function 'px4_getopt'`
**Location:** `littlefs_mount.c:86`
**Fix:** Added `#include <px4_platform_common/getopt.h>`

### Error 5: NSH Syntax Errors (Runtime)
**Error:** `nsh: test: syntax error`, `nsh: else: not valid in this context`
**Root Cause:** NSH has `CONFIG_NSH_DISABLE_TEST=y`
**Fix:** Rewrote script using pure command chaining

### Error 6: ENOTBLK Error
**Error:** `Mount failed: ENOTBLK (ret=-15)`
**Root Cause:** Trying to mount character device instead of block device
**Fix:** Changed from `/dev/mtdparams` to `/dev/mtdblock0`

### Error 7: DATAMAN_OPT Not Used
**Error:** Dataman still using `/fs/microsd/dataman` instead of flash
**Root Cause:** rcS not using `$DATAMAN_OPT` variable
**Fix:** Modified rcS to use `dataman start $DATAMAN_OPT`

### Error 8: Out of Space (Current Issue)
**Error:** `ERROR [dataman] file write failed 28` (ENOSPC)
**Root Cause:** Dataman needs 68KB but partition is only 32KB
**Fix Required:** Increase `PARAM_FLASHFS_PARTITION_SIZE` from 32KB to 128KB

---

## Current Status

### ✅ Working:
1. LittleFS mounts successfully on `/fs/mtd_params`
2. Flash operations work correctly (erase/write to 0x5f8000-0x5fc000)
3. NSH shell starts without syntax errors
4. Dataman uses correct path (`/fs/mtd_params/dataman`)
5. No garbage output from DatamanClient
6. Parameters configured to use `/fs/mtd_params`

### ❌ Issues:
1. **Flash partition too small:** 32KB insufficient for dataman (needs 68KB+)
2. **Build timestamp caching:** Version string shows old timestamp despite ROMFS being updated

### 📋 TODO (Not Implemented Yet):
1. Increase flash partition to 128KB
2. Or switch to SD card implementation
3. Enable SD card HSMCI driver in NuttX
4. Add SD card initialization in `board_app_initialize()`

---

## SD Card Implementation Notes (Future)

### What's Needed:
The SAMV71-XULT has SD card hardware (HSMCI), but it's not initialized in PX4 port.

**NuttX Support Status:** ✅ Complete
- Driver exists: `arch/arm/src/samv7/sam_hsmci.c`
- Board configs exist: Standard SAMV71-XULT configs enable it

**PX4 Port Status:** ❌ Not Configured
- No HSMCI initialization in `board_app_initialize()`
- No device registration for `/dev/mmcsd0`
- No mount for `/fs/microsd`

### Required Steps for SD Card:
1. **Enable in defconfig:**
   ```
   CONFIG_SAMV7_HSMCI0=y
   CONFIG_MMCSD=y
   CONFIG_MMCSD_MMCSUPPORT=y
   CONFIG_MMCSD_HAVECARDDETECT=y
   ```

2. **Add initialization code in `init.c`:**
   ```c
   sam_sdinitialize();  // Initialize HSMCI
   mmcsd_slotinitialize(0, ...);  // Register device
   mount("/dev/mmcsd0", "/fs/microsd", "vfat", 0, NULL);
   ```

3. **Update board config:**
   - Change ROOT_PATH back to `/fs/microsd`
   - Remove DATAMAN_OPT override

---

## Memory Usage

**Current Build:**
- Flash: 906,904 bytes / 2 MB (43.24%)
- SRAM: 22,312 bytes / 384 KB (5.67%)

**Flash Partition Layout (2MB total):**
- 0x00400000 - 0x005F8000: Application code (2,016 KB)
- 0x005F8000 - 0x00600000: LittleFS partition (32 KB)

---

## Important Technical Details

### NSH Shell Limitations:
1. **No test command:** `CONFIG_NSH_DISABLE_TEST=y` removes `[` and `test`
2. **No stderr redirect:** `2>&1` syntax not supported
3. **Limited conditionals:** Complex if/else nesting unreliable
4. **Command chaining works:** `&&` and `||` operators supported

### Flash Write Behavior:
- Reported offset: 0x38000 (224 KB from start of partition)
- Physical address: 0x5f8000 (6,160,384 bytes from 0x0)
- This is correct! Offset is within partition, physical is absolute address

### LittleFS Mount:
- **Must use block device** `/dev/mtdblock0`
- Character device `/dev/mtdparams` causes ENOTBLK error
- Auto-format on first boot with `-f` flag

### Dataman Requirements:
- Needs ~68 KB for full mission/geofence storage
- Current 32 KB partition insufficient
- Recommend 128 KB minimum for production use

---

## Files Modified Summary

**Created:**
- `src/systemcmds/littlefs_mount/littlefs_mount.c`
- `src/systemcmds/littlefs_mount/Kconfig`
- `src/systemcmds/littlefs_mount/CMakeLists.txt`

**Modified:**
- `boards/microchip/samv71-xult-clickboards/default.px4board`
- `boards/microchip/samv71-xult-clickboards/src/init.c`
- `boards/microchip/samv71-xult-clickboards/init/rc.board_defaults`
- `boards/microchip/samv71-xult-clickboards/nuttx-config/nsh/defconfig`
- `boards/microchip/samv71-xult-clickboards/src/etc/extras.d/test_px4_full.sh`
- `ROMFS/px4fmu_common/init.d/rcS`

---

## Backup Commands

### Save Current State:
```bash
cd /media/bhanu1234/Development/PX4-Autopilot-Private
git diff > CLAUDE_CHANGES.patch
```

### Restore Changes:
```bash
cd /media/bhanu1234/Development/PX4-Autopilot-Private
git apply CLAUDE_CHANGES.patch
```

### Create Backup Branch:
```bash
git checkout -b samv71-littlefs-backup
git add -A
git commit -m "Backup: SAMV71 LittleFS implementation with 32KB partition"
```

---

## Testing Results

### ✅ Successful Tests:
1. LittleFS format and mount
2. Flash erase operations (0x5f8000 region)
3. Flash write operations (16 pages × 512 bytes)
4. Dataman path configuration
5. NSH script execution without errors

### ⚠️ Known Warnings:
- Sensor drivers fail (expected - no hardware connected)
- GPS/RC invalid devices (expected - UARTs not configured)
- Missing SD card (expected - not implemented yet)
- Dataman write failures (expected - partition too small)

### ❌ Failures:
- Dataman file writes: ENOSPC (28) - partition full

---

## Next Steps

### Option A: Increase Flash Partition
1. Change `PARAM_FLASHFS_PARTITION_SIZE` to `128 * 1024`
2. Rebuild and reflash
3. Test dataman storage

### Option B: Switch to SD Card (Recommended)
1. Enable HSMCI in NuttX defconfig
2. Add SD initialization code
3. Mount `/fs/microsd`
4. Remove `DATAMAN_OPT` override
5. Use standard SD card storage paths

---

## Patch File Location

Full diff saved to: `/media/bhanu1234/Development/PX4-Autopilot-Private/CLAUDE_CHANGES.patch`

Apply with: `git apply CLAUDE_CHANGES.patch`

---

**End of Documentation**
