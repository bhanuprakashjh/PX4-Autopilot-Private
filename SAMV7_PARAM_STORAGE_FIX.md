# SAMV7 Parameter Storage Fix - Implementation Plan

**Board:** Microchip SAMV71-XULT-Clickboards
**Issue:** Parameters cannot be set or saved due to SAMV7 C++ static initialization bug
**Date:** November 19, 2025
**Status:** Root cause identified, solution designed, implementation pending

---

## Executive Summary

The SAMV71 board's parameter system is broken due to a **C++ static initialization bug** where `malloc()` fails during global object construction before `main()` runs. This causes the parameter storage layers (`DynamicSparseLayer` objects) to have zero allocated slots, preventing any parameter writes.

**Chosen Solution:** Implement lazy allocation in `DynamicSparseLayer` to defer `malloc()` until the heap is available at runtime.

---

## Root Cause Analysis

### The Problem

1. **Static Initialization Order:** PX4's parameter system uses statically initialized storage layers:
   ```cpp
   // src/lib/parameters/parameters.cpp:103-105
   static ConstLayer firmware_defaults;
   static DynamicSparseLayer runtime_defaults{&firmware_defaults};
   DynamicSparseLayer user_config{&runtime_defaults};  // <-- Fails here
   ```

2. **Constructor Allocation:** `DynamicSparseLayer` constructor attempts `malloc()` during static init:
   ```cpp
   // src/lib/parameters/DynamicSparseLayer.h:43-58
   DynamicSparseLayer(ParamLayer *parent, int n_prealloc = 32, int n_grow = 4)
       : ParamLayer(parent), _n_slots(n_prealloc), _n_grow(n_grow)
   {
       Slot *slots = (Slot *)malloc(sizeof(Slot) * n_prealloc);  // <-- Fails on SAMV7

       if (slots == nullptr) {
           PX4_ERR("Failed to allocate memory for dynamic sparse layer");
           _n_slots = 0;  // <-- Result: zero slots
           return;
       }
       // ...
   }
   ```

3. **SAMV7 Heap Not Ready:** On SAMV7, the heap is not fully initialized during C++ static construction phase, causing `malloc()` to return `nullptr`.

4. **Failure Cascade:**
   - Zero slots allocated → `_n_slots = 0`
   - `store()` calls `_grow()` → returns false because `_n_slots == 0` (line 202-204)
   - `param_set()` fails with "failed to store param" error
   - `param_save()` writes empty storage array → 5-byte BSON header only

### Evidence

**Boot log confirmation:**
```
INFO  [parameters] runtime_defaults size: 0 byteSize: 0
INFO  [parameters] user_config size: 0 byteSize: 0
```

**Existing workaround comment in code:**
```cpp
// DynamicSparseLayer.h:124-128
// Workaround for C++ static initialization bug on SAMV7
// If parent is null, return default value instead of crashing
if (_parent == nullptr) {
    return px4::parameters[param].val;
}
```

This proves the SAMV7 static init issue is already known and partially worked around.

---

## Solution: Lazy Allocation

### Design

**Core Concept:** Defer `malloc()` from constructor to first use when heap is guaranteed available.

**Key Changes:**
1. Constructor sets `_slots = nullptr` instead of allocating
2. Add `_ensure_allocated()` helper called before any slot access
3. Use `AtomicTransaction` to ensure thread-safe lazy initialization
4. Minimal impact on existing logic

### Implementation

**File:** `src/lib/parameters/DynamicSparseLayer.h`

**Modified Constructor:**
```cpp
DynamicSparseLayer(ParamLayer *parent, int n_prealloc = 32, int n_grow = 4)
    : ParamLayer(parent), _n_prealloc(n_prealloc), _n_slots(0), _n_grow(n_grow)
{
    _slots.store(nullptr);  // Lazy allocation - defer until first use
}
```

**New Helper Method:**
```cpp
private:
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

        // Atomic swap
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

    int _n_prealloc;  // New member: remember prealloc size for lazy init
```

