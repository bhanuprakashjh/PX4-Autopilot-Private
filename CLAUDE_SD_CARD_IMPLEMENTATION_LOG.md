# SD CARD IMPLEMENTATION - CONTINUOUS CHANGE LOG
**Board:** SAMV71-XULT-Clickboards
**Project:** PX4 Autopilot v1.17.0-alpha
**Goal:** Replace insufficient internal flash (32KB LittleFS) with SD card storage (needed 68KB+ for dataman)
**Started:** 2025-11-18
**Last Updated:** 2025-11-18 14:10

---

## Implementation Plan (5 Stages)

- **Stage 0:** Reference study of existing boards (fmu-v6x) - ✅ COMPLETE
- **Stage 1:** Enable HSMCI/MMC in NuttX configs - ✅ COMPLETE
- **Stage 2:** SD hardware bring-up in board_app_initialize() - 🔄 IN PROGRESS
- **Stage 3:** Repoint PX4 to SD storage - ⏸️ BLOCKED (Stage 2 incomplete)
- **Stage 4:** Cleanup and fallbacks - ⏸️ PENDING

---

## Change History (Chronological)

### Change #1: Enable NuttX SD/MMC Drivers (Stage 1)
**Date:** 2025-11-18
**File:** `boards/microchip/samv71-xult-clickboards/nuttx-config/nsh/defconfig`
**Changes:**
```kconfig
CONFIG_MMCSD=y
CONFIG_MMCSD_MMCSUPPORT=y
CONFIG_MMCSD_HAVECARDDETECT=y
CONFIG_MMCSD_SDIO=y
CONFIG_MMCSD_NSLOTS=1
# CONFIG_MMCSD_SPI is not set
CONFIG_SDIO_BLOCKSETUP=y
CONFIG_SDIO_DMA=y
CONFIG_SAMV7_HSMCI0=y
```
**Build Result:** ✅ Success
**Test Result:** Boots normally, no crashes

---

### Change #2: Implement SD Card Initialization Code (Stage 2)
**Date:** 2025-11-18
**File:** `boards/microchip/samv71-xult-clickboards/src/init.c`
**Changes:**

**Added Headers:**
```c
#include <nuttx/sdio.h>
#include <sys/mount.h>
#include <sys/stat.h>

#ifdef CONFIG_SAMV7_HSMCI0
#  include "sam_hsmci.h"
#endif
```

**Added Function (lines 101-148):**
```c
static int samv71_sdcard_initialize(void)
{
    int ret;
    syslog(LOG_INFO, "[sd] Initializing HSMCI slot 0\n");

    struct sdio_dev_s *sdio = sdio_initialize(0);
    if (sdio == NULL) {
        syslog(LOG_ERR, "[sd] sdio_initialize failed\n");
        return -ENODEV;
    }

    ret = mmcsd_slotinitialize(0, sdio);
    if (ret < 0) {
        syslog(LOG_ERR, "[sd] mmcsd_slotinitialize failed: %d\n", ret);
        return ret;
    }

    syslog(LOG_INFO, "[sd] Driver initialized, notifying media presence\n");
    sdio_mediachange(sdio, true);

    (void)mkdir("/fs", 0777);
    (void)mkdir("/fs/microsd", 0777);
    up_mdelay(1000);

    syslog(LOG_INFO, "[sd] Attempting to mount /dev/mmcsd0\n");
    ret = nx_mount("/dev/mmcsd0", "/fs/microsd", "vfat", 0, NULL);

    if (ret < 0) {
        syslog(LOG_ERR, "[sd] Mount failed: %d (%s)\n", ret, strerror(-ret));
        return ret;
    }

    syslog(LOG_INFO, "[sd] Successfully mounted /fs/microsd\n");
    return OK;
}
```

**Integrated in board_app_initialize() (lines 245-250):**
```c
#ifdef CONFIG_SAMV7_HSMCI0
    syslog(LOG_ERR, "[boot] Initializing SD card...\n");
    if (samv71_sdcard_initialize() < 0) {
        syslog(LOG_ERR, "[boot] SD initialization failed (continuing)\n");
    }
#endif
```

