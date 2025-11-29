# SAMV71 Implementation Tracker

**Document Purpose:** Track implementation progress, bugs, and fixes for remaining SAMV71 features
**Created:** November 28, 2025
**Status:** ACTIVE - In Progress
**Related:** `SAMV71_MASTER_PORTING_GUIDE.md`

---

## Current State Summary

| Component | Status | Last Updated |
|-----------|--------|--------------|
| Full Boot | ✅ Working | Nov 28, 2025 |
| MAVLink | ✅ Working | Nov 28, 2025 |
| Logger | ❌ Disabled (SD write hang) | Nov 28, 2025 |
| Click Board Drivers | ✅ Configured (7 boards) | Nov 28, 2025 |
| SPI DMA | ✅ Enabled | Nov 28, 2025 |
| Boot Warnings | ⚠️ Expected (documented) | Nov 28, 2025 |
| PWM/DShot Output | ❌ Not implemented | Nov 28, 2025 |
| Console Buffer | ❌ Disabled (static init) | Nov 28, 2025 |
| External QSPI Flash | Not started (SD card works) | Nov 28, 2025 |

### Engineering Task Assignments

| Task Document | Engineer | Priority | Status |
|---------------|----------|----------|--------|
| `TASK_LOGGER_DEBUG_ENABLE.md` | TBD | HIGH | Ready |
| `TASK_CLICK_BOARD_SENSOR_TESTING.md` | TBD | HIGH | Ready |
| `TASK_HITL_TESTING.md` | TBD | MEDIUM | Ready |
| `TASK_CONSOLE_BUFFER_DEBUG.md` | TBD | LOW | Ready |
| `TASK_DSHOT_PWM_IMPLEMENTATION.md` | TBD | HIGH | Ready |

### Configured Click Board Drivers

| MIKROE PID | Board Name | Sensor | Interface | Driver Status |
|------------|------------|--------|-----------|---------------|
| MIKROE-4044 | 6DOF IMU 6 Click | ICM-20689 | SPI | ✅ Enabled |
| MIKROE-6514 | 6DOF IMU 27 Click | ICM-45686 | SPI | Commented |
| MIKROE-4231 | Compass 4 Click | AK09915/16 | I2C | ✅ Enabled |
| MIKROE-2293 | Pressure 3 Click | DPS310 | I2C | ✅ Enabled |
| MIKROE-3566 | Pressure 5 Click | BMP388 | I2C | Commented |
| MIKROE-2935 | GeoMagnetic Click | BMM150 | I2C | Commented |
| MIKROE-3775 | 13DOF Click | BMI088+BMM150 | I2C | Commented |

**Note:** "Commented" drivers are compiled but not started by default. Uncomment in `rc.board_sensors` when hardware is connected.

---

## Phase 1: Enable Full Boot

### Status: IN PROGRESS

### 1.1 Remove Safe Mode Exit

**File:** `ROMFS/px4fmu_common/init.d/rcS`

**Finding (Nov 28, 2025):** No explicit `exit 0` safe mode line exists. The rcS file is complete.
However, several features are commented out:
- `dataman` (lines 277-287) - waypoint storage disabled
- `navigator` (line 543-544) - navigation disabled
- `payload_deliverer` (line 587) - disabled

These remain disabled as they depend on dataman which needs SD card.

**Implementation Steps:**
- [x] Locate `exit 0` line in rcS - **NOT FOUND** (file is complete)
- [x] Verify rcS runs full boot sequence

**Progress Log:**
| Date | Action | Result | Notes |
|------|--------|--------|-------|
| Nov 28, 2025 | Reviewed rcS | No exit 0 found | File already runs full sequence |

---

### 1.2 Re-enable MAVLink

**File:** `boards/microchip/samv71-xult-clickboards/init/rc.board_defaults`

**Finding (Nov 28, 2025):** MAVLink was ALREADY enabled!
```bash
param set-default MAV_0_CONFIG 101   # Already set
param set-default MAV_0_RATE 57600   # Already set
```

**rc.board_mavlink Status:** Already correct (just has echo statement)

**Implementation Steps:**
- [x] Verify MAVLink configuration - **ALREADY ENABLED**
- [ ] Test with QGroundControl