**Modified Methods:**
```cpp
bool store(param_t param, param_value_u value) override
{
    AtomicTransaction transaction;

    if (!_ensure_allocated()) {  // <-- Add lazy init check
        return false;
    }

    Slot *slots = _slots.load();
    // ... rest unchanged
}

param_value_u get(param_t param) const override
{
    const AtomicTransaction transaction;

    if (!_ensure_allocated()) {  // <-- Add lazy init check
        // Fallback to parent or firmware defaults
        if (_parent == nullptr) {
            return px4::parameters[param].val;
        }
        return _parent->get(param);
    }

    Slot *slots = _slots.load();
    // ... rest unchanged
}

// Similar changes for contains(), reset(), etc.
```

### Thread Safety

- `_ensure_allocated()` uses `compare_exchange` for race-free initialization
- Only one thread succeeds in allocating; losers free their allocation
- `AtomicTransaction` already held by calling methods ensures mutual exclusion
- Once `_slots != nullptr`, fast path skips allocation logic

---

## Why This Works

1. **Heap Available:** By the time `param_init()` calls `param_load_default()` which calls `user_config.get()`, the heap is fully initialized
2. **Transparent:** No changes needed to calling code - works with existing PX4 scripts
3. **Backwards Compatible:** Other platforms unaffected - they already succeed in constructor
4. **Proven Pattern:** Similar to the existing `_parent == nullptr` workaround already in the code
5. **Minimal Risk:** Isolated to one header file, leverages existing atomic infrastructure

---

## Implementation Steps

1. **Modify DynamicSparseLayer.h:**
   - Add `_n_prealloc` member variable
   - Update constructor to defer allocation
   - Add `_ensure_allocated()` helper
   - Add allocation checks to `store()`, `get()`, `contains()`, `reset()`

2. **Build and Test:**
   - Compile firmware
   - Boot on SAMV71 board
   - Verify boot log shows successful parameter loading
   - Test `param set CAL_ACC0_ID 12345`
   - Test `param save`
   - Verify file size > 5 bytes
   - Reboot and confirm persistence

3. **Re-enable Boot Services:**
   - Restore dataman startup
   - Restore logger startup
   - Restore mavlink autostart
   - Test full system boot

4. **Upstream Contribution:**
   - Clean up diagnostic code
   - Add code comments explaining SAMV7 fix
   - Submit PR to PX4 with explanation
   - Reference this bug in commit message

---

## Alternative Solutions (Not Chosen)

### Option 2: Fix Flash Backend

**Approach:** Use `FLASH_BASED_PARAMS` with corrected sector map for SAMV71Q21.

**Why Rejected:**
- Flash wear concerns (parameters change frequently)
- More complex debugging
- Requires precise flash layout knowledge
- Still want SD card for logs/dataman anyway
- Doesn't fix underlying SAMV7 heap issue

**If Needed Later:**
Correct sector map for SAMV71Q21 (2MB flash):
```c
static sector_descriptor_t params_sector_map[] = {
    {126, 8 * 1024, 0x001FC000},  // 8KB sector at -16KB
    {127, 8 * 1024, 0x001FE000},  // 8KB sector at -8KB
    {0, 0, 0},
};
```

### Option 3: Runtime Placement New

**Approach:** Detect failed static init in `param_init()`, use placement new to reconstruct.

**Why Failed:**
- Calling destructor on corrupted object → crashes (tries to `free()` garbage pointer)
- Placement new without destructor → memory corruption (overlapping allocations)
- Observed: "sam_reserved: PANIC!!! Reserved interrupt" crash loop

**Evidence:**
```
WARN  [parameters] user_config storage uninitialized, reinitializing
sam_reserved: PANIC!!! Reserved interrupt
up_assert: Assertion failed at file:chip/sam_irq.c line: 222 task: param
```

---

## Testing Plan

### Phase 1: Basic Functionality
- [ ] Build completes without errors
- [ ] Board boots to NSH prompt
- [ ] No "byteSize: 0" errors in boot log
- [ ] `param show` displays all parameters
- [ ] `param set TEST_PARAM 123` succeeds
- [ ] `param save` succeeds
- [ ] `/fs/microsd/params` file size > 100 bytes

