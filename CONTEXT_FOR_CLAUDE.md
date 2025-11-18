# Context Prompt for Claude - SAMV7 Parameter Storage Project

**Read this file to understand the current project state and continue development.**

---

## Project Overview

**Goal:** Fix parameter storage on Microchip SAMV71-XULT-Clickboards PX4 autopilot board by migrating from broken flash backend to SD card storage.

**Board:** Microchip SAMV71-XULT with Click sensor boards
**MCU:** ATSAM V71Q21 (Cortex-M7 @ 300MHz, 2MB flash, 384KB SRAM)
**Repository:** https://github.com/bhanuprakashjh/PX4-Autopilot-Private
**Working Directory:** `/media/bhanu1234/Development/PX4-Autopilot-Private`

---

## Current Status: BLOCKED - Awaiting Implementation

### ✅ What's Working

1. **SD Card Storage**
   - HSMCI driver functional (15.5GB FAT32 at `/fs/microsd`)
   - File read/write operations verified
   - Mount happens at boot with 1000ms async delay

2. **HRT (High Resolution Timer)**
   - TC0 timer configured correctly (MCK/128 = 2.34MHz)
   - Self-test passing consistently
   - File: `platforms/nuttx/src/px4/microchip/samv7/hrt/hrt.c`

3. **DMA & Cache Coherency**
   - Fixed in `platforms/nuttx/src/px4/common/board_dma_alloc.c`
   - SDIO transfers working properly

4. **Boot System**
   - Clean boot to NSH prompt
   - 376/1081 parameters load from compiled defaults
   - Services disabled for testing (mavlink, logger, dataman)

5. **Documentation**
   - 20+ comprehensive guides created (21,000+ lines)
   - All committed to `samv7-custom` branch

### ⚠️ CRITICAL ISSUE - Parameter Storage Blocked

**Problem:** SAMV7 C++ static initialization bug

**Root Cause:**
- `malloc()` fails during global object construction (before `main()`)
- `DynamicSparseLayer` objects (`runtime_defaults`, `user_config`) have ZERO allocated slots
- Located in: `src/lib/parameters/parameters.cpp:103-105`

**Evidence:**
```bash
# Boot log shows:
INFO  [parameters] runtime_defaults size: 0 byteSize: 0
INFO  [parameters] user_config size: 0 byteSize: 0
```

**Symptoms:**
- `param set CAL_ACC0_ID 12345` → "failed to store param" error
- `param save` creates 5-byte files (BSON header only, no parameter data)
- Parameters can be read but not modified or saved

**Why Attempted Runtime Fixes Failed:**
- Calling destructor → PANIC (tries to `free()` garbage pointer)
- Placement new → Memory corruption and crashes
- Objects are too corrupted to reconstruct at runtime

---

## 🎯 THE SOLUTION (Ready to Implement)

### Lazy Allocation in DynamicSparseLayer

**File to Modify:** `src/lib/parameters/DynamicSparseLayer.h`

**Strategy:** Defer `malloc()` from constructor to first use when heap is guaranteed available.

**Implementation Steps:**

1. **Add member variable** (line ~249):
   ```cpp
   int _n_prealloc;  // Remember prealloc size for lazy init
   ```

2. **Modify constructor** (lines 43-59) to defer allocation:
   ```cpp
   DynamicSparseLayer(ParamLayer *parent, int n_prealloc = 32, int n_grow = 4)
       : ParamLayer(parent), _n_prealloc(n_prealloc), _n_slots(0), _n_grow(n_grow)
   {
       _slots.store(nullptr);  // Lazy allocation - defer until first use
   }
   ```

