# SAMV7 Static Initialization Issue: Complete Explanation

**Date:** November 28, 2025
**Affects:** SAMV7 (ATSAMV71) and potentially other embedded ARM targets
**Status:** Understood and Fixed

---

## Executive Summary

C-style aggregate initializers like `PTHREAD_MUTEX_INITIALIZER` and `SEM_INITIALIZER` do not work
reliably when used as **C++ class member default initializers** on SAMV7/NuttX. The same code works
on STM32 by coincidence, not by specification. The fix is to use runtime initialization
(`pthread_mutex_init()`, `px4_sem_init()`) instead.

---

## The Problem

### What Doesn't Work

```cpp
class MyClass {
private:
    pthread_mutex_t _mutex = PTHREAD_MUTEX_INITIALIZER;  // FAILS on SAMV7
    px4_sem_t _lock = SEM_INITIALIZER(1);                // FAILS on SAMV7
};
```

### Symptoms

- System hangs or crashes when the mutex/semaphore is first used
- `pthread_mutex_lock()` called on garbage data
- Hard fault or infinite loop
- Works fine on STM32, fails on SAMV7

---

## Root Cause Analysis

### POSIX Specification

The POSIX standard states:

> "In cases where default mutex attributes are appropriate, the macro `PTHREAD_MUTEX_INITIALIZER`
> can be used to initialize mutexes that are **statically allocated**."

Key phrase: **"statically allocated"** - this means file-scope static variables, NOT C++ class members.

### What the Initializers Actually Are

```c
// NuttX pthread.h
#define PTHREAD_MUTEX_INITIALIZER {NULL, SEM_INITIALIZER(1), -1, flags, type, 0}

// NuttX semaphore.h
#define SEM_INITIALIZER(c) {(c), 0, NULL}
```

These are **C aggregate initializers** - struct literals that work by placing initial values
directly into the `.data` section at compile time.

### Why It Works for File-Scope Static Variables

```c
// This works on ALL platforms:
static pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;
```

1. Compiler sees static variable with aggregate initializer
2. Initial values placed in `.data` section at compile time
3. During startup, `.data` is copied from flash to RAM
4. Variable is ready to use before any code runs

### Why It Fails for C++ Class Members

```cpp
class Foo {
    pthread_mutex_t _mutex = PTHREAD_MUTEX_INITIALIZER;  // Default member initializer
};

Foo* obj = new Foo();  // Constructed at runtime
```

1. This is a **default member initializer** - a C++11 feature
2. The initialization happens when the object is **constructed at runtime**
3. The compiler must generate code to copy the struct values at construction time
4. This is NOT the same as placing values in `.data` at compile time

### Why STM32 Works But SAMV7 Doesn't

| Aspect | STM32 | SAMV7 |
|--------|-------|-------|
| NuttX startup sequence | Identical | Identical |
| pthread configuration | Same | Same |
| Compiler (arm-none-eabi-gcc) | Same | Same |
| **Result** | Works | Crashes |

The difference is in **implementation details** we haven't fully identified. Possible factors:

1. **Memory layout/alignment** - Different struct packing or cache behavior
2. **Semaphore internals** - How the semaphore holder list is managed
3. **Timing** - When exactly the struct is accessed vs initialized
4. **Compiler code generation** - Subtle differences in how the copy is performed

**Important:** The "success" on STM32 is **coincidental and not guaranteed by any specification**.
The code is technically incorrect according to POSIX on both platforms.

---

## The Correct Fix

### For C++ Class Members

```cpp
// BEFORE (incorrect, non-portable):
class MyClass {
    pthread_mutex_t _mutex = PTHREAD_MUTEX_INITIALIZER;
    px4_sem_t _lock = SEM_INITIALIZER(1);
};

// AFTER (correct, POSIX-compliant):
class MyClass {
    pthread_mutex_t _mutex{};
    px4_sem_t _lock{};

public:
    MyClass() {
        pthread_mutex_init(&_mutex, nullptr);
        px4_sem_init(&_lock, 0, 1);
    }

    ~MyClass() {
        pthread_mutex_destroy(&_mutex);
        px4_sem_destroy(&_lock);
    }
};
```

### For File-Scope Static Variables (pthread_once pattern)

```cpp
// BEFORE (broken on SAMV7):
static pthread_mutex_t my_mutex = PTHREAD_MUTEX_INITIALIZER;

// AFTER (works on all platforms):
#if defined(CONFIG_ARCH_CHIP_SAMV7)
static pthread_mutex_t my_mutex{};
static pthread_once_t my_mutex_once = PTHREAD_ONCE_INIT;

static void my_mutex_init() {
    if (pthread_mutex_init(&my_mutex, nullptr) != 0) {
        abort();
    }
}

static inline void my_mutex_lock() {
    pthread_once(&my_mutex_once, my_mutex_init);
    pthread_mutex_lock(&my_mutex);
}

static inline void my_mutex_unlock() {
    pthread_mutex_unlock(&my_mutex);
}
#else
static pthread_mutex_t my_mutex = PTHREAD_MUTEX_INITIALIZER;

static inline void my_mutex_lock() {
    pthread_mutex_lock(&my_mutex);
}

static inline void my_mutex_unlock() {
    pthread_mutex_unlock(&my_mutex);
}
#endif
```

---

## Files Fixed in PX4

### Core Containers

| File | Component | Fix Type |
|------|-----------|----------|
| `src/include/containers/BlockingList.hpp` | BlockingList | Constructor init |

### Platform Common