### Phase 2: Persistence
- [ ] Reboot board
- [ ] `param show TEST_PARAM` shows saved value
- [ ] Modify 10 parameters
- [ ] `param save`
- [ ] Reboot
- [ ] Verify all 10 parameters persisted

### Phase 3: Stress Testing
- [ ] Set 100 parameters rapidly
- [ ] Verify autosave works
- [ ] Force reboot during autosave (corruption test)
- [ ] Verify backup file restores correctly
- [ ] Test parameter reset (`param reset_all`)

### Phase 4: Integration
- [ ] Re-enable dataman
- [ ] Re-enable logger with parameters
- [ ] Re-enable mavlink with config
- [ ] Full boot sequence completes
- [ ] All sensors initialize with calibration params

---

## Known Issues & Limitations

### SD Card Initialization Errors (Non-Critical)

Current boot log shows:
```
sam_waitresponse: ERROR: cmd: 00008101 events: 009b0001 SR: 04100024
mmcsd_sendcmdpoll: ERROR: Wait for response to cmd: 00008101 failed: -116
mmcsd_cardidentify: ERROR: CMD1 RECVR3: -22
mmcsd_recv_r1: ERROR: R1=00400120
mmcsd_cardidentify: ERROR: mmcsd_recv_r1(CMD55) failed: -5
```

**Analysis:**
- CMD1 (SEND_OP_COND) fails → card is SD not MMC
- Driver falls back to SD mode successfully
- Mount works, file I/O works
- Errors are harmless but noisy

**Fix (Low Priority):**
Modify `sam_hsmci_initialize()` to skip MMC detection and go straight to SD mode.

### MTD Caldata Errors (Expected)

```
ERROR [bsondump] open '/fs/mtd_caldata' failed (2)
```

**Analysis:**
- No MTD flash parameter backend configured
- Expected when using SD-only parameter storage
- Can be suppressed by removing MTD caldata from init scripts

---

## File Change Summary

**Modified:**
- `src/lib/parameters/DynamicSparseLayer.h` - Lazy allocation implementation

**Previously Modified (Complete):**
- `boards/microchip/samv71-xult-clickboards/src/board_config.h` - Commented out `FLASH_BASED_PARAMS`
- `boards/microchip/samv71-xult-clickboards/src/init.c` - Removed flash param init, added SD logging
- `boards/microchip/samv71-xult-clickboards/default.px4board` - Added SD parameter paths
- `boards/microchip/samv71-xult-clickboards/init/rc.board_defaults` - Disabled services for testing

**No Changes Needed:**
- Parameter system core (`parameters.cpp`) - works once storage layer is fixed
- NuttX configuration - SD card driver already working
- Build system - paths already configured

---

## Success Criteria

1. ✅ Root cause identified and documented
2. ✅ Solution designed with minimal risk
3. ⏳ Implementation completed
4. ⏳ Parameters can be set individually
5. ⏳ Parameters can be saved to SD card
6. ⏳ Parameters persist across reboots
7. ⏳ All boot services re-enabled
8. ⏳ Full system functional

---

## References

- **SAMV7 Workaround:** `DynamicSparseLayer.h:124-128` (existing NULL parent check)
- **Static Init Bug:** https://gcc.gnu.org/bugzilla/show_bug.cgi?id=70013 (similar GCC issue)
- **PX4 Parameter Docs:** https://docs.px4.io/main/en/advanced_config/parameters.html
- **NuttX Heap Init:** `nuttx/arch/arm/src/armv7-m/arm_initialize.c`

---

## Author Notes

This issue affects **all SAMV7-based PX4 boards** using file-based parameters. The lazy allocation fix should be contributed upstream to benefit:
- Microchip SAMV71-XULT variants
- Custom SAMV7-based autopilots
- Any ARM platform with similar heap init timing issues

The fix is architecturally sound and follows PX4's existing atomic transaction patterns for thread safety.
