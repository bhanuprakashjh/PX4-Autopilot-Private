CODEx Session Notes – SAMV71 Bring-Up
=====================================

Context
-------
- Board: `microchip_samv71-xult-clickboards`
- Goal: stabilize flash/LittleFS storage, get PX4 booting cleanly, and prepare for SD-card usage.
- Branch tip: `52c71d60a3ef85c092ca598d8863448f2253201d` (PX4 v1.17.0-alpha1-106).

Changes Made This Session (Detailed)
------------------------------------
1. **Flash parameter storage**
   - Updated `boards/microchip/samv71-xult-clickboards/src/init.c` so the `params_sector_map` table points at the correct top-of-flash addresses (`0x001C0000` and `0x001E0000`).
   - Extended `samv71_setup_param_mtd()` to call `progmem_initialize()`, read `struct mtd_geometry_s`, partition the final 32 KB, wrap it in `ftl_initialize()`/`bchdev_register()`, and log the geometry.
   - Verified `parameter_flashfs_init()` now runs immediately after the partition is ready, so the flash-backed param store succeeds every boot.

2. **LittleFS tooling and build plumbing**
   - Added `src/systemcmds/littlefs_mount/Kconfig` and enabled it (along with `CONFIG_SYSTEMCMDS_MFT=y`) in `boards/.../default.px4board`.
   - Patched `src/systemcmds/littlefs_mount/littlefs_mount.c` to use `NULL`, include `<px4_platform_common/getopt.h>`, and clarify the `-d/-m/-f` usage text.
   - Confirmed via `build.ninja`/`.map` that `libsystemcmds__littlefs_mount.a` is compiled and linked into the firmware.

3. **Board init scripts + ROMFS cleanup**
   - Converted the local `test_px4_{quick,full}.sh` scripts to ASCII-only (replaced `✓/✗`) so `Tools/px_romfs_pruner.py` stops erroring out.
   - Simplified `boards/.../init/rc.board_defaults` to pure command chaining (no `if`, `[ ]`, or command substitution):
     ```
     littlefs_mount -d /dev/mtdparams -m /fs/mtd_params >/dev/null 2>&1 || \
     	littlefs_mount -d /dev/mtdparams -m /fs/mtd_params -f >/dev/null 2>&1
     ls /fs/mtd_params >/dev/null 2>&1 && echo ok > /fs/mtd_params/probe >/dev/null 2>&1 && rm -f /fs/mtd_params/probe >/dev/null 2>&1
     ls /fs/mtd_params >/dev/null 2>&1 || echo "[rc.board_defaults] littlefs_mount failed for /dev/mtdparams"
     ```
   - Boot logs now show `INFO [littlefs_mount] Formatting and mounting /dev/mtdblock0…` followed by a successful smoke write.

4. **ROMFS pruning issues**
   - Removed Unicode glyphs (✓/✗) from the board’s test scripts so the ROMFS pruner can ASCII-encode them.

5. **HRT / platform tweaks**
   - Hooked `hrt_absolute_time()` through the existing `hrt_absolute_time_locked()` helper to avoid unused-static warnings and ensure consistent locking.
   - Confirmed the new TC0-based HRT self-test passes every boot (`[hrt] TC0 self-test passed`) and the WorkQueue manager starts cleanly after `px4_platform_init()`.

6. **Builds**
   - Rebuilt PX4 repeatedly with `CCACHE_DISABLE=1 ninja` to reflect config/script updates.
   - Final binary size: ~907 kB flash (43%), ~22 kB SRAM (5.7%).

7. **Runtime observations**
   - Boot now shows full LittleFS format/mount logs (`INFO [littlefs_mount] Successfully mounted /dev/mtdblock0`).
   - Param import still fails on first boot (happens before mount), but should succeed on subsequent boots with a file present.
   - Dataman/logging still target `/fs/microsd`, so without SD the clients spam timeouts (expected until SD slot is enabled or dataman is pointed at `/fs/mtd_params`).