3. **Add `_ensure_allocated()` helper** (in private section):
   ```cpp
   bool _ensure_allocated()
   {
       // Fast path: already allocated
       if (_slots.load() != nullptr) {
           return true;
       }

       // Lazy allocation on first use (heap now available)
       Slot *slots = (Slot *)malloc(sizeof(Slot) * _n_prealloc);

       if (slots == nullptr) {
           PX4_ERR("Failed to allocate memory for dynamic sparse layer");
           return false;
       }

       // Initialize slots
       for (int i = 0; i < _n_prealloc; i++) {
           slots[i] = {UINT16_MAX, param_value_u{}};
       }

       // Atomic swap for thread safety
       Slot *expected = nullptr;
       if (!_slots.compare_exchange(&expected, slots)) {
           // Another thread won the race, free our allocation
           free(slots);
       } else {
           // We won the race, update slot count
           _n_slots = _n_prealloc;
       }

       return true;
   }
   ```

4. **Add allocation checks** to methods:
   - `store()` (line 68): Add `if (!_ensure_allocated()) return false;` at start
   - `get()` (line 113): Add `if (!_ensure_allocated()) { /* fallback */ }`
   - `contains()` (line 94): Add allocation check
   - `reset()` (line 133): Add allocation check

**Complete Implementation Plan:** See `SAMV7_PARAM_STORAGE_FIX.md`

---

## Build and Test Commands

### Build Firmware
```bash
cd /media/bhanu1234/Development/PX4-Autopilot-Private
make microchip_samv71-xult-clickboards_default
```

### Flash to Board
```bash
make microchip_samv71-xult-clickboards_default upload
```

### Test Parameters on Hardware
```bash
# After boot, in NSH:
param show | head
param set CAL_ACC0_ID 123456  # Should work after fix
param save                     # Should create >100 byte file
ls -l /fs/microsd/params       # Verify file size
reboot                         # Test persistence
param show CAL_ACC0_ID         # Should show 123456
```

---

## Git Branch Structure

### Branches
- **`main`** - Original/clean PX4 baseline (unchanged)
- **`samv7-custom`** ⭐ - Enhanced implementation (current work)
- **`backup-i2c-spi-dma-20251117`** - Old backup

### Current Branch
```bash
git branch  # Should show: * samv7-custom
```

### Key Commits on samv7-custom
- `f443412135` - Main implementation (SD card, HRT, DMA, 60 files)
- `bd1e3b09f6` - README_SAMV7.md added

---

## Critical Files & Locations

### Files That Need Modification (Next Step)
- `src/lib/parameters/DynamicSparseLayer.h` - **NEEDS LAZY ALLOCATION FIX**

### Files Already Modified (Complete)
- `boards/microchip/samv71-xult-clickboards/src/board_config.h` - Removed `FLASH_BASED_PARAMS`
- `boards/microchip/samv71-xult-clickboards/default.px4board` - Added SD paths
- `boards/microchip/samv71-xult-clickboards/src/init.c` - Removed flash param init, added SD logging
- `platforms/nuttx/src/px4/microchip/samv7/hrt/hrt.c` - Fixed HRT
- `platforms/nuttx/src/px4/common/board_dma_alloc.c` - Fixed DMA coherency

### Documentation (All Complete)
- `SAMV7_PARAM_STORAGE_FIX.md` - Implementation guide (YOU MUST READ THIS)
- `SAMV7_ACHIEVEMENTS.md` - Progress log and lessons learned
- `RESEARCH_TOPICS.md` - Study guide (14 technical topics)
- `README_SAMV7.md` - Branch usage guide

---

## Important Technical Context

### PX4 Parameter Layer Architecture
```
firmware_defaults (ConstLayer - compiled defaults)
     ↓
runtime_defaults (DynamicSparseLayer - init script overrides) ← FAILED STATIC INIT
     ↓
user_config (DynamicSparseLayer - user modifications) ← FAILED STATIC INIT
```

**Problem Location:** `src/lib/parameters/parameters.cpp:103-105`
```cpp
static ConstLayer firmware_defaults;
static DynamicSparseLayer runtime_defaults{&firmware_defaults};  // _n_slots=0
DynamicSparseLayer user_config{&runtime_defaults};              // _n_slots=0
```