**Test Criteria:**
- [ ] QGroundControl connects via USB
- [ ] Telemetry data received
- [ ] Parameters sync properly
- [ ] No connection drops

**Progress Log:**
| Date | Action | Result | Notes |
|------|--------|--------|-------|
| Nov 28, 2025 | Verified config | Already enabled | MAV_0_CONFIG=101 present |

---

### 1.3 Re-enable Logger

**File:** `boards/microchip/samv71-xult-clickboards/init/rc.board_defaults`

**Change Made (Nov 28, 2025):**
```bash
# Before:
param set-default SDLOG_MODE -1    # Logger disabled

# After:
param set-default SDLOG_MODE 0     # Log when armed
```

**Note:** Logger is controlled via rc.logging sourced from rcS (lines 617-622).
SDLOG_MODE values:
- -1: Disabled (was set for debugging)
- 0: Log when armed (now enabled)
- 1: Log from boot (alternative)

**Implementation Steps:**
- [x] Update SDLOG_MODE in rc.board_defaults (-1 → 0)
- [ ] Rebuild firmware
- [ ] Test logging functionality

**Test Criteria:**
- [ ] Logger starts without errors
- [ ] Log files created in `/fs/microsd/log/`
- [ ] No SD card errors during logging
- [ ] System remains stable under continuous logging

**Progress Log:**
| Date | Action | Result | Notes |
|------|--------|--------|-------|
| Nov 28, 2025 | Changed SDLOG_MODE | -1 → 0 | Logger enabled when armed |
| Nov 28, 2025 | Build completed | ✅ SUCCESS | Flash 60.66%, SRAM 6.73% |
| Nov 28, 2025 | First boot test | ⚠️ Logger blocked | Custom rc.logging was overriding |
| Nov 28, 2025 | Fixed rc.logging | Replaced with standard | Now uses SDLOG_MODE/SDLOG_BACKEND |
| Nov 28, 2025 | Logger test | ❌ HANG | SD card hangs on continuous writes |
| Nov 28, 2025 | Reverted SDLOG_MODE | -1 (disabled) | Logger disabled for stability |
| Nov 28, 2025 | SD card formatted | ✅ Works | param save works, logger writes fail |
| Nov 28, 2025 | **Phase 1 Status** | ⚠️ PARTIAL | Boot works, logger disabled |

### Known Issue: Logger/SD Card Continuous Write Hang

**Status:** UNRESOLVED - Documented for future investigation

**Symptom:**
- `logger on` command causes system hang
- Small writes work (`param save` succeeds)
- Continuous/streaming writes hang

**Technical Details:**
- SD card DMA is only enabled for **READS**, not writes
- Writes use PIO (programmed I/O) mode
- The hang occurs during sustained PIO writes, not DMA
- Logger creates continuous streaming writes which triggers the issue

**Workaround:**
- Keep `SDLOG_MODE=-1` (logger disabled)
- Logging not available on SAMV7 until write path is fixed

**Potential Root Causes to Investigate:**
1. PIO write timeout handling in HSMCI driver
2. Buffer management during sustained writes
3. FAT filesystem sync/flush blocking
4. Interrupt handling during continuous writes

**Files to investigate:**
- `platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/sam_hsmci.c`
- Logger write path in `src/modules/logger/`

---

## Known Boot Warnings (Expected)

The following "command not found" warnings appear during boot and are **expected behavior**:

### Source: rcS (Generic Startup Script)
| Command | Purpose | Why Missing |
|---------|---------|-------------|
| `tone_alarm` | Buzzer driver | No buzzer hardware on SAMV71-XULT |
| `rgbled*` | RGB LED drivers | No status LED hardware |
| `px4io` | PX4IO coprocessor | Not used on this board |
| `dshot` | DShot motor output | Motor output not implemented (Phase 3) |
| `pwm_out` | PWM motor output | Motor output not implemented (Phase 3) |

### Source: rc.sensors (Generic Sensor Probing)
| Command | Purpose | Why Missing |
|---------|---------|-------------|
| `icm20948_i2c_passthrough` | ICM-20948 passthrough | Not using this sensor |
| `hmc5883`, `lis3mdl`, etc. | Generic magnetometer probing | Disabled via `SENS_EXT_I2C_PRB=0` |