Phase 0 Audit (SD Migration Prereqs)
------------------------------------
- **DMA allocator / write-through policy**: `platforms/nuttx/src/px4/common/board_dma_alloc.c` already guards re-entry (magic value) and aligns the pool to 64 B; any SD/HSMCI buffers must be allocated via `board_dma_alloc()` to inherit this policy. Documented in this file and ready for reuse.
- **Static initialization hazards**: Reviewed the board tree—`board_app_initialize()` only touches C globals after `px4_platform_init()`. No brace-initialized singletons remain in the SAM-specific files beyond `hrt.c` (already switched to runtime init). Additional modules added later must continue to avoid C++ static singletons.
- **Semaphore usage**: The only `nxsem_wait_uninterruptible()` usage is inside `sam_progmem.c`’s page-lock routine, which runs in task context. No board files call semaphores from ISRs, so HSMCI work can safely use the NuttX MMC semaphores.
- **C vs C++ boundaries**: Confirmed that board drivers remain in `.c` files (SD glue should stay C unless PX4 logging is needed). Mixed C/C++ hazards will be avoided by keeping SD init in C or wrapping C++ logging with `extern "C"`.

Phase 1 – NuttX Config (HSMCI/MMC)
----------------------------------
- Updated `boards/.../nuttx-config/nsh/defconfig` to enable:
  ```
  CONFIG_SAMV7_HSMCI0=y
  CONFIG_MMCSD=y
  CONFIG_MMCSD_MMCSUPPORT=y
  CONFIG_MMCSD_HAVECARDDETECT=y
  CONFIG_MMCSD_SDIO=y
  CONFIG_MMCSD_NSLOTS=1
  # CONFIG_MMCSD_SPI is not set
  CONFIG_SDIO_DMA=y
  CONFIG_SDIO_BLOCKSETUP=y
  ```
- Re-ran cmake/ninja so NuttX regenerated `.config` and headers. Verified `platforms/nuttx/NuttX/nuttx/.config` now contains the SDIO/MMC symbols (while SPI support stays disabled).
- No functional code yet—this phase merely ensures the SAMV71 build pulls in the HSMCI/SDIO driver ready for the next step.

Phase 2 – SD Hardware Bring-Up (in progress)
--------------------------------------------
- Added `samv71_sdcard_initialize()` (guarded by `CONFIG_SAMV7_HSMCI0`) to `boards/.../src/init.c`. The helper:
  * calls `sdio_initialize(0)`/`mmcsd_slotinitialize(0, sdio)`
  * forces a media-change notification (`sdio_mediachange`)
  * creates `/fs` + `/fs/microsd`
  * sleeps briefly, then mounts `/dev/mmcsd0` at `/fs/microsd`
  * logs success/failure (with special cases for missing/unformatted cards)
- Invoked the helper right after `px4_platform_init()` (non-fatal; logs continue even if SD absent).
- Pulled in the necessary headers (`<nuttx/sdio.h>`, `<sys/mount.h>`, `<sys/stat.h>`).
- Enabled cache-coherency options in defconfig: `CONFIG_SAMV7_HSMCI_RDPROOF=y` and `_WRPROOF=y`.
- Full PX4 build completes (flash ~907 kB, RAM ~22 kB). Next step is to test with/without a card to verify `/fs/microsd` actually mounts.

Next Steps (Agreed)
-------------------
1. Enable SD-card slot (HSMCI) in the NuttX defconfig (`CONFIG_SAMV7_HSMCI0`, `CONFIG_MMCSD_*` etc.).
2. Add HSMCI init + `/fs/microsd` mount in `board_app_initialize()`.
3. Decide whether to keep parameters on flash or migrate them to SD once the slot is reliable.
4. Start dataman/logging on the chosen storage (either override `dataman start -f /fs/mtd_params/dataman` or rely on `/fs/microsd`).
5. Add a SYS_AUTOSTART script (`60100_samv71_dev`) to eliminate the “No airframe file” error.

Notes / Caveats
---------------
- LittleFS consumes the last 32 kB of Bank 1. The sector map is correct now, but writing/formatting is slow (RA/RC operations visible in logs).
- Dataman spam will persist until `/fs/microsd` exists or we start it with `-f /fs/mtd_params/dataman`.
- Many `nsh: <cmd> not found` logs (tone_alarm, rgbled, etc.) are expected because the drivers aren’t built for this constrained target.

Prepared by Codex (PX4 bring-up session – 2025-11-18)