**Build Errors Fixed:**
- Missing `sdio_initialize()` declaration → Fixed by adding `#include "sam_hsmci.h"`
- `LOG_WARN` undeclared → Changed to `LOG_WARNING` (NuttX uses WARNING not WARN)

**Build Result:** ✅ Success (after fixes)
**Test Result:** ❌ SD card mount fails with ENODEV error
**Console Output:**
```
[sd] Initializing HSMCI slot 0
[sd] Driver initialized, notifying media presence
[sd] Attempting to mount /dev/mmcsd0
[sd] Mount failed: -19 (ENODEV)
```
**Issue:** Device node `/dev/mmcsd0` created but card never responds to open() calls

---

### Change #3: Disable Hardware Card Detect
**Date:** 2025-11-18
**File:** `boards/microchip/samv71-xult-clickboards/nuttx-config/nsh/defconfig`
**Rationale:** SAMV71-XULT has card detect on PD18, but forcing "always present" to bypass potential CD pin issues
**Changes:**
```kconfig
# CONFIG_MMCSD_HAVE_CARDDETECT is not set
```
(Changed from `CONFIG_MMCSD_HAVECARDDETECT=y`)

**Build Result:** ✅ Success
**Test Result:** ❌ Still ENODEV, no improvement
**Conclusion:** Card detect was not the issue

---

### Change #4: Add Cache-Proof DMA Settings
**Date:** 2025-11-18
**File:** `boards/microchip/samv71-xult-clickboards/nuttx-config/nsh/defconfig`
**Rationale:** SAMV7 has D-cache enabled; RDPROOF/WRPROOF ensure DMA waits for cache flush/update
**Changes:**
```kconfig
CONFIG_SAMV7_HSMCI_RDPROOF=y
CONFIG_SAMV7_HSMCI_WRPROOF=y
```

**Build Result:** ✅ Success
**Test Result:** ❌ Still ENODEV, no improvement
**Conclusion:** Not a DMA cache coherency issue

---

### Change #5: Enable HSMCI Command Debug
**Date:** 2025-11-18
**File:** `boards/microchip/samv71-xult-clickboards/nuttx-config/nsh/defconfig`
**Rationale:** Need command-level debug to see what SD initialization is doing
**Changes:**
```kconfig
# CONFIG_NDEBUG is not set
CONFIG_DEBUG_FEATURES=y
CONFIG_DEBUG_WARN=y
CONFIG_DEBUG_INFO=y
CONFIG_DEBUG_FS_INFO=y
CONFIG_SAMV7_HSMCI_CMDDEBUG=y
```

**Build Result:** ✅ Success
**Test Result:** ❌ No debug output appeared (complex CONFIG dependencies not fully resolved)
**Issue:** Debug settings didn't take effect in final `.config` file

---

### Change #6: Disable Dataman to Prevent System Hang
**Date:** 2025-11-18
**Problem:** Dataman and dataman_client were hanging system with garbage output, preventing NSH prompt access

**File 1:** `boards/microchip/samv71-xult-clickboards/init/rc.board_defaults`
**Changes:**
```bash
# TEMPORARILY DISABLED - filesystem issues causing system hang
# littlefs_mount -d /dev/mtdblock0 -m /fs/mtd_params || littlefs_mount -d /dev/mtdblock0 -m /fs/mtd_params -f

# TEMPORARILY DISABLED - causes dataman to hang when filesystem not available
# set DATAMAN_OPT "-f /fs/mtd_params/dataman"
```
**Test Result:** ❌ System still hung with dataman

**File 2:** `ROMFS/px4fmu_common/init.d/rcS` (lines 274-284)
**Changes:**
```bash
# TEMPORARILY DISABLED - dataman hangs when no storage available
# if param compare -s SYS_DM_BACKEND 1
# then
#     dataman start -r
# else
#     if param compare SYS_DM_BACKEND 0
#     then
#         dataman start $DATAMAN_OPT
#     fi
# fi
```
**Test Result:** ❌ System still hung (dataman_client still being called)

