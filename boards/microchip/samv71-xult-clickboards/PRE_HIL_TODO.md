# Pre-HIL Testing Todo List

**Tasks to Complete Before Gazebo HIL Testing**

This document outlines all critical fixes, hardware configuration, and testing procedures required before attempting Hardware-In-the-Loop (HIL) simulation with Gazebo.

---

## Priority Levels

- 🔴 **CRITICAL** - Must fix, system won't work without it
- 🟠 **HIGH** - Should fix, major functionality affected
- 🟡 **MEDIUM** - Important, but system can work without it
- 🟢 **LOW** - Nice to have, optimization

---

## Phase 1: Critical HRT Fixes (MUST DO FIRST)

### Task 1.1: Fix HRT Time Units Conversion 🔴 CRITICAL

**Problem:** HRT returns timer ticks instead of microseconds, breaking all timing in PX4.

**Impact:**
- EKF2 sensor fusion timing wrong by 4.7x
- Control loops running at incorrect rates
- MAVLink timestamps incorrect
- Logger timestamps incorrect

**Fix Location:** `platforms/nuttx/src/px4/microchip/samv7/hrt/hrt.c`

**Code Changes:**

```c
// Line 89: Add actual frequency calculation
#define HRT_ACTUAL_FREQ  (HRT_TIMER_CLOCK / HRT_DIVISOR)

// Lines 124-141: Replace hrt_absolute_time() function
hrt_abstime hrt_absolute_time(void)
{
    uint32_t count;
    irqstate_t flags;
    uint64_t abstime;

    flags = enter_critical_section();

    // Read 32-bit counter value
    count = getreg32(rCV);

    // Calculate absolute time in timer ticks
    uint64_t ticks = hrt_absolute_time_base + count;

    // Convert ticks to microseconds
    abstime = (ticks * 1000000ULL) / HRT_ACTUAL_FREQ;

    leave_critical_section(flags);

    return abstime;
}
```

**Test Procedure:**
```bash
# 1. Build with fix
make microchip_samv71-xult-clickboards_default

# 2. Flash to board
make microchip_samv71-xult-clickboards_default upload

# 3. Connect serial console
minicom -D /dev/ttyACM0 -b 115200

# 4. Test time measurement
nsh> hrt_test
# Read HRT multiple times, verify ~1µs increments

nsh> logger start
nsh> logger status
# Check timestamp rates, should be in microseconds

# 5. Expected: Time increases by ~1 per microsecond
```

**Success Criteria:**
- ✅ `hrt_absolute_time()` increases by ~1 per microsecond
- ✅ EKF2 reports reasonable dt values (~0.01s for 100Hz)
- ✅ Control loops run at expected rates

---

### Task 1.2: Enable HRT Overflow Interrupt 🔴 CRITICAL

**Problem:** Counter wraps after 15.3 minutes, time jumps backwards.

**Impact:** System crash after 15 minutes of operation.

**Fix Location:** `platforms/nuttx/src/px4/microchip/samv7/hrt/hrt.c`

**Code Changes:**