### Why Static Init Fails on SAMV7
1. Global constructors run before `main()`
2. NuttX heap not fully initialized yet during this phase
3. `malloc()` in `DynamicSparseLayer` constructor returns `nullptr`
4. Constructor sets `_n_slots = 0`
5. Result: Storage layers can't grow or store parameters

### Existing Workaround Evidence
**File:** `src/lib/parameters/DynamicSparseLayer.h:124-128`
```cpp
// Workaround for C++ static initialization bug on SAMV7
// If parent is null, return default value instead of crashing
if (_parent == nullptr) {
    return px4::parameters[param].val;
}
```
This proves the SAMV7 issue is known and already has partial workarounds.

### Thread Safety
- Use `AtomicTransaction` (already present in methods)
- Use `compare_exchange` for lazy init race condition
- Only one thread wins allocation, losers free their allocation

---

## What To Do Next (Immediate Action)

### Step 1: Implement Lazy Allocation (2-4 hours)
1. Read `SAMV7_PARAM_STORAGE_FIX.md` carefully
2. Modify `src/lib/parameters/DynamicSparseLayer.h`
3. Add `_n_prealloc` member
4. Change constructor to defer malloc
5. Add `_ensure_allocated()` helper
6. Add checks to `store()`, `get()`, `contains()`, `reset()`

### Step 2: Build and Test
```bash
make microchip_samv71-xult-clickboards_default
make microchip_samv71-xult-clickboards_default upload
# Test param set/save/reboot as shown above
```

### Step 3: Re-enable Services
Once parameters work:
- Edit `boards/microchip/samv71-xult-clickboards/init/rc.board_defaults`
- Uncomment dataman, logger, mavlink lines
- Rebuild and test full boot

### Step 4: Commit and Push
```bash
git add src/lib/parameters/DynamicSparseLayer.h
git commit -m "Fix SAMV7 param storage with lazy allocation

Implements lazy allocation pattern to work around SAMV7 C++ static
initialization bug where malloc fails during global construction.

- Defer malloc from constructor to first use
- Add _ensure_allocated() helper with thread-safe initialization
- Use compare_exchange to handle race conditions
- Fixes param set/save functionality

Tested on SAMV71-XULT-Clickboards.
"
git push origin samv7-custom
```

---

## TODO List (From TodoWrite Tool)

- [ ] Implement lazy allocation in DynamicSparseLayer.h
- [ ] Add _ensure_allocated() helper method with thread safety
- [ ] Update constructor to defer malloc until first use
- [ ] Add allocation checks to store(), get(), contains(), reset()
- [ ] Build firmware with lazy allocation fix
- [ ] Test param set command on hardware
- [ ] Test param save and verify file size > 5 bytes
- [ ] Test parameter persistence across reboot
- [ ] Re-enable dataman in boot scripts
- [ ] Re-enable logger in boot scripts
- [ ] Re-enable mavlink autostart
- [ ] Verify full system boot with all services

---

## Common Issues & Solutions

### Issue: Build fails with submodule errors
```bash
git submodule update --init --recursive
```

### Issue: Upload fails - board not detected
```bash
# Check USB connection
ls -l /dev/ttyACM*
# Try manual reset on board
```

### Issue: Parameters still show 0 byteSize after fix
- Verify you modified `DynamicSparseLayer.h` correctly
- Check constructor actually sets `_slots = nullptr`
- Verify `_ensure_allocated()` is called before `_slots.load()`

### Issue: Crash after implementing lazy allocation
- Don't call destructor on failed static init objects
- Use only placement new, not destructor + placement new
- Check thread safety with `AtomicTransaction`

---

## Key Insights & Lessons Learned

1. **Static initialization is platform-dependent** - Heap may not be ready
2. **Lazy patterns are safer** - Defer allocations until runtime when possible
3. **Existing code has clues** - Look for "workaround" comments
4. **Placement new is dangerous** - Only works on properly initialized memory
5. **Kconfig hierarchy matters** - Parent options gate code compilation, not just logging

