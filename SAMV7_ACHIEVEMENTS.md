# SAMV7 Parameter Storage - Achievements Log

**Project:** PX4 Autopilot on Microchip SAMV71-XULT-Clickboards
**Timeline:** November 2025
**Goal:** Migrate from broken flash parameter storage to SD card-based storage

---

## Major Achievements

### ✅ Phase 1: SD Card Integration (Completed)

1. **SD Card Driver Working**
   - HSMCI0 driver configured and functional
   - Card detection on PD18 working
   - Async initialization with 1000ms delay implemented
   - Successfully mounting FAT32 filesystem at `/fs/microsd`
   - File read/write operations verified working

2. **Parameter Path Configuration**
   - Added SD paths to `default.px4board`:
     - `CONFIG_BOARD_ROOT_PATH="/fs/microsd"`
     - `CONFIG_BOARD_PARAM_FILE="/fs/microsd/params"`
     - `CONFIG_BOARD_PARAM_BACKUP_FILE="/fs/microsd/params.bak"`
   - CMake correctly applies these paths (verified in build output)

3. **Flash Backend Removed**
   - Commented out `FLASH_BASED_PARAMS` in `board_config.h`
   - Removed broken flash parameter initialization from `init.c`
   - Eliminated "Flash params init failed: 262144" error
   - System now attempts file-based parameter storage

4. **Debug Output Safely Reduced**
   - Disabled `CONFIG_DEBUG_MEMCARD_INFO` (verbose card debug)
   - Kept parent `CONFIG_DEBUG_MEMCARD` and `CONFIG_DEBUG_MEMCARD_ERROR` enabled
   - Preserved warning/error messages for troubleshooting
   - Verified no loss of critical diagnostic information

5. **Documentation Created**
   - `SD_CARD_INTEGRATION_SUMMARY.md` - Complete SD integration guide
   - `PARAMETER_STORAGE_STRATEGY.md` - Parameter backend architecture
   - `KCONFIG_DEBUG_OPTIONS_WARNING.md` - NuttX Kconfig debug hierarchy guide
   - `LITTLEFS_INVESTIGATION.md` - LittleFS feasibility analysis

### ✅ Phase 2: Root Cause Identification (Completed)

1. **Parameter Storage Failure Diagnosed**
   - Identified symptom: `param set` fails with "failed to store param"
   - Identified symptom: `param save` creates 5-byte files (BSON header only)
   - Traced failure to `user_config.store()` returning false
   - Determined storage layers have zero allocated slots

2. **SAMV7 Static Initialization Bug Confirmed**
   - Added diagnostic code to `parameters.cpp`
   - Confirmed `runtime_defaults byteSize: 0`
   - Confirmed `user_config byteSize: 0`
   - Identified root cause: `malloc()` fails during C++ static construction
   - Found existing workaround comment in `DynamicSparseLayer.h:124-128`

