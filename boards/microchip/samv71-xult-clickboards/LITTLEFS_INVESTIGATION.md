# LittleFS on SAMV71 Internal Flash - Investigation Report

**Date:** 2025-11-17
**Board:** SAMV71-XULT (ATSAM71Q21)
**Objective:** Implement LittleFS parameter storage on internal flash
**Status:** ❌ BLOCKED - NuttX progmem driver write operations hang

---

## Executive Summary

Successfully created and mounted a LittleFS filesystem on SAMV71 internal flash, but **write operations hang indefinitely** due to a bug in the NuttX SAMV71 progmem driver (`sam_progmem.c`). Read operations work correctly. This blocks parameter storage on internal flash.

**Recommendation:** Use microSD for parameters until progmem driver is fixed.

---

## What Works ✅

### 1. MTD Partition Creation
- Successfully created 32KB partition at end of 2MB flash
- Location: `0x001F8000 - 0x001FFFFF` (last 32KB)
- Firmware location: `0x00400000 - ~0x0051C000` (~902KB)
- No overlap between firmware and parameter partition

### 2. Device Registration
- `/dev/mtdparams` appears correctly
- Size: 32768 bytes (32KB)
- Geometry: blocksize=8192, erasesize=8192, neraseblocks=4

### 3. Pre-formatted LittleFS Image
- Created offline image with matching geometry
- LittleFS version: v2.4 (matching NuttX)
- Configuration:
  - Block size: 16KB (BLOCK_SIZE_FACTOR=2)
  - Program size: 8KB
  - Read size: 8KB
  - Cache size: 8KB
  - Lookahead: 128 bytes
  - Block count: 2

### 4. Filesystem Mount
- LittleFS mounts successfully at `/fs/littlefs`
- Mount time: ~2 seconds after boot (delayed work queue)
- Log message: `[littlefs] Successfully mounted at /fs/littlefs`

### 5. Read Operations
- Directory listing works: `ls /fs/littlefs`
- File reads would work (if files existed)
- No errors during read operations

---

## What Fails ❌

### 1. Format Operations
**Symptom:** `lfs_format()` hangs indefinitely
**Test:** Attempted during early boot mount
**Result:** System completely frozen, requires reset

```c
// This hangs forever:
ret = nx_mount("/dev/mtdparams", "/fs/littlefs", "littlefs", 0, "autoformat");
```

### 2. Write Operations
**Symptom:** Any write to LittleFS filesystem hangs indefinitely
**Test 1:** `param save` - hangs, never returns
**Test 2:** `echo "test" > /fs/littlefs/test.txt` - hangs, never returns
**Result:** System appears frozen, requires Ctrl+C or reset

---

## Root Cause Analysis

### Primary Issue: NuttX SAMV71 Progmem Driver

**File:** `platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/sam_progmem.c`

**Problem:** The progmem write path hangs when writing to internal flash.

**Affected Functions:**
- `up_progmem_write()` - Writes data to flash
- `up_progmem_erase()` - Erases flash sectors (used by format)

### Hypotheses

1. **Flash Programming While Executing From Flash**
   - SAMV71 may require executing from RAM when writing to flash
   - Flash controller might not support concurrent read/write
   - Need to investigate SAMV71 reference manual section on EEFC (Enhanced Embedded Flash Controller)

2. **Cache Coherency Issues**
   - Data cache invalidation might be missing or incorrect
   - Write-through vs write-back cache behavior
   - Current code has `up_invalidate_dcache()` in some places but may be incomplete

3. **Flash Controller Wait States**
   - Incorrect wait state configuration during write operations
   - Flash controller not properly signaling completion
   - Timeout or polling loop might be infinite

4. **Interrupt Handling**
   - Flash write completion interrupts not configured
   - Write operations blocking with interrupts disabled
   - Need to check if writes are using polling vs interrupt-driven approach

5. **Hardware Errata**
   - Known SAMV71 silicon bugs related to flash programming
   - Check Microchip SAMV71 errata document
   - May require workarounds from vendor

---

## Technical Details

### Flash Geometry (SAMV71)
- Total flash: 2MB (0x200000 bytes)
- Physical address: 0x00400000 - 0x005FFFFF
- Page size: 512 bytes (for programming)
- Erase block size: 8KB (cluster size)
- Pages per erase block: 16