---

## References for Deep Understanding

### Must-Read Documentation (In Order)
1. `SAMV7_PARAM_STORAGE_FIX.md` - Complete implementation plan
2. `SAMV7_ACHIEVEMENTS.md` - What's been done and why
3. `RESEARCH_TOPICS.md` - Technical deep-dives

### Code References
- Parameter system: `src/lib/parameters/parameters.cpp`
- Storage layers: `src/lib/parameters/DynamicSparseLayer.h`
- Atomic operations: `src/lib/parameters/atomic_transaction.h`

### External Resources
- PX4 Parameter Docs: https://docs.px4.io/main/en/advanced_config/parameters.html
- NuttX Heap Init: `nuttx/arch/arm/src/armv7-m/arm_initialize.c`
- ARM Cortex-M7: ARMv7-M Architecture Reference Manual

---

## Success Criteria

You'll know it's fixed when:
1. ✅ Boot log shows `user_config byteSize: 256` (or 512) instead of 0
2. ✅ `param set CAL_ACC0_ID 12345` succeeds without error
3. ✅ `param save` creates file >100 bytes (not 5 bytes)
4. ✅ After reboot, `param show CAL_ACC0_ID` displays 12345
5. ✅ All boot services can be re-enabled successfully

---

## Conversation History Highlights

### Session 1-3: SD Card Integration
- Configured HSMCI driver for SD card
- Fixed async initialization timing
- Reduced debug output safely (Kconfig hierarchy lesson)

### Session 4-6: Parameter System Investigation
- Identified root cause: malloc fails during static init
- Traced through parameter layer architecture
- Documented BSON file format issue (5-byte headers)

### Session 7-9: Fix Attempts & Root Cause Confirmation
- Attempted placement new → crashed (destructor on garbage pointer)
- Added diagnostic code to confirm _n_slots = 0
- Designed lazy allocation solution (thread-safe with compare_exchange)

### Session 10: Documentation & Git Workflow
- Created comprehensive documentation (21K+ lines)
- Set up branch structure (main vs samv7-custom)
- Committed all work to GitHub
- Created this context document

---

## Contact & Continuation

**Repository Owner:** bhanuprakashjh
**GitHub:** https://github.com/bhanuprakashjh/PX4-Autopilot-Private
**Branch to Continue:** `samv7-custom`

**Next Session Start:**
```bash
cd /media/bhanu1234/Development/PX4-Autopilot-Private
git checkout samv7-custom
git pull origin samv7-custom  # Get any remote changes
# Read this file again
# Read SAMV7_PARAM_STORAGE_FIX.md
# Start implementing lazy allocation
```

---

## Quick Commands Cheat Sheet

```bash
# Status check
git branch                          # Should show: * samv7-custom
git status                          # Check working tree
git log --oneline -5                # Recent commits

# Build
make microchip_samv71-xult-clickboards_default

# Flash
make microchip_samv71-xult-clickboards_default upload

# Clean build
make clean
make microchip_samv71-xult-clickboards_default

# Submodules
git submodule update --init --recursive

# Switch branches
git checkout samv7-custom           # Enhanced version
git checkout main                   # Original version

# Documentation
cat SAMV7_PARAM_STORAGE_FIX.md      # Implementation guide
cat SAMV7_ACHIEVEMENTS.md           # Progress log
cat README_SAMV7.md                 # Branch usage
```

---

**Last Updated:** November 19, 2025
**Status:** Ready for lazy allocation implementation
**Estimated Time to Fix:** 2-4 hours of focused work

---

## Final Note

The solution is fully designed and documented. The implementation is straightforward:
1. Read `SAMV7_PARAM_STORAGE_FIX.md`
2. Modify `DynamicSparseLayer.h` as specified
3. Build, test, verify
4. Re-enable services
5. Success!

All the hard debugging and analysis work is complete. Now it's just careful implementation of the designed solution.

**You can do this!** 🚀
