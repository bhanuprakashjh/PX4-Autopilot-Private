# SAMV7 Static Initialization Issues

## Summary

SAMV7 (and likely other ARM Cortex-M embedded targets) have issues with C++ static initialization of synchronization primitives. This affects:

1. `std::atomic` variables with static initializers
2. `pthread_mutex_t` with `PTHREAD_MUTEX_INITIALIZER`
3. `pthread_cond_t` with `PTHREAD_COND_INITIALIZER`

## Root Cause

On embedded ARM targets, C++ static initializers may not execute before code tries to use them. This is a known issue with the ARM toolchain and NuttX RTOS.

## Affected Components

### 1. WorkQueueManager (FIXED)

**File:** `platforms/common/px4_work_queue/WorkQueueManager.cpp`

**Problem:**
```cpp
static px4::atomic_bool _wq_manager_should_exit{true};
static px4::atomic_bool _wq_manager_running{false};
```

**Fix Applied:** Explicit initialization on first call:
```cpp
if (!_wq_manager_initialized.load()) {
    _wq_manager_should_exit.store(true);
    _wq_manager_running.store(false);
    _wq_manager_initialized.store(true);
}
```

### 2. BlockingList (NOT FIXED - Workaround in place)

**File:** `src/include/containers/BlockingList.hpp`

**Problem:**
```cpp
pthread_mutex_t _mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  _cv = PTHREAD_COND_INITIALIZER;
```

**Impact:** Crash when `MixingOutput::updateSubscriptions()` tries to switch work queues.

**Workaround:** Skip `updateSubscriptions()` call on SAMV7:
```cpp
// In pwm_out_sim Run():
#if !defined(CONFIG_ARCH_CHIP_SAMV7)
    _mixing_output.updateSubscriptions(true);
#endif
```

**Proper Fix (TODO):** Modify BlockingList to use explicit `pthread_mutex_init()` in constructor:
```cpp
BlockingList() {
    pthread_mutex_init(&_mutex, nullptr);
    pthread_cond_init(&_cv, nullptr);
}
```

### 3. Other Potentially Affected Code

- `SubscriptionBlocking.hpp` - uses `PTHREAD_MUTEX_INITIALIZER`
- Any code using static `std::atomic` initialization

## Symptoms

- Hard fault / crash when accessing mutex
- Crash during work queue switching
- Random crashes during module startup

## Testing

To verify if a crash is related to static initialization:
1. Add debug output before/after the suspected operation
2. Check if crash happens consistently at same point
3. Static init issues often manifest on first access

## References

- Similar issue in NuttX: Static constructors not always called
- ARM GCC embedded has known issues with `.init_array` section
- PX4 GitHub discussions on static initialization

## Future Work

1. Audit all uses of `PTHREAD_MUTEX_INITIALIZER` in PX4 codebase
2. Consider PX4-wide fix for BlockingList/BlockingQueue
3. Add SAMV7-specific initialization hooks if needed
