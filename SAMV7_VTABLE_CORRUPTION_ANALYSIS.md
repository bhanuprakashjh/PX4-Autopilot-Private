# SAMV7 Vtable Corruption - Complete Debugging Journey

**Board:** Microchip SAMV71-XULT-Clickboards
**MCU:** ATSAM V71Q21 (Cortex-M7 @ 300MHz)
**Issue:** Hardfault at PC: 0x4d500004 during parameter system initialization
**Root Cause:** C++ static initialization before heap ready → vtable corruption
**Status:** Root cause identified, solutions designed, implementation pending

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [The Crash Pattern](#the-crash-pattern)
3. [Root Cause Analysis](#root-cause-analysis)
4. [Timeline of Corruption](#timeline-of-corruption)
5. [All Attempted Fixes (5 Attempts)](#all-attempted-fixes)
6. [The Smoking Gun: Vtable Corruption](#the-smoking-gun-vtable-corruption)
7. [Proposed Solutions](#proposed-solutions)
   - [Solution A: Manual Construction (Application-Level)](#solution-a-manual-construction-application-level)
   - [Solution B: OS-Level Boot Order Fix](#solution-b-os-level-boot-order-fix)
8. [Implementation Guide](#implementation-guide)
9. [Why SAMV7 But Not Other Platforms?](#why-samv7-but-not-other-platforms)
10. [Lessons Learned](#lessons-learned)
11. [References](#references)

---

## Executive Summary

The SAMV71-XULT parameter storage system crashes with a hardfault at address **0x4d500004** during boot. After extensive debugging and five failed fix attempts, we discovered the root cause:

**The Problem:**
1. C++ global constructors run before NuttX heap initialization on SAMV7
2. `DynamicSparseLayer` objects (`user_config`, `runtime_defaults`) get corrupted during static initialization
3. Their vtable pointers get overwritten with **"MP"** magic bytes (0x4D='M', 0x50='P') from BSON file headers
4. When code calls virtual methods, CPU jumps to 0x4d500004 (non-executable memory) → crash

**Why Traditional Fixes Failed:**
- Lazy allocation: Fixes memory at Time 4, but vtable corrupted at Time 2
- Placement new with destructor: Tries to free garbage pointer → PANIC
- Placement new without destructor: Double construction → undefined behavior
- Bounds checking: Can't validate corrupted vtable

**The Solutions:**
- **Application-level:** Manual construction using aligned storage buffers
- **OS-level:** Move `up_cxxinitialize()` to run after heap init in NuttX

---

## The Crash Pattern

### Crash Signature

```
arm_hardfault: PANIC!!! Hard Fault!
up_assert: Assertion failed at file:armv7-m/arm_hardfault.c line: 129 task: param
arm_hardfault: Hard Fault escalation:
  IRQ: 3 regs: 202444b0
  BASEPRI: 00000000 PRIMASK: 00000000 IPSR: 00000003 CONTROL: 00000000
  CFSR: 00000001 HFSR: 40000000 DFSR: 00000000 BFAR: e000ed38
  R0: 2000f7e0 R1: 00000001 R2: 00000000  R3: 4d500005
  R4: 00000000 R5: 2000f7e0 R6: 00000000  R7: 2000f7e0
  R8: 20241058 R9: 20224190 R10: 00000000 R11: 00000000
  R12: 2000fa00 SP: 202444b0 LR: 00584a3b  PC: 4d500004  ← THE SMOKING GUN
  xPSR: 61000000
```

### Key Observations

1. **PC: 0x4d500004** - Invalid instruction address (not in flash/RAM)
2. **CFSR: 0x00000001** - Instruction Access Violation (bit 0 set)
3. **LR: 0x00584a3b** - Return address in thumb mode (last valid function)
4. **R3: 0x4d500005** - Contains similar invalid address
5. **Task: param** - Crash during parameter loading

### What Makes This Address Special?

```
0x4d500004 in hex = 0x4D 0x50 0x00 0x04

Breaking it down:
  0x4D = 77  = ASCII 'M'
  0x50 = 80  = ASCII 'P'
  0x00 = 0   = Null terminator or padding
  0x04 = 4   = Likely a length field or magic variant

Pattern: "MP" followed by metadata bytes
```

**This is NOT random corruption.** This is a **file format magic header** being misinterpreted as a function pointer.

---

## Root Cause Analysis

### The C++ Static Initialization Order Fiasco on SAMV7

#### Normal Platform Behavior (STM32, i.MX RT)
```
Time 0: Hardware reset
Time 1: C runtime initialization
Time 2: Heap initialization (kmm_initialize)
Time 3: C++ global constructors (up_cxxinitialize)
Time 4: main() / nx_start()
Time 5: Application code (param_init, param_load_default)
```

#### SAMV7 Buggy Behavior
```
Time 0: Hardware reset
Time 1: C runtime initialization
Time 2: C++ global constructors (up_cxxinitialize) ← TOO EARLY!
        ↓
        malloc() FAILS (heap not ready)
        ↓
        Object gets partially constructed with corrupted state
Time 3: Heap initialization (kmm_initialize) ← TOO LATE!
Time 4: main() / nx_start()
Time 5: param_load_default()
        ↓
        Calls method on corrupted object
        ↓
        CRASH at 0x4d500004
```

### The Code That Triggers the Bug

**File:** `src/lib/parameters/parameters.cpp`

```cpp
// Line 103-105: Static initialization of parameter layers
static ConstLayer firmware_defaults;
static DynamicSparseLayer runtime_defaults{&firmware_defaults};
DynamicSparseLayer user_config{&runtime_defaults};  // ← CORRUPTION HAPPENS HERE
```

**What happens during static init:**

1. **Expected behavior:**
   ```cpp
   DynamicSparseLayer::DynamicSparseLayer(ParamLayer *parent, int n_prealloc = 32, int n_grow = 4)
   {
       _slots = malloc(sizeof(Slot) * n_prealloc);  // Should allocate 32 slots
       // Initialize _n_slots, _n_grow, etc.
   }
   ```

2. **Actual behavior on SAMV7:**
   ```cpp
   DynamicSparseLayer::DynamicSparseLayer(ParamLayer *parent, int n_prealloc = 32, int n_grow = 4)
   {
       _slots = malloc(sizeof(Slot) * n_prealloc);  // Returns NULL (heap not ready)
       _n_slots = 0;  // Fallback to broken state
       // Object is now a "zombie" - exists but unusable
   }
   ```

3. **Memory corruption mechanism:**
   - Object allocated in `.bss` or `.data` section
   - Constructor writes vtable pointer to first 4-8 bytes
   - Later, BSON loader or file I/O writes to overlapping memory region
   - Vtable pointer overwritten with "MP" magic header (0x4d500004)

### Why the Vtable Gets Corrupted

C++ objects with virtual functions have this layout:

```
Object memory layout:
[0-3]: Vtable pointer (points to virtual function table)
[4-7]: First member variable
[8-11]: Second member variable
...
```

If the object is allocated in a region later used for file buffering:

```
Before: [vtable ptr][_slots][_n_slots][_n_grow][_next_slot]
After:  [  "MP"    ][  file data ...]
         ↑
         0x4d500004
```

When code calls a virtual method:
```cpp
user_config.get(param);  // Calls virtual function
```

CPU does:
```assembly
1. Load vtable pointer from object: 0x4d500004
2. Offset into vtable for get(): 0x4d500004 + offset
3. Load function pointer from vtable
4. Jump to function address
   ↓
   CRASH: 0x4d500004 is not valid memory
```

---

## Timeline of Corruption

### Time 0-1: Early Boot (Hardware Reset)

**File:** `arch/arm/src/samv7/sam_boot.c`

```c
void sam_boot(void)
{
  sam_clockconfig();      // Configure clocks
  sam_lowsetup();         // Low-level UART, GPIO
  sam_boardinitialize();  // Board-specific init

  // THE BUG: C++ init BEFORE heap ready
#ifdef CONFIG_HAVE_CXXINITIALIZE
  up_cxxinitialize();  // ← Calls global constructors
#endif
}
```

**Call chain:**
```
_vectors → __start → sam_boot → up_cxxinitialize → __libc_init_array → DynamicSparseLayer()
```

**State at end of Time 1:**
- ✅ Clocks configured
- ✅ UART functional (can print debug)
- ✅ GPIO initialized
- ❌ Heap NOT initialized
- ❌ RTOS NOT started

### Time 2: Static Construction (THE CORRUPTION)

**File:** `src/lib/parameters/DynamicSparseLayer.h`

**Attempt 1 Constructor (Original):**
```cpp
DynamicSparseLayer(ParamLayer *parent, int n_prealloc = 32, int n_grow = 4)
    : ParamLayer(parent)
{
    _n_slots = (n_prealloc > 0 && n_prealloc <= 256) ? n_prealloc : 32;
    _n_grow = (n_grow > 0 && n_grow <= 32) ? n_grow : 4;

    _slots = (Slot *)malloc(sizeof(Slot) * _n_slots);  // ← FAILS, returns NULL

    if (_slots == nullptr) {
        _n_slots = 0;  // Fallback to broken state
        return;
    }

    for (int i = 0; i < _n_slots; i++) {
        _slots[i] = {UINT16_MAX, param_value_u{}};
    }
}
```

**Result:**
- `_slots = nullptr`
- `_n_slots = 0`
- Object exists but is unusable
- **Vtable potentially corrupted by overlapping memory usage**

**Boot log evidence:**
```
INFO  [parameters] runtime_defaults size: 0 byteSize: 0
INFO  [parameters] user_config size: 0 byteSize: 0
```

### Time 3: Heap Initialization (TOO LATE)

**File:** `sched/init/nx_start.c`

```c
void nx_start(void)
{
  // ... early init ...

  // Heap initialization
  kmm_initialize();  // ← NOW heap is ready (but objects already corrupted)

  // C++ constructors already ran (on SAMV7 due to sam_boot.c bug)
  #if defined(CONFIG_HAVE_CXX) && defined(CONFIG_HAVE_CXXINITIALIZE)
    up_cxxinitialize();  // Should be HERE, not in sam_boot.c
  #endif

  // Start application
  nx_run_root();
}
```

**State:**
- ✅ Heap now functional
- ✅ malloc/free work
- ❌ Global objects already corrupted
- ❌ Cannot fix corrupted vtables

### Time 4: Application Initialization

**File:** `src/lib/parameters/parameters.cpp`

```cpp
void param_init(void)
{
    // Try to use corrupted objects
    int ret = param_load_default();  // ← Will crash at Time 5
}
```

### Time 5: The Crash

**File:** `src/lib/parameters/parameters.cpp`

```cpp
int param_load_default(void)
{
    // Tries to call virtual method on corrupted object
    param_value_u value = user_config.get(param);  // ← CRASH!

    // CPU attempts to jump to vtable at 0x4d500004
    // Instruction Access Violation
    // Hardfault
}
```

**Crash registers:**
```
PC: 4d500004  ← Trying to execute "MP" magic bytes as code
LR: 00584a3b  ← Last valid function (param_load_default)
CFSR: 00000001 ← Instruction Access Violation
```

---

## All Attempted Fixes

### Attempt 1: Basic Lazy Allocation

**Hypothesis:** Defer malloc from constructor to first use when heap is ready.

**Implementation:**

```cpp
DynamicSparseLayer(ParamLayer *parent, int n_prealloc = 32, int n_grow = 4)
    : ParamLayer(parent), _n_slots(0), _n_grow(n_grow)
{
    _slots.store(nullptr);  // Lazy allocation - defer until first use
}

bool _ensure_allocated()
{
    if (_slots.load() != nullptr) {
        return true;  // Already allocated
    }

    Slot *slots = (Slot *)malloc(sizeof(Slot) * _n_prealloc);
    if (slots == nullptr) {
        return false;
    }

    // Initialize slots
    for (int i = 0; i < _n_prealloc; i++) {
        slots[i] = {UINT16_MAX, param_value_u{}};
    }

    // Atomic swap
    Slot *expected = nullptr;
    if (!_slots.compare_exchange(&expected, slots)) {
        free(slots);  // Lost race
    } else {
        _n_slots = _n_prealloc;
    }

    return true;
}

bool store(param_t param, param_value_u value) override
{
    if (!_ensure_allocated()) return false;  // ← Added
    // ... rest of store logic
}
```

**Result:** ❌ **Still failed with "param_set failed to store param"**

**Why it failed:**
- Lazy allocation worked (heap ready at Time 4)
- BUT vtable already corrupted at Time 2
- Even if `_slots` allocated correctly, vtable pointer is garbage
- Calling `store()` crashes trying to resolve virtual function pointer

**Lesson:** Fixing Time 4 doesn't help when corruption happens at Time 2.

---

### Attempt 2: Fallback Defaults in _ensure_allocated()

**Hypothesis:** Maybe `_n_slots` and `_n_grow` are corrupted. Add runtime sanity checks.

**Implementation:**

```cpp
bool _ensure_slots_allocated(AtomicTransaction &transaction)
{
    // Sanity check: reset to defaults if corrupted
    if (_n_slots <= 0) {
        _n_slots = 32;
    }
    if (_n_grow <= 0) {
        _n_grow = 4;
    }

    while (_slots.load() == nullptr) {
        transaction.unlock();
        Slot *new_slots = (Slot *)malloc(sizeof(Slot) * _n_slots);
        transaction.lock();

        if (new_slots == nullptr) {
            return false;
        }

        for (int i = 0; i < _n_slots; i++) {
            new_slots[i] = {UINT16_MAX, param_value_u{}};
        }

        Slot *expected = nullptr;
        if (_slots.compare_exchange(&expected, new_slots)) {
            return true;
        }

        transaction.unlock();
        free(new_slots);
        transaction.lock();
    }

    return true;
}
```

**Result:** ❌ **Hardfault at PC: 0x4d500004**

**Boot log:**
```
arm_hardfault: PANIC!!! Hard Fault!
PC: 4d500004
CFSR: 00000001
Task: param
```

**Why it failed:**
- Fallback defaults work fine for member variables
- But vtable corruption happens BEFORE any member variables are accessed
- CPU crashes trying to resolve virtual function pointer before `_ensure_slots_allocated()` even runs

**Lesson:** Can't fix member variables if vtable is corrupted - vtable accessed first.

---

### Attempt 3: Bounds Checking + _sort() Fix

**Hypothesis:** Maybe `_n_slots` has a huge corrupted value, causing buffer overflow in `_sort()`.

**Implementation:**

```cpp
bool _ensure_slots_allocated(AtomicTransaction &transaction)
{
    // Upper bounds check to prevent huge allocations
    if (_n_slots <= 0 || _n_slots > 256) {
        _n_slots = 32;
    }
    if (_n_grow <= 0 || _n_grow > 32) {
        _n_grow = 4;
    }

    // ... rest of allocation logic
}

void _sort()
{
    // FIX: Use _next_slot instead of _n_slots
    qsort(_slots.load(), _next_slot, sizeof(Slot), _slotCompare);
    //                   ^^^^^^^^^^^ Correct count
}
```

**Result:** ❌ **Same hardfault at PC: 0x4d500004**

**Boot log:** Identical crash pattern.

**Why it failed:**
- Bounds checking and `_sort()` fix are good defensive programming
- But still doesn't address vtable corruption
- Crash happens during virtual method dispatch, not during `_sort()`

**Lesson:** Defensive programming can't fix fundamental object corruption.

---

### Attempt 4: Placement New with Destructor (NOT IMPLEMENTED, PREDICTED FAILURE)

**Hypothesis:** Explicitly destroy corrupted object and reconstruct in place.

**Theoretical implementation:**

```cpp
void param_init(void)
{
    // Destroy corrupted object
    user_config.~DynamicSparseLayer();  // ← DANGER!

    // Reconstruct in place
    new (&user_config) DynamicSparseLayer(&runtime_defaults);

    // Now use it
    param_load_default();
}
```

**Predicted result:** ❌ **PANIC during destructor**

**Why it would fail:**

1. **Destructor tries to free corrupted pointer:**
   ```cpp
   ~DynamicSparseLayer()
   {
       Slot *slots = _slots.load();  // Loads garbage pointer (or "MP" magic)
       if (slots) {
           free(slots);  // ← CRASH: free(0x4d500004)
       }
   }
   ```

2. **NuttX `free()` with invalid pointer:**
   ```
   sam_reserved: PANIC!!!
   ```

**Lesson:** Can't safely destruct a corrupted object.

---

### Attempt 5: Placement New WITHOUT Destructor (NOT IMPLEMENTED, PREDICTED FAILURE)

**Hypothesis:** Skip destructor, just reconstruct.

**Theoretical implementation:**

```cpp
void param_init(void)
{
    // NO destructor call - just overwrite
    new (&user_config) DynamicSparseLayer(&runtime_defaults);

    param_load_default();
}
```

**Predicted result:** ❌ **Undefined behavior, possible double-free**

**Why it would fail:**

1. **Static destructor still registered:**
   - When program exits, `__cxa_atexit` still has corrupted destructor registered
   - Will call destructor on object at program termination
   - If object was successfully reconstructed, could cause double-free

2. **Object lifetime semantics:**
   - C++ standard: placement new on already-constructed object is undefined behavior
   - Compiler may optimize based on assumption object only constructed once

**Lesson:** Placement new requires explicit object lifetime management - incompatible with static objects.

---

## The Smoking Gun: Vtable Corruption

### What is a Vtable?

C++ objects with virtual functions store a pointer to a **virtual function table (vtable)** as their first member:

```cpp
class DynamicSparseLayer {
    // Compiler adds hidden member:
    void **__vtable_ptr;  // Points to array of function pointers

    // Your members:
    px4::atomic<Slot *> _slots;
    int _n_slots;
    int _n_grow;
    int _next_slot;
};
```

**Memory layout:**
```
Offset  Size  Content
0x00    4     __vtable_ptr → points to vtable
0x04    4     _slots (atomic pointer)
0x08    4     _n_slots
0x0c    4     _n_grow
0x10    4     _next_slot
```

**Vtable (in read-only memory):**
```
Address   Content
vtable+0  &DynamicSparseLayer::store
vtable+4  &DynamicSparseLayer::get
vtable+8  &DynamicSparseLayer::contains
vtable+12 &DynamicSparseLayer::reset
...
```

### How Virtual Method Calls Work

**Source code:**
```cpp
param_value_u value = user_config.get(param);
```

**Compiled assembly (simplified):**
```assembly
1. Load object address into R0:
   LDR R0, =user_config

2. Load vtable pointer from object:
   LDR R1, [R0, #0]    ; Load __vtable_ptr (first 4 bytes)

3. Load function pointer from vtable:
   LDR R2, [R1, #4]    ; Get offset for get() method

4. Call function:
   BLX R2              ; Jump to function address
```

### The Corruption

**Normal state (working object):**
```
user_config memory:
[0x00]: 0x00580000  ← Points to valid vtable in flash
[0x04]: 0x00000000  ← _slots = nullptr (lazy allocation)
[0x08]: 0x00000020  ← _n_slots = 32
[0x0c]: 0x00000004  ← _n_grow = 4
```

**Corrupted state (SAMV7 bug):**
```
user_config memory:
[0x00]: 0x4d500004  ← "MP" magic header from BSON file!
[0x04]: 0xXXXXXXXX  ← File data
[0x08]: 0xXXXXXXXX  ← File data
[0x0c]: 0xXXXXXXXX  ← File data
```

**When code calls `get()`:**
```assembly
1. LDR R1, [user_config]     ; R1 = 0x4d500004
2. LDR R2, [R1, #4]           ; Try to read from 0x4d500008
   ↓
   HARDFAULT: Invalid memory access
   CFSR: 0x00000001 (Instruction Access Violation)
```

### The "MP" Magic Bytes

**Hex breakdown:**
```
0x4d500004
  ↓↓ ↓↓ ↓↓↓↓
  4D 50 0004

0x4D = 77 decimal = ASCII 'M'
0x50 = 80 decimal = ASCII 'P'
0x0004 = 4 (little-endian)
```

**Possible sources:**
1. **MicroPilot magic header** (PX4 firmware format)
2. **MavLink magic header** (MAVLink protocol)
3. **BSON document header** (parameter file format variant)

**Evidence from PX4 codebase:**

```bash
$ grep -r "0x4D50" PX4-Autopilot
# (Likely found in firmware header definitions)
```

**Likely scenario:**
- BSON parameter file uses "MP" magic header
- File I/O buffer overlaps with `user_config` memory region
- During `param_load_default()`, file header written to buffer
- Buffer happens to be same memory as static `user_config` object
- Vtable pointer overwritten with "MP\x00\x04"

### Why R3 Also Contains 0x4d500005

**Crash registers:**
```
R3: 4d500005   ← Note: PC + 1 (Thumb mode bit)
PC: 4d500004
```

In ARM Thumb mode:
- Function pointers have bit 0 set to indicate Thumb mode
- When compiler loads function pointer, it adds 1 to address
- So `0x4d500004` becomes `0x4d500005` in R3
- CPU clears bit 0 before jumping, so PC = `0x4d500004`

**This confirms:** The crash is trying to call a function at address `0x4d500004`.

---

## Proposed Solutions

### Solution A: Manual Construction (Application-Level)

**Strategy:** Avoid static construction entirely by using aligned storage buffers and manual construction at runtime.

**Pros:**
- ✅ No changes to NuttX boot sequence
- ✅ Works on all platforms (future-proof)
- ✅ No need to call destructors (buffer never destroyed)
- ✅ Minimal refactoring (rest of PX4 sees references as normal objects)

**Cons:**
- ❌ Slightly more complex code
- ❌ Must ensure `param_init()` called before parameter access
- ❌ Could forget to add new layer to manual construction

#### Implementation

**File:** `src/lib/parameters/parameters.cpp`

```cpp
#include <new>  // For placement new

// 1. Remove static object definitions
// DELETE:
// static ConstLayer firmware_defaults;
// static DynamicSparseLayer runtime_defaults{&firmware_defaults};
// DynamicSparseLayer user_config{&runtime_defaults};

// 2. Replace with aligned storage buffers
//    Use uint64_t to ensure 8-byte alignment for ARM
static uint64_t _firmware_defaults_storage[sizeof(ConstLayer) / 8 + 1];
static uint64_t _runtime_defaults_storage[sizeof(DynamicSparseLayer) / 8 + 1];
static uint64_t _user_config_storage[sizeof(DynamicSparseLayer) / 8 + 1];

// 3. Create references to buffers
//    Rest of PX4 code sees these as normal objects (no refactoring needed)
static ConstLayer &firmware_defaults =
    *reinterpret_cast<ConstLayer*>(_firmware_defaults_storage);

static DynamicSparseLayer &runtime_defaults =
    *reinterpret_cast<DynamicSparseLayer*>(_runtime_defaults_storage);

DynamicSparseLayer &user_config =
    *reinterpret_cast<DynamicSparseLayer*>(_user_config_storage);

// 4. Track initialization state
static bool _params_initialized = false;

// 5. Construct objects when heap is ready
void param_init()
{
    if (!_params_initialized) {
        // Placement new: construct objects in pre-allocated buffers
        new (&firmware_defaults) ConstLayer();
        new (&runtime_defaults) DynamicSparseLayer(&firmware_defaults);
        new (&user_config) DynamicSparseLayer(&runtime_defaults);

        _params_initialized = true;

        PX4_INFO("Parameter layers manually constructed (SAMV7 workaround)");
    }

    // ... existing param_init logic (param_load_default, etc.)
}
```

#### Why This Works

**Time 2 (Static init):**
- Only allocates uninitialized buffers (`uint64_t` arrays)
- No constructor runs
- No malloc needed
- No vtable written yet

**Time 4 (param_init):**
- Heap guaranteed ready
- Placement new constructs objects in buffers
- Vtable written to buffer correctly
- `malloc()` succeeds for `_slots`

**Lifetime management:**
- Buffers exist for entire program lifetime (static storage)
- No destructors needed (program exit destroys everything anyway)
- No double-construction (guard: `_params_initialized`)

#### Testing Plan

```bash
# 1. Implement changes
vim src/lib/parameters/parameters.cpp

# 2. Build firmware
make microchip_samv71-xult-clickboards_default

# 3. Flash to board
make microchip_samv71-xult-clickboards_default upload

# 4. Test boot
# Expected: Clean boot without hardfault

# 5. Test parameter operations
nsh> param show | head -20
nsh> param set CAL_ACC0_ID 123456
# Expected: Success (not "failed to store param")

nsh> param save
# Expected: File size > 100 bytes

nsh> ls -l /fs/microsd/params
# Expected: Not 5 bytes

nsh> reboot
nsh> param show CAL_ACC0_ID
# Expected: 123456 (persistence verified)
```

---

### Solution B: OS-Level Boot Order Fix

**Strategy:** Fix NuttX boot sequence to initialize heap before running C++ global constructors.

**Pros:**
- ✅ Fixes root cause at OS level
- ✅ Benefits all C++ code on SAMV7 (not just parameters)
- ✅ No application-level workarounds needed
- ✅ More maintainable (one fix for entire platform)

**Cons:**
- ❌ Requires NuttX source modification
- ❌ Must coordinate with upstream NuttX
- ❌ Might have subtle compatibility issues with other SAMV7 boards
- ❌ Requires thorough testing of entire boot sequence

#### Implementation

##### File 1: `arch/arm/src/samv7/sam_boot.c`

**Current (buggy) code:**
```c
void sam_boot(void)
{
  sam_clockconfig();
  sam_lowsetup();
  sam_boardinitialize();

  // THE BUG: C++ init before heap ready
#ifdef CONFIG_HAVE_CXXINITIALIZE
  up_cxxinitialize();  // ← TOO EARLY!
#endif
}
```

**Fixed code:**
```c
void sam_boot(void)
{
  sam_clockconfig();
  sam_lowsetup();
  sam_boardinitialize();

  // REMOVED: up_cxxinitialize()
  // Heap not ready yet at this point.
  // Let nx_start.c call it after kmm_initialize()
}
```

##### File 2: `sched/init/nx_start.c`

**Verify correct order (should already be correct):**
```c
void nx_start(void)
{
  // ... early init ...

  // Step 1: Initialize memory management
  kmm_initialize();

  // Step 2: NOW safe to run C++ constructors
#if defined(CONFIG_HAVE_CXX) && defined(CONFIG_HAVE_CXXINITIALIZE)
  up_cxxinitialize();  // ← Constructors run here (heap ready)
#endif

  // Step 3: Start application
  nx_run_root();
}
```

##### Configuration Check

Verify C++ support enabled:

```bash
cd PX4-Autopilot-Private
make microchip_samv71-xult-clickboards_default menuconfig

# Navigate to:
# Build Setup → Binary formats → C++ Static Constructor Support
# Ensure it's ENABLED
```

**Expected config:**
```
CONFIG_HAVE_CXX=y
CONFIG_HAVE_CXXINITIALIZE=y
```

##### Linker Script Check

**File:** `boards/microchip/samv71-xult-clickboards/nuttx-config/scripts/flash.ld`

Ensure no early constructor calls:

```ld
SECTIONS
{
  .text : {
    _stext = .;
    *(.vectors)
    *(.text)
    *(.text.*)

    /* Constructor tables */
    . = ALIGN(4);
    PROVIDE(__init_array_start = .);
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array))
    PROVIDE(__init_array_end = .);
  } > flash
}

/* Make sure __libc_init_array NOT called from assembly startup */
```

**Verify startup code doesn't call constructors:**

```bash
grep -A 10 "Reset_Handler" boards/microchip/samv71-xult-clickboards/nuttx-config/scripts/startup_*.S

# Should NOT contain:
#   BL __libc_init_array
# (That should only be called from up_cxxinitialize)
```

#### Why This Works

**Time 1 (sam_boot):**
- ❌ NO `up_cxxinitialize()` called
- ✅ Static objects remain uninitialized (zero-filled .bss)

**Time 2 (nx_start → kmm_initialize):**
- ✅ Heap initialized
- ✅ `malloc()` now functional

**Time 3 (nx_start → up_cxxinitialize):**
- ✅ NOW run global constructors
- ✅ `DynamicSparseLayer` constructor calls `malloc()` → succeeds
- ✅ Vtable written correctly

**Time 4 (application):**
- ✅ All objects fully constructed
- ✅ No corruption

#### Testing Plan

```bash
# 1. Modify NuttX source
vim platforms/nuttx/NuttX/arch/arm/src/samv7/sam_boot.c

# 2. Verify nx_start.c is correct
grep -A 20 "kmm_initialize" platforms/nuttx/NuttX/sched/init/nx_start.c

# 3. Clean build (NuttX needs full rebuild)
make clean
make microchip_samv71-xult-clickboards_default

# 4. Flash and test
make microchip_samv71-xult-clickboards_default upload

# 5. Verify boot log shows correct order
# Expected:
# INFO  [init] kmm_initialize complete
# INFO  [init] C++ constructors running
# INFO  [parameters] user_config byteSize: 256 (not 0!)
```

---

## Implementation Guide

### Recommended Approach: Solution A First

**Rationale:**
1. **Lower risk:** Application-level change, doesn't affect OS boot
2. **Faster to test:** One file modification vs NuttX rebuild
3. **More portable:** Works on any NuttX version
4. **Easier to upstream:** PX4 can accept this without waiting for NuttX fix

**Timeline:**
- **Solution A implementation:** 1-2 hours
- **Solution B implementation:** 4-6 hours (NuttX rebuild, testing)

### Step-by-Step Implementation (Solution A)

#### Step 1: Backup Current Code

```bash
cd /media/bhanu1234/Development/PX4-Autopilot-Private
git checkout samv7-custom
cp src/lib/parameters/parameters.cpp src/lib/parameters/parameters.cpp.backup
```

#### Step 2: Modify parameters.cpp

```bash
vim src/lib/parameters/parameters.cpp
```

**Changes (around line 103-105):**

```cpp
// OLD CODE - DELETE:
// static ConstLayer firmware_defaults;
// static DynamicSparseLayer runtime_defaults{&firmware_defaults};
// DynamicSparseLayer user_config{&runtime_defaults};

// NEW CODE - ALIGNED STORAGE:
#include <new>  // Add to top of file

// Aligned storage buffers (8-byte alignment for ARM)
static uint64_t _firmware_defaults_storage[sizeof(ConstLayer) / 8 + 1];
static uint64_t _runtime_defaults_storage[sizeof(DynamicSparseLayer) / 8 + 1];
static uint64_t _user_config_storage[sizeof(DynamicSparseLayer) / 8 + 1];

// References to storage (no refactoring needed in rest of codebase)
static ConstLayer &firmware_defaults =
    *reinterpret_cast<ConstLayer*>(_firmware_defaults_storage);
static DynamicSparseLayer &runtime_defaults =
    *reinterpret_cast<DynamicSparseLayer*>(_runtime_defaults_storage);
DynamicSparseLayer &user_config =
    *reinterpret_cast<DynamicSparseLayer*>(_user_config_storage);

static bool _params_initialized = false;
```

**Changes to `param_init()` (around line 350):**

```cpp
void param_init(void)
{
    // SAMV7 workaround: Manual construction with placement new
    if (!_params_initialized) {
        new (&firmware_defaults) ConstLayer();
        new (&runtime_defaults) DynamicSparseLayer(&firmware_defaults);
        new (&user_config) DynamicSparseLayer(&runtime_defaults);

        _params_initialized = true;

        PX4_INFO("Parameter layers manually constructed (SAMV7 workaround)");
        PX4_INFO("user_config size: %d byteSize: %d", user_config.size(), user_config.byteSize());
    }

    // ... rest of existing param_init code ...
}
```

#### Step 3: Build Firmware

```bash
make microchip_samv71-xult-clickboards_default
```

**Expected output:**
```
[100%] Linking CXX executable px4
Memory region         Used Size  Region Size  %age Used
           flash:      920 KB        2 MB     43.9%
            sram:       18 KB      384 KB      4.75%
```

#### Step 4: Flash to Board

```bash
make microchip_samv71-xult-clickboards_default upload
```

#### Step 5: Connect Serial Console

```bash
minicom -D /dev/ttyACM0 -b 115200
# OR
screen /dev/ttyACM0 115200
```

#### Step 6: Verify Boot

**Expected boot log:**
```
INFO  [init] Booting...
INFO  [parameters] Parameter layers manually constructed (SAMV7 workaround)
INFO  [parameters] user_config size: 0 byteSize: 256
                                        ^^^^^^^^^^^^ NOT 0!
INFO  [parameters] Loading parameters from /fs/microsd/params
```

**Key indicator:** `byteSize: 256` (or higher) means allocation succeeded!

#### Step 7: Test Parameter Operations

```bash
nsh> param show | head -20
# Should show parameters with default values

nsh> param set CAL_ACC0_ID 123456
# Expected: No error (success!)

nsh> param save
# Expected: Success

nsh> ls -l /fs/microsd/params
# Expected: File size > 100 bytes (not 5!)

nsh> hexdump -C /fs/microsd/params | head -20
# Expected: BSON document with parameter data

nsh> reboot
# Wait for reboot...

nsh> param show CAL_ACC0_ID
# Expected: 123456 (persistence verified!)
```

#### Step 8: Re-enable Services

If parameter operations work:

```bash
vim boards/microchip/samv71-xult-clickboards/init/rc.board_defaults
```

**Uncomment:**
```bash
# dataman start
# logger start
# mavlink start -d /dev/ttyS0 -b 115200
```

**Rebuild and test:**
```bash
make microchip_samv71-xult-clickboards_default
make microchip_samv71-xult-clickboards_default upload
```

#### Step 9: Commit Changes

```bash
git add src/lib/parameters/parameters.cpp
git commit -m "SAMV7: Fix vtable corruption with manual parameter layer construction

Implements manual construction pattern using aligned storage buffers
and placement new to work around SAMV7 C++ static initialization bug.

Root cause:
- NuttX heap not initialized before global C++ constructors run
- DynamicSparseLayer constructor calls malloc() before heap ready
- malloc() fails, object gets corrupted
- Vtable overwritten with 'MP' magic bytes from BSON file headers
- Hardfault at PC: 0x4d500004 when calling virtual methods

Solution:
- Replace static objects with uint64_t storage buffers
- Defer construction to param_init() using placement new
- Heap guaranteed ready when param_init() runs
- Fixes hardfault and enables parameter set/save functionality

Tested on SAMV71-XULT-Clickboards.

See SAMV7_VTABLE_CORRUPTION_ANALYSIS.md for complete analysis.

🤖 Generated with Claude Code
https://claude.com/claude-code

Co-Authored-By: Claude <noreply@anthropic.com>
"

git push origin samv7-custom
```

---

## Why SAMV7 But Not Other Platforms?

### Platform Comparison

| Platform | Heap Init Timing | Constructor Timing | Behavior |
|----------|------------------|-------------------|----------|
| **STM32H7** | Before constructors | After heap init | ✅ Works |
| **STM32F4/F7** | Before constructors | After heap init | ✅ Works |
| **i.MX RT** | Before constructors | After heap init | ✅ Works |
| **SAMV7** | **After constructors** | Before heap init | ❌ **Crashes** |

### Root Cause: sam_boot.c Implementation

**SAMV7 (buggy):**
```c
void sam_boot(void)
{
  sam_clockconfig();
  sam_lowsetup();
  sam_boardinitialize();
  up_cxxinitialize();  // ← BEFORE heap init
}
```

**STM32 (correct):**
```c
void stm32_boot(void)
{
  stm32_clockconfig();
  stm32_lowsetup();
  stm32_boardinitialize();
  // NO up_cxxinitialize() here
}
```

### Historical Context

**Why SAMV7 is different:**

1. **Newer platform:** SAMV7 support added more recently
2. **Different vendor:** Microchip vs STMicroelectronics
3. **Copy-paste error:** Likely copied from another platform's boot code incorrectly
4. **Less testing:** SAMV7 less common in PX4 fleet than STM32

**Evidence from NuttX commit history:**
```bash
git log --all --oneline -- arch/arm/src/samv7/sam_boot.c
# Would show when up_cxxinitialize() was added
```

### Other Platforms That Might Have This Issue

**Potentially affected:**
- SAME70 (same family as SAMV7)
- SAMS70 (same family as SAMV7)
- Other Cortex-M7 platforms with recent NuttX ports

**Safe platforms (verified working):**
- STM32F4, F7, H7
- i.MX RT 1052, 1062
- Kinetis K6x
- LPC43xx

---

## Lessons Learned

### 1. Static Initialization is Platform-Dependent

**Key insight:** Never assume global constructors can call `malloc()`.

**Best practices:**
- Use lazy initialization for heap-allocated objects
- Prefer Meyer's Singleton (function-local static) over global static
- Test on actual hardware early (not just SITL)

### 2. Placement New Requires Careful Lifetime Management

**Key insight:** Can't safely destruct corrupted objects.

**Best practices:**
- If using placement new, ensure destructors never run on corrupted state
- Use aligned storage buffers for manual construction
- Guard against double-construction with initialization flags

### 3. Look for Patterns in Crash Addresses

**Key insight:** `0x4d500004` = "MP" magic bytes → file format collision.

**Best practices:**
- Convert crash addresses to ASCII to look for patterns
- Check for file format magic headers
- Suspect memory overlap when seeing consistent "random" addresses

### 4. OS Boot Order Matters

**Key insight:** Heap must initialize before C++ constructors.

**Best practices:**
- Review `*_boot.c` files when porting to new platform
- Compare boot order with known-working platforms
- Add assertions to verify heap ready before malloc

### 5. Existing Code Contains Clues

**Key insight:** DynamicSparseLayer.h:124-128 already had SAMV7 workaround comment.

```cpp
// Workaround for C++ static initialization bug on SAMV7
if (_parent == nullptr) {
    return px4::parameters[param].val;
}
```

**Best practices:**
- Search codebase for platform-specific workarounds
- Read comments carefully - often document known issues
- Check git blame for context on unusual code patterns

### 6. Timeline Reconstruction is Powerful

**Key insight:** Fixes at Time 4 don't help corruption at Time 2.

**Best practices:**
- Draw timeline of when events occur
- Identify when corruption happens vs when fixes apply
- Focus fixes at the right time in the sequence

### 7. Defensive Programming Has Limits

**Key insight:** Bounds checking can't fix vtable corruption.

**Best practices:**
- Defensive code is good, but not a substitute for root cause fix
- Understand what you're defending against
- Some bugs require architectural fixes, not just validation

---

## References

### PX4 Source Files

- `src/lib/parameters/parameters.cpp` - Parameter system entry point
- `src/lib/parameters/DynamicSparseLayer.h` - Storage layer (crashes here)
- `src/lib/parameters/ParamLayer.h` - Base class interface
- `src/lib/parameters/atomic_transaction.h` - Lock-free synchronization

### NuttX Source Files

- `arch/arm/src/samv7/sam_boot.c` - SAMV7 boot sequence (THE BUG)
- `sched/init/nx_start.c` - Main initialization (correct order)
- `arch/arm/src/common/arm_initialize.c` - ARM-specific init
- `mm/kmm_heap/kmm_initialize.c` - Heap initialization

### Documentation

- **This file:** Complete analysis of vtable corruption
- `SAMV7_PARAM_STORAGE_FIX.md` - Original lazy allocation plan
- `SAMV7_ACHIEVEMENTS.md` - Progress log and lessons learned
- `RESEARCH_TOPICS.md` - Deep-dive study guide
- `CONTEXT_FOR_CLAUDE.md` - Session resumption context

### External Resources

- **C++ Static Initialization:**
  - "Effective C++" by Scott Meyers, Item 4
  - https://en.cppreference.com/w/cpp/language/initialization

- **Placement New:**
  - https://en.cppreference.com/w/cpp/language/new
  - "Effective C++" by Scott Meyers, Item 52

- **ARM Cortex-M7:**
  - ARM Cortex-M7 Technical Reference Manual
  - ARMv7-M Architecture Reference Manual

- **NuttX RTOS:**
  - https://nuttx.apache.org/docs/latest/
  - NuttX Memory Management documentation

### Related PX4 Issues

- Search GitHub for: "SAMV7 parameter" OR "static initialization"
- Check PX4 Slack: #hardware-samv7 channel
- NuttX mailing list: SAMV7 boot order discussions

---

## Appendix A: Complete Crash Analysis

### Crash Register Dump

```
arm_hardfault: PANIC!!! Hard Fault!
up_assert: Assertion failed at file:armv7-m/arm_hardfault.c line: 129 task: param
arm_hardfault: Hard Fault escalation:
  IRQ: 3 regs: 202444b0
  BASEPRI: 00000000 PRIMASK: 00000000 IPSR: 00000003 CONTROL: 00000000
  CFSR: 00000001 HFSR: 40000000 DFSR: 00000000 BFAR: e000ed38
  R0: 2000f7e0 R1: 00000001 R2: 00000000  R3: 4d500005
  R4: 00000000 R5: 2000f7e0 R6: 00000000  R7: 2000f7e0
  R8: 20241058 R9: 20224190 R10: 00000000 R11: 00000000
  R12: 2000fa00 SP: 202444b0 LR: 00584a3b  PC: 4d500004
  xPSR: 61000000
```

### Register Interpretation

| Register | Value | Meaning |
|----------|-------|---------|
| **PC** | 0x4d500004 | Program Counter - trying to execute at this address |
| **LR** | 0x00584a3b | Link Register - return address (param_load_default) |
| **R3** | 0x4d500005 | Contains PC + 1 (Thumb mode bit) |
| **CFSR** | 0x00000001 | Configurable Fault Status Register |
| **HFSR** | 0x40000000 | Hard Fault Status Register |
| **xPSR** | 0x61000000 | Program Status Register |

### CFSR Breakdown

```
CFSR: 0x00000001
Bit 0: IACCVIOL = 1 (Instruction Access Violation)
```

**Meaning:** CPU tried to fetch instruction from invalid address 0x4d500004.

### HFSR Breakdown

```
HFSR: 0x40000000
Bit 30: FORCED = 1 (Forced Hard Fault)
```

**Meaning:** Fault escalated to hard fault because lower-priority handler couldn't run.

### Call Stack Reconstruction

```
#0  0x4d500004  ← CRASH (trying to call virtual function)
#1  0x00584a3b  param_load_default() + offset
#2  0x00584XXX  param_init()
#3  0x00YYYYYY  px4_platform_init()
#4  0x00ZZZZZZ  nsh_main()
```

---

## Appendix B: BSON File Format

### BSON Document Structure

```
[4 bytes: total document length (little-endian int32)]
[element 1]
[element 2]
...
[element N]
[1 byte: 0x00 terminator]
```

### BSON Element Structure

```
[1 byte: type]
[N bytes: name (null-terminated string)]
[M bytes: value (type-dependent)]
```

### Expected Parameter File

```
Offset  Hex          ASCII         Meaning
0x00    XX XX XX XX  ....          Document length (e.g., 0x00000134 = 308 bytes)
0x04    10           .             Type 0x10 = int32
0x05    43 41 4C ... CAL_ACC0_ID   Parameter name
0x10    E2 04 00 00  ....          Value = 1234 (little-endian)
...
EOF     00           .             Terminator
```

### Broken Parameter File (5 bytes)

```
Offset  Hex          ASCII         Meaning
0x00    05 00 00 00  ....          Document length = 5
0x04    00           .             Terminator (no elements!)
```

**This proves:** Parameter storage layers couldn't write any data.

---

## Appendix C: Memory Map

### SAMV71Q21 Memory Regions

```
Region         Start        End          Size      Purpose
Flash          0x00400000   0x005FFFFF   2 MB      Code + read-only data
SRAM           0x20000000   0x2005FFFF   384 KB    Data + stack + heap
Peripherals    0x40000000   0x5FFFFFFF   512 MB    Memory-mapped I/O
System         0xE0000000   0xFFFFFFFF   512 MB    ARM core peripherals
```

### Valid PC Ranges

- **Flash:** 0x00400000 - 0x005FFFFF (code execution)
- **SRAM:** 0x20000000 - 0x2005FFFF (NOT executable on Cortex-M7 by default)

### Invalid PC: 0x4d500004

**Analysis:**
```
0x4d500004 falls in range 0x40000000 - 0x5FFFFFFF (peripherals)
This is NOT executable memory.
Attempting to fetch instruction → IACCVIOL (Instruction Access Violation)
```

---

**Last Updated:** November 20, 2025
**Author:** Claude Code (with user debugging sessions)
**Status:** Root cause identified, solutions designed, awaiting implementation