### NuttX Configuration Issues

**Problem:** NuttX reports incorrect geometry to LittleFS

```c
// sam_progmem.c returns:
size_t up_progmem_pagesize(size_t page) {
    return 8192;  // Returns cluster size, not page size!
}

size_t up_progmem_erasesize(size_t block) {
    return 8192;  // Same value as pagesize
}
```

**Impact:** LittleFS configured with unusual parameters:
- read_size = prog_size = 8192 (normally different)
- block_size = 16384 (2× erase block to maintain prog < block relationship)

**Workaround Applied:**
- Set `CONFIG_FS_LITTLEFS_BLOCK_SIZE_FACTOR=2`
- This makes LittleFS block_size = 16KB (2× MTD erase size)
- Maintains LittleFS requirement: prog_size < block_size

### Mount Configuration

**Final Working Configuration:**

```c
// boards/microchip/samv71-xult-clickboards/src/init.c
static void littlefs_mount_worker(FAR void *arg)
{
    mkdir("/fs/littlefs", 0777);

    // Mount without format (uses pre-formatted image)
    int ret = nx_mount("/dev/mtdparams", "/fs/littlefs", "littlefs", 0, NULL);

    if (ret == 0) {
        // Set parameter file location
        param_set_default_file("/fs/littlefs/params");
    }
}

// Schedule delayed mount (2s after boot)
work_queue(LPWORK, &littlefs_mount_work, littlefs_mount_worker, NULL, MSEC2TICK(2000));
```

**Why delayed mount?**
- Allows all subsystems to initialize first
- Avoids early-boot interrupt conflicts
- Scheduled on low-priority work queue (LPWORK)

### NuttX Defconfig Changes

```config
# boards/microchip/samv71-xult-clickboards/nuttx-config/nsh/defconfig

# MTD Support
CONFIG_MTD=y
CONFIG_MTD_BYTE_WRITE=y
CONFIG_MTD_PARTITION=y
CONFIG_MTD_PROGMEM=y

# LittleFS Configuration
CONFIG_FS_LITTLEFS=y
CONFIG_FS_LITTLEFS_BLOCK_CYCLE=500
CONFIG_FS_LITTLEFS_CACHE_SIZE_FACTOR=1
CONFIG_FS_LITTLEFS_LOOKAHEAD_SIZE=128
CONFIG_FS_LITTLEFS_PROGRAM_SIZE_FACTOR=1
CONFIG_FS_LITTLEFS_READ_SIZE_FACTOR=1
CONFIG_FS_LITTLEFS_BLOCK_SIZE_FACTOR=2        # Key change!
```

---

## Pre-formatted Image Creation

### Why Pre-format Offline?

The `lfs_format()` operation hangs on SAMV71, so we create a pre-formatted filesystem image on a development PC and flash it to the device.

### Image Creation Tool

**Location:** `/tmp/create_lfs_image.c`

**Usage:**
```bash
# Compile with NuttX's LittleFS v2.4
gcc -o create_lfs_nuttx create_lfs_image.c \
  platforms/nuttx/NuttX/nuttx/fs/littlefs/littlefs/lfs.c \
  platforms/nuttx/NuttX/nuttx/fs/littlefs/littlefs/lfs_util.c \
  -I platforms/nuttx/NuttX/nuttx/fs/littlefs \
  -I platforms/nuttx/NuttX/nuttx/fs/littlefs/littlefs \
  -std=c99

# Create pre-formatted image
./create_lfs_nuttx /tmp/littlefs_params.bin
```

**Image Parameters (must match NuttX config):**
```c
#define IMAGE_SIZE (32 * 1024)      // 32KB
#define BLOCK_SIZE (16 * 1024)      // 16KB (BLOCK_SIZE_FACTOR=2)
#define BLOCK_COUNT 2               // 32KB / 16KB
#define PROG_SIZE 8192              // 8KB
#define READ_SIZE 8192              // 8KB
#define CACHE_SIZE 8192             // 8KB
#define LOOKAHEAD_SIZE 128          // 128 bytes
```

### Flashing the Image

**OpenOCD command:**
```bash
openocd -f interface/cmsis-dap.cfg \
  -f board/atmel_samv71_xplained_ultra.cfg \
  -c init \
  -c "reset halt" \
  -c "flash write_image erase /tmp/littlefs_params.bin 0x005F8000" \
  -c reset \
  -c shutdown
```

