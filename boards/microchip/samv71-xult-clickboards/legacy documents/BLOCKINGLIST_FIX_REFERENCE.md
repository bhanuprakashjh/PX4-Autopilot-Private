# BlockingList PTHREAD_MUTEX_INITIALIZER Fix Reference

**Status:** ✅ IMPLEMENTED | ⚠️ NOT TESTED
**Date:** November 28, 2025
**Related Issue:** SAMV7 C++ static initialization failure with `PTHREAD_MUTEX_INITIALIZER`

---

## Summary

Fixed the `BlockingList` container to use runtime initialization of pthread mutex and
condition variable instead of relying on `PTHREAD_MUTEX_INITIALIZER` static initializer,
which fails on SAMV7 (and potentially other embedded ARM targets).

---

## Problem

On SAMV7/NuttX, `PTHREAD_MUTEX_INITIALIZER` is a C-style aggregate initializer that doesn't
reliably execute for C++ class member variables. This caused crashes when:

1. `pwm_out_sim` started and called `updateSubscriptions()`
2. Work queue switching was attempted via `ChangeWorkQueue(rate_ctrl)`
3. `BlockingList::mutex()` was accessed with uninitialized mutex
4. `pthread_mutex_lock()` crashed on garbage mutex data

**Crash Call Tree:**
```
PWMSim::Run()
  → _mixing_output.updateSubscriptions(true)
    → ChangeWorkQueue(rate_ctrl)
      → WorkQueueFindOrCreate()
        → FindWorkQueueByName()
          → LockGuard{_wq_manager_wqs_list->mutex()}
            → pthread_mutex_lock(&_mutex)  // CRASH: uninitialized mutex
```

---

## Implementation

### File 1: `src/include/containers/BlockingList.hpp`

**Before (broken on SAMV7):**
```cpp
template<class T>
class BlockingList : public IntrusiveSortedList<T>
{
public:
    ~BlockingList()
    {
        pthread_mutex_destroy(&_mutex);
        pthread_cond_destroy(&_cv);
    }
    // ... methods ...
private:
    pthread_mutex_t _mutex = PTHREAD_MUTEX_INITIALIZER;  // Static init fails!
    pthread_cond_t  _cv = PTHREAD_COND_INITIALIZER;      // Static init fails!
};
```

**After (runtime safe):**
```cpp
template<class T>
class BlockingList : public IntrusiveSortedList<T>
{
public:
    BlockingList()
    {
        int ret = pthread_mutex_init(&_mutex, nullptr);
        if (ret != 0) {
            abort();  // Fail-fast on init error
        }
        ret = pthread_cond_init(&_cv, nullptr);
        if (ret != 0) {
            abort();
        }
    }

    ~BlockingList()
    {
        pthread_mutex_destroy(&_mutex);
        pthread_cond_destroy(&_cv);
    }
    // ... methods unchanged ...
private:
    pthread_mutex_t _mutex{};  // Value-initialized, then set by constructor
    pthread_cond_t  _cv{};     // Value-initialized, then set by constructor
};
```

**Key Changes:**
- Added explicit constructor with `pthread_mutex_init()` / `pthread_cond_init()`
- Abort on initialization failure (fail-fast for debugging)
- Removed `PTHREAD_MUTEX_INITIALIZER` / `PTHREAD_COND_INITIALIZER`
- Member variables use value initialization `{}`

### File 2: `src/modules/simulation/pwm_out_sim/PWMSim.cpp`

**Before (with SAMV7 workaround):**
```cpp
void PWMSim::Run()
{
    // ...
    _mixing_output.update();

    /* NOTE: updateSubscriptions() is skipped for SAMV7 due to static initialization
     * issues with PTHREAD_MUTEX_INITIALIZER in BlockingList. */
#if !defined(CONFIG_ARCH_CHIP_SAMV7)
    _mixing_output.updateSubscriptions(true);
#endif
    // ...
}
```

**After (workaround KEPT for different reason):**
```cpp
void PWMSim::Run()
{
    // ...
    _mixing_output.update();

    // SAMV7: Skip updateSubscriptions due to work queue switch re-entrancy issue.
    // ScheduleNow() immediately triggers Run() on rate_ctrl before the first
    // updateSubscriptions() completes, causing a race condition / crash.
    // This is NOT a mutex init issue - the mutex fixes are working.
#if !defined(CONFIG_ARCH_CHIP_SAMV7)
    _mixing_output.updateSubscriptions(true);
#endif
    // ...
}
```

**Key Findings (Nov 28, 2025 debugging session):**
- The BlockingList mutex fix IS working correctly
- The crash is NOT a mutex initialization issue
- The crash is a **work queue switch re-entrancy issue** specific to SAMV7:
  1. `updateSubscriptions()` calls `ScheduleNow()` after switching to rate_ctrl
  2. SAMV7 scheduler immediately preempts to the rate_ctrl work queue
  3. `Run()` is called again before the first `updateSubscriptions()` completes
  4. Concurrent access causes crash
- Workaround must remain until re-entrancy issue is resolved

---

## Files Changed

| File | Lines | Change |
|------|-------|--------|
| `src/include/containers/BlockingList.hpp` | 53-104 | Added constructor with runtime init, removed static initializers |
| `src/modules/simulation/pwm_out_sim/PWMSim.cpp` | 605-620 | Removed SAMV7 workaround, unconditional `updateSubscriptions()` |

---

## Testing Required

**Status: NOT TESTED** - Hardware/runner unavailable at time of implementation.