```c
// Lines 146-205: Update hrt_init()
void hrt_init(void)
{
    syslog(LOG_ERR, "[hrt] hrt_init starting\n");
    sq_init(&callout_queue);

    // Enable peripheral clock for TC0
    uint32_t regval = getreg32(SAM_PMC_PCER0);
    regval |= HRT_TIMER_PCER;
    putreg32(regval, SAM_PMC_PCER0);

    // Disable TC clock
    putreg32(TC_CCR_CLKDIS, rCCR);

    // Configure TC channel mode
    uint32_t cmr = TC_CMR_WAVE | TC_CMR_WAVSEL_UP;

    // Select prescaler
    if (HRT_TIMER_CLOCK / 8 > 1000000) {
        cmr |= TC_CMR_TCCLKS_MCK32;
        syslog(LOG_ERR, "[hrt] Using MCK/32 prescaler\n");
    } else {
        cmr |= TC_CMR_TCCLKS_MCK8;
        syslog(LOG_ERR, "[hrt] Using MCK/8 prescaler\n");
    }

    putreg32(cmr, rCMR);

    // Set RC to maximum value for free-running mode
    putreg32(0xFFFFFFFF, rRC);

    // Attach interrupt handler
    irq_attach(HRT_TIMER_VECTOR, hrt_tim_isr, NULL);

    // Enable RC compare interrupt (overflow)
    putreg32(TC_INT_CPCS, rIER);

    // Clear status
    (void)getreg32(rSR);

    // Enable TC clock and trigger
    putreg32(TC_CCR_CLKEN | TC_CCR_SWTRG, rCCR);

    // Enable interrupt
    up_enable_irq(HRT_TIMER_VECTOR);

    // Initialize absolute time base
    hrt_absolute_time_base = 0;
    hrt_counter_wrap_count = 0;

    syslog(LOG_ERR, "[hrt] HRT initialized with overflow interrupt\n");
}

// Lines 258-277: Update hrt_tim_isr()
static int hrt_tim_isr(int irq, void *context, void *arg)
{
    uint32_t status;

    // Read and clear status
    status = getreg32(rSR);

    // Handle counter overflow/wrap
    if (status & TC_INT_CPCS) {
        // Add full 32-bit range in ticks
        hrt_absolute_time_base += 0xFFFFFFFFULL;
        hrt_counter_wrap_count++;

        // Note: hrt_absolute_time() will convert to microseconds
    }

    // Process callouts (will be used after Task 1.3)
    hrt_call_invoke();
    hrt_call_reschedule();

    return OK;
}
```

**Test Procedure:**
```bash
# 1. Build and flash with overflow fix
make microchip_samv71-xult-clickboards_default
make microchip_samv71-xult-clickboards_default upload

# 2. Test wraparound (wait 16+ minutes)
nsh> date
# Record current time

nsh> hrt_absolute_time
# Record value (e.g., 60,000,000 µs = 60 seconds)

# Wait 16 minutes...

nsh> hrt_absolute_time
# Should be ~960,000,000 higher (16 minutes in µs)
# Must NOT jump backwards!

# 3. Check wrap count
nsh> dmesg | grep hrt
# Should see "Counter wrapped" messages
```

**Success Criteria:**
- ✅ Time continues increasing after 16 minutes
- ✅ No backwards time jumps
- ✅ `hrt_counter_wrap_count` increments

---

### Task 1.3: Enable HRT Callback Interrupts 🟠 HIGH

**Problem:** Callbacks use polling instead of hardware interrupts (high latency).

**Impact:** Control loop timing imprecise, higher CPU overhead.

**Fix Location:** `platforms/nuttx/src/px4/microchip/samv7/hrt/hrt.c`

**Code Changes:**

```c
// Lines 240-252: Update hrt_call_reschedule()
static void hrt_call_reschedule(void)
{
    hrt_abstime now = hrt_absolute_time();
    struct hrt_call *next = (struct hrt_call *)sq_peek(&callout_queue);

    if (next != NULL) {
        hrt_abstime deadline_usec = next->deadline;

        // Convert microseconds to timer ticks
        uint64_t deadline_ticks = (deadline_usec * HRT_ACTUAL_FREQ) / 1000000ULL;

        // Calculate compare value relative to base
        uint32_t compare = (uint32_t)(deadline_ticks - hrt_absolute_time_base);

        // Set RA register for compare interrupt
        putreg32(compare, rRA);

        // Enable RA compare interrupt
        putreg32(TC_INT_CPAS, rIER);
    } else {
        // No callbacks, disable RA interrupt
        putreg32(TC_INT_CPAS, rIDR);
    }
}

// Lines 258-285: Update hrt_tim_isr() to handle callbacks
static int hrt_tim_isr(int irq, void *context, void *arg)
{
    uint32_t status;

    // Read and clear status
    status = getreg32(rSR);

    // Handle counter overflow/wrap (RC compare)
    if (status & TC_INT_CPCS) {
        hrt_absolute_time_base += 0xFFFFFFFFULL;
        hrt_counter_wrap_count++;
    }

    // Handle callback deadline (RA compare)
    if (status & TC_INT_CPAS) {
        // Invoke scheduled callbacks
        hrt_call_invoke();

        // Schedule next callback
        hrt_call_reschedule();
    }

    return OK;
}
```