---

### Change #7: Disable Navigator to Prevent Dataman Client Hang
**Date:** 2025-11-18
**File:** `ROMFS/px4fmu_common/init.d/rcS` (line 537)
**Rationale:** Navigator calls dataman_client which hangs waiting for dataman service
**Changes:**
```bash
# Start the navigator.
# TEMPORARILY DISABLED - navigator requires dataman which isn't available
# navigator start
```

**Build Result:** ✅ Success
- Flash: 913800 B / 2 MB (43.57%)
- RAM: 23816 B / 384 KB (6.06%)

**Test Result:** ❌ System still hung - dataman_client still being called
**Console Output:**
```
ERROR [dataman_client] timeout after 1000 ms!
ERROR [dataman_client] Failed to get client ID!
ERROR [dataman_��f��`ff~���f�f~��� [garbage]
```
**Issue:** Mavlink module's mission handler also calls dataman_client

---

### Change #8: Disable Mavlink USB Auto-Start
**Date:** 2025-11-18
**File:** `ROMFS/px4fmu_common/init.d/rcS` (lines 515-524)
**Rationale:** Mavlink mission handler (mavlink_mission.cpp) calls dataman_client during initialization, causing hang
**Changes:**
```bash
# TEMPORARILY DISABLED - mavlink mission handler calls dataman_client which hangs
# if param greater -s SYS_USB_AUTO -1
# then
#     if ! cdcacm_autostart start
#     then
#         sercon
#         echo "Starting MAVLink on /dev/ttyACM0"
#         mavlink start -d /dev/ttyACM0
#     fi
# fi
```

**Build Result:** ✅ Success
- Flash: 913688 B / 2 MB (43.57%)
- RAM: 23816 B / 384 KB (6.06%)

**Test Result:** ❌ System still hung - dataman_client still being called
**Console Output:**
```
ERROR [dataman_client] timeout after 1000 ms!
ERROR [dataman_client] Failed to get client ID!
```
**Issue:** Modules cannot be disabled at build level due to interdependencies (commander needs GF_ACTION from navigator, dataman_client library needs CONFIG_NUM_MISSION_ITMES_SUPPORTED from dataman module)

**Root Cause Discovery:** Modules are compiled into binary and something is auto-starting dataman even with rcS startup disabled

---

### Change #9: Disable Default Airframe Auto-Start
**Date:** 2025-11-18
**File:** `boards/microchip/samv71-xult-clickboards/init/rc.board_defaults` (lines 14-15)
**Rationale:** Airframe initialization (SYS_AUTOSTART) may trigger automatic dataman start
**Changes:**
```bash
# Default airframe
# TEMPORARILY DISABLED - testing if this triggers dataman
# param set-default SYS_AUTOSTART 60100
```

**Build Result:** ✅ Success
- Flash: 913640 B / 2 MB (43.57%)
- RAM: 23816 B / 384 KB (6.06%)

**Test Result:** ❌ System still hung - dataman_client still being called
**Console Output:**
```
ERROR [dataman_client] timeout after 1000 ms!
ERROR [dataman_client] Failed to get client ID!
```
**Issue:** Airframe auto-start not the cause

**Root Cause Discovery:** cdcacm_autostart driver automatically starts mavlink when USB VBUS detected or MAVLink heartbeat received

---

### Change #10: Disable CDC/ACM Auto-Start Driver (BREAKTHROUGH!)
**Date:** 2025-11-18
**File:** `boards/microchip/samv71-xult-clickboards/default.px4board` (line 15)
**Rationale:** cdcacm_autostart automatically launches mavlink when USB is connected, which initializes mavlink_mission handler with DatamanClient, causing dataman_client hang
**Root Cause:** `src/drivers/cdcacm_autostart/cdcacm_autostart.cpp:495` - `start_mavlink()` function auto-starts mavlink on USB detection
**Changes:**
```kconfig
# USB CDC/ACM
# CONFIG_DRIVERS_CDCACM_AUTOSTART is not set  # TEMPORARILY DISABLED - auto-starts mavlink which calls dataman_client
```

**Build Result:** ✅ Success
- Flash: 910440 B / 2 MB (43.41%) - 3.2KB smaller than before!
- RAM: 23752 B / 384 KB (6.04%)

**Test Result:** ❌ System still hung - dataman_client still being called
**Console Output:** Still getting dataman_client timeout errors

**Root Cause Discovery:** Modules with static DatamanClient members (navigator, mavlink_mission) instantiate DatamanClient at module init time, triggering connection attempts **regardless of rcS startup scripts**

---

### Change #11: Patch DatamanClient to Fail Silently (CRITICAL FIX!)
**Date:** 2025-11-18
**Files:**
- `src/lib/dataman_client/DatamanClient.hpp` (line 187)
- `src/lib/dataman_client/DatamanClient.cpp` (lines 48-75, 153, 190, 226, 253, 292, 333)

**Rationale:** DatamanClient is compiled into binary and instantiated by multiple modules at startup. Cannot be disabled at build level due to hard dependencies. Instead, patch client to detect missing dataman service and fail silently without console spam.

**Root Cause Analysis (from ChatGPT feedback):**
- dataman_client **library** is compiled in even when dataman **module** isn't started
- Commander needs navigator's GF_ACTION parameter (can't disable navigator)
- dataman_client needs CONFIG_NUM_MISSION_ITMES_SUPPORTED from dataman Kconfig (can't disable dataman at build)
- Several modules instantiate DatamanClient **statically** (navigator, mavlink_mission, mission_feasibility_checker)
- Static members construct at module load time, **before** rcS scripts run
- Disabling runtime start doesn't prevent construction → timeout spam inevitable

**Solution:** Graceful degradation pattern
- Add `_available` flag to track service availability
- Modify constructor to set `_available = false` if handshake fails (silently, no error print)
- Guard all API methods (`readSync`, `writeSync`, `clearSync`, `readAsync`, `writeAsync`, `clearAsync`) to return `false` immediately if `!_available`
- Modules using DatamanClient will see failures but won't spam console

**Changes:**

**DatamanClient.hpp:**
```cpp
bool _available{false};  // TEMPORARY: Silently fail if dataman service not available
```

**DatamanClient.cpp constructor:**
```cpp
if (_dataman_response_sub < 0) {
    // TEMPORARY PATCH: Silently fail if dataman service not available (for SD card debugging)
    _available = false;
} else {
    // ... handshake code ...
    if (success && (response.client_id > CLIENT_ID_NOT_SET)) {
        _client_id = response.client_id;
        _available = true;  // TEMPORARY PATCH: Service is available
    } else {
        // TEMPORARY PATCH: Silently fail if dataman service not available
        _available = false;
    }
}
```

**All API functions (readSync, writeSync, clearSync, readAsync, writeAsync, clearAsync):**
```cpp
if (!_available) { return false; }  // TEMPORARY PATCH: Silently fail if service unavailable
```

**Build Result:** ✅ Success
- Flash: 910384 B / 2 MB (43.41%)
- RAM: 23752 B / 384 KB (6.04%)

**Test Result:** ❌ Still getting "ERROR [dataman_" followed by garbage

**Issue:** syncHandler() (called from constructor) still prints timeout error at line 145, px4_poll error at line 103

**Additional Fix Applied:**
- Commented out timeout error print in syncHandler (line 145-147)
- Commented out px4_poll error print in syncHandler (line 103-104)

**Build Result (after additional fix):** ✅ Success
- Flash: 910264 B / 2 MB (43.40%) - 120B smaller!
- RAM: 23752 B / 384 KB (6.04%)

**Test Result:** ❌ Still hanging with "WARN [mavlin" followed by garbage

**Issue:** `mavlink boot_complete` command at rcS line 683 runs even when no mavlink instances started, tries to print warning, system hangs

---

### Change #12: Disable Mavlink Boot Complete Command
**Date:** 2025-11-18
**File:** `ROMFS/px4fmu_common/init.d/rcS` (line 683)
**Rationale:** `mavlink boot_complete` command runs unconditionally at end of boot, tries to notify mavlink instances. With no instances running, prints warning and hangs system.
**Changes:**
```bash
# TEMPORARILY DISABLED - no mavlink instances running during SD debug
# mavlink boot_complete
```

**Build Result:** ✅ Success
- Flash: 910240 B / 2 MB (43.40%) - 24B smaller
- RAM: 23752 B / 384 KB (6.04%)

**Test Result:** ❌ Still hanging with "WARN [mavli" followed by garbage

**Issue:** Mavlink warning still printing despite boot_complete disabled. Cannot disable mavlink at build level (flight_mode_manager depends on MNT_MAN_PITCH parameter).

---

### Change #13: Stop All Mavlink Instances in Board Extras
**Date:** 2025-11-18
**File:** `boards/microchip/samv71-xult-clickboards/init/rc.board_extras` (line 7)
**Rationale:** Some mavlink warning printing during boot causes console hang due to known console buffer bug. Adding `mavlink stop-all` in board extras to kill any mavlink instances before they can print warnings.
**Changes:**
```bash
# TEMPORARILY DISABLED: Stop all mavlink instances for SD card debugging (console buffer bug causes hangs on warnings)
mavlink stop-all >/dev/null 2>&1
```

**Build Result:** ✅ Success
- Flash: 910280 B / 2 MB (43.41%)
- RAM: 23752 B / 384 KB (6.04%)

**Test Result:** ❌ Still hanging - mavlink stop-all in board_extras runs too late

**Issue:** rc.board_extras is sourced at line 589 of rcS, long after modules start and warnings print

---

### Change #14: Move Mavlink Stop to Very Early in rcS
**Date:** 2025-11-18
**Files:**
- `ROMFS/px4fmu_common/init.d/rcS` (line 51) - Added mavlink stop-all right after "ver all"
- `boards/microchip/samv71-xult-clickboards/init/rc.board_extras` (removed duplicate)

**Rationale:** Move `mavlink stop-all` to absolute beginning of rcS (right after version print, before any module starts) to prevent any mavlink code from running and printing warnings that trigger console buffer bug.

**Changes:**
```bash
ver all