| File | Component | Fix Type |
|------|-----------|----------|
| `platforms/common/uORB/SubscriptionBlocking.hpp` | SubscriptionBlocking | Constructor init |
| `platforms/common/uORB/uORBManager.cpp` | uORBManager | pthread_once |
| `platforms/common/module.cpp` | px4_modules_mutex | pthread_once |
| `platforms/common/shutdown.cpp` | shutdown_mutex | pthread_once |
| `platforms/common/i2c_spi_buses.cpp` | i2c_spi instances | pthread_once |
| `platforms/nuttx/src/px4/common/console_buffer.cpp` | ConsoleBuffer | Constructor init |

### Libraries

| File | Component | Fix Type |
|------|-----------|----------|
| `src/lib/perf/perf_counter.cpp` | perf_counters_mutex | pthread_once |
| `src/lib/parameters/parameters.cpp` | file_mutex | pthread_once |
| `src/lib/events/events.cpp` | publish_event_mutex | pthread_once |

### Modules

| File | Component | Fix Type |
|------|-----------|----------|
| `src/modules/mavlink/mavlink_main.cpp` | mavlink mutexes | pthread_once |
| `src/modules/ekf2/EKF2.cpp` | ekf2_module_mutex | pthread_once |

---

## Why pthread_once?

For file-scope static variables, we use `pthread_once` instead of a simple boolean flag because:

1. **Thread safety** - `pthread_once` guarantees the init function runs exactly once, even if
   multiple threads call it simultaneously
2. **Memory barriers** - Proper synchronization ensures all threads see the initialized state
3. **Standard compliance** - `PTHREAD_ONCE_INIT` IS designed for static initialization

```cpp
static pthread_once_t once = PTHREAD_ONCE_INIT;  // This DOES work statically
```

---

## Testing Verification

After applying fixes, test on SAMV7:

```bash
# Boot should complete without hangs
nsh>

# Console buffer working
nsh> dmesg

# Work queues operational
nsh> work_queue status

# Parameters working
nsh> param show

# EKF2 starts without crash
nsh> ekf2 status

# Mavlink operational
nsh> mavlink status

# PWM sim (HITL) working
nsh> pwm_out_sim start -m hil
```

---

## Platform-Specific Notes

### Platforms NOT Affected

These platforms don't compile the NuttX code paths:

- `platforms/posix/*` - Uses native POSIX threads
- `src/lib/drivers/device/qurt/*` - Qualcomm QURT
- `src/modules/muorb/*` - SLPI/DSP builds

### Other ARM Targets

This issue may affect other embedded ARM targets running NuttX. If porting PX4 to a new ARM MCU,
audit for `PTHREAD_MUTEX_INITIALIZER` and `SEM_INITIALIZER` usage in C++ class members.

---

## Global Static C++ Objects - CRITICAL WARNING

**DO NOT call `px4_sem_init()`, `pthread_mutex_init()`, or any NuttX kernel functions
in constructors of global static C++ objects!**

### Why It Fails

```cpp
class MyClass {
public:
    MyClass() {
        px4_sem_init(&_sem, 0, 1);  // CRASH! Kernel not ready yet!
    }
private:
    px4_sem_t _sem{};
};

static MyClass g_instance;  // Constructor runs BEFORE NuttX kernel starts!
```

**Boot sequence:**
1. `_stext` → Reset handler
2. `.data` copy, `.bss` zero
3. `lib_cxx_initialize()` → **C++ static constructors run HERE**
4. `nx_start()` → NuttX kernel initialization
5. `os_start()` → Scheduler starts

Global static C++ constructors run at step 3, but NuttX kernel isn't ready until step 4.
Any call to `px4_sem_init()`, `pthread_mutex_init()`, etc. will crash.

### Console Buffer Example (Nov 28, 2025)

`console_buffer.cpp` has `static ConsoleBuffer g_console_buffer;`. Adding a constructor
with `px4_sem_init()` caused complete boot failure (no LED blink).

**Solution:** Console buffer remains DISABLED for SAMV7. To enable it would require
lazy initialization (init on first use, not in constructor).

### Safe Patterns for Global Static Objects

1. **No constructor** - Use default member initializers only (but NOT SEM_INITIALIZER on SAMV7)
2. **Lazy init** - Initialize in a function called after kernel starts
3. **Explicit init function** - Call from `px4_platform_init()` or later

---

## Lessons Learned

1. **POSIX initializer macros are for C static variables only** - Don't use them as C++ member
   default initializers

2. **"Works on one platform" doesn't mean correct** - STM32 success was coincidental

3. **Runtime initialization is always safe** - `pthread_mutex_init()` and `px4_sem_init()` work
   everywhere, BUT only after kernel is running

4. **Global static C++ objects cannot use runtime init** - Their constructors run before kernel

5. **Guard with `#if defined(CONFIG_ARCH_CHIP_SAMV7)`** - Keeps changes isolated to affected
   platform, easy to rollback if issues found

6. **Test on target hardware** - This issue was only discoverable on actual SAMV7 hardware

---

## References

- POSIX.1-2017 pthread_mutex specification
- NuttX `include/pthread.h` - PTHREAD_MUTEX_INITIALIZER definition
- NuttX `include/semaphore.h` - SEM_INITIALIZER definition
- `PTHREAD_MUTEX_AUDIT.md` - Full audit of affected files
- `BLOCKINGLIST_FIX_REFERENCE.md` - BlockingList fix details

---

*Document created: November 28, 2025*
*Based on debugging session identifying root cause of SAMV7 mutex crashes*