**Test Procedure:**
```bash
# 1. Build and flash
make microchip_samv71-xult-clickboards_default
make microchip_samv71-xult-clickboards_default upload

# 2. Test callback scheduling
nsh> mc_rate_control start
nsh> mc_rate_control status
# Check: "cycle: XXX events" should increment
# Check: "XXXus avg" should be ~2000µs (500Hz rate)

# 3. Monitor CPU usage
nsh> top
# CPU usage should be LOW for hrt_call operations

# 4. Test callback latency
# Add instrumentation to measure time from deadline to callback
# Expected: 2-10µs latency (hardware interrupt)
```

**Success Criteria:**
- ✅ Callbacks triggered at precise intervals
- ✅ Low CPU overhead (<1% for HRT)
- ✅ Latency <10µs

---

## Phase 2: Hardware Configuration

### Task 2.1: Configure SPI Buses 🔴 CRITICAL

**Problem:** SPI pin mappings not configured, ICM-20689 IMU cannot be used.

**Impact:** No IMU data = no flight control.

**Fix Location:** `boards/microchip/samv71-xult-clickboards/src/spi.cpp`

**Code Changes:**

**Step 1: Determine Pin Mapping from Schematic**

Check SAMV71-XULT schematic for SPI routing to mikroBUS sockets:
```
mikroBUS Socket 1:
- MOSI: PA21 (SPI0_MOSI)
- MISO: PA22 (SPI0_MISO)
- SCK:  PA23 (SPI0_SPCK)
- CS:   PA11 (GPIO, manual control)

mikroBUS Socket 2:
- MOSI: PD20 (SPI1_MOSI)
- MISO: PD21 (SPI1_MISO)
- SCK:  PD22 (SPI1_SPCK)
- CS:   PD25 (GPIO, manual control)
```

**Step 2: Update spi.cpp**

```cpp
#include <px4_arch/spi_hw_description.h>
#include <drivers/drv_sensor.h>
#include <nuttx/spi/spi.h>

/* SPI bus configuration for SAMV71-XULT with Click sensor boards */

constexpr px4_spi_bus_t px4_spi_buses[SPI_BUS_MAX_BUS_ITEMS] = {
    // SPI0: mikroBUS Socket 1
    initSPIBus(SPI::Bus::SPI0, {
        initSPIDevice(DRV_IMU_DEVTYPE_ICM20689, SPI::CS{GPIO::PortA, GPIO::Pin11}),
    }),

    // SPI1: mikroBUS Socket 2 (future expansion)
    // initSPIBus(SPI::Bus::SPI1, {
    //     initSPIDevice(DRV_BARO_DEVTYPE_DPS310, SPI::CS{GPIO::PortD, GPIO::Pin25}),
    // }),
};

/* SPI chip select functions */
extern "C" {

void sam_spi0select(uint32_t devid, bool selected)
{
    // CS is active low
    if (devid == DRV_IMU_DEVTYPE_ICM20689) {
        sam_gpiowrite(GPIO_SPI0_CS_ICM20689, !selected);
    }
}

uint8_t sam_spi0status(struct spi_dev_s *dev, uint32_t devid)
{
    return 0;  // Device always present
}

void sam_spi1select(uint32_t devid, bool selected)
{
    // Future: Add SPI1 device CS control
}

uint8_t sam_spi1status(struct spi_dev_s *dev, uint32_t devid)
{
    return 0;
}

} // extern "C"
```

**Step 3: Add GPIO Definitions**

**File:** `boards/microchip/samv71-xult-clickboards/src/board_config.h`