**Note:** These warnings are harmless. The system continues booting normally.
- Generic commands try to start drivers not compiled for this board
- Motor output warnings will be resolved when Phase 3 (PWM) is implemented
- External I2C probing is disabled since we use specific Click board drivers in `rc.board_sensors`

---

## Phase 2: Click Board Sensor Drivers

### Status: ✅ COMPLETE (Drivers Configured)

**Note:** All drivers are compiled and configured. Hardware testing pending Click board availability.

### 2.1 Configuration Files Modified

**Files Updated (Nov 28, 2025):**

1. **`default.px4board`** - Added all sensor driver configs:
   ```cmake
   # SPI IMU Sensors (Click boards on SPI0)
   CONFIG_DRIVERS_IMU_INVENSENSE_ICM20689=y
   CONFIG_DRIVERS_IMU_INVENSENSE_ICM45686=y

   # I2C Sensors (Click boards on I2C0/TWIHS0)
   CONFIG_DRIVERS_MAGNETOMETER_AKM_AK09916=y
   CONFIG_DRIVERS_MAGNETOMETER_BOSCH_BMM150=y
   CONFIG_DRIVERS_BAROMETER_DPS310=y
   CONFIG_DRIVERS_BAROMETER_BMP388=y
   CONFIG_DRIVERS_IMU_BOSCH_BMI088_I2C=y
   ```

2. **`nuttx-config/nsh/defconfig`** - Added SPI DMA:
   ```
   CONFIG_SAMV7_SPI0=y
   CONFIG_SAMV7_SPI1=y
   CONFIG_SAMV7_SPI_DMA=y
   CONFIG_SAMV7_SPI_DMATHRESHOLD=4
   ```

3. **`init/rc.board_sensors`** - Complete Click board sensor startup:
   ```bash
   # SPI IMU Sensors (on SPI0)
   icm20689 -s -b 0 start          # Enabled
   # icm45686 -s -b 0 start        # Commented (alternate)

   # I2C Sensors (on I2C0 / TWIHS0)
   ak09916 -I -b 0 start           # Enabled
   dps310 -I -b 0 start            # Enabled
   # bmp388 -I -b 0 start          # Commented
   # bmm150 -I -b 0 start          # Commented
   # bmi088_i2c -b 0 start         # Commented
   ```

4. **`init/rc.board_defaults`** - Disabled external I2C probing:
   ```bash
   param set-default SENS_EXT_I2C_PRB 0
   ```

### 2.2 SPI DMA and Cache Coherency

**D-Cache Configuration (Already Correct):**
- `CONFIG_ARMV7M_DCACHE=y` - Data cache enabled
- `CONFIG_ARMV7M_DCACHE_WRITETHROUGH=y` - Write-through mode (safe for DMA)

**NuttX SPI Driver:** Has RX cache invalidation (`up_invalidate_dcache()`) at line 1761 in `sam_spi.c`

### 2.3 Hardware Testing (Pending)

**Test Commands (when Click boards connected):**
```bash
nsh> icm20689 status
nsh> ak09916 status
nsh> dps310 status
nsh> sensor status
nsh> listener sensor_accel
nsh> listener sensor_gyro
nsh> listener sensor_mag
nsh> listener sensor_baro
```

**Test Criteria:**
- [ ] Sensors detected on their respective buses
- [ ] Data publishing at expected rates
- [ ] No bus errors
- [ ] EKF2 starts with sensor data

**Progress Log:**
| Date | Action | Result | Notes |
|------|--------|--------|-------|
| Nov 28, 2025 | Added all driver configs | ✅ Build success | 7 Click boards supported |
| Nov 28, 2025 | Enabled SPI DMA | ✅ Configured | D-Cache write-through safe |
| Nov 28, 2025 | Updated rc.board_sensors | ✅ Complete | ICM, AK09916, DPS310 enabled |
| Nov 28, 2025 | Disabled SENS_EXT_I2C_PRB | ✅ Done | Eliminates generic mag probing |

---

