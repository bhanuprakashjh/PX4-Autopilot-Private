# SAMV71 PX4 Port - Todo List

**Status:** Pre-HIL Testing Phase
**Priority:** 🔴 CRITICAL | 🟠 HIGH | 🟡 MEDIUM | 🟢 LOW

---

## Phase 1: Critical HRT Fixes (MUST DO FIRST)

### 🔴 1. Fix HRT time units conversion (CRITICAL)
**Status:** ⏳ Pending
**File:** `platforms/nuttx/src/px4/microchip/samv7/hrt/hrt.c`
**Issue:** Returns timer ticks instead of microseconds
**Impact:** ALL timing wrong by 4.7x factor
**See:** PRE_HIL_TODO.md Task 1.1 for code

### 🔴 2. Enable HRT overflow interrupt for wraparound handling
**Status:** ⏳ Pending
**File:** `platforms/nuttx/src/px4/microchip/samv7/hrt/hrt.c`
**Issue:** Counter wraps after 15.3 minutes
**Impact:** Time jumps backwards, system crash
**See:** PRE_HIL_TODO.md Task 1.2 for code

### 🟠 3. Enable HRT callback interrupts (RA compare)
**Status:** ⏳ Pending
**File:** `platforms/nuttx/src/px4/microchip/samv7/hrt/hrt.c`
**Issue:** Uses polling instead of hardware interrupts
**Impact:** High latency (50-500µs vs 2-5µs)
**See:** PRE_HIL_TODO.md Task 1.3 for code

---

## Phase 2: Hardware Configuration

### 🔴 4. Configure SPI buses for Click sensor boards
**Status:** ⏳ Pending
**File:** `boards/microchip/samv71-xult-clickboards/src/spi.cpp`
**Issue:** Pin mappings not configured
**Impact:** ICM-20689 IMU cannot be used
**See:** PRE_HIL_TODO.md Task 2.1 for code

### 🟡 5. Test ICM-20689 IMU sensor via SPI with DMA
**Status:** ⏳ Pending (blocked by Task 4)
**Depends on:** SPI configuration
**Test:** Connect sensor, start driver, verify data

### 🟡 6. Test AK09916 magnetometer via I2C
**Status:** ⏳ Pending
**Hardware:** Connect to I2C bus 0 (address 0x0C)
**Test:** `ak09916 start -I -b 0`

### 🟡 7. Test DPS310 barometer via I2C
**Status:** ⏳ Pending
**Hardware:** Connect to I2C bus 0 (address 0x77)
**Test:** `dps310 start -I -b 0`

### 🟠 8. Configure PWM outputs using Timer/Counter channels
**Status:** ⏳ Pending
**File:** `boards/microchip/samv71-xult-clickboards/src/timer_config.cpp`
**Issue:** Timer channels not mapped to GPIO pins
**Impact:** Motor outputs won't work in HIL
**See:** PRE_HIL_TODO.md Task 2.3 for code

---

## Phase 3: Integration Testing

### 🔴 9. Verify EKF2 sensor fusion timing with fixed HRT
**Status:** ⏳ Pending (blocked by Tasks 1-5)
**Depends on:** HRT fixes + sensor data
**Test:** Check dt values, innovation test ratios

### 🟠 10. Test control loop rates (attitude, position, rate)
**Status:** ⏳ Pending (blocked by Tasks 1-3)
**Depends on:** HRT fixes
**Test:** Verify 500Hz, 250Hz, 50Hz rates

### 🟡 11. Verify MAVLink timestamps and telemetry
**Status:** ⏳ Pending (blocked by Tasks 1-3)
**Depends on:** HRT fixes
**Test:** Connect QGC, check timestamps

### 🟡 12. Test logger timestamps and ULog file integrity
**Status:** ⏳ Pending (blocked by Tasks 1-3)
**Depends on:** HRT fixes
**Test:** Download .ulg, analyze with Flight Review

### 🟡 13. Configure UART ports for GPS and RC input
**Status:** ⏳ Pending
**Ports:** UART2 (GPS), UART4 (RC)
**Test:** GPS and RC driver startup

### 🟢 14. Add HRT latency tracking and diagnostics
**Status:** ⏳ Pending
**File:** `platforms/nuttx/src/px4/microchip/samv7/hrt/hrt.c`
**Purpose:** Performance monitoring

### 🟡 15. Verify DMA pool usage under load
**Status:** ⏳ Pending
**Test:** Monitor peak DMA allocation
**Command:** Check `board_get_dma_usage()`

### 🟡 16. Test cache coherency with multiple DMA peripherals
**Status:** ⏳ Pending
**Test:** Run all DMA peripherals simultaneously for 30 min
**Monitor:** Data corruption, DMA errors

---

## Phase 4: HIL Preparation

### 🟡 17. Configure HIL simulation parameters for SAMV71
**Status:** ⏳ Pending
**Config:** SYS_AUTOSTART=1001, SYS_HITL=1
**See:** PRE_HIL_TODO.md Task 4.1

### 🟡 18. Set up Gazebo SITL with SAMV71 firmware
**Status:** ⏳ Pending (blocked by Tasks 1-10)
**Setup:** Gazebo + MAVLink connection
**See:** PRE_HIL_TODO.md Task 4.2

### 🟡 19. Test basic flight modes in HIL (stabilize, altitude hold)
**Status:** ⏳ Pending (blocked by Task 18)
**Test:** Manual, stabilize, altitude hold, position hold
**See:** PRE_HIL_TODO.md Task 4.3

---

## Phase 5: Documentation

### 🟢 20. Document all fixes and test results
**Status:** ⏳ Pending
**Files:** HRT_ANALYSIS.md, PORTING_REFERENCE.md
**Update:** Add "Fixes Applied" sections

---

## Summary Statistics

**Total Tasks:** 20
**Completed:** 0
**In Progress:** 0
**Pending:** 20

**By Priority:**
- 🔴 CRITICAL: 4 tasks
- 🟠 HIGH: 3 tasks
- 🟡 MEDIUM: 11 tasks
- 🟢 LOW: 2 tasks

**By Phase:**
- Phase 1 (HRT Fixes): 3 tasks
- Phase 2 (Hardware): 5 tasks
- Phase 3 (Integration): 8 tasks
- Phase 4 (HIL Prep): 3 tasks
- Phase 5 (Documentation): 1 task

---

## Critical Path to HIL

**Minimum tasks to start HIL testing:**

1. ✅ Task 1: Fix HRT time units
2. ✅ Task 2: Enable HRT overflow interrupt
3. ✅ Task 3: Enable HRT callback interrupts
4. ✅ Task 4: Configure SPI buses
5. ✅ Task 5: Test IMU sensor
6. ✅ Task 9: Verify EKF2 timing
7. ✅ Task 10: Test control loop rates

**Estimated Time:** 10-14 hours

---

## Quick Commands

**Check Task Status:**
```bash
# View this file
cat TODO_LIST.md

# View detailed procedures
cat PRE_HIL_TODO.md
```

**Start Working on HRT Fixes:**
```bash
# Edit HRT implementation
nano platforms/nuttx/src/px4/microchip/samv7/hrt/hrt.c

# Build and test
make microchip_samv71-xult-clickboards_default
make microchip_samv71-xult-clickboards_default upload
```

---

**Last Updated:** 2025-11-13
**Next Review:** After completing Phase 1 (HRT fixes)