```c
/* SPI Chip Select GPIOs */
#define GPIO_SPI0_CS_ICM20689  (GPIO_OUTPUT|GPIO_CFG_DEFAULT|GPIO_OUTPUT_SET| \
                                GPIO_PORT_PIOA|GPIO_PIN11)

#define PX4_GPIO_INIT_LIST { \
    GPIO_nLED_BLUE,           \
    GPIO_SPI0_CS_ICM20689,    \
}
```

**Test Procedure:**
```bash
# 1. Build with SPI configuration
make microchip_samv71-xult-clickboards_default
make microchip_samv71-xult-clickboards_default upload

# 2. Verify SPI buses detected
nsh> spi_dev
# Should show SPI0 available

# 3. Enable ICM-20689 in rc.board_sensors
# Edit: boards/.../init/rc.board_sensors
# Uncomment: icm20689 start -s -b 0

# 4. Test sensor startup
nsh> icm20689 start -s -b 0
nsh> icm20689 status
# Should show "Running" and sample rates

# 5. Verify sensor data
nsh> listener sensor_gyro
nsh> listener sensor_accel
# Should see IMU data streaming
```

**Success Criteria:**
- ✅ SPI0 detected and initialized
- ✅ ICM-20689 driver starts without errors
- ✅ Gyro and accel data streaming

---

### Task 2.2: Test I2C Sensors 🟡 MEDIUM

**Sensors:** AK09916 (magnetometer), DPS310 (barometer)

**Status:** I2C already configured, just need to test with hardware.

**Test Procedure:**
```bash
# 1. Connect sensors to I2C bus
# - AK09916: Address 0x0C
# - DPS310: Address 0x77

# 2. Scan I2C bus
nsh> i2c dev 0 0x03 0x77
# Should detect devices at 0x0C and 0x77

# 3. Start sensor drivers (already in rc.board_sensors)
nsh> ak09916 start -I -b 0
nsh> dps310 start -I -b 0

# 4. Verify sensor status
nsh> ak09916 status
nsh> dps310 status
# Both should show "Running"

# 5. Check sensor data
nsh> listener sensor_mag
nsh> listener sensor_baro
# Should see magnetometer and barometer data
```

**Success Criteria:**
- ✅ I2C devices detected at correct addresses
- ✅ Drivers start successfully
- ✅ Sensor data streaming

---

### Task 2.3: Configure PWM Outputs 🟠 HIGH

**Problem:** Timer/Counter not configured for PWM, motor outputs won't work.

**Impact:** Cannot control motors in HIL.

**Fix Location:** `boards/microchip/samv71-xult-clickboards/src/timer_config.cpp`

**Code Changes:**

```cpp
#include <px4_arch/io_timer_hw_description.h>
#include <px4_arch/io_timer.h>

/* SAMV71 PWM Configuration using TC (Timer/Counter)
 *
 * TC0 Channel 1: PWM outputs 1-2 (TIOA0, TIOB0)
 * TC0 Channel 2: PWM outputs 3-4 (TIOA1, TIOB1)
 * TC1 Channel 0: PWM outputs 5-6 (TIOA3, TIOB3)
 */

constexpr io_timers_t io_timers[MAX_IO_TIMERS] = {
    // TC0 Channel 1
    initIOTimer(Timer::TC0_CH1, DMA{DMA::Index1}),

    // TC0 Channel 2
    initIOTimer(Timer::TC0_CH2, DMA{DMA::Index2}),

    // TC1 Channel 0
    initIOTimer(Timer::TC1_CH0, DMA{DMA::Index3}),
};

constexpr timer_io_channels_t timer_io_channels[MAX_TIMER_IO_CHANNELS] = {
    // PWM outputs on TC0 CH1
    initIOTimerChannel(io_timers, {Timer::TC0_CH1, Timer::ChannelA}, {GPIO::PortA, GPIO::Pin0}),
    initIOTimerChannel(io_timers, {Timer::TC0_CH1, Timer::ChannelB}, {GPIO::PortA, GPIO::Pin1}),

    // PWM outputs on TC0 CH2
    initIOTimerChannel(io_timers, {Timer::TC0_CH2, Timer::ChannelA}, {GPIO::PortA, GPIO::Pin2}),
    initIOTimerChannel(io_timers, {Timer::TC0_CH2, Timer::ChannelB}, {GPIO::PortA, GPIO::Pin3}),

    // PWM outputs on TC1 CH0
    initIOTimerChannel(io_timers, {Timer::TC1_CH0, Timer::ChannelA}, {GPIO::PortA, GPIO::Pin4}),
    initIOTimerChannel(io_timers, {Timer::TC1_CH0, Timer::ChannelB}, {GPIO::PortA, GPIO::Pin5}),
};
```