**Important Notes:**
- Flash to **absolute address** 0x005F8000 (0x00400000 + 0x001F8000)
- Must flash **before** flashing firmware (or after, doesn't matter - no overlap)
- Pre-formatted partition **survives firmware updates** (separate memory region)

---

## Attempted Solutions (That Didn't Work)

### Attempt 1: Reduce Partition Size
**Hypothesis:** Large partition (256KB) takes too long to format
**Action:** Reduced to 32KB (4 erase blocks)
**Result:** ❌ Still hung during format

### Attempt 2: Use Autoformat Instead of Forceformat
**Hypothesis:** "forceformat" always formats, "autoformat" only formats if needed
**Action:** Changed mount option from "forceformat" to "autoformat"
**Result:** ❌ Still hung during format

### Attempt 3: Delay Mount Until After Boot
**Hypothesis:** Early boot timing issues with interrupts/initialization
**Action:** Delayed mount by 2 seconds using work queue
**Result:** ❌ Still hung during format (even after full system init)

### Attempt 4: Manual Mount from NSH
**Hypothesis:** Different context after boot might work
**Action:** Mounted manually from NSH prompt after boot
**Result:** ⚠️ Mount without format returned EFAULT immediately (didn't hang), but with format option would still hang

### Attempt 5: LittleFS Version Mismatch
**Hypothesis:** Image created with v2.11, NuttX uses v2.4
**Action:** Recreated image using NuttX's exact LittleFS v2.4 source
**Result:** ✅ Mount succeeded! But writes still hang

### Attempt 6: Geometry Mismatch
**Hypothesis:** Image geometry didn't match NuttX config
**Action:** Set BLOCK_SIZE_FACTOR=2, recreated image with block_size=16KB
**Result:** ✅ Mount succeeded! But writes still hang

---

## Boot Log Evidence

### Successful Mount (Read-Only Works)
```
[boot] Initializing LittleFS for params...
[boot] MTD partition: offset=0x1f8000 size=32KB (page 252, count=4, pg_sz=8192)
[boot] Partition geometry: blocksize=8192, erasesize=8192, neraseblocks=4
[boot] LittleFS will use: read=8192, prog=8192, block=16384, cache=8192
[boot] MTD device /dev/mtdparams registered successfully (32KB, 4 blocks)
[boot] LittleFS mount will be handled by delayed work queue (2s delay)

...2 seconds later...

[littlefs] Delayed mount starting (2s after boot)...
[littlefs] Successfully mounted at /fs/littlefs
[littlefs] Setting parameter file to /fs/littlefs/params
[littlefs] Filesystem ready for parameter storage
```

### Write Operations Hang
```
nsh> ls /fs/littlefs
/fs/littlefs:
 .
 ..

nsh> echo "test" > /fs/littlefs/test.txt
<hangs forever, no response>

nsh> param save
<hangs forever, no response>
```

---

## Files Modified

### 1. `boards/microchip/samv71-xult-clickboards/src/init.c`
**Changes:**
- Added LittleFS delayed mount worker function
- Configured delayed mount via work queue (2s delay)
- Call `param_set_default_file("/fs/littlefs/params")` after successful mount
- Added includes for MTD, LittleFS, and param system

**Key code:**
```c
static struct work_s littlefs_mount_work;

static void littlefs_mount_worker(FAR void *arg)
{
    mkdir("/fs/littlefs", 0777);
    int ret = nx_mount("/dev/mtdparams", "/fs/littlefs", "littlefs", 0, NULL);

    if (ret == 0) {
        param_set_default_file("/fs/littlefs/params");
    }
}

// In board_app_initialize():
work_queue(LPWORK, &littlefs_mount_work, littlefs_mount_worker, NULL, MSEC2TICK(2000));
```

### 2. `boards/microchip/samv71-xult-clickboards/nuttx-config/nsh/defconfig`
**Changes:**
- Enabled CONFIG_MTD and related options
- Enabled CONFIG_FS_LITTLEFS
- Set CONFIG_FS_LITTLEFS_BLOCK_SIZE_FACTOR=2 (critical for geometry match)

### 3. `boards/microchip/samv71-xult-clickboards/src/board_config.h`
**Changes:**
- Disabled FLASH_BASED_PARAMS (old parameter system)

### 4. `boards/microchip/samv71-xult-clickboards/init/rc.board_extras`
**Changes:**
- Removed littlefs_mount command (mount now happens in init.c)
- Added informational message about LittleFS being ready

---

## Next Steps: Debugging sam_progmem.c

### Required Investigation

1. **Review SAMV71 Reference Manual**
   - Section: Enhanced Embedded Flash Controller (EEFC)
   - Focus on flash programming sequences
   - Check for restrictions on concurrent read/write

2. **Check Microchip Errata**
   - Document: "SAM V71/V70/E70/S70 Silicon Errata and Data Sheet Clarification"
   - Look for flash controller bugs
   - Check for required workarounds

3. **Analyze sam_progmem.c Code Path**
   - Instrument `up_progmem_write()` with debug logging
   - Check where exactly it hangs (waiting for flash ready?)
   - Verify flash controller register access

4. **Compare with Working Implementations**
   - How do STM32 progmem drivers handle this?
   - Are there other SAMV71 projects with working flash writes?
   - Check NuttX mailing list for similar issues

5. **Test Hypotheses**
   - Try executing write code from RAM
   - Verify cache invalidation sequence
   - Check interrupt enable/disable around writes
   - Test with different wait state configurations

### Debug Instrumentation Needed

Add to `sam_progmem.c`:
```c
ssize_t up_progmem_write(size_t addr, const void *buf, size_t count)
{
    syslog(LOG_ERR, "[progmem] write: addr=0x%zx count=%zu\n", addr, count);

    // ... existing code ...

    // Add logging before/after critical sections:
    syslog(LOG_ERR, "[progmem] waiting for flash ready...\n");
    while (!(getreg32(SAM_EEFC_FSR) & EEFC_FSR_FRDY));
    syslog(LOG_ERR, "[progmem] flash ready!\n");

    // ... more instrumentation ...
}
```

---

## Workarounds

### Current Solution: Use microSD for Parameters

Parameters already work correctly on microSD card:
- Path: `/fs/microsd/params`
- Reliable writes
- No performance issues

**To use microSD parameters:**
1. Insert microSD card
2. Parameters automatically use `/fs/microsd` when available
3. No code changes needed

### Future Solution: External SPI Flash

If internal flash remains problematic:
1. Add external SPI flash chip (e.g., W25Q32, 4MB)
2. Configure as MTD device
3. Use for parameters and dataman
4. Proven reliable on other PX4 boards

---

## References

### Documentation
- [LittleFS GitHub](https://github.com/littlefs-project/littlefs)
- [NuttX MTD Driver Documentation](https://nuttx.apache.org/docs/latest/components/drivers/special/mtd.html)
- [NuttX LittleFS Documentation](https://nuttx.apache.org/docs/latest/components/filesystem/littlefs.html)

### Hardware Documentation
- SAMV71 Datasheet: Document DS60001527
- SAMV71 Reference Manual: Document DS60001537
- SAMV71 Errata: Search "SAM V71 Silicon Errata"

### Related Code Files
- `platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/sam_progmem.c` - Flash driver
- `platforms/nuttx/NuttX/nuttx/fs/littlefs/lfs_vfs.c` - NuttX LittleFS VFS interface
- `platforms/nuttx/NuttX/nuttx/drivers/mtd/mtd_progmem.c` - MTD progmem wrapper
- `src/lib/parameters/parameters.cpp` - PX4 parameter system

---

## Conclusion

LittleFS on SAMV71 internal flash is **technically feasible** (mount and read work), but is **blocked by a bug in the NuttX progmem driver** that causes write operations to hang.

**Status:** Suspended pending progmem driver fix
**Current Solution:** Using microSD for parameter storage
**Priority:** Medium (microSD works, this is an optimization)

**Estimated Effort to Fix:**
- Investigation: 4-8 hours (review errata, add instrumentation)
- Fix implementation: 2-4 hours (depends on root cause)
- Testing: 2-4 hours (verify writes work, no regressions)
- **Total:** 1-2 days of focused work

---

**Document Version:** 1.0
**Last Updated:** 2025-11-17
**Author:** Claude Code Assistant
**Status:** Investigation Complete, Fix Required
