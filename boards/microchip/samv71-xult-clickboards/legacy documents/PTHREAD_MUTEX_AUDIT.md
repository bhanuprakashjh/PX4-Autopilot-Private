# PTHREAD_MUTEX_INITIALIZER Audit for SAMV7

**Status:** Audit Complete | Fixes In Progress
**Date:** November 28, 2025
**Purpose:** Track all `PTHREAD_MUTEX_INITIALIZER` / `PTHREAD_COND_INITIALIZER` usages that need conversion to runtime initialization for SAMV7 compatibility.

---

## Background

On SAMV7/NuttX, `PTHREAD_MUTEX_INITIALIZER` is a C-style aggregate initializer that doesn't
reliably execute for C++ class member variables or file-scope static objects before they're
first used. This causes crashes when `pthread_mutex_lock()` is called on uninitialized mutex data.

**Fix Pattern:**
```cpp
// Before (broken on SAMV7):
pthread_mutex_t _mutex = PTHREAD_MUTEX_INITIALIZER;

// After (runtime safe):
pthread_mutex_t _mutex{};
// In constructor or init function:
pthread_mutex_init(&_mutex, nullptr);
```

---

## Fix Status Summary

| Batch | Status | Files | Priority |
|-------|--------|-------|----------|
| ✅ Already Fixed | DONE | BlockingList, WorkQueueManager | - |
| ✅ Batch 1 | DONE (NOT TESTED) | SubscriptionBlocking, perf_counter, parameters | HIGH |
| ✅ Batch 2 | DONE (NOT TESTED) | events, module, i2c_spi_buses, shutdown, mavlink, ekf2, uORBManager | MEDIUM |
| Batch 3 | 🔴 TODO | Platform-specific (if needed) | LOW |

---

## ✅ Already Fixed

| File | Component | Status |
|------|-----------|--------|
| `src/include/containers/BlockingList.hpp` | BlockingList container | ✅ FIXED (Nov 28, 2025) |
| `platforms/common/px4_work_queue/WorkQueueManager.cpp` | WorkQueue atomic_bool | ✅ FIXED |
| `src/modules/simulation/pwm_out_sim/PWMSim.cpp` | SAMV7 workaround removed | ✅ FIXED (Nov 28, 2025) |

---

## ✅ Batch 1: High Priority (IMPLEMENTED - NOT TESTED)

These are single-file fixes with high boot-time impact.
**Implementation Date:** November 28, 2025
**Test Status:** ⚠️ NOT TESTED (no hardware runner available)

### 1.1 SubscriptionBlocking.hpp ✅

| Item | Details |
|------|---------|
| **File** | `platforms/common/uORB/SubscriptionBlocking.hpp:57-161` |
| **What** | Per-subscription mutex/condvar guarding `updatedBlocking()` |
| **Fix Applied** | SAMV7-guarded constructor with `pthread_mutex_init`/`pthread_cond_init`, value-init members |
| **Status** | ✅ IMPLEMENTED |

```cpp
// SAMV7 path (lines 60-72):
#if defined(CONFIG_ARCH_CHIP_SAMV7)
    int ret = pthread_mutex_init(&_mutex, nullptr);
    if (ret != 0) { abort(); }
    ret = pthread_cond_init(&_cv, nullptr);
    if (ret != 0) { abort(); }
#endif

// Member declarations (lines 155-161):
#if defined(CONFIG_ARCH_CHIP_SAMV7)
    pthread_mutex_t _mutex{};
    pthread_cond_t  _cv{};
#else
    pthread_mutex_t _mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t  _cv = PTHREAD_COND_INITIALIZER;
#endif
```

### 1.2 perf_counter.cpp ✅

| Item | Details |
|------|---------|
| **File** | `src/lib/perf/perf_counter.cpp:102-135` |
| **What** | `perf_counters_mutex` guarding the global perf counter list |
| **Fix Applied** | `pthread_once` guarded init + wrapper functions `perf_counters_mutex_lock/unlock` |
| **Status** | ✅ IMPLEMENTED |