# TEMPORARILY DISABLED: Stop all mavlink instances EARLY for SD card debugging (console buffer bug causes hangs on warnings)
mavlink stop-all >/dev/null 2>&1
```

**Build Result:** ✅ Success
- Flash: 910240 B / 2 MB (43.40%)
- RAM: 23752 B / 384 KB (6.04%)

**Test Result:** ❌ Still hanging - rc.serial (line 512 of rcS) starts mavlink instances AFTER our early stop-all

**Root Cause:** Auto-generated `rc.serial` file contains `mavlink start` commands (lines 54, 93, 132) for MAV_0_CONFIG, MAV_1_CONFIG, MAV_2_CONFIG. These run after our line 51 stop-all.

---

### Change #15: Stop Mavlink After rc.serial Sources It
**Date:** 2025-11-18
**File:** `ROMFS/px4fmu_common/init.d/rcS` (line 515)
**Rationale:** rc.serial (sourced at line 512) auto-starts mavlink for configured MAV ports. Add second `mavlink stop-all` immediately after rc.serial sources to catch these instances.

**Changes:**
```bash
. ${R}etc/init.d/rc.serial

# TEMPORARILY DISABLED: Stop mavlink instances started by rc.serial (console buffer bug causes hangs on warnings)
mavlink stop-all >/dev/null 2>&1
```

**Build Result:** ✅ Success
- Flash: 910264 B / 2 MB (43.40%)
- RAM: 23752 B / 384 KB (6.04%)

**Test Result:** ❌ Still hanging - mavlink instances started by rc.serial print warning BEFORE our stop-all at line 306 can kill them

**Root Cause:** rc.serial starts mavlink → mavlink immediately prints warning → system hangs → our stop-all never executes

---

### Change #16: Disable MAVLink Serial Port Configs
**Date:** 2025-11-18
**File:** `boards/microchip/samv71-xult-clickboards/init/rc.board_defaults` (lines 6-8)
**Rationale:** Setting MAV_0_CONFIG, MAV_1_CONFIG, MAV_2_CONFIG to 0 (disabled) prevents rc.serial from starting mavlink instances in the first place. This is better than trying to stop them after they start (and print warnings that hang the system).

**Changes:**
```bash
# TEMPORARILY DISABLED: Disable all MAVLink instances for SD card debugging (console buffer bug causes hangs)
param set-default MAV_0_CONFIG 0
param set-default MAV_1_CONFIG 0
param set-default MAV_2_CONFIG 0
```

**Build Result:** ✅ Success
- Flash: 910320 B / 2 MB (43.41%)
- RAM: 23752 B / 384 KB (6.04%)

**Test Result:** ❌ Still hanging with "WARN [mavlin" followed by garbage

**Issue:** `param set-default` only sets parameter if it's currently undefined. The MAV_*_CONFIG parameters have compiled-in defaults, so the set-default commands have no effect. Mavlink still auto-starts.

---

### Change #17: Disable Console Buffer Entirely
**Date:** 2025-11-18
**File:** `boards/microchip/samv71-xult-clickboards/src/board_config.h` (line 133)
**Rationale:** After 16 attempts to suppress individual warning sources, attacking the root problem instead: THE CONSOLE BUFFER ITSELF. The board has a KNOWN console buffer bug (documented in defconfig) where any warning/error causes system hang mid-print with garbage. Instead of trying to suppress every possible warning, disable the console buffer feature entirely by commenting out `BOARD_ENABLE_CONSOLE_BUFFER`.

**Changes:**
```c
// TEMPORARILY DISABLED: Console buffer causes system hang on warnings/errors (known board issue)
// #define BOARD_ENABLE_CONSOLE_BUFFER
```

**Build Result:** ✅ Success
- Flash: 909680 B / 2 MB (43.32%) - **640B smaller!**
- RAM: 19204 B / 384 KB (4.88%) - **4.5 KB RAM freed!**

**Test Result:** ✅ **MAJOR BREAKTHROUGH!** System no longer hangs!
- Boot progresses 90% successfully
- All errors/warnings print cleanly (SD, params, sensors, GPS)
- Still gets garbage at "WARN [mavli..." but much later

**Issue:** NSH prompt exists but buried under mavlink binary garbage. Mavlink starts, can't talk to dataman (which is silent now), dumps binary mission buffers to console.

---

### Change #18: Attempt to Disable MAVLink via param set (FAILED)
**Date:** 2025-11-18
**File:** `boards/microchip/samv71-xult-clickboards/init/rc.board_mavlink`
**Rationale:** User's analysis: NSH prompt exists but buried under mavlink binary garbage. Try using `param set` to override MAV_*_CONFIG.

**Test Result:** ❌ **FAILED** - `param set` requires storage backend
- Boot log showed: `ERROR [parameters] param_set failed to store param MAV_0_CONFIG`
- Parameters can't be set in-memory without SD or working flash params
- Mavlink still started, still dumped garbage

---

### Change #19: Override Serial Port Detection (SUCCESS!)
**Date:** 2025-11-18
**Files:**
- `boards/microchip/samv71-xult-clickboards/init/rc.board_mavlink` (modified)
- `boards/microchip/samv71-xult-clickboards/src/etc/init.d/rc.serial_port` (NEW - overlay file)

**Rationale:** Since `param set` doesn't work without storage, attack the problem differently. The auto-generated `rc.serial` script sources `rc.serial_port` to determine which device to use for each serial port (GPS, RC, MAVLink). By creating a board-specific override of `rc.serial_port` in the overlay directory (`src/etc/init.d/`) that ALWAYS returns `SERIAL_DEV=none`, we prevent rc.serial from starting ANY serial devices including mavlink.

**Changes:**
```bash
# boards/microchip/samv71-xult-clickboards/src/etc/init.d/rc.serial_port
set SERIAL_DEV none
set BAUD_PARAM none
```

```bash
# boards/microchip/samv71-xult-clickboards/init/rc.board_mavlink
echo "INFO: Board mavlink config - disabling all mavlink for SD debugging"
set SERIAL_DEV none  # (Won't help but doesn't hurt)
```

**Build Result:** ✅ Success
- Flash: 909680 B / 2 MB (43.32%)
- RAM: 19640 B / 384 KB (4.99%)
- Overlay confirmed: rc.serial_port present in build ROMFS

**Test Result:** ✅ **SUCCESS!** 🎉

**How It Works:**
1. rc.serial auto-generated by build system
2. For each port, rc.serial does: `set PRT MAV_0_CONFIG; . $PRT_F` (sources rc.serial_port)
3. Default rc.serial_port reads parameter and sets SERIAL_DEV
4. Our override ignores parameter, always sets SERIAL_DEV=none
5. rc.serial checks: `if [ $SERIAL_DEV != none ]` → ALWAYS FALSE → skips mavlink start!

**Result:** NO GPS, NO RC input, NO mavlink → **CLEAN NSH PROMPT ACHIEVED!**

**Breakthrough:** After 19 changes fighting console bugs, dataman hangs, and mavlink garbage, we finally have clean NSH access for SD card debugging!

**Revert Plan:** Delete `src/etc/init.d/rc.serial_port` and `init/rc.board_mavlink` when serial devices needed

---

### Change #20: Disable Logger to Prevent Hang
**Date:** 2025-11-18
**Files:**
- `boards/microchip/samv71-xult-clickboards/init/rc.logging` (NEW FILE)
- `ROMFS/CMakeLists.txt` (added rc.logging to OPTIONAL_BOARD_RC list)

**Rationale:** After Change #19, system reached NSH prompt on first boot but hung on subsequent resets. Boot log showed logger starting successfully but then hanging:
```
INFO  [logger] logger started (mode=all)
INFO  [logger]
[HANG]
```
Logger was trying to write to non-existent storage (SD failed, flash params failed), causing system hang. Created board-specific rc.logging override that does nothing instead of starting logger.

**Changes:**
```bash
# boards/microchip/samv71-xult-clickboards/init/rc.logging
echo "INFO: Logging disabled for SD card debugging"
```

```cmake
# ROMFS/CMakeLists.txt (line 200)
list(APPEND OPTIONAL_BOARD_RC
	rc.board_defaults
	rc.board_sensors
	rc.board_extras
	rc.board_mavlink
	rc.logging  # Added this line
)
```

**Build Result:** ✅ Success
- Flash: 909680 B / 2 MB (43.32%)
- RAM: 19640 B / 384 KB (4.99%)

**Test Result:** ⏳ Flashed, ready for testing

**Expected Result:** System boots fully without logger hang → **RELIABLE, CLEAN NSH PROMPT!**

**Revert Plan:** Delete `init/rc.logging` and remove from CMakeLists.txt when logger needed

---

## Current Status

**Stage Completion:**
- Stage 0: ✅ Complete (reference study done)
- Stage 1: ✅ Complete (NuttX drivers enabled)
- Stage 2: 🔄 In Progress (SD hardware responds but card not initializing)
- Stage 3: ⏸️ Blocked (waiting for Stage 2)
- Stage 4: ⏸️ Pending (waiting for Stage 3)

**Active Issue:** SD card ENODEV error
- Device node `/dev/mmcsd0` created ✅
- SDIO driver initialized ✅
- Card never responds to open() calls ❌
- Mount fails with ENODEV ❌

**Suspected Root Cause:** HSMCI NOTBUSY hang issue (per hardware research)
- SAMV7 HSMCI driver has known issue where NOTBUSY flag doesn't clear after write operations
- Driver waits indefinitely for flag that never clears
- Modern high-capacity cards may have compatibility issues

**Blocking Issues:**
1. Cannot enable HSMCI command debug (CONFIG dependencies)
2. Cannot access NSH prompt to manually test SD card (dataman hanging system)
3. No visibility into what SD initialization commands are being sent

**Next Steps:**
1. Flash latest firmware with navigator disabled
2. Verify NSH prompt appears (confirm dataman hang resolved)
3. Run manual SD card diagnostics at NSH:
   ```bash
   ls -l /dev/mmcsd*
   mkdir /mnt
   mount -t vfat /dev/mmcsd0 /mnt
   ls /mnt
   free
   cat /proc/mounts
   ```
4. Based on results, determine if:
   - Device driver not loaded → Hardware/driver init problem
   - Device exists but ENODEV → Card not responding (likely NOTBUSY hang)
   - Device opens but mount fails → Filesystem/format issue

---

## Hardware Details

**Board:** SAMV71-XULT Development Board
**MCU:** ATSAM V71Q21 (ARM Cortex-M7 @ 300MHz)
**Flash:** 2MB
**RAM:** 384KB
**SD Interface:** HSMCI0 (High Speed Multimedia Card Interface)

**SD Card Pinout:**
- Data[3:0]: PA30, PA31, PA26, PA27
- Clock: PA25
- Command: PA28
- Card Detect: PD18

**SD Card Specs:**
- Type: Standard SD card (not micro)
- Tested: 16GB card inserted
- Expected Format: FAT32 (vfat)

---

## Build Commands Reference

**Full Clean Build:**
```bash
make microchip_samv71-xult-clickboards_default
```

**Flash Firmware:**
```bash
openocd -f platforms/nuttx/NuttX/nuttx/boards/arm/samv7/common/tools/atmel-samv71-xult.cfg \
  -c "program build/microchip_samv71-xult-clickboards_default/microchip_samv71-xult-clickboards_default.elf verify reset exit"