**Test Procedure:**
```bash
# 1. Build with PWM configuration
make microchip_samv71-xult-clickboards_default
make microchip_samv71-xult-clickboards_default upload

# 2. Test PWM output
nsh> pwm_out_sim start
nsh> pwm_out_sim arm
nsh> pwm_out_sim test

# 3. Measure PWM signals with oscilloscope/logic analyzer
# - Frequency: 50-400 Hz
# - Pulse width: 1000-2000µs

# 4. Test all 6 channels
nsh> pwm_out_sim channel 1 1500
nsh> pwm_out_sim channel 2 1500
# ... repeat for channels 3-6
```

**Success Criteria:**
- ✅ PWM driver initializes
- ✅ 6 PWM channels output correct signals
- ✅ Frequency and pulse width adjustable

---

### Task 2.4: Configure Additional UARTs 🟡 MEDIUM

**Ports Needed:**
- GPS (UART2)
- RC input (UART4)

**Fix Location:** Board initialization and defconfig

**Test Procedure:**
```bash
# 1. Enable UART2 and UART4 in defconfig
CONFIG_SAMV7_UART2=y
CONFIG_SAMV7_UART4=y

# 2. Test UART2 (GPS)
nsh> gps start -d /dev/ttyS2
nsh> gps status
# Should show "Running"

# 3. Test UART4 (RC input - requires RC receiver)
nsh> rc_input start
nsh> rc_input status
```

**Success Criteria:**
- ✅ UARTs initialize without errors
- ✅ GPS data received (if GPS connected)
- ✅ RC input detected (if receiver connected)

---

## Phase 3: Integration Testing

### Task 3.1: Verify EKF2 Timing 🔴 CRITICAL

**Purpose:** Ensure HRT fixes result in correct sensor fusion.

**Test Procedure:**
```bash
# 1. Start all sensors
nsh> icm20689 start -s -b 0
nsh> ak09916 start -I -b 0
nsh> dps310 start -I -b 0

# 2. Start EKF2
nsh> ekf2 start

# 3. Check EKF2 status
nsh> ekf2 status

# Expected output:
# - dt values: ~0.008-0.010s (gyro at 100-125Hz)
# - No "time went backwards" errors
# - Innovation test ratios < 1.0 (good fusion)
# - Position/velocity estimates reasonable

# 4. Monitor for 5 minutes
# Watch for timing anomalies or divergence
```

**Success Criteria:**
- ✅ dt values consistent and reasonable
- ✅ No timing errors in dmesg
- ✅ EKF2 estimates stable

---

### Task 3.2: Test Control Loop Rates 🟠 HIGH

**Purpose:** Verify control loops run at correct frequencies.

**Test Procedure:**
```bash
# 1. Start control modules
nsh> mc_rate_control start
nsh> mc_att_control start
nsh> mc_pos_control start

# 2. Check execution rates
nsh> mc_rate_control status
# Expected: ~500 Hz (2000µs period)

nsh> mc_att_control status
# Expected: ~250 Hz (4000µs period)

nsh> mc_pos_control status
# Expected: ~50 Hz (20000µs period)

# 3. Monitor for rate consistency
nsh> top
# Check CPU usage, should be reasonable
```

**Success Criteria:**
- ✅ Rates match expected values
- ✅ No timing jitter or drift
- ✅ CPU usage acceptable