```cpp
// SAMV7 path (lines 102-122):
#if defined(CONFIG_ARCH_CHIP_SAMV7)
static pthread_mutex_t perf_counters_mutex{};
static pthread_once_t perf_counters_mutex_once = PTHREAD_ONCE_INIT;

static void perf_counters_mutex_init() {
    if (pthread_mutex_init(&perf_counters_mutex, nullptr) != 0) { abort(); }
}

static inline void perf_counters_mutex_lock() {
    pthread_once(&perf_counters_mutex_once, perf_counters_mutex_init);
    pthread_mutex_lock(&perf_counters_mutex);
}

static inline void perf_counters_mutex_unlock() {
    pthread_mutex_unlock(&perf_counters_mutex);
}
#else
// ... standard PTHREAD_MUTEX_INITIALIZER path
#endif
```

### 1.3 parameters.cpp ✅

| Item | Details |
|------|---------|
| **File** | `src/lib/parameters/parameters.cpp:126-171` |
| **What** | `file_mutex` around parameter save/flash operations |
| **Fix Applied** | `pthread_once` guarded init + wrapper functions `file_mutex_lock/trylock/unlock` |
| **Call Sites Updated** | Lines 850, 853, 1092, 1111 now use wrapper functions |
| **Status** | ✅ IMPLEMENTED |

```cpp
// SAMV7 path (lines 126-152):
#if defined(CONFIG_ARCH_CHIP_SAMV7)
static pthread_mutex_t file_mutex{};
static pthread_once_t file_mutex_once = PTHREAD_ONCE_INIT;

static void file_mutex_init() {
    if (pthread_mutex_init(&file_mutex, nullptr) != 0) { abort(); }
}

static inline void file_mutex_lock() {
    pthread_once(&file_mutex_once, file_mutex_init);
    pthread_mutex_lock(&file_mutex);
}

static inline int file_mutex_trylock() {
    pthread_once(&file_mutex_once, file_mutex_init);
    return pthread_mutex_trylock(&file_mutex);
}

static inline void file_mutex_unlock() {
    pthread_mutex_unlock(&file_mutex);
}
#else
// ... standard PTHREAD_MUTEX_INITIALIZER path
#endif
```

---

## ✅ Batch 2: Medium Priority (IMPLEMENTED - NOT TESTED)

Same pattern as Batch 1, more call sites.
**Implementation Date:** November 28, 2025

### 2.1 events.cpp ✅

| Item | Details |
|------|---------|
| **File** | `src/lib/events/events.cpp:42` |
| **What** | `publish_event_mutex` protecting `orb_event_pub` and sequence numbers |
| **Fix Applied** | `pthread_once` guarded init + wrapper functions |
| **Status** | ✅ IMPLEMENTED |

### 2.2 module.cpp ✅

| Item | Details |
|------|---------|
| **File** | `platforms/common/module.cpp:48` + `module.h:62-85` |
| **What** | `px4_modules_mutex` controlling module list/help output |
| **Fix Applied** | `pthread_once` guarded init + wrapper functions in header |
| **Status** | ✅ IMPLEMENTED |

### 2.3 i2c_spi_buses.cpp ✅

| Item | Details |
|------|---------|
| **File** | `platforms/common/i2c_spi_buses.cpp:51-84` |
| **What** | `i2c_spi_module_instances_mutex` guarding driver list |
| **Fix Applied** | `pthread_once` guarded init + wrapper functions |
| **Status** | ✅ IMPLEMENTED |

### 2.4 shutdown.cpp ✅

| Item | Details |
|------|---------|
| **File** | `platforms/common/shutdown.cpp:64-98` |
| **What** | `shutdown_mutex` guarding shutdown hooks/lock counter |
| **Fix Applied** | `pthread_once` guarded init + wrapper functions |
| **Status** | ✅ IMPLEMENTED |

### 2.5 mavlink_main.cpp ✅

| Item | Details |
|------|---------|
| **File** | `src/modules/mavlink/mavlink_main.cpp:84-121` |
| **What** | `mavlink_module_mutex`, `mavlink_event_buffer_mutex` |
| **Fix Applied** | `pthread_once` guarded init + getter functions returning mutex refs |
| **Status** | ✅ IMPLEMENTED |