## Phase 3: PWM Motor Output

### Status: NOT STARTED

### 3.1 Research SAMV71 Timer Pinout

**Task:** Identify which TC channels are routed to accessible headers

**SAMV71-XULT Available Timers:**
| Timer | Channels | HRT Reserved | Available |
|-------|----------|--------------|-----------|
| TC0 | CH0, CH1, CH2 | Yes (HRT) | No |
| TC1 | CH0, CH1, CH2 | No | Yes |
| TC2 | CH0, CH1, CH2 | No | Yes |
| TC3 | CH0, CH1, CH2 | No | Yes |

**mikroBUS PWM Pins:**
- Socket 1 PWM: PC19
- Socket 2 PWM: PB1

**Arduino Header PWM Pins:**
- Need to check schematic for available PWM-capable pins

**Research Steps:**
- [ ] Download SAMV71-XULT schematic
- [ ] Identify TC output pins routed to headers
- [ ] Map TC channels to physical pins
- [ ] Document pin assignments

**Pin Mapping Table (to be filled):**
| PWM Channel | Timer | TC Channel | GPIO Pin | Header Location |
|-------------|-------|------------|----------|-----------------|
| CH1 | TC1 | CH0 | | |
| CH2 | TC1 | CH1 | | |
| CH3 | TC1 | CH2 | | |
| CH4 | TC2 | CH0 | | |
| CH5 | TC2 | CH1 | | |
| CH6 | TC2 | CH2 | | |

**Progress Log:**
| Date | Action | Result | Notes |
|------|--------|--------|-------|
| | | | |

---

### 3.2 Implement timer_config.cpp

**File:** `boards/microchip/samv71-xult-clickboards/src/timer_config.cpp`

**Current Code (empty):**
```cpp
constexpr io_timers_t io_timers[MAX_IO_TIMERS] = {
    // EMPTY - No timers configured
};

constexpr timer_io_channels_t timer_io_channels[MAX_TIMER_IO_CHANNELS] = {
    // EMPTY - No channels configured
};
```

**Required Implementation (template - pins TBD):**
```cpp
#include <px4_arch/io_timer_hw_description.h>

constexpr io_timers_t io_timers[MAX_IO_TIMERS] = {
    initIOTimer(Timer::TC1),  // PWM channels 1-3
    initIOTimer(Timer::TC2),  // PWM channels 4-6
};

constexpr timer_io_channels_t timer_io_channels[MAX_TIMER_IO_CHANNELS] = {
    // Channel 1: TC1 CH0
    initIOTimerChannel(io_timers, {Timer::TC1, Timer::Channel0},
                       {GPIO::PortX, GPIO::PinY}),  // TBD from schematic
    // Channel 2: TC1 CH1
    initIOTimerChannel(io_timers, {Timer::TC1, Timer::Channel1},
                       {GPIO::PortX, GPIO::PinY}),  // TBD from schematic
    // ... add more channels
};

constexpr io_timers_channel_mapping_t io_timers_channel_mapping =
    initIOTimerChannelMapping(io_timers, timer_io_channels);
```

**Implementation Steps:**
- [ ] Complete pin mapping research (3.1)
- [ ] Implement timer_config.cpp with correct pins
- [ ] Update board_config.h with PWM GPIO definitions
- [ ] Verify io_timer arch layer supports TC1/TC2/TC3

**Progress Log:**
| Date | Action | Result | Notes |
|------|--------|--------|-------|
| | | | |

---

### 3.3 Enable PWM_OUT Driver

**File:** `boards/microchip/samv71-xult-clickboards/default.px4board`

**Add Configuration:**
```cmake
CONFIG_DRIVERS_PWM_OUT=y
```

**Also verify these are set:**
```cmake
CONFIG_BOARD_IO_TIMER=y
```

**Implementation Steps:**
- [ ] Add CONFIG_DRIVERS_PWM_OUT=y
- [ ] Verify timer dependencies
- [ ] Rebuild firmware
- [ ] Test PWM output

**Progress Log:**
| Date | Action | Result | Notes |
|------|--------|--------|-------|
| | | | |

---

### 3.4 Implement board_on_reset()

