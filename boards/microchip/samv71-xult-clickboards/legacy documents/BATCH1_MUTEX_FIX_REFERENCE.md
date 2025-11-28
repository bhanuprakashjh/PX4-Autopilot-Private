# Batch 1 PTHREAD_MUTEX_INITIALIZER Fix Reference

**Status:** ✅ IMPLEMENTED | ⚠️ NOT TESTED
**Date:** November 28, 2025
**Scope:** SubscriptionBlocking, perf_counter, parameters

---

## Summary

Fixed three high-priority files that use `PTHREAD_MUTEX_INITIALIZER` static initialization,
which fails on SAMV7. All fixes are **SAMV7-specific** using `#if defined(CONFIG_ARCH_CHIP_SAMV7)`
guards, making them easy to revert by removing the guarded blocks.

---

## Design Approach

Two patterns were used:

1. **Class member pattern** (SubscriptionBlocking): Add SAMV7-guarded constructor with
   `pthread_mutex_init`/`pthread_cond_init`, switch members to value-init `{}`

2. **File-scope static pattern** (perf_counter, parameters): Use `pthread_once` to ensure
   exactly-once initialization, with wrapper functions that call `pthread_once` before lock

---

## File 1: SubscriptionBlocking.hpp

**Location:** `platforms/common/uORB/SubscriptionBlocking.hpp:57-161`

**Problem:** Per-subscription mutex/condvar used `PTHREAD_MUTEX_INITIALIZER` as class member.

**Fix:** SAMV7-guarded constructor with explicit init.

```cpp
// Constructor (lines 57-73):
SubscriptionBlocking(const orb_metadata *meta, uint32_t interval_us = 0, uint8_t instance = 0) :
    SubscriptionCallback(meta, interval_us, instance)
{
#if defined(CONFIG_ARCH_CHIP_SAMV7)
    int ret = pthread_mutex_init(&_mutex, nullptr);
    if (ret != 0) {
        abort();
    }
    ret = pthread_cond_init(&_cv, nullptr);
    if (ret != 0) {
        abort();
    }
#endif
}

// Member declarations (lines 153-161):
private:
#if defined(CONFIG_ARCH_CHIP_SAMV7)
    pthread_mutex_t _mutex{};
    pthread_cond_t  _cv{};
#else
    pthread_mutex_t _mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t  _cv = PTHREAD_COND_INITIALIZER;
#endif
```

**Destructor:** Already existed, calls `pthread_mutex_destroy`/`pthread_cond_destroy`.

---

## File 2: perf_counter.cpp

**Location:** `src/lib/perf/perf_counter.cpp:102-135`

**Problem:** `perf_counters_mutex` guards global perf counter list, used very early during boot.

**Fix:** `pthread_once` guarded initialization with wrapper functions.

```cpp
#if defined(CONFIG_ARCH_CHIP_SAMV7)
static pthread_mutex_t perf_counters_mutex{};
static pthread_once_t perf_counters_mutex_once = PTHREAD_ONCE_INIT;

static void perf_counters_mutex_init()
{
    if (pthread_mutex_init(&perf_counters_mutex, nullptr) != 0) {
        abort();
    }
}

static inline void perf_counters_mutex_lock()
{
    pthread_once(&perf_counters_mutex_once, perf_counters_mutex_init);
    pthread_mutex_lock(&perf_counters_mutex);
}

static inline void perf_counters_mutex_unlock()
{
    pthread_mutex_unlock(&perf_counters_mutex);
}
#else
pthread_mutex_t perf_counters_mutex = PTHREAD_MUTEX_INITIALIZER;

static inline void perf_counters_mutex_lock()
{
    pthread_mutex_lock(&perf_counters_mutex);
}

static inline void perf_counters_mutex_unlock()
{
    pthread_mutex_unlock(&perf_counters_mutex);
}
#endif
```

**Call sites updated:** All `pthread_mutex_lock(&perf_counters_mutex)` replaced with
`perf_counters_mutex_lock()`, same for unlock.

---

## File 3: parameters.cpp

**Location:** `src/lib/parameters/parameters.cpp:126-171`

**Problem:** `file_mutex` protects parameter save/flash operations, used during autosave.

**Fix:** `pthread_once` guarded initialization with wrapper functions.