### 2.6 EKF2.cpp ✅

| Item | Details |
|------|---------|
| **File** | `src/modules/ekf2/EKF2.cpp:47-59` + `EKF2.hpp:128-151` |
| **What** | `ekf2_module_mutex` guarding module start/stop |
| **Fix Applied** | `pthread_once` guarded init + wrapper functions in header |
| **Status** | ✅ IMPLEMENTED |

### 2.7 uORBManager.cpp ✅

| Item | Details |
|------|---------|
| **File** | `platforms/common/uORB/uORBManager.cpp:53-76` + `uORBManager.hpp:498-501` |
| **What** | `_communicator_mutex` (only when `CONFIG_ORB_COMMUNICATOR` is on) |
| **Fix Applied** | `pthread_once` guarded init + getter function |
| **Status** | ✅ IMPLEMENTED |

---

## ⚪ Batch 3: Low Priority / Platform-Specific

These don't run on SAMV7 or are optional configs.

| File | Notes | Status |
|------|-------|--------|
| `platforms/posix/src/px4/common/tasks.cpp:65` | POSIX-only; not compiled for NuttX | N/A |
| `platforms/posix/src/px4/common/lockstep_scheduler/...` | POSIX-only | N/A |
| `platforms/posix/src/px4/common/px4_daemon/server.cpp` | POSIX-only | N/A |
| `src/lib/drivers/device/qurt/*` | QURT (Qualcomm) builds only | N/A |
| `src/modules/muorb/*` | SLPI/Apps builds; not SAMV7 | N/A |
| `src/lib/cdev/posix/cdev_platform.cpp` | POSIX-only | N/A |
| `platforms/nuttx/NuttX/apps/...` | Third-party test/demo code; not shipped | N/A |

---

## Safe (No Action Needed)

These use runtime initialization or wrapper classes internally:

- Anything using `px4::device::Device::lock`
- Anything using `px4::Mutex` wrapper classes
- `BlockingList` (fixed)
- `WorkQueueManager` atomic_bool (fixed)

---

## Fix Template

For file-scope static mutexes:

```cpp
// Before:
static pthread_mutex_t my_mutex = PTHREAD_MUTEX_INITIALIZER;

// After:
static pthread_mutex_t my_mutex{};
static bool my_mutex_initialized = false;

static void init_my_mutex_once()
{
    if (!my_mutex_initialized) {
        int ret = pthread_mutex_init(&my_mutex, nullptr);
        if (ret != 0) {
            PX4_ERR("mutex init failed: %d", ret);
            return;
        }
        my_mutex_initialized = true;
    }
}

// Call init_my_mutex_once() at start of functions that use the mutex
```

For class members (like SubscriptionBlocking):

```cpp
// Before:
class Foo {
    pthread_mutex_t _mutex = PTHREAD_MUTEX_INITIALIZER;
};

// After:
class Foo {
    pthread_mutex_t _mutex{};
public:
    Foo() {
        int ret = pthread_mutex_init(&_mutex, nullptr);
        if (ret != 0) { abort(); }
    }
    ~Foo() {
        pthread_mutex_destroy(&_mutex);
    }
};
```

---

## Testing Notes

After each batch, test on SAMV7:

1. Clean boot without crashes
2. `work_queue status` - verify work queues
3. `pwm_out_sim start -m hil` - verify no crash
4. `param save` / `param load` - verify parameter operations
5. MAVLink connection - verify communication
6. Sensor startup - verify I2C/SPI drivers

---

## References

- `BLOCKINGLIST_FIX_REFERENCE.md` - BlockingList fix details
- `SAMV7_STATIC_INIT_ISSUES.md` - Overview of static init issues
- `FMU6X_SAMV71_DETAILED_COMPARISON.md` Section 9 - Work queue analysis

---

*Audit completed: November 28, 2025*
*Last updated: November 28, 2025*
*Batch 1 implemented: November 28, 2025 (NOT TESTED)*