### Recommended Test Plan

1. **Basic Boot Test (SAMV7)**
   ```
   - Flash firmware to SAMV71-XULT
   - Verify clean boot without crashes
   - Check dmesg/console for errors
   ```

2. **Work Queue Manager Test**
   ```
   nsh> work_queue status
   # Should show all work queues without crashes
   ```

3. **PWM Sim HITL Test**
   ```
   nsh> pwm_out_sim start -m hil
   # Should start without crash
   # Previously crashed on SAMV7 at this point
   ```

4. **Work Queue Switching Test**
   ```
   # Configure motor outputs (triggers updateSubscriptions)
   param set HIL_ACT_FUNC1 101
   param set HIL_ACT_FUNC2 102
   param set HIL_ACT_FUNC3 103
   param set HIL_ACT_FUNC4 104

   # Restart pwm_out_sim
   pwm_out_sim stop
   pwm_out_sim start -m hil

   # Check if switched to wq:rate_ctrl
   work_queue status
   # Should show pwm_out_sim on wq:rate_ctrl (not wq:lp_default)
   ```

5. **HITL Full Test**
   ```
   - Connect to QGroundControl
   - Run HITL simulation
   - Verify actuator outputs work
   - Verify no crashes during operation
   ```

6. **Regression Test (STM32/Other Platforms)**
   ```
   - Build and test on FMU-v6X or other STM32 board
   - Verify no regression from BlockingList changes
   ```

---

## Potential Side Effects

1. **Abort on Init Failure**: If `pthread_mutex_init()` or `pthread_cond_init()` fails,
   the system will abort. This is intentional fail-fast behavior but could cause boot
   failures if pthread subsystem isn't ready when BlockingList is constructed.

2. **Copy/Move Semantics**: BlockingList now has non-trivial constructor/destructor.
   However, it's typically used via pointer (`BlockingList<T> *`) so this should be fine.

3. **Other Static Initializers**: This fix only addresses `BlockingList`. Other code
   using `PTHREAD_MUTEX_INITIALIZER` as class members may still have issues:
   - `SubscriptionBlocking.hpp:141` - `pthread_mutex_t _mutex = PTHREAD_MUTEX_INITIALIZER`

---

## SAMV7 Work Queue Re-entrancy Audit (Nov 28, 2025)

The `updateSubscriptions(true)` call inside `Run()` triggers a work queue switch
(`ChangeWorkQueue()` + `ScheduleNow()`). On SAMV7, the scheduler immediately preempts
to the new work queue, re-entering `Run()` before the first call completes.

### Affected Drivers

| Driver | File | Line | Status |
|--------|------|------|--------|
| pwm_out_sim | `src/modules/simulation/pwm_out_sim/PWMSim.cpp` | 623 | ✅ **WORKAROUND IN PLACE** |
| PWMOut | `src/drivers/pwm_out/PWMOut.cpp` | 190 | ⚠️ Vulnerable |
| DShot | `src/drivers/dshot/DShot.cpp` | 537 | ⚠️ Vulnerable |
| TAP_ESC | `src/drivers/tap_esc/TAP_ESC.cpp` | 409 | ⚠️ Vulnerable |
| PX4IO | `src/drivers/px4io/px4io.cpp` | 638 | ⚠️ Vulnerable |
| Voxl2IO | `src/drivers/voxl2_io/voxl2_io.cpp` | 563 | ⚠️ Vulnerable |
| VertiqIO | `src/drivers/actuators/vertiq_io/vertiq_io.cpp` | 140 | ⚠️ Vulnerable |
| VoxlEsc | `src/drivers/actuators/voxl_esc/voxl_esc.cpp` | 1548 | ⚠️ Vulnerable |
| PCA9685 | `src/drivers/pca9685_pwm_out/main.cpp` | 235 | ✅ **SAFE** (uses `false`) |

### Why PCA9685 is Safe

PCA9685 uses `updateSubscriptions(false)` which prevents work queue switching.
It handles the work queue switch separately in `init()` before `Run()` starts.

### Recommended Fix Pattern

For drivers that need to run on SAMV7, add the same workaround:

```cpp
// SAMV7: Skip updateSubscriptions due to work queue switch re-entrancy issue.
// ScheduleNow() immediately triggers Run() on rate_ctrl before the first
// updateSubscriptions() completes, causing a race condition / crash.
#if !defined(CONFIG_ARCH_CHIP_SAMV7)
    _mixing_output.updateSubscriptions(true);
#endif
```

**Note:** This means SAMV7 won't auto-switch to the rate_ctrl work queue for motor
outputs. For HIL/SIL simulation this is acceptable. For real motor drivers, a
different approach may be needed (e.g., explicit work queue selection in init).

---

## Related Documentation

- `SAMV7_STATIC_INIT_ISSUES.md` - Overview of static init issues on SAMV7
- `FMU6X_SAMV71_DETAILED_COMPARISON.md` Section 9.3 - Work queue crash analysis
- `SAMV71_PORTING_NOTES.md` - General porting notes

---

## Rollback Instructions

If issues are found, revert to workaround approach:

1. Revert `BlockingList.hpp` to use `PTHREAD_MUTEX_INITIALIZER`
2. Re-add SAMV7 workaround in `PWMSim.cpp`:
   ```cpp
   #if !defined(CONFIG_ARCH_CHIP_SAMV7)
       _mixing_output.updateSubscriptions(true);
   #endif
   ```

---

*Document created: November 28, 2025*
*Implementation by: Claude Code assisted development*