---

### Task 3.3: Verify Logger and MAVLink 🟡 MEDIUM

**Test Procedure:**
```bash
# 1. Start logger
nsh> logger start
nsh> logger status
# Check: "Not logging" → start logging with "logger on"

# 2. Start MAVLink
nsh> mavlink start -d /dev/ttyS1 -b 57600

# 3. Connect QGroundControl
# Verify:
# - Timestamps reasonable
# - Sensor data displays correctly
# - No timestamp warnings

# 4. Stop logger and check ULog
nsh> logger stop
nsh> ls /fs/microsd/log
# Download .ulg file and analyze with Flight Review
```

**Success Criteria:**
- ✅ ULog timestamps reasonable
- ✅ QGC displays correct data
- ✅ Flight Review analysis works

---

### Task 3.4: DMA and Cache Stress Test 🟡 MEDIUM

**Purpose:** Verify DMA coherency under load.

**Test Procedure:**
```bash
# 1. Start all DMA peripherals simultaneously
nsh> icm20689 start -s -b 0      # SPI DMA
nsh> ak09916 start -I -b 0       # I2C (optionally DMA)
nsh> dps310 start -I -b 0        # I2C
nsh> logger start; logger on      # SD card DMA
nsh> mavlink start -d /dev/ttyS1  # UART DMA

# 2. Monitor for 30 minutes
# Watch for:
# - Sensor data corruption (unlikely values)
# - EKF2 divergence (bad sensor fusion)
# - Logger errors (SD card failures)
# - MAVLink CRC errors

# 3. Check DMA pool usage
nsh> # TODO: Add command to check board_get_dma_usage()
```

**Success Criteria:**
- ✅ No data corruption
- ✅ No DMA pool exhaustion
- ✅ Stable operation under load

---

## Phase 4: HIL Preparation

### Task 4.1: Configure HIL Parameters 🟡 MEDIUM

**Purpose:** Set parameters for HIL simulation mode.

**Commands:**
```bash
# Enable HIL mode
param set SYS_AUTOSTART 1001  # HIL quadcopter
param set SYS_HITL 1          # Enable HITL
param set MAV_TYPE 2          # Quadcopter

# Set simulation parameters
param set SIL_GPS_ENABLE 1
param set SIL_MAG_ENABLE 1
param set SIL_BARO_ENABLE 1

# Disable real sensors in HIL
param set SENS_EN_BARO 0
param set SENS_EN_MAG 0

# Save and reboot
param save
reboot
```

---

### Task 4.2: Set Up Gazebo SITL Connection 🟡 MEDIUM

**Purpose:** Connect SAMV71 to Gazebo simulator.

**Steps:**

1. **On Development PC:**
```bash
# Start Gazebo with PX4 SITL
cd PX4-Autopilot
make px4_sitl gazebo
```

2. **On SAMV71:**
```bash
# Connect to Gazebo via MAVLink
nsh> mavlink start -d /dev/ttyS0 -b 921600 -m onboard -r 4000
```

3. **Configure QGroundControl:**
- Add HIL connection
- Set baud rate and port

**Success Criteria:**
- ✅ Gazebo receives sensor data from SAMV71
- ✅ SAMV71 receives actuator commands from Gazebo
- ✅ QGC displays simulated vehicle

---

### Task 4.3: Test Basic Flight Modes in HIL 🟡 MEDIUM

**Test Sequence:**

1. **Manual Mode:**
```bash
nsh> commander mode manual
# Test: Stick inputs control simulated vehicle
```

2. **Stabilize Mode:**
```bash
nsh> commander mode stabilized
# Test: Vehicle self-levels when sticks centered
```

3. **Altitude Hold:**
```bash
nsh> commander mode altctl
# Test: Vehicle maintains altitude
```

4. **Position Hold:**
```bash
nsh> commander mode posctl
# Test: Vehicle holds position in simulation
```

**Success Criteria:**
- ✅ Mode transitions work
- ✅ Vehicle responds correctly in simulation
- ✅ No crashes or anomalies