```

**Connect Serial Console:**
```bash
picocom /dev/ttyACM0 -b 115200
```

---

## Files Modified (Summary)

1. `boards/microchip/samv71-xult-clickboards/nuttx-config/nsh/defconfig`
   - Enabled MMCSD/HSMCI drivers
   - Disabled hardware card detect
   - Added cache-proof DMA settings
   - Enabled debug features

2. `boards/microchip/samv71-xult-clickboards/src/init.c`
   - Added SD card initialization function
   - Integrated into board_app_initialize()

3. `boards/microchip/samv71-xult-clickboards/init/rc.board_defaults`
   - Disabled LittleFS mount (causing hangs)
   - Disabled dataman path override

4. `ROMFS/px4fmu_common/init.d/rcS`
   - Disabled dataman start (lines 274-284)
   - Disabled navigator start (line 537)
   - Disabled mavlink USB auto-start (lines 515-524)

---

## Notes and Observations

1. **Console Garbage Was Terminal Issue:** Initial "garbage" console output was picocom terminal problem, not firmware crash

2. **Systematic Reverse Testing:** When issues occurred, tested changes in reverse order to isolate problem

3. **ENODEV Persists Across All Fixes:** SD card ENODEV error not resolved by:
   - Disabling hardware card detect
   - Adding cache-proof DMA
   - Increasing initialization wait time
   - Different SD cards (only 16GB tested so far)

4. **Dataman Cascade Failure:** Dataman → dataman_client → navigator form dependency chain; all three needed disabling to prevent system hang

5. **No Debug Visibility:** Cannot see HSMCI command-level operations due to CONFIG system complexity

---

**END OF LOG**
**Next Update:** After flashing Change #7 and testing NSH prompt access