**File:** `boards/microchip/samv71-xult-clickboards/src/init.c`

**Current Code (stub):**
```c
__EXPORT void board_on_reset(int status)
{
    /* No PWM channels configured yet for SAMV71 - TODO */
    (void)status;
}
```

**Required Implementation:**
```c
__EXPORT void board_on_reset(int status)
{
    /* Set all PWM outputs to GPIO mode (low) to prevent motor spin */
    for (int i = 0; i < DIRECT_PWM_OUTPUT_CHANNELS; ++i) {
        px4_arch_configgpio(io_timer_channel_get_gpio_output(i));
    }

    /* On system reset (not boot), wait for ESCs to disarm */
    if (status >= 0) {
        up_mdelay(100);
    }
}
```

**Implementation Steps:**
- [ ] Implement board_on_reset() after timer_config.cpp is done
- [ ] Test that PWM pins go LOW on reset
- [ ] Verify ESC safety (motors don't spin unexpectedly)

**Progress Log:**
| Date | Action | Result | Notes |
|------|--------|--------|-------|
| | | | |

---

### 3.5 Test PWM Output

**Test Commands:**
```bash
# Start PWM driver
nsh> pwm_out start

# Check PWM info
nsh> pwm info

# Test individual channels (NO PROPS!)
nsh> pwm test -c 1 -p 1000   # Channel 1 at 1000µs (minimum)
nsh> pwm test -c 1 -p 1500   # Channel 1 at 1500µs (center)
nsh> pwm test -c 1 -p 2000   # Channel 1 at 2000µs (maximum)

# Test all channels
nsh> pwm test -a -p 1200

# Stop test
nsh> pwm test -c 1 -p 0
```

**Test Criteria:**
- [ ] `pwm info` shows configured channels
- [ ] PWM signals measurable with oscilloscope
- [ ] Correct frequency (typically 400Hz for motors)
- [ ] Pulse width matches commanded value
- [ ] No glitches or unexpected signals

**Progress Log:**
| Date | Action | Result | Notes |
|------|--------|--------|-------|
| | | | |

---

## Phase 4: Static Initialization Fixes

### Status: PARTIALLY COMPLETE

### 4.1 Console Buffer Fix

**File:** `platforms/nuttx/src/px4/common/console_buffer.cpp`

**Current Status:** DISABLED via `BOARD_ENABLE_CONSOLE_BUFFER` in board_config.h

**Root Cause:** Global static `ConsoleBuffer g_console_buffer;` constructor runs before NuttX kernel starts. `px4_sem_init()` cannot be called that early.

**Option A: Lazy Initialization (Preferred)**
```cpp
class ConsoleBuffer {
public:
    ConsoleBuffer() {
        // NO initialization in constructor!
    }

    void write(const char* buffer, size_t len) {
        ensure_initialized();
        // ... existing code
    }

private:
    std::atomic<bool> _initialized{false};
    px4_sem_t _lock;

    void ensure_initialized() {
        bool expected = false;
        if (_initialized.compare_exchange_strong(expected, true)) {
            px4_sem_init(&_lock, 0, 1);
        }
    }
};
```

**Option B: Keep Disabled (Current Workaround)**
- Acceptable for testing
- dmesg command won't work

**Implementation Steps:**
- [ ] Decide on approach (A or B)
- [ ] If A: Implement lazy init pattern
- [ ] If A: Re-enable BOARD_ENABLE_CONSOLE_BUFFER
- [ ] Test dmesg command

**Progress Log:**
| Date | Action | Result | Notes |
|------|--------|--------|-------|
| Nov 28 | Disabled feature | Working | Workaround in place |
| | | | |

---

### 4.2 Work Queue BlockingList

**File:** `src/include/containers/BlockingList.hpp`

**Current Status:** Fixed with constructor init, but workaround still in PWMSim

**Remaining Issue:** `updateSubscriptions()` causes re-entrancy crash on SAMV7

**Workaround in Place:**
```cpp
// PWMSim.cpp
#if !defined(CONFIG_ARCH_CHIP_SAMV7)
    _mixing_output.updateSubscriptions(true);
#endif
```

**Impact:** PWM sim stays on default work queue instead of switching to `wq:rate_ctrl`

**Future Fix:** Investigate scheduler re-entrancy on SAMV7

**Progress Log:**
| Date | Action | Result | Notes |
|------|--------|--------|-------|
| Nov 28 | Workaround applied | Working | Stays on default WQ |
| | | | |

---

## Phase 5: External SPI Flash for Parameters

### Status: NOT STARTED

**Note:** Internal flash progmem has dual-bank issues (EEFC0 vs EEFC1). Instead of fixing internal flash, we will use the **external SPI flash** on the SAMV71-XULT board for parameter storage.

### SAMV71-XULT External Flash
- **Device:** SST26VF064B (or similar)
- **Interface:** QSPI (Quad SPI)
- **Capacity:** 8MB (64 Mbit)
- **Location:** On-board, directly connected to QSPI pins

### Implementation Options

**Option A: QSPI Flash + LittleFS**
- Use NuttX QSPI driver
- Mount LittleFS filesystem on QSPI flash
- Store parameters in `/fs/qspiflash/params`

**Option B: QSPI Flash + MTD**
- Use MTD (Memory Technology Device) layer
- Direct block access for parameter storage

### Files to Investigate
- `platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/sam_qspi.c`
- `platforms/nuttx/NuttX/nuttx/drivers/mtd/`
- Board config for QSPI pin definitions

### Implementation Steps
- [ ] Enable QSPI in defconfig (`CONFIG_SAMV7_QSPI=y`)
- [ ] Configure QSPI flash chip (SST26 driver)
- [ ] Mount filesystem on QSPI flash
- [ ] Update `CONFIG_BOARD_PARAM_FILE` path
- [ ] Test parameter save/load from QSPI flash

### Current Workaround
Using SD card for parameter storage (working fine)

**Progress Log:**
| Date | Action | Result | Notes |
|------|--------|--------|-------|
| | | | |

---

## Phase 6: Production Hardening

### Status: NOT STARTED

### 6.1 ADC Battery Monitoring

**Files to Modify:**
- `boards/microchip/samv71-xult-clickboards/src/board_config.h`
- `boards/microchip/samv71-xult-clickboards/default.px4board`

**Current Status:** Placeholder only

**Implementation Steps:**
- [ ] Identify AFEC channels for voltage/current sensing
- [ ] Configure ADC pins in board_config.h
- [ ] Enable `CONFIG_DRIVERS_ADC_BOARD_ADC`
- [ ] Test battery voltage reading

**Progress Log:**
| Date | Action | Result | Notes |
|------|--------|--------|-------|
| | | | |

---

### 6.2 Safety Switch

**Implementation Steps:**
- [ ] Identify GPIO for safety switch input
- [ ] Identify GPIO for safety LED output
- [ ] Configure in board_config.h
- [ ] Enable safety_button module
- [ ] Test arming/disarming

**Progress Log:**
| Date | Action | Result | Notes |
|------|--------|--------|-------|
| | | | |

---

## Bugs Discovered During Implementation

### Bug Tracker

| Bug # | Date | Phase | Description | Status | Fix |
|-------|------|-------|-------------|--------|-----|
| | | | | | |

### Bug Details

*(Add detailed bug reports here as they are discovered)*

---

## Build & Test Log

### Build History

| Date | Commit | Build Result | Flash Size | Notes |
|------|--------|--------------|------------|-------|
| | | | | |

### Test Sessions

| Date | Phase | Tests Run | Pass/Fail | Notes |
|------|-------|-----------|-----------|-------|
| | | | | |

---

## Notes & Observations

*(Add implementation notes, lessons learned, and observations here)*

---

## References

- `SAMV71_MASTER_PORTING_GUIDE.md` - Master consolidated reference
- `FMU6X_SAMV71_DETAILED_COMPARISON.md` - FMU-6X comparison for reference
- `SAMV71_REMAINING_ISSUES.md` - Complete list of remaining issues
- SAMV71-XULT Schematic (Microchip)
- SAMV71 Datasheet Section 36: Timer Counter (TC)

---

**Document Version:** 1.0
**Last Updated:** November 28, 2025
**Status:** Active Implementation Tracker