```cpp
#if defined(CONFIG_ARCH_CHIP_SAMV7)
static pthread_mutex_t file_mutex{};
static pthread_once_t file_mutex_once = PTHREAD_ONCE_INIT;

static void file_mutex_init()
{
    if (pthread_mutex_init(&file_mutex, nullptr) != 0) {
        abort();
    }
}

static inline void file_mutex_lock()
{
    pthread_once(&file_mutex_once, file_mutex_init);
    pthread_mutex_lock(&file_mutex);
}

static inline int file_mutex_trylock()
{
    pthread_once(&file_mutex_once, file_mutex_init);
    return pthread_mutex_trylock(&file_mutex);
}

static inline void file_mutex_unlock()
{
    pthread_mutex_unlock(&file_mutex);
}
#else
static pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;

static inline void file_mutex_lock()
{
    pthread_mutex_lock(&file_mutex);
}

static inline int file_mutex_trylock()
{
    return pthread_mutex_trylock(&file_mutex);
}

static inline void file_mutex_unlock()
{
    pthread_mutex_unlock(&file_mutex);
}
#endif
```

**Call sites updated:**
- Line 850: `file_mutex_lock()`
- Line 853: `file_mutex_trylock()`
- Line 1092: `file_mutex_trylock()`
- Line 1111: `file_mutex_unlock()`

---

## Files Changed Summary

| File | Lines | Change |
|------|-------|--------|
| `platforms/common/uORB/SubscriptionBlocking.hpp` | 57-161 | SAMV7-guarded ctor + value-init members |
| `src/lib/perf/perf_counter.cpp` | 102-135 | `pthread_once` + wrapper functions |
| `src/lib/parameters/parameters.cpp` | 126-171, 850, 853, 1092, 1111 | `pthread_once` + wrapper functions |

---

## Testing Required

**Status: NOT TESTED** - Hardware/runner unavailable at time of implementation.

### Recommended Test Plan

1. **Boot Test**
   ```
   - Flash firmware to SAMV71-XULT
   - Verify clean boot without crashes
   - Check console for abort() messages
   ```

2. **Perf Counter Test**
   ```
   nsh> perf
   # Should list all perf counters without crash
   ```

3. **Parameter Operations Test**
   ```
   nsh> param show
   nsh> param set SYS_AUTOSTART 4001
   nsh> param save
   nsh> reboot
   nsh> param show SYS_AUTOSTART
   # Should persist value
   ```

4. **SubscriptionBlocking Test**
   ```
   # Any module using SubscriptionBlocking (e.g., some sensor drivers)
   # Should not crash on startup
   ```

5. **Combined HITL Test**
   ```
   nsh> pwm_out_sim start -m hil
   # Connect QGroundControl
   # Verify full operation
   ```

---

## Rollback Instructions

To revert, simply remove the `#if defined(CONFIG_ARCH_CHIP_SAMV7)` blocks:

### SubscriptionBlocking.hpp
- Remove lines 60-72 (constructor init)
- Remove lines 155-157 and 161 (keep only the `PTHREAD_MUTEX_INITIALIZER` path)

### perf_counter.cpp
- Remove lines 102-122 (SAMV7 path)
- Keep only lines 123-134 (standard path)
- Revert wrapper function calls to direct `pthread_mutex_lock/unlock`

### parameters.cpp
- Remove lines 126-152 (SAMV7 path)
- Keep only lines 153-171 (standard path)
- Revert wrapper function calls to direct mutex operations

---

## Related Fixes

| Fix | Status |
|-----|--------|
| BlockingList | ✅ IMPLEMENTED (NOT TESTED) |
| WorkQueueManager atomic_bool | ✅ FIXED |
| PWMSim workaround removed | ✅ IMPLEMENTED (NOT TESTED) |
| Batch 2 (events, module, etc.) | 🔴 TODO |

---

## References

- `PTHREAD_MUTEX_AUDIT.md` - Full audit of all affected files
- `BLOCKINGLIST_FIX_REFERENCE.md` - BlockingList fix details
- `SAMV7_STATIC_INIT_ISSUES.md` - Overview of static init issues

---

*Document created: November 28, 2025*
*Implementation by: Claude Code assisted development*