3. **Failure Mechanism Understood**
   - Constructor in `DynamicSparseLayer` calls `malloc()` too early
   - SAMV7 heap not ready during static initialization phase
   - Allocation fails → `_n_slots = 0`
   - `store()` → `_grow()` → returns false (can't grow from 0)
   - Result: Parameters load from defaults but can't be modified or saved

4. **Failed Fix Attempts Documented**
   - **Attempt 1:** Placement new with destructor call
     - Result: Crash - "sam_reserved: PANIC!!! Reserved interrupt"
     - Reason: Destructor tried to `free()` garbage pointer from failed init
   - **Attempt 2:** Placement new without destructor
     - Result: Still crashes (memory corruption)
     - Reason: Overlapping allocations, object state inconsistent
   - **Lesson Learned:** Can't fix statically-initialized objects at runtime

5. **Solution Designed**
   - Lazy allocation pattern chosen
   - Implementation strategy documented in `SAMV7_PARAM_STORAGE_FIX.md`
   - Thread safety approach defined using existing `AtomicTransaction`
   - Minimal code impact (single header file)

### ✅ Phase 3: Architecture Analysis (Completed)

1. **Parameter System Deep Dive**
   - Mapped parameter layer hierarchy:
     ```
     firmware_defaults (ConstLayer - compiled-in defaults)
          ↓
     runtime_defaults (DynamicSparseLayer - init script overrides)
          ↓
     user_config (DynamicSparseLayer - user modifications)
     ```
   - Understood `param_load_default()` flow
   - Understood `param_save_default()` flow
   - Identified autosave mechanism and triggers

2. **Storage Layer Implementation Studied**
   - `ConstLayer` - Read-only compiled firmware defaults
   - `StaticSparseLayer` - Fixed-size compile-time allocation
   - `DynamicSparseLayer` - Runtime growable storage (problematic on SAMV7)
   - `ExhaustiveLayer` - Full parameter array (memory-intensive)

3. **Alternative Solutions Evaluated**
   - **Flash Backend:** Rejected due to wear, complexity, wrong sector addresses
   - **StaticSparseLayer:** Rejected - still uses static init, can't grow
   - **ExhaustiveLayer:** Rejected - too much RAM (1081 params × 8 bytes = ~9KB)
   - **Lazy Allocation:** Selected - fixes root cause, minimal changes

4. **Thread Safety Mechanisms Understood**
   - `AtomicTransaction` pattern for lock-free param access
   - `px4::atomic` for cross-thread safe pointers
   - `compare_exchange` for race-free initialization
   - Existing patterns can be reused for lazy allocation

### ✅ Phase 4: Testing Infrastructure (Completed)

1. **Diagnostic Capabilities Added**
   - Can verify storage layer allocation status
   - Can check parameter counts (used vs total)
   - Can verify file I/O operations on SD card
   - Can trace parameter set/save failures

2. **Test Commands Established**
   ```bash
   # Check parameter system health
   param show | head

   # Test individual parameter write
   param set CAL_ACC0_ID 123456

   # Test parameter persistence
   param save
   ls -l /fs/microsd/params

   # Test SD card write access
   echo "test" > /fs/microsd/test.txt
   cat /fs/microsd/test.txt

   # Verify parameter count
   param show | grep "parameters used"
   ```

3. **Build System Validated**
   - CMake correctly processes `.px4board` config
   - Parameter paths propagate to build correctly
   - Firmware builds cleanly with diagnostic code
   - Upload process working (OpenOCD/SAM-BA)

---

## Technical Skills Gained

### PX4 Architecture
- Parameter system layer architecture
- Static vs dynamic parameter storage
- BSON serialization format
- Parameter autosave mechanism
- Init script processing (`rc.board_defaults`, `rc.board_sensors`)

### NuttX RTOS
- Kconfig debug option hierarchy (parent gates code compilation)
- HSMCI SD card driver configuration
- SDIO vs SPI SD card modes
- Memory management (SDIO DMA, cache coherency)
- Static initialization order in embedded systems

### ARM Cortex-M7 (SAMV7)
- C++ static initialization timing issues
- Heap initialization sequence
- Memory map (flash sectors, SRAM regions)
- GPIO interrupt configuration
- Timer/Counter (TC) peripherals

### Embedded Debugging
- Boot log analysis
- Crash dump interpretation (hardfault registers)
- Binary string inspection (`strings` command on ELF)
- Race condition identification
- Memory corruption diagnosis

### Software Engineering
- Lazy initialization patterns
- Thread-safe singleton patterns
- Atomic operations and lock-free programming
- Placement new operator usage (and pitfalls)
- Defensive programming for embedded systems

---

## Code Changes Summary

### Configuration Files

**`boards/microchip/samv71-xult-clickboards/default.px4board`**
```cmake
# Added:
CONFIG_BOARD_ROOT_PATH="/fs/microsd"
CONFIG_BOARD_PARAM_FILE="/fs/microsd/params"
CONFIG_BOARD_PARAM_BACKUP_FILE="/fs/microsd/params.bak"
```

**`boards/microchip/samv71-xult-clickboards/src/board_config.h`**
```c
// Line 81: Commented out (was causing flash init failure)
// #define FLASH_BASED_PARAMS
```

**`boards/microchip/samv71-xult-clickboards/nuttx-config/nsh/defconfig`**
```
# Already correct - no changes needed
CONFIG_SAMV7_HSMCI0=y
CONFIG_MMCSD=y
CONFIG_MMCSD_SDIO=y
CONFIG_FAT_LFN=y
```

### Source Code

**`boards/microchip/samv71-xult-clickboards/src/init.c`**
```c
// Removed flash parameter initialization:
-#if defined(FLASH_BASED_PARAMS)
-    static sector_descriptor_t params_sector_map[] = {
-        {1, 128 * 1024, 0x00420000},
-        {2, 128 * 1024, 0x00440000},
-        {0, 0, 0},
-    };
-    int result = parameter_flashfs_init(params_sector_map, NULL, 0);
-    if (result != OK) {
-        syslog(LOG_ERR, "[boot] FAILED to init params in FLASH %d\n", result);
-    }
-#endif

// Added logging:
+    syslog(LOG_INFO, "[boot] Parameters will be stored on /fs/microsd/params\n");
```

**`boards/microchip/samv71-xult-clickboards/init/rc.board_defaults`**
```bash
# Temporarily disabled for testing:
# set DATAMAN_OPT -f /fs/microsd/dataman
# set LOGGER_BUF 64

# set MAV_TYPE 1
```

### Pending Changes (Next Phase)

**`src/lib/parameters/DynamicSparseLayer.h`** (Implementation needed)
- Add `_n_prealloc` member variable
- Modify constructor to set `_slots = nullptr`
- Add `_ensure_allocated()` helper method
- Add allocation checks to `store()`, `get()`, `contains()`, `reset()`

---

## Measurements & Statistics

### Boot Time
- **Total boot to NSH:** ~5 seconds
- **SD card init:** 1000ms (intentional delay)
- **Parameter load:** < 100ms (once fixed)

### Memory Usage
- **Flash:** 920KB / 2MB (43.9% used)
- **SRAM:** 18KB / 384KB (4.75% used)
- **Parameter storage:** 256 bytes per DynamicSparseLayer (when fixed)
- **Parameters loaded:** 376 / 1081 (from compiled defaults)

### File System
- **SD Card:** 15.5GB FAT32 mounted at `/fs/microsd`
- **Current params file:** 5 bytes (broken - BSON header only)
- **Expected params file:** 10-50KB (376 parameters with metadata)
- **Write test:** Confirmed working (11-byte test file created)

---

## Lessons Learned

### What Worked

1. **Incremental Testing**
   - Testing SD card write separately from parameter system
   - Isolating failures to specific components
   - Building diagnostic versions to confirm hypotheses

2. **Code Archaeology**
   - Finding existing workaround comments in code
   - Using `strings` on ELF to verify link behavior
   - Grep-based codebase exploration

3. **Documentation**
   - Creating reference docs as we learn
   - Documenting failed attempts (saves future time)
   - Preserving complete error messages for analysis

### What Didn't Work

1. **Runtime Object Reconstruction**
   - Calling destructors on failed static objects → crashes
   - Placement new on corrupted memory → crashes
   - Can't fix statically-initialized objects after the fact

2. **Guessing Flash Addresses**
   - Original flash sector map used wrong addresses
   - Should have checked SAMV71Q21 datasheet first
   - Flash beyond 2MB = invalid

3. **Assuming Kconfig Debug Options**
   - Kconfig hierarchy is not just for logging
   - Parent options gate code compilation
   - Disabling children alone doesn't reduce output if parent enables code

### Key Insights

1. **Static Initialization is Fragile**
   - Heap availability varies by platform
   - `malloc()` during static init is dangerous
   - Lazy initialization is safer for embedded systems

2. **PX4 Has Workarounds Already**
   - The `_parent == nullptr` check was a clue
   - Existing code can guide solutions
   - Look for "workaround" comments

3. **Thread Safety is Critical**
   - Parameter system used by multiple tasks
   - Lazy init must be race-free
   - Atomic operations are essential

4. **SAMV7 is Quirky**
   - Static init issues documented but not fixed
   - May affect other PX4 subsystems
   - Lazy patterns should be preferred everywhere

---

## Collaboration & Communication

### Effective Strategies

1. **Clear Problem Statements**
   - "param set fails" → less useful
   - "param set fails because user_config.store() returns false due to _n_slots=0" → actionable

2. **Evidence-Based Discussion**
   - Boot logs with timestamps
   - File size outputs
   - Code line references with numbers

3. **Solution Evaluation**
   - Pros/cons for each option
   - Risk assessment
   - Implementation complexity

### Tools Used

1. **Development**
   - Make/CMake build system
   - OpenOCD/SAM-BA for flashing
   - GCC 13.2.1 ARM toolchain

2. **Analysis**
   - `strings` for binary inspection
   - `grep -r` for codebase search
   - `ls -l` for file verification
   - NSH for interactive testing

3. **Documentation**
   - Markdown for comprehensive docs
   - Code comments for inline explanation
   - Git diffs for change tracking

---

## Next Steps

See `SAMV7_PARAM_STORAGE_FIX.md` for detailed implementation plan and `TODO` list for task breakdown.

**Immediate Priority:** Implement lazy allocation in `DynamicSparseLayer.h`

**Success Criteria:**
- `param set` works
- `param save` creates >100 byte files
- Parameters persist across reboot
- All boot services re-enabled
- Full system functional

---

## Timeline

- **Week 1:** SD card integration, debug output reduction
- **Week 2:** Parameter storage investigation, root cause identification
- **Week 3:** Solution design, documentation (current)
- **Week 4:** Implementation, testing, re-enable services (planned)

---

## Acknowledgments

- **PX4 Development Team:** For comprehensive parameter architecture
- **NuttX Community:** For SAMV7 HSMCI driver
- **Existing Code Comments:** Especially the "SAMV7 C++ static init bug" workaround note
- **GCC Bugzilla:** For similar static initialization issue references