---

## Phase 5: Documentation

### Task 5.1: Document All Fixes 🟢 LOW

**Purpose:** Update documentation with HRT fixes and test results.

**Files to Update:**
- `HRT_ANALYSIS.md` - Add "Fixes Applied" section
- `PORTING_REFERENCE.md` - Add troubleshooting entries
- `TESTING_GUIDE.md` - Add HIL test procedures

---

### Task 5.2: Create HIL Testing Guide 🟢 LOW

**Purpose:** Provide step-by-step HIL testing instructions.

**Content:**
- Gazebo setup
- MAVLink configuration
- Parameter settings
- Test flight procedures
- Troubleshooting

---

## Summary Checklist

### Critical Path (Must Complete)

- [ ] 1.1 Fix HRT time units conversion
- [ ] 1.2 Enable HRT overflow interrupt
- [ ] 1.3 Enable HRT callback interrupts
- [ ] 2.1 Configure SPI buses
- [ ] 3.1 Verify EKF2 timing
- [ ] 3.2 Test control loop rates

### High Priority (Should Complete)

- [ ] 2.3 Configure PWM outputs
- [ ] 2.2 Test I2C sensors

### Medium Priority (Nice to Have)

- [ ] 2.4 Configure additional UARTs
- [ ] 3.3 Verify logger and MAVLink
- [ ] 3.4 DMA cache stress test
- [ ] 4.1 Configure HIL parameters
- [ ] 4.2 Set up Gazebo SITL
- [ ] 4.3 Test flight modes in HIL

### Low Priority (Optional)

- [ ] 5.1 Document all fixes
- [ ] 5.2 Create HIL testing guide

---

## Estimated Timeline

**Phase 1 (HRT Fixes):** 2-4 hours
- Critical for system stability
- Must be done first

**Phase 2 (Hardware Config):** 4-8 hours
- SPI: 2-3 hours (pin mapping, testing)
- I2C: 1 hour (already configured, just test)
- PWM: 2-3 hours (timer setup, testing)
- UART: 1 hour

**Phase 3 (Integration Testing):** 4-6 hours
- EKF2: 1-2 hours
- Control loops: 1-2 hours
- Logger/MAVLink: 1-2 hours
- DMA stress: 1-2 hours (long duration test)

**Phase 4 (HIL Prep):** 2-4 hours
- Parameter configuration: 30 minutes
- Gazebo setup: 1-2 hours
- Flight tests: 1-2 hours

**Total Estimated Time:** 12-22 hours

---

## Risk Assessment

### High Risk Issues

1. **HRT timing bugs cause flight instability**
   - Mitigation: Thorough testing after each HRT fix
   - Fallback: Revert to working configuration

2. **SPI pin mapping incorrect**
   - Mitigation: Verify with schematic before flashing
   - Test: Use logic analyzer to confirm signals

3. **Cache coherency issues with DMA**
   - Mitigation: Already configured correctly (write-through)
   - Test: Stress test multiple DMA peripherals

### Medium Risk Issues

1. **PWM timer configuration errors**
   - Mitigation: Test each channel individually
   - Fallback: Use simpler timer configuration

2. **HIL communication failures**
   - Mitigation: Test MAVLink separately first
   - Debug: Use serial sniffer to verify packets

---

## Quick Reference Commands

**Build and Flash:**
```bash
make microchip_samv71-xult-clickboards_default
make microchip_samv71-xult-clickboards_default upload
```

**Serial Console:**
```bash
minicom -D /dev/ttyACM0 -b 115200
```

**Useful Debug Commands:**
```bash
nsh> dmesg                    # System log
nsh> top                      # CPU usage
nsh> free                     # Memory usage
nsh> ps                       # Process list
nsh> work_queue status        # Task timing
nsh> param show               # All parameters
nsh> listener sensor_gyro     # Sensor data
```

---

**Document Version:** 1.0
**Date:** 2025-11-13
**Status:** Ready for implementation
