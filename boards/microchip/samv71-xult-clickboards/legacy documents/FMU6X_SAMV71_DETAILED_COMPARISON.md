# FMU-6X vs SAMV71-XULT: Comprehensive Implementation Comparison

**Document Version:** 1.5
**Date:** November 28, 2025
**Purpose:** Detailed technical comparison for achieving FMU-6X feature parity on SAMV71-XULT
**Reference Boards:**
- FMU-6X: `boards/px4/fmu-v6x/` (STM32H753, production flight controller)
- SAMV71-XULT: `boards/microchip/samv71-xult-clickboards/` (ATSAMV71Q21, development board)

---

## Executive Summary

| Metric | FMU-6X | SAMV71-XULT | Status |
|--------|--------|-------------|--------|
| **MCU** | STM32H753 (480MHz Cortex-M7) | ATSAMV71Q21 (300MHz Cortex-M7) | Different |
| **Flash** | 2MB | 2MB | Same |
| **RAM** | 1MB | 384KB | FMU-6X larger |
| **Core Flight Software** | 100% | ~90% | ⚠️ Missing PWM_OUT, ADC, dmesg |
| **Hardware Drivers** | 100% | ~30% | ❌ Gap |
| **Flight Ready** | ✅ Yes | ❌ No (missing PWM) | ❌ Gap |
| **HITL Ready** | ✅ Yes | ✅ Yes | ✅ Parity |

> **IMPORTANT**: "Core Flight Software" parity requires PWM_OUT, ADC, and console buffer to be
> re-enabled. Currently these are disabled in `default.px4board`:
> - `CONFIG_DRIVERS_PWM_OUT` - disabled (timer layer incomplete)
> - `CONFIG_DRIVERS_ADC_BOARD_ADC` - disabled (not implemented for SAMV7)
> - `CONFIG_SYSTEMCMDS_DMESG` - disabled (console buffer static init bug)
> - `CONFIG_SYSTEMCMDS_HARDFAULT_LOG` - disabled (needs PROGMEM infrastructure)

---

## Section 1: Boot & Core Infrastructure

### 1.1 Board Initialization Sequence

#### FMU-6X (`boards/px4/fmu-v6x/src/init.cpp`)

```cpp
// Early init - called before devices initialized
void stm32_boardinitialize(void)
{
    board_on_reset(-1);              // Reset PWM pins to safe state
    board_autoled_initialize();       // Configure LED GPIOs
    px4_gpio_init(gpio, arraySize(gpio));  // Init all GPIOs from PX4_GPIO_INIT_LIST
    stm32_usbinitialize();           // USB peripheral init
    VDD_3V3_ETH_POWER_EN(true);      // Enable Ethernet power
}

// App init - called via boardctl(BOARDIOC_INIT)
int board_app_initialize(uintptr_t arg)
{
    // Power on all interfaces
    VDD_3V3_SD_CARD_EN(true);
    VDD_5V_PERIPH_EN(true);
    VDD_5V_HIPOWER_EN(true);
    VDD_3V3_SENSORS4_EN(true);
    VDD_3V3_SPEKTRUM_POWER_EN(true);

    px4_platform_init();              // HRT, workqueues, uORB
    stm32_spiinitialize();            // SPI buses
    px4_platform_configure();         // MTD manifest
    board_determine_hw_info();        // Read HW version from ADC
    stm32_spiinitialize();            // Re-init SPI with correct HW config
    board_spi_reset(10, 0xffff);      // Reset all SPI sensors
    board_dma_alloc_init();           // DMA buffer pool

    // Serial DMA polling for UART RX
    hrt_call_every(&serial_dma_call, 1000, 1000, stm32_serial_dma_poll, NULL);

    drv_led_start();                  // LED driver
    board_hardfault_init(2, true);    // Hardfault logging
    stm32_sdio_initialize();          // SD card

    return OK;
}
```

#### SAMV71-XULT (`boards/microchip/samv71-xult-clickboards/src/init.c`)

```c
// Early init
void sam_boardinitialize(void)
{
    board_on_reset(-1);              // Stub - does nothing
    configure_hrt_pck6();            // Configure 1MHz clock for HRT
    board_autoled_initialize();
    led_init();
    px4_gpio_init(gpio, arraySize(gpio));
    sam_usbinitialize();
    // Note: No power control; Ethernet not yet enabled (hardware available)
}

// App init
int board_app_initialize(uintptr_t arg)
{
    px4_platform_init();
    samv71_sdcard_initialize();      // HSMCI SD card

    // I2C bus initialization
    struct i2c_master_s *i2c0 = sam_i2cbus_initialize(0);
    i2c_register(i2c0, 0);

    board_dma_alloc_init();
    drv_led_start();
    board_hardfault_init(2, true);

    return OK;
}
```

### 1.2 Comparison Table

| Feature | FMU-6X | SAMV71-XULT | Gap Analysis |
|---------|--------|-------------|--------------|
| `stm32/sam_boardinitialize()` | Full power sequencing | Basic GPIO + HRT | ❌ Missing power control |
| `board_app_initialize()` | Power + SPI + HW detect + SD | I2C + SD only | ❌ Missing SPI init, HW detect |
| Power rail control | 7 enable pins | None | ❌ **Gap** |
| HW version detection | ADC-based auto-detect | Not implemented | ⚠️ Optional |
| Serial DMA polling | Enabled | Not implemented | ⚠️ Optional |
| Console buffer | `BOARD_ENABLE_CONSOLE_BUFFER` | Disabled (bug) | ❌ **Gap** |

### 1.3 GPIO Initialization Lists

#### FMU-6X `PX4_GPIO_INIT_LIST` (28 pins)

```cpp
#define PX4_GPIO_INIT_LIST { \
    GPIO_TRACE,                       \
    PX4_ADC_GPIO,                     \  // 9 ADC pins
    GPIO_HW_VER_REV_DRIVE,            \
    GPIO_CAN1_TX, GPIO_CAN1_RX,       \
    GPIO_CAN2_TX, GPIO_CAN2_RX,       \
    GPIO_HEATER_OUTPUT,               \
    GPIO_nPOWER_IN_A,                 \  // Power brick 1 valid
    GPIO_nPOWER_IN_B,                 \  // Power brick 2 valid
    GPIO_nPOWER_IN_C,                 \  // USB valid
    GPIO_VDD_5V_PERIPH_nEN,           \  // 5V peripheral enable
    GPIO_VDD_5V_PERIPH_nOC,           \  // 5V overcurrent
    GPIO_VDD_5V_HIPOWER_nEN,          \  // 5V high-power enable
    GPIO_VDD_5V_HIPOWER_nOC,          \  // 5V HP overcurrent
    GPIO_VDD_3V3_SENSORS4_EN,         \  // Sensor power
    GPIO_VDD_3V3_SPEKTRUM_POWER_EN,   \  // RC power
    GPIO_VDD_3V3_SD_CARD_EN,          \  // SD card power
    GPIO_PD15,                        \
    GPIO_SYNC,                        \
    SPI6_nRESET_EXTERNAL1,            \
    GPIO_ETH_POWER_EN,                \  // Ethernet power
    GPIO_NFC_GPIO,                    \
    GPIO_TONE_ALARM_IDLE,             \
    GPIO_nSAFETY_SWITCH_LED_OUT_INIT, \
    GPIO_SAFETY_SWITCH_IN,            \
    GPIO_PG6,                         \
    GPIO_nARMED_INIT                  \
}
```

#### SAMV71-XULT `PX4_GPIO_INIT_LIST` (3 pins)

```c
#define PX4_GPIO_INIT_LIST { \
    GPIO_nLED_BLUE,           \
    GPIO_SPI0_CS_ICM20689,    \
    GPIO_SPI0_DRDY_ICM20689,  \
}
```

### 1.4 Required SAMV71 Additions for Parity

```c
// Add to board_config.h for mikroBUS support
#define GPIO_MB1_INT     (GPIO_INPUT|GPIO_CFG_PULLUP|GPIO_PORT_PIOA|GPIO_PIN0)
#define GPIO_MB1_RST     (GPIO_OUTPUT|GPIO_OUTPUT_SET|GPIO_PORT_PIOA|GPIO_PIN19)
#define GPIO_MB1_PWM     (GPIO_OUTPUT|GPIO_OUTPUT_CLEAR|GPIO_PORT_PIOC|GPIO_PIN19)
#define GPIO_MB2_INT     (GPIO_INPUT|GPIO_CFG_PULLUP|GPIO_PORT_PIOA|GPIO_PIN6)
#define GPIO_MB2_RST     (GPIO_OUTPUT|GPIO_OUTPUT_SET|GPIO_PORT_PIOB|GPIO_PIN0)
#define GPIO_MB2_PWM     (GPIO_OUTPUT|GPIO_OUTPUT_CLEAR|GPIO_PORT_PIOB|GPIO_PIN1)

// Updated init list
#define PX4_GPIO_INIT_LIST { \
    GPIO_nLED_BLUE,           \
    GPIO_SPI0_CS_ICM20689,    \
    GPIO_SPI0_DRDY_ICM20689,  \
    GPIO_MB1_INT,             \
    GPIO_MB1_RST,             \
    GPIO_MB2_INT,             \
    GPIO_MB2_RST,             \
}
```

---

## Section 2: Clock & Timing (HRT)

### 2.1 Architecture Comparison

| Aspect | FMU-6X (STM32H7) | SAMV71-XULT |
|--------|------------------|-------------|
| **HRT Timer** | TIM8 (32-bit native) | TC0 (16-bit + software accumulator) |
| **Clock Source** | APB2 timer clock | PCK6 (programmable) |
| **Target Frequency** | 1 MHz | 1 MHz |
| **Resolution** | 1 µs | 1 µs |
| **Configuration** | In Kconfig/defconfig | `configure_hrt_pck6()` |
| **Wrap Handling** | Hardware 32-bit | Software extension (see note) |

> **SAMV7 HRT Note**: The SAMV7 TC channels are 16-bit only—there is NO hardware chain-to-32-bit
> mode. The time base is extended in software via an accumulator. The compare interrupt must fire
> at least once every ~65 ms (`HRT_INTERVAL_MAX = 50000 µs`) to track wraps correctly. Longer
> stalls would require a more complex software chain or a device with native 32-bit timers.
> See: `platforms/nuttx/src/px4/microchip/samv7/hrt/hrt.c:76-85`

### 2.2 FMU-6X HRT Configuration

```cpp
// board_config.h
#define HRT_TIMER               8   // TIM8
#define HRT_TIMER_CHANNEL       3   // Channel 3 for capture/compare
#define HRT_PPM_CHANNEL         1   // Channel 1 for PPM input
#define GPIO_PPM_IN             GPIO_TIM8_CH1IN_2  // PI5
```

Timer 8 is a 32-bit advanced timer with DMA support, auto-reload, and multiple capture/compare channels.

### 2.3 SAMV71 HRT Configuration

```c
// board_config.h
#define HRT_TIMER               0   // TC0
#define HRT_TIMER_CHANNEL       0   // Channel 0

// init.c - PCK6 configuration
static void configure_hrt_pck6(void)
{
    uint32_t actual_freq;
    uint32_t regval;
    int timeout = 100000;

    g_samv7_hrt_pck6_configured = false;

    // Configure PCK6 to output 1 MHz from MCK
    actual_freq = sam_pck_configure(PCK6, PCKSRC_MCK, 1000000);
    if (actual_freq != 1000000) {
        syslog(LOG_ERR, "[hrt] PCK6 configure failed (%lu Hz)\n", actual_freq);
        return;
    }

    // Enable PCK6
    sam_pck_enable(PCK6, true);

    // Wait for PCK6 ready
    while ((getreg32(SAM_PMC_SR) & PMC_INT_PCKRDY6) == 0) {
        if (--timeout <= 0) {
            syslog(LOG_ERR, "[hrt] PCK6 ready timeout\n");
            return;
        }
    }

    // Route TC0 clock to PCK6 via MATRIX CCFG_PCCR
    putreg32(MATRIX_WPMR_WPKEY, SAM_MATRIX_WPMR);  // Unlock MATRIX
    regval = getreg32(SAM_MATRIX_CCFG_PCCR);
    regval &= ~MATRIX_CCFG_PCCR_TC0CC;  // TC0CC=0 selects PCK6
    putreg32(regval, SAM_MATRIX_CCFG_PCCR);

    g_samv7_hrt_pck6_configured = true;
    syslog(LOG_INFO, "[hrt] PCK6 configured for 1 MHz TC0 clock\n");
}
```

### 2.4 HRT Health Monitoring

| Feature | FMU-6X | SAMV71-XULT | Status |
|---------|--------|-------------|--------|
| Health flag | Implicit | `g_samv7_hrt_pck6_configured` | ✅ SAMV71 better |
| Boot panic on fail | Not explicit | Logs error, continues | ⚠️ Could improve |
| `hrt_test` command | Available | Available | ✅ Parity |

### 2.5 Recommended SAMV71 Improvements

```c
// Add to init.c - panic on HRT failure
static void configure_hrt_pck6(void)
{
    // ... existing code ...

    if (!g_samv7_hrt_pck6_configured) {
        // HRT is critical - halt if it fails
        syslog(LOG_EMERG, "[hrt] FATAL: HRT clock configuration failed!\n");
        syslog(LOG_EMERG, "[hrt] System cannot operate without HRT. Halting.\n");

        // Blink LED rapidly to indicate failure
        while (1) {
            led_on(LED_RED);
            up_mdelay(100);
            led_off(LED_RED);
            up_mdelay(100);
        }
    }
}
```

---

## Section 3: Power & Reset

### 3.1 `board_peripheral_reset()` Comparison

#### FMU-6X Implementation (Full)

```cpp
__EXPORT void board_peripheral_reset(int ms)
{
    /* Disable all peripheral power rails */
    VDD_5V_PERIPH_EN(false);
    board_control_spi_sensors_power(false, 0xffff);
    VDD_3V3_SENSORS4_EN(false);

    /* Keep Spektrum power on to discharge rail */
    bool last = READ_VDD_3V3_SPEKTRUM_POWER_EN();
    VDD_3V3_SPEKTRUM_POWER_EN(false);

    /* Wait for rails to reach GND */
    usleep(ms * 1000);
    syslog(LOG_DEBUG, "reset done, %d ms\n", ms);

    /* Re-enable power rails */
    VDD_3V3_SPEKTRUM_POWER_EN(last);
    board_control_spi_sensors_power(true, 0xffff);
    VDD_3V3_SENSORS4_EN(true);
    VDD_5V_PERIPH_EN(true);
}
```

#### SAMV71 Implementation (Stub)

```c
__EXPORT void board_peripheral_reset(int ms)
{
    UNUSED(ms);
    // TODO: Implement sensor reset via mikroBUS RST pins
}
```

### 3.2 `board_on_reset()` Comparison

#### FMU-6X Implementation

```cpp
__EXPORT void board_on_reset(int status)
{
    // Set all PWM outputs to GPIO mode (low) to prevent motor spin
    for (int i = 0; i < DIRECT_PWM_OUTPUT_CHANNELS; ++i) {
        px4_arch_configgpio(io_timer_channel_get_gpio_output(i));
    }

    // On system reset (not boot), wait for ESCs to disarm
    if (status >= 0) {
        up_mdelay(100);
    }
}
```

#### SAMV71 Implementation (Stub)

```c
__EXPORT void board_on_reset(int status)
{
    /* No PWM channels configured yet for SAMV71 - TODO */
    (void)status;
}
```

### 3.3 Power Control GPIO Definitions

#### FMU-6X Power GPIOs

```cpp
// Power input monitoring
#define GPIO_nPOWER_IN_A          (GPIO_INPUT|GPIO_PULLUP|GPIO_PORTG|GPIO_PIN1)  // Brick 1
#define GPIO_nPOWER_IN_B          (GPIO_INPUT|GPIO_PULLUP|GPIO_PORTG|GPIO_PIN2)  // Brick 2
#define GPIO_nPOWER_IN_C          (GPIO_INPUT|GPIO_PULLUP|GPIO_PORTG|GPIO_PIN3)  // USB

// Power output control (active low enable)
#define GPIO_VDD_5V_PERIPH_nEN    (GPIO_OUTPUT|GPIO_PUSHPULL|GPIO_OUTPUT_SET|GPIO_PORTG|GPIO_PIN4)
#define GPIO_VDD_5V_HIPOWER_nEN   (GPIO_OUTPUT|GPIO_PUSHPULL|GPIO_OUTPUT_SET|GPIO_PORTG|GPIO_PIN10)
#define GPIO_VDD_3V3_SENSORS4_EN  (GPIO_OUTPUT|GPIO_PUSHPULL|GPIO_OUTPUT_CLEAR|GPIO_PORTG|GPIO_PIN8)
#define GPIO_VDD_3V3_SPEKTRUM_POWER_EN (GPIO_OUTPUT|GPIO_PUSHPULL|GPIO_OUTPUT_CLEAR|GPIO_PORTH|GPIO_PIN2)
#define GPIO_VDD_3V3_SD_CARD_EN   (GPIO_OUTPUT|GPIO_PUSHPULL|GPIO_OUTPUT_CLEAR|GPIO_PORTC|GPIO_PIN13)
#define GPIO_ETH_POWER_EN         (GPIO_OUTPUT|GPIO_PUSHPULL|GPIO_OUTPUT_CLEAR|GPIO_PORTG|GPIO_PIN15)

// Overcurrent detection
#define GPIO_VDD_5V_PERIPH_nOC    (GPIO_INPUT|GPIO_FLOAT|GPIO_PORTE|GPIO_PIN15)
#define GPIO_VDD_5V_HIPOWER_nOC   (GPIO_INPUT|GPIO_FLOAT|GPIO_PORTF|GPIO_PIN13)

// Control macros
#define VDD_5V_PERIPH_EN(on)      px4_arch_gpiowrite(GPIO_VDD_5V_PERIPH_nEN, !(on))
#define VDD_5V_HIPOWER_EN(on)     px4_arch_gpiowrite(GPIO_VDD_5V_HIPOWER_nEN, !(on))
#define VDD_3V3_SENSORS4_EN(on)   px4_arch_gpiowrite(GPIO_VDD_3V3_SENSORS4_EN, (on))
// ... etc
```

#### SAMV71 - No Power Control (Development Board)

The SAMV71-XULT is a development board with fixed power rails. Sensor power control would need external circuitry.

### 3.4 Recommended SAMV71 Implementation

```c
// board_config.h - Add mikroBUS reset control
#define GPIO_MB1_RST    (GPIO_OUTPUT|GPIO_OUTPUT_SET|GPIO_PORT_PIOA|GPIO_PIN19)
#define GPIO_MB2_RST    (GPIO_OUTPUT|GPIO_OUTPUT_SET|GPIO_PORT_PIOB|GPIO_PIN0)

#define MB1_RST_ASSERT()   px4_arch_gpiowrite(GPIO_MB1_RST, 0)
#define MB1_RST_RELEASE()  px4_arch_gpiowrite(GPIO_MB1_RST, 1)
#define MB2_RST_ASSERT()   px4_arch_gpiowrite(GPIO_MB2_RST, 0)
#define MB2_RST_RELEASE()  px4_arch_gpiowrite(GPIO_MB2_RST, 1)

// init.c - Implement peripheral reset
__EXPORT void board_peripheral_reset(int ms)
{
    syslog(LOG_INFO, "[reset] Resetting sensors for %d ms\n", ms);

    // Assert reset on all mikroBUS sensors
    MB1_RST_ASSERT();
    MB2_RST_ASSERT();

    // Wait for reset
    usleep(ms * 1000);

    // Release reset
    MB1_RST_RELEASE();
    MB2_RST_RELEASE();

    // Allow sensors to initialize
    usleep(10 * 1000);

    syslog(LOG_INFO, "[reset] Sensor reset complete\n");
}
```

---

## Section 4: Persistent Storage

### 4.1 Storage Architecture Comparison

| Aspect | FMU-6X | SAMV71-XULT |
|--------|--------|-------------|
| **Parameter Storage** | FRAM (SPI) | SD Card |
| **Calibration Storage** | EEPROM (I2C) | SD Card |
| **Available Flash** | FRAM 32KB + EEPROM 16KB | **QSPI 8MB** (unused) |
| **MTD Manifest** | Yes (`mtd.cpp`) | No |
| **Wear Leveling** | Hardware (FRAM) | FAT FS |

### 4.2 FMU-6X MTD Configuration (`mtd.cpp`)

```cpp
// Device definitions
static const px4_mft_device_t spi5 = {  // FM25V02A FRAM
    .bus_type = px4_mft_device_t::SPI,
    .devid    = SPIDEV_FLASH(0)
};

static const px4_mft_device_t i2c3 = {  // 24LC64T Base EEPROM
    .bus_type = px4_mft_device_t::I2C,
    .devid    = PX4_MK_I2C_DEVID(3, 0x51)
};

static const px4_mft_device_t i2c4 = {  // 24LC64T IMU EEPROM
    .bus_type = px4_mft_device_t::I2C,
    .devid    = PX4_MK_I2C_DEVID(4, 0x50)
};

// Partition layouts
static const px4_mtd_entry_t fmum_fram = {
    .device = &spi5,
    .npart = 1,
    .partd = {
        {
            .type = MTD_PARAMETERS,
            .path = "/fs/mtd_params",
            .nblocks = (32768 / (1 << CONFIG_RAMTRON_EMULATE_SECTOR_SHIFT))
        }
    },
};

static const px4_mtd_entry_t base_eeprom = {
    .device = &i2c3,
    .npart = 2,
    .partd = {
        {
            .type = MTD_MFT_VER,
            .path = "/fs/mtd_mft_ver",
            .nblocks = 248
        },
        {
            .type = MTD_NET,
            .path = "/fs/mtd_net",
            .nblocks = 8
        }
    },
};

static const px4_mtd_entry_t imu_eeprom = {
    .device = &i2c4,
    .npart = 3,
    .partd = {
        {
            .type = MTD_CALDATA,
            .path = "/fs/mtd_caldata",
            .nblocks = 240
        },
        {
            .type = MTD_MFT_REV,
            .path = "/fs/mtd_mft_rev",
            .nblocks = 8
        },
        {
            .type = MTD_ID,
            .path = "/fs/mtd_id",
            .nblocks = 8
        }
    },
};

// Manifest registration
static const px4_mtd_manifest_t board_mtd_config = {
    .nconfigs = 3,
    .entries = { &fmum_fram, &base_eeprom, &imu_eeprom }
};

const px4_mft_s *board_get_manifest(void) { return &mft; }
```

### 4.3 SAMV71 Current Status

```c
// board_config.h - Storage currently on SD card
// #define FLASH_BASED_PARAMS  // Commented out

// init.c
syslog(LOG_INFO, "[boot] Parameters will be stored on /fs/microsd/params\n");
```

### 4.4 SAMV71 QSPI Flash Implementation Plan

The SAMV71-XULT has an **SST26VF064B 8MB QSPI flash** that can provide better storage than FMU-6X's FRAM+EEPROM.

```cpp
// Proposed mtd.cpp for SAMV71

#include <nuttx/config.h>
#include <board_config.h>
#include <px4_platform_common/px4_manifest.h>

// QSPI Flash device (SST26VF064B)
static const px4_mft_device_t qspi0 = {
    .bus_type = px4_mft_device_t::SPI,  // QSPI treated as SPI
    .devid    = SPIDEV_FLASH(0)
};

// Partition layout for 8MB flash
static const px4_mtd_entry_t qspi_flash = {
    .device = &qspi0,
    .npart = 3,
    .partd = {
        {
            .type = MTD_PARAMETERS,
            .path = "/fs/mtd_params",
            .nblocks = 512      // 64KB for parameters
        },
        {
            .type = MTD_CALDATA,
            .path = "/fs/mtd_caldata",
            .nblocks = 512      // 64KB for calibration
        },
        {
            .type = MTD_WAYPOINTS,
            .path = "/fs/mtd_waypoints",
            .nblocks = 16384    // 2MB for missions
        }
    },
};

static const px4_mtd_manifest_t board_mtd_config = {
    .nconfigs = 1,
    .entries = { &qspi_flash }
};

static const px4_mft_entry_s mtd_mft = {
    .type = MTD,
    .pmft = (void *)&board_mtd_config,
};

static const px4_mft_s mft = {
    .nmft = 1,
    .mfts = { &mtd_mft }
};

const px4_mft_s *board_get_manifest(void)
{
    return &mft;
}
```

### 4.5 Storage Capacity Comparison

| Storage Type | FMU-6X | SAMV71 (Planned) |
|--------------|--------|------------------|
| Parameters | 32KB (FRAM) | 64KB (QSPI) |
| Calibration | 7.5KB (EEPROM) | 64KB (QSPI) |
| Missions | SD Card | 2MB (QSPI) + SD |
| Logs | SD Card | SD Card |
| **Total Flash** | 48KB | **8MB** |
| **Advantage** | - | SAMV71 250x more |

---

## Section 5: Communications Buses

### 5.1 SPI Bus Configuration

#### FMU-6X SPI Architecture

```cpp
// 6 SPI buses with hardware version detection
constexpr px4_spi_bus_all_hw_t px4_spi_buses_all_hw[7] = {
    initSPIFmumID(V6X_0, {
        initSPIBus(SPI::Bus::SPI1, {  // IMU 1
            initSPIDevice(DRV_IMU_DEVTYPE_ICM20649,
                         SPI::CS{GPIO::PortI, GPIO::Pin9},
                         SPI::DRDY{GPIO::PortF, GPIO::Pin2}),
        }, {GPIO::PortI, GPIO::Pin11}),  // Power enable

        initSPIBus(SPI::Bus::SPI2, {  // IMU 2
            initSPIDevice(DRV_IMU_DEVTYPE_ICM42688P,
                         SPI::CS{GPIO::PortH, GPIO::Pin5},
                         SPI::DRDY{GPIO::PortA, GPIO::Pin10}),
        }, {GPIO::PortF, GPIO::Pin4}),

        initSPIBus(SPI::Bus::SPI3, {  // IMU 3 (BMI088 Gyro + Accel)
            initSPIDevice(DRV_GYR_DEVTYPE_BMI088,
                         SPI::CS{GPIO::PortI, GPIO::Pin8},
                         SPI::DRDY{GPIO::PortI, GPIO::Pin7}),
            initSPIDevice(DRV_ACC_DEVTYPE_BMI088,
                         SPI::CS{GPIO::PortI, GPIO::Pin4},
                         SPI::DRDY{GPIO::PortI, GPIO::Pin6}),
        }, {GPIO::PortE, GPIO::Pin7}),

        initSPIBus(SPI::Bus::SPI5, {  // FRAM
            initSPIDevice(SPIDEV_FLASH(0), SPI::CS{GPIO::PortG, GPIO::Pin7})
        }),

        initSPIBusExternal(SPI::Bus::SPI6, {  // External sensors
            initSPIConfigExternal(SPI::CS{GPIO::PortI, GPIO::Pin10},
                                 SPI::DRDY{GPIO::PortD, GPIO::Pin11}),
            initSPIConfigExternal(SPI::CS{GPIO::PortA, GPIO::Pin15},
                                 SPI::DRDY{GPIO::PortD, GPIO::Pin12}),
        }),
    }),
    // ... 6 more hardware versions with different sensor configs
};
```

#### SAMV71 SPI Architecture

```cpp
// Single SPI bus with one sensor
constexpr px4_spi_bus_t px4_spi_buses[SPI_BUS_MAX_BUS_ITEMS] = {
    {
        .devices = {
            make_spidev(DRV_IMU_DEVTYPE_ICM20689,
                       GPIO_SPI0_CS_ICM20689,
                       GPIO_SPI0_DRDY_ICM20689),
        },
        .power_enable_gpio = 0,  // No power control
        .bus = static_cast<int8_t>(SPI::Bus::SPI0),
        .is_external = false,
        .requires_locking = false,
    },
};
```

### 5.2 I2C Bus Configuration

| Aspect | FMU-6X | SAMV71-XULT |
|--------|--------|-------------|
| **Number of buses** | 4 (I2C1-4) | 1 (TWIHS0) |
| **Internal bus** | I2C1 | TWIHS0 |
| **External bus** | I2C2, I2C3 | None dedicated |
| **MTD EEPROM bus** | I2C3, I2C4 | N/A |
| **Speed** | 400kHz | 400kHz |

#### FMU-6X I2C Setup

```cpp
// i2c.cpp
constexpr px4_i2c_bus_t px4_i2c_buses[I2C_BUS_MAX_BUS_ITEMS] = {
    initI2CBusInternal(I2C::Bus::I2C1, 3),   // Internal sensors, 3 devices
    initI2CBusExternal(I2C::Bus::I2C2, 1),   // External, 1 slot
    initI2CBusExternal(I2C::Bus::I2C3, 2),   // External, 2 slots
    initI2CBusMTD(I2C::Bus::I2C4, 2),        // MTD EEPROM
};
```

#### SAMV71 I2C Setup

```c
// init.c - Manual initialization
struct i2c_master_s *i2c0 = sam_i2cbus_initialize(0);
i2c_register(i2c0, 0);  // /dev/i2c0
```

### 5.3 Recommended SAMV71 Improvements

```cpp
// spi.cpp - Add mikroBUS socket 2 support
constexpr px4_spi_bus_t px4_spi_buses[SPI_BUS_MAX_BUS_ITEMS] = {
    {
        .devices = {
            // Socket 1: ICM-20689 (primary IMU)
            make_spidev(DRV_IMU_DEVTYPE_ICM20689,
                       GPIO_SPI0_CS_ICM20689,
                       GPIO_SPI0_DRDY_ICM20689),
            // Socket 2: Future expansion (e.g., ICM-45686)
            // make_spidev(DRV_IMU_DEVTYPE_ICM45686, GPIO_SPI0_CS_MB2, GPIO_MB2_INT),
        },
        .power_enable_gpio = 0,
        .bus = static_cast<int8_t>(SPI::Bus::SPI0),
        .is_external = false,
        .requires_locking = true,  // Enable if multiple devices
    },
};

// i2c.cpp - Define I2C bus properly
constexpr px4_i2c_bus_t px4_i2c_buses[I2C_BUS_MAX_BUS_ITEMS] = {
    {
        .bus = 0,
        .is_external = false,
        .devid_start = PX4_I2C_DEVID_BUS(0),
    },
};
```

---

## Section 6: Sensors & Drivers

### 6.1 Sensor Driver Comparison

| Sensor Type | FMU-6X Drivers | SAMV71 Drivers | Gap |
|-------------|----------------|----------------|-----|
| **IMU** | ICM20602, ICM20649, ICM20948, ICM42670P, ICM42688P, ICM45686, IIM42652, BMI088, ADIS16470 | ICM20689 only | ⚠️ Limited |
| **Magnetometer** | All (via COMMON_MAGNETOMETER) | AK09916 only | ⚠️ Limited |
| **Barometer** | BMP388, ICP201XX, MS5611 | DPS310 only | ⚠️ Limited |
| **GPS** | All + Septentrio RTK | Standard GPS | ⚠️ Limited |
| **Power Monitor** | INA226, INA228, INA238 | None | ❌ Gap |
| **Airspeed** | All differential pressure | None | ⚠️ MC only |

### 6.2 Sensor Start Scripts

#### FMU-6X `rc.board_sensors`

```bash
# Auto-detection via SPI bus manifest
# Sensors start automatically based on px4_spi_buses_all_hw configuration
```

#### SAMV71 `rc.board_sensors` (Actual Current State)

```bash
#!/bin/sh
# SAMV71-XULT with Click sensor boards - sensors init

# Start I2C sensors on bus 0 (TWIHS0)

# ICM-20689 IMU - NOTE: Requires SPI configuration (not yet implemented)
# TODO: Configure SPI buses in spi.cpp before enabling this
# icm20689 start -s -b 0    # <-- COMMENTED OUT (SPI not ready)

# AK09916 Magnetometer on Arduino headers (I2C address 0x0C)
# Note: Using AK09916 driver which is compatible with AK09915
ak09916 start -I -b 0       # <-- ENABLED (I2C)

# DPS310 Barometer on mikroBUS Socket 2 (I2C address 0x77)
dps310 start -I -b 0        # <-- ENABLED (I2C)
```

> **Current Sensor Status**:
> - **ICM-20689 (SPI IMU)**: ❌ Disabled - SPI driver configured in `spi.cpp` but start command
>   is commented out pending full SPI bus validation
> - **AK09916 (I2C Mag)**: ✅ Enabled - Uses AK09916 driver (compatible with AK09915 on Compass 4 Click)
> - **DPS310 (I2C Baro)**: ✅ Enabled - Works on I2C bus 0

### 6.3 Available mikroBUS Click Boards (Detailed Pin Mapping)

| MIKROE PID | Click Board | Sensor IC(s) | Interface | INT | RST | CS | Driver Status |
|------------|-------------|--------------|-----------|-----|-----|-----|---------------|
| MIKROE-4044 | 6DOF IMU 6 | **ICM-20689** | I2C / SPI | INT | NC | CS | ⚠️ SPI configured, start commented |
| MIKROE-4231 | Compass 4 | **AK09915** | I2C / SPI | DRY | RST | CS | ✅ Enabled (via AK09916 driver) |
| MIKROE-2293 | Pressure 3 | **DPS310** | I2C / SPI | INT | NC | CS | ✅ Enabled |
| MIKROE-3775 | 13DOF | **BME680 + BMM150 + BMI088** | **I2C only** | NC | NC | NC | ⚠️ Partial (drivers exist) |
| MIKROE-2935 | GeoMagnetic | **BMM150** | I2C / SPI | INT | NC | CS | ❌ Not enabled |
| MIKROE-3566 | Pressure 5 | **BMP388** | I2C / SPI | INT | NC | CS | ❌ Not enabled |
| MIKROE-6514 | 6DOF IMU 27 | **ICM-45686** | I2C / I3C / SPI | IT1 | ID SEL | SPI SEL | ❌ Not enabled |

#### Expected Click Board Population

For initial flight testing, the recommended minimum sensor stack:

| Socket | Click Board | Sensor | Role | Priority |
|--------|-------------|--------|------|----------|
| Socket 1 | 6DOF IMU 6 | ICM-20689 | Primary IMU | 🔴 Required |
| Socket 2 | Pressure 3 | DPS310 | Barometer | 🔴 Required |
| Arduino I2C | Compass 4 | AK09915 | Magnetometer | 🟠 Recommended |

#### Full Click Board Pin Mapping Reference

```
| Pin | Socket 1 GPIO | Socket 2 GPIO | Click Function |
|-----|---------------|---------------|----------------|
| AN  | -             | -             | Analog (NC on most sensors) |
| RST | PA19          | PB0           | Reset (active low) |
| CS  | PD25          | -             | SPI Chip Select |
| SCK | PD22 (SPI0)   | PD22 (SPI0)   | SPI Clock (shared) |
| MISO| PD20 (SPI0)   | PD20 (SPI0)   | SPI Data Out (shared) |
| MOSI| PD21 (SPI0)   | PD21 (SPI0)   | SPI Data In (shared) |
| PWM | PC19          | PB1           | PWM output |
| INT | PA0           | PA6           | Interrupt input |
| RX  | -             | -             | UART RX (NC on sensors) |
| TX  | -             | -             | UART TX (NC on sensors) |
| SCL | PA4 (TWIHS0)  | PA4 (TWIHS0)  | I2C Clock (shared) |
| SDA | PA3 (TWIHS0)  | PA3 (TWIHS0)  | I2C Data (shared) |
```

> **Note**: SPI0 and TWIHS0 are shared between both mikroBUS sockets. Socket differentiation
> is via separate CS pins (SPI) or different I2C addresses.

---

## Section 7: Actuators & IO (CRITICAL)

### 7.1 Timer Configuration Comparison

#### FMU-6X `timer_config.cpp` (Complete Implementation)

```cpp
/* Timer allocation:
 * TIM5_CH4  FMU_CH1    TIM4_CH2  FMU_CH5
 * TIM5_CH3  FMU_CH2    TIM4_CH3  FMU_CH6
 * TIM5_CH2  FMU_CH3    TIM12_CH1 FMU_CH7
 * TIM5_CH1  FMU_CH4    TIM12_CH2 FMU_CH8
 * TIM1_CH2  FMU_CAP1 (capture)
 */

constexpr io_timers_t io_timers[MAX_IO_TIMERS] = {
    initIOTimer(Timer::Timer5, DMA{DMA::Index1}),   // PWM CH1-4
    initIOTimer(Timer::Timer4, DMA{DMA::Index1}),   // PWM CH5-6
    initIOTimer(Timer::Timer12),                     // PWM CH7-8
    initIOTimer(Timer::Timer1),                      // Capture
    initIOTimer(Timer::Timer2),                      // Heater PWM
};

constexpr timer_io_channels_t timer_io_channels[MAX_TIMER_IO_CHANNELS] = {
    initIOTimerChannel(io_timers, {Timer::Timer5, Timer::Channel4}, {GPIO::PortI, GPIO::Pin0}),   // CH1
    initIOTimerChannel(io_timers, {Timer::Timer5, Timer::Channel3}, {GPIO::PortH, GPIO::Pin12}),  // CH2
    initIOTimerChannel(io_timers, {Timer::Timer5, Timer::Channel2}, {GPIO::PortH, GPIO::Pin11}),  // CH3
    initIOTimerChannel(io_timers, {Timer::Timer5, Timer::Channel1}, {GPIO::PortH, GPIO::Pin10}),  // CH4
    initIOTimerChannel(io_timers, {Timer::Timer4, Timer::Channel2}, {GPIO::PortD, GPIO::Pin13}),  // CH5
    initIOTimerChannel(io_timers, {Timer::Timer4, Timer::Channel3}, {GPIO::PortD, GPIO::Pin14}),  // CH6
    initIOTimerChannel(io_timers, {Timer::Timer12, Timer::Channel1}, {GPIO::PortH, GPIO::Pin6}),  // CH7
    initIOTimerChannel(io_timers, {Timer::Timer12, Timer::Channel2}, {GPIO::PortH, GPIO::Pin9}),  // CH8
    initIOTimerChannelCapture(io_timers, {Timer::Timer1, Timer::Channel2}, {GPIO::PortE, GPIO::Pin11}),
};

constexpr io_timers_channel_mapping_t io_timers_channel_mapping =
    initIOTimerChannelMapping(io_timers, timer_io_channels);
```

#### SAMV71 `timer_config.cpp` (Empty Stub - CRITICAL GAP)

```cpp
/* STUB - PWM outputs will NOT function!
 * TODO: Map TC0 timer channels to GPIO pins based on hardware
 */

constexpr io_timers_t io_timers[MAX_IO_TIMERS] = {
    // EMPTY - No timers configured
};

constexpr timer_io_channels_t timer_io_channels[MAX_TIMER_IO_CHANNELS] = {
    // EMPTY - No channels configured
};
```

### 7.2 SAMV71 Timer Resources

The SAMV71 has different timer architecture than STM32:

| Timer | Channels | PWM Capable | PX4 io_timer Support | Notes |
|-------|----------|-------------|---------------------|-------|
| TC0 | 3 | Yes | ❌ Reserved for HRT | Used for high-resolution timer |
| TC1 | 3 | Yes | ⚠️ Partial | Available for PWM |
| TC2 | 3 | Yes | ⚠️ Partial | Available for PWM |
| TC3 | 3 | Yes | ⚠️ Partial | Available for PWM |
| PWM0 | 4 | Yes | ❌ **Not implemented** | Dedicated PWM peripheral |
| PWM1 | 4 | Yes | ❌ **Not implemented** | Dedicated PWM peripheral |

> **CRITICAL ARCHITECTURAL GAP**: The SAMV7 PX4 io_timer layer currently only supports TC-based
> timers. The `io_timer_hw_description.h` defines:
> - `MAX_IO_TIMERS = 3` (TC peripherals only)
> - `MAX_TIMER_IO_CHANNELS = 9` (3 timers × 3 channels)
> - NO `Timer::PWM0` or `Timer::PWM1` enumerations
> - NO PWM peripheral init helpers, clock gating, or base address mappings
>
> See: `platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/io_timer_hw_description.h`

### 7.3 Architectural Work Required Before Board-Level Timer Config

Before a board-level `timer_config.cpp` can use PWM peripherals, the following must be
implemented in the SAMV7 architecture layer:

#### Option A: Extend io_timer for PWM0/PWM1 (Preferred for Motor Output)

```cpp
// Required additions to platforms/nuttx/src/px4/microchip/samv7/include/px4_arch/

// 1. Add Timer enum values for PWM peripherals
namespace Timer {
    enum Timer : uint8_t {
        TC0 = 0,
        TC1 = 1,
        TC2 = 2,
        TC3 = 3,
        PWM0 = 4,  // NEW: PWM peripheral 0 (4 channels)
        PWM1 = 5,  // NEW: PWM peripheral 1 (4 channels)
    };
}

// 2. Add clock gating for PWM peripherals
// In sam_pmc.h or io_timer.c:
#define SAM_PID_PWM0  31  // PWM0 peripheral ID
#define SAM_PID_PWM1  60  // PWM1 peripheral ID

// 3. Add base addresses
#define SAM_PWM0_BASE  0x40020000
#define SAM_PWM1_BASE  0x4005C000

// 4. Implement initIOTimer() for PWM peripherals
// This requires understanding PWM register layout vs TC register layout

// 5. Implement channel mapping and interrupt handling for PWM
```

**Effort Estimate**: 16-24 hours (significant arch work)

#### Option B: Use TC1/TC2/TC3 for PWM (Simpler, Limited Channels)

```cpp
// timer_config.cpp - TC-only implementation (works with current layer)
// Limited to 9 channels max (3 timers × 3 channels), but TC0 reserved for HRT

#include <px4_arch/io_timer_hw_description.h>

/* SAMV71-XULT PWM via TC1/TC2
 * TC0 is reserved for HRT
 * TC1, TC2 available for PWM output
 * Maximum 6 channels (2 timers × 3 channels)
 *
 * Pin mapping TBD based on SAMV71-XULT schematic:
 * - Check which TC outputs are routed to accessible headers
 * - TC1 and TC2 TIOA/TIOB pins must be verified
 */

// NOTE: This is a PLACEHOLDER - actual GPIO pins must be verified from schematic
// TC outputs require specific pin mux configuration via PIO controller

constexpr io_timers_t io_timers[MAX_IO_TIMERS] = {
    // TC0 reserved for HRT - do not use
    // initIOTimer(Timer::TC1),  // PWM channels 1-3
    // initIOTimer(Timer::TC2),  // PWM channels 4-6
};

constexpr timer_io_channels_t timer_io_channels[MAX_TIMER_IO_CHANNELS] = {
    // TODO: Map TC1/TC2 channels to GPIO pins based on:
    // 1. SAMV71-XULT schematic (which TC outputs are routed to headers)
    // 2. PIO mux configuration for TC alternate functions
    // 3. Available pins on Arduino/mikroBUS headers
};
```

**Effort Estimate**: 8-12 hours (uses existing layer)

### 7.4 Recommended Approach

Given the current state:

1. **Short-term (HITL)**: Continue using `pwm_out_sim` - no hardware PWM needed
2. **Medium-term (First Flight)**: Implement Option B (TC-based PWM) - 6 channels sufficient for quad
3. **Long-term (Production)**: Implement Option A (PWM0/PWM1) - full 8+ channel support with DShot

### 7.5 Pin Mapping Investigation Required

Before implementing either option, verify from SAMV71-XULT schematic:

| Signal | TC Function | Possible Pins | Header | Status |
|--------|-------------|---------------|--------|--------|
| TIOA1 | TC1 Ch0 output | PA2 | ? | TBD |
| TIOB1 | TC1 Ch0 capture | PA3 | ? | TBD |
| TIOA2 | TC1 Ch1 output | PA5 | ? | TBD |
| TIOB2 | TC1 Ch1 capture | PA6 | ? | TBD |
| ... | ... | ... | ... | TBD |

mikroBUS PWM pins (directly usable if PWM peripheral support added):
- PC19 - mikroBUS Socket 1 PWM
- PB1  - mikroBUS Socket 2 PWM

### 7.6 PWM Driver Status

| Feature | FMU-6X | SAMV71 | Gap |
|---------|--------|--------|-----|
| `DIRECT_PWM_OUTPUT_CHANNELS` | 9 | 6 (defined, not working) | ❌ Critical |
| PWM_OUT driver | Enabled | Disabled | ❌ Critical |
| DShot driver | Enabled | Disabled | ❌ Gap |
| Input capture | TIM1_CH2 | None | ❌ Gap |
| `pwm info` command | Works | Empty | ❌ Critical |

---

## Section 8: Networking / Telemetry

### 8.1 MAVLink Configuration

#### FMU-6X `rc.board_defaults`

```bash
# Ethernet MAVLink (primary)
param set-default MAV_2_CONFIG 1000     # Ethernet
param set-default MAV_2_BROADCAST 1
param set-default MAV_2_MODE 0          # Normal
param set-default MAV_2_RATE 100000     # 100 KB/s
param set-default MAV_2_UDP_PRT 14550

# Safety button
safety_button start
```

#### SAMV71 `rc.board_defaults`

```bash
# USB CDC MAVLink (primary)
param set SYS_AUTOSTART 4001            # Generic quad
param set-default SYS_USB_AUTO 2
param set-default USB_MAV_MODE 0
param set-default MAV_0_CONFIG 101      # USB CDC
param set-default MAV_0_RATE 57600

# HITL configuration
param set-default CBRK_SUPPLY_CHK 894281
param set-default COM_ARM_WO_GPS 1
param set-default SYS_HAS_MAG 0
param set-default SYS_HAS_BARO 0
param set-default CBRK_FLIGHTTERM 121212

# HITL actuator mapping
param set-default HIL_ACT_FUNC1 101
param set-default HIL_ACT_FUNC2 102
param set-default HIL_ACT_FUNC3 103
param set-default HIL_ACT_FUNC4 104
```

### 8.2 Ethernet Configuration

| Aspect | FMU-6X | SAMV71-XULT |
|--------|--------|-------------|
| MAC Controller | STM32H7 ETH | SAMV71 GMAC |
| PHY | LAN8742A | KSZ8081 |
| Connector | RJ45 | RJ45 |
| NuttX Driver | `CONFIG_STM32H7_ETHMAC` | `CONFIG_SAMV7_EMAC` |
| PX4 Status | ✅ Enabled | ❌ Not configured |

> **SAMV71 Ethernet Capability**: The hardware is fully present and functional. To enable:
> ```bash
> # In nuttx-config/nsh/defconfig:
> CONFIG_SAMV7_EMAC=y
> CONFIG_SAMV7_EMAC0=y
> CONFIG_NET=y
> CONFIG_NET_ETHERNET=y
> CONFIG_NET_UDP=y
> CONFIG_NET_TCP=y
> CONFIG_NETUTILS_NETLIB=y
>
> # In rc.board_defaults:
> param set-default MAV_2_CONFIG 1000
> param set-default MAV_2_BROADCAST 1
> param set-default MAV_2_UDP_PRT 14550
> ```

### 8.3 CAN Bus Configuration

| Aspect | FMU-6X | SAMV71 |
|--------|--------|--------|
| CAN interfaces | 2 (FDCAN1, FDCAN2) | 2 (MCAN0, MCAN1) available |
| UAVCAN driver | Enabled | Disabled |
| Transceiver GPIO | Defined | Not configured |

### 8.4 Serial Port Mapping

| Port | FMU-6X | SAMV71 |
|------|--------|--------|
| GPS1 | /dev/ttyS0 | /dev/ttyS2 |
| TEL1 | /dev/ttyS6 | /dev/ttyACM0 (USB) |
| TEL2 | /dev/ttyS4 | /dev/ttyS1 |
| RC | /dev/ttyS5 | /dev/ttyS4 |
| Console | USB/UART | USB CDC |

---

## Section 9: PX4 Services & Modules

### 9.1 System Commands Comparison

| Command | FMU-6X | SAMV71 | Notes |
|---------|--------|--------|-------|
| `dmesg` | ✅ Enabled | ❌ Disabled | Console buffer bug |
| `hardfault_log` | ✅ Enabled | ❌ Disabled | Needs PROGMEM fix |
| `top` | ✅ Enabled | ✅ Enabled | Working |
| `param` | ✅ Enabled | ✅ Enabled | Working |
| `reboot` | ✅ Enabled | ✅ Enabled | Working |
| `i2cdetect` | ✅ Enabled | ⚠️ Could enable | Optional |
| `spi` | ✅ Enabled | ✅ Enabled | Working |
| `perf` | ✅ Enabled | ⚠️ Could enable | Optional |
| `work_queue` | ✅ Enabled | ⚠️ Could enable | Optional |

### 9.2 Console Buffer Fix Required

```c
// Current issue in console_buffer.cpp
class ConsoleBuffer {
    // ...
    px4_sem_t _lock = SEM_INITIALIZER(1);  // Static init fails on SAMV7!
};

// Fix: Use manual construction
class ConsoleBuffer {
    px4_sem_t _lock;
public:
    ConsoleBuffer() {
        px4_sem_init(&_lock, 0, 1);  // Runtime initialization
    }
};
```

### 9.3 Work Queue / BlockingList Static Initialization Issue

**Problem**: The `pwm_out_sim` module (and potentially other modules) crashed on SAMV7 when
calling `updateSubscriptions()`. Root cause is `PTHREAD_MUTEX_INITIALIZER` in `BlockingList`
not being properly initialized before use.

**The `wq:rate_ctrl` Work Queue**:

`MixingOutput::updateSubscriptions()` is called by actuator output modules to potentially switch
to the high-priority `wq:rate_ctrl` work queue for low-latency motor control. This is defined in
`WorkQueueManager.hpp`:

```cpp
static constexpr wq_config_t rate_ctrl{
    "wq:rate_ctrl",
    CONFIG_WQ_RATE_CTRL_STACKSIZE,
    CONFIG_WQ_RATE_CTRL_PRIORITY  // High priority for motor timing
};
```

When motor outputs (Motor1-MotorMax) are configured, `updateSubscriptions()` calls:
```cpp
if (allow_wq_switch && switch_requested) {
    _interface.ChangeWorkQueue(px4::wq_configurations::rate_ctrl);
}
```

**The `wq:manager` Task (WorkQueueManager)**:

The work queue manager is the central task that creates and manages all work queues in PX4.
It is started during platform initialization and runs for the lifetime of the system.

**Location**: `platforms/common/px4_work_queue/WorkQueueManager.cpp`

**Startup Chain**:
```
board_app_initialize()                    [init.c:267]
    │
    └── px4_platform_init()               [px4_init.cpp:205]
        │
        └── px4::WorkQueueManagerStart()  [WorkQueueManager.cpp:377]
            │
            └── px4_task_spawn_cmd("wq:manager", ...)  [line 398]
                │
                └── WorkQueueManagerRun()  [line 259-374]
                    │
                    ├── new BlockingList<WorkQueue *>()  ← _wq_manager_wqs_list
                    ├── new BlockingQueue<wq_config_t*>() ← _wq_manager_create_queue
                    │
                    └── Main loop: waits for work queue creation requests
                        │
                        └── When request received → pthread_create(WorkQueueRunner)
                            │
                            └── Creates individual work queues like:
                                • wq:rate_ctrl
                                • wq:SPI0, wq:I2C0
                                • wq:hp_default
                                • wq:lp_default
                                • etc.
```

**Key Data Structures**:

| Variable | Type | Purpose |
|----------|------|---------|
| `_wq_manager_wqs_list` | `BlockingList<WorkQueue *> *` | List of all active work queues |
| `_wq_manager_create_queue` | `BlockingQueue<wq_config_t *, 1> *` | Queue for work queue creation requests |
| `_wq_manager_running` | `atomic_bool` | Manager running state |
| `_wq_manager_should_exit` | `atomic_bool` | Shutdown flag |

**What Was Fixed vs What's Not Fixed**:

| Component | Status | Issue | Fix Applied |
|-----------|--------|-------|-------------|
| **WorkQueueManager** | ✅ FIXED | `atomic_bool` static init | Explicit `.store()` on first call |
| **BlockingList** | ❌ NOT FIXED | `PTHREAD_MUTEX_INITIALIZER` | Workaround: skip `updateSubscriptions()` |

The WorkQueueManager itself was fixed for the `atomic_bool` static initialization issue:
```cpp
// WorkQueueManager.cpp - FIXED with explicit init:
if (!_wq_manager_initialized.load()) {
    _wq_manager_should_exit.store(true);
    _wq_manager_running.store(false);
    _wq_manager_initialized.store(true);
}
```

However, `BlockingList` (used by the manager and work queues) still has the broken
`PTHREAD_MUTEX_INITIALIZER` issue. The `wq:manager` task **starts successfully**, but crashes
happen **later** when modules try to switch work queues.

**Detailed Crash Call Tree**:

```
PWMSim::Run()                                           [PWMSim.cpp:614-629]
│
├── _mixing_output.update()                             [line 616]
│
└── _mixing_output.updateSubscriptions(true)            [line 625 - SKIPPED ON SAMV7]
    │
    └── MixingOutput::updateSubscriptions()             [mixer_module.cpp:218-257]
        │
        │   [Checks if any output has Motor1-MotorMax function assigned]
        │   [If motors configured, triggers work queue switch:]
        │
        └── _interface.ChangeWorkQueue(rate_ctrl)       [line 249]
            │
            └── WorkItem::ChangeWorkQueue()             [WorkItem.hpp:82]
                │
                └── WorkItem::Init(config)              [WorkItem.cpp:68-83]
                    │
                    ├── Deinit()                        [line 71]
                    │
                    └── WorkQueueFindOrCreate(config)   [line 73]
                        │
                        └── FindWorkQueueByName()       [WorkQueueManager.cpp:69-87]
                            │
                            └── LockGuard lg{_wq_manager_wqs_list->mutex()}  [line 77]
                                │
                                └── BlockingList::mutex()    [BlockingList.hpp:83]
                                    │
                                    └── Returns reference to _mutex
                                        │
                                        └── LockGuard constructor calls:
                                            pthread_mutex_lock(&_mutex)
                                            │
                                            ╔════════════════════════════════════════╗
                                            ║  💥 CRASH: _mutex NOT INITIALIZED!     ║
                                            ║                                        ║
                                            ║  pthread_mutex_t _mutex =              ║
                                            ║    PTHREAD_MUTEX_INITIALIZER;          ║
                                            ║                                        ║
                                            ║  On SAMV7, this static initializer     ║
                                            ║  doesn't execute properly for C++      ║
                                            ║  class members before pthread init.    ║
                                            ╚════════════════════════════════════════╝
```

**Key Code Locations in Call Tree**:

| Step | File | Line | Function |
|------|------|------|----------|
| 1 | `PWMSim.cpp` | 625 | `_mixing_output.updateSubscriptions(true)` |
| 2 | `mixer_module.cpp` | 249 | `_interface.ChangeWorkQueue(px4::wq_configurations::rate_ctrl)` |
| 3 | `WorkItem.hpp` | 82 | `ChangeWorkQueue() → Init(config)` |
| 4 | `WorkItem.cpp` | 73 | `WorkQueueFindOrCreate(config)` |
| 5 | `WorkQueueManager.cpp` | 77 | `LockGuard lg{_wq_manager_wqs_list->mutex()}` |
| 6 | `BlockingList.hpp` | 87 | `pthread_mutex_t _mutex = PTHREAD_MUTEX_INITIALIZER` ← **Root Cause** |

**Root Cause** (`platforms/common/include/px4_platform_common/px4_work_queue/BlockingList.hpp`):
```cpp
class BlockingList {
    pthread_mutex_t _mutex = PTHREAD_MUTEX_INITIALIZER;  // Static init fails on SAMV7!
    pthread_cond_t _cv = PTHREAD_COND_INITIALIZER;       // Same issue
};
```

On SAMV7, `PTHREAD_MUTEX_INITIALIZER` is a struct initializer that doesn't execute properly
for C++ static/global objects before `main()`. The mutex remains uninitialized (garbage values),
causing a hard fault when `pthread_mutex_lock()` is called.

**Current Workaround** (`src/modules/simulation/pwm_out_sim/PWMSim.cpp:618-626`):
```cpp
/* NOTE: updateSubscriptions() is skipped for SAMV7 due to static initialization
 * issues with PTHREAD_MUTEX_INITIALIZER in BlockingList. The mutex isn't properly
 * initialized, causing a crash when work queue switching is attempted.
 */
#if !defined(CONFIG_ARCH_CHIP_SAMV7)
    _mixing_output.updateSubscriptions(true);
#endif
```

**Impact of Workaround**:
- ✅ `pwm_out_sim` no longer crashes
- ⚠️ Module stays on default work queue instead of switching to `wq:rate_ctrl`
- ⚠️ For HITL simulation this is acceptable (no real motor timing requirements)
- ❌ For real PWM output, motor control timing may be affected (lower priority work queue)
- ⚠️ When real PWM is implemented, the BlockingList fix becomes more critical

**Proper Fix Required**:
Same pattern as console buffer - use runtime initialization:

```cpp
class BlockingList {
    pthread_mutex_t _mutex;
    pthread_cond_t _cv;
    bool _initialized{false};

public:
    BlockingList() {
        pthread_mutex_init(&_mutex, nullptr);
        pthread_cond_init(&_cv, nullptr);
        _initialized = true;
    }
};
```

**Affected Modules** (potentially any module using `SubscriptionBlocking` or `BlockingList`):
- `pwm_out_sim` - Workaround applied
- `dataman` - May be affected
- `logger` - May be affected
- Any module doing dynamic work queue subscription changes

### 9.4 Summary of SAMV7 C++ Static Initialization Issues

| Component | Initializer | Status | Workaround |
|-----------|-------------|--------|------------|
| Console Buffer | `SEM_INITIALIZER(1)` | ❌ Broken | Disabled `BOARD_ENABLE_CONSOLE_BUFFER` |
| BlockingList | `PTHREAD_MUTEX_INITIALIZER` | ❌ Broken | Skip `updateSubscriptions()` on SAMV7 |
| BlockingList | `PTHREAD_COND_INITIALIZER` | ❌ Broken | Same as above |
| Parameters | Various static objects | ✅ Fixed | Lazy allocation (see `SAMV7_PARAM_STORAGE_FIX.md`) |

> **Architecture Note**: This is a fundamental issue with how NuttX/SAMV7 handles C++ static
> initialization. The `.init_array` section may not execute constructors in the expected order,
> or POSIX initializer macros may expand to values that require runtime fixup. A comprehensive
> fix would involve auditing all PX4 code for static POSIX primitive initialization.

---

## Section 10: Safety / Failsafe

### 10.1 Safety Hardware Comparison

| Feature | FMU-6X | SAMV71 |
|---------|--------|--------|
| Safety switch GPIO | PF5 (input) | Not defined |
| Safety LED GPIO | PD10 (output) | Not defined |
| nARMED signal | PE6 | Not defined |
| Tone alarm | TIM14_CH1 (PF9) | Not defined |
| ADC battery voltage | Multiple channels | Placeholder only |
| ADC battery current | Multiple channels | Placeholder only |

### 10.2 FMU-6X Safety GPIO Definitions

```cpp
// Safety switch
#define GPIO_nSAFETY_SWITCH_LED_OUT_INIT  (GPIO_INPUT|GPIO_FLOAT|GPIO_PORTD|GPIO_PIN10)
#define GPIO_nSAFETY_SWITCH_LED_OUT       (GPIO_OUTPUT|GPIO_PUSHPULL|GPIO_OUTPUT_SET|GPIO_PORTD|GPIO_PIN10)
#define GPIO_SAFETY_SWITCH_IN             (GPIO_INPUT|GPIO_PULLUP|GPIO_PORTF|GPIO_PIN5)
#define GPIO_LED_SAFETY                   GPIO_nSAFETY_SWITCH_LED_OUT
#define GPIO_BTN_SAFETY                   GPIO_SAFETY_SWITCH_IN

// Armed indicator
#define GPIO_nARMED_INIT  (GPIO_INPUT|GPIO_PULLUP|GPIO_PORTE|GPIO_PIN6)
#define GPIO_nARMED       (GPIO_OUTPUT|GPIO_PUSHPULL|GPIO_OUTPUT_CLEAR|GPIO_PORTE|GPIO_PIN6)
#define BOARD_INDICATE_EXTERNAL_LOCKOUT_STATE(enabled) \
    px4_arch_configgpio((enabled) ? GPIO_nARMED : GPIO_nARMED_INIT)

// Tone alarm
#define TONE_ALARM_TIMER    14
#define TONE_ALARM_CHANNEL  1
#define GPIO_BUZZER_1       (GPIO_OUTPUT|GPIO_PUSHPULL|GPIO_OUTPUT_CLEAR|GPIO_PORTF|GPIO_PIN9)
#define GPIO_TONE_ALARM_IDLE GPIO_BUZZER_1
#define GPIO_TONE_ALARM     GPIO_TIM14_CH1OUT_2
```

### 10.3 ADC Configuration

#### FMU-6X ADC (9 channels)

```cpp
#define PX4_ADC_GPIO  \
    GPIO_ADC1_INP16,   /* PA0  - VDD_3V3_SENSORS1 */ \
    GPIO_ADC12_INP18,  /* PA4  - VDD_3V3_SENSORS2 */ \
    GPIO_ADC12_INP9,   /* PB0  - VDD_3V3_SENSORS3 */ \
    GPIO_ADC12_INP5,   /* PB1  - SCALED_V5        */ \
    GPIO_ADC123_INP12, /* PC2  - 6V6              */ \
    GPIO_ADC12_INP13,  /* PC3  - 3V3              */ \
    GPIO_ADC1_INP6,    /* PF12 - VDD_3V3_SENSORS4 */ \
    GPIO_ADC3_INP14,   /* PH3  - HW_VER           */ \
    GPIO_ADC3_INP15    /* PH4  - HW_REV           */

#define ADC_SCALED_VDD_3V3_SENSORS1_CHANNEL  ADC1_CH(16)
#define ADC_SCALED_VDD_3V3_SENSORS2_CHANNEL  ADC1_CH(18)
#define ADC_SCALED_VDD_3V3_SENSORS3_CHANNEL  ADC1_CH(9)
#define ADC_SCALED_V5_CHANNEL                ADC1_CH(5)
// ... etc
```

#### SAMV71 ADC (Placeholder only)

```c
// Placeholders - not functional
#define ADC_BATTERY_VOLTAGE_CHANNEL  0
#define ADC_BATTERY_CURRENT_CHANNEL  1
#define BOARD_NUMBER_BRICKS          1
#define BOARD_ADC_BRICK_VALID        1
```

---

## Section 11: Testing & Tooling

### 11.1 Available Test Commands

| Test | FMU-6X | SAMV71 | Status |
|------|--------|--------|--------|
| `hrt_test` | ✅ Works | ✅ Works | Parity |
| `sensor status` | ✅ Works | ✅ Works | Parity |
| `pwm info` | ✅ Shows 9 channels | ❌ Empty | Gap |
| `pwm test` | ✅ Works | ❌ No channels | Gap |
| `listener sensor_accel` | ✅ Works | ✅ Works | Parity |
| `i2cdetect -r 0` | ✅ Works | ⚠️ Could enable | Optional |
| `spi status` | ✅ Works | ✅ Works | Parity |

### 11.2 Recommended Test Script

```bash
#!/bin/sh
# test_samv71_quick.sh - Quick hardware validation

echo "=== SAMV71-XULT Hardware Test ==="

echo "\n--- HRT Test ---"
hrt_test

echo "\n--- SPI Status ---"
spi status

echo "\n--- I2C Devices ---"
# i2cdetect -r 0  # Enable if needed

echo "\n--- Sensor Status ---"
sensor status

echo "\n--- PWM Info ---"
pwm info

echo "\n--- Parameter Status ---"
param status

echo "\n--- USB/MAVLink ---"
mavlink status

echo "\n=== Test Complete ==="
```

---

## Section 12: Summary & Action Items

### 12.1 Critical Gaps (Must Fix for Flight)

| Priority | Gap | Effort | Blocking | Notes |
|----------|-----|--------|----------|-------|
| 🔴 P0 | **SAMV7 io_timer arch layer** | 16-24h | All PWM output | Must add PWM0/PWM1 support OR implement TC-based PWM |
| 🔴 P0 | Board timer_config.cpp | 4-8h | Motor output | Depends on arch layer completion |
| 🔴 P0 | Enable PWM_OUT driver | 2-4h | Motor output | Depends on timer config |
| 🔴 P0 | board_on_reset() impl | 2-4h | Safe shutdown | Set PWM pins low on reset |

> **Architecture Prerequisite**: The SAMV7 PX4 io_timer layer only supports TC-based timers today.
> PWM0/PWM1 peripherals require new Timer enums, clock gating, base addresses, and init helpers
> in `platforms/nuttx/src/px4/microchip/samv7/`. See Section 7.3 for details.

### 12.2 High Priority Gaps

| Priority | Gap | Effort | Blocking | Notes |
|----------|-----|--------|----------|-------|
| 🟠 P1 | Console buffer fix | 4-8h | dmesg, hardfault_log | `SEM_INITIALIZER` static init bug |
| 🟠 P1 | BlockingList fix | 4-8h | Work queue switching | `PTHREAD_MUTEX_INITIALIZER` static init bug |
| 🟠 P1 | ADC battery monitoring | 4-8h | Battery failsafe | Need SAMV7 ADC driver |
| 🟠 P1 | QSPI flash storage | 6-12h | Persistent storage | SST26VF064B on QSPI |
| 🟠 P1 | Enable ICM-20689 SPI | 2-4h | Primary IMU | Start command commented out |

> **Static Init Note**: Console buffer and BlockingList share the same root cause - POSIX
> static initializers (`SEM_INITIALIZER`, `PTHREAD_MUTEX_INITIALIZER`) don't work on SAMV7.
> Both need runtime initialization via constructor calls. See Section 9.3-9.4 for details.

### 12.3 Medium Priority Gaps

| Priority | Gap | Effort | Blocking | Notes |
|----------|-----|--------|----------|-------|
| 🟡 P2 | board_peripheral_reset() | 2-4h | Sensor reset | Drive mikroBUS RST pins |
| 🟡 P2 | mikroBUS GPIO macros | 2-4h | Sensor resets | PA19, PB0, PA0, PA6 |
| 🟡 P2 | Additional sensor drivers | 4-8h | Redundancy | BMP388, BMI088, etc. |

### 12.4 Low Priority / Optional

| Priority | Gap | Effort | Blocking | Notes |
|----------|-----|--------|----------|-------|
| 🟢 P3 | Safety switch | 2-4h | Physical arming | Need GPIO + driver |
| 🟢 P3 | Tone alarm | 2-4h | Audio feedback | PWM-based buzzer |
| 🟢 P3 | CAN/UAVCAN | 8-16h | CAN devices | MCAN0/MCAN1 available |
| 🟢 P3 | DShot | 8-12h | Digital ESC | Requires timer arch work |

### 12.5 Open Questions / Decisions Needed

#### Q1: Timer Architecture Approach

**Question**: Should we pursue PWM0/PWM1 peripheral support or TC-based PWM?

| Approach | Effort | Channels | DShot | Recommendation |
|----------|--------|----------|-------|----------------|
| **Option A: PWM0/PWM1** | 16-24h | 8 | Possible | Long-term, production |
| **Option B: TC1/TC2/TC3** | 8-12h | 6-9 | Unlikely | Short-term, first flight |

**Recommended Path**:
1. For HITL testing: Continue with `pwm_out_sim` (no hardware changes needed)
2. For first flight: Implement TC-based PWM (Option B) - 6 channels is enough for a quad
3. For production: Evaluate PWM peripheral support based on DShot/feature requirements

#### Q2: Expected Sensor Population

**Question**: Which click boards should `rc.board_sensors` be aligned with?

**Current Configuration**:
- ✅ AK09916 (Compass 4 Click) - I2C - Enabled
- ✅ DPS310 (Pressure 3 Click) - I2C - Enabled
- ⚠️ ICM-20689 (6DOF IMU 6 Click) - SPI - Commented out

**Recommended Minimum for Flight**:

| Socket | Click Board | Sensor | Interface | Status |
|--------|-------------|--------|-----------|--------|
| Socket 1 | 6DOF IMU 6 | ICM-20689 | SPI | 🔴 Must enable |
| Socket 2 | Pressure 3 | DPS310 | I2C | ✅ Enabled |
| Arduino I2C | Compass 4 | AK09915 | I2C | ✅ Enabled |

**Action**: Once SPI bus is validated, uncomment `icm20689 start -s -b 0` in `rc.board_sensors`

#### Q3: TC0 Usage Conflict

**Question**: HRT uses TC0 - does this limit PWM channels?

**Answer**: Yes. TC0 is reserved for HRT (high-resolution timer). Available for PWM:
- TC1: 3 channels
- TC2: 3 channels
- TC3: 3 channels (if not used for other purposes)

Maximum TC-based PWM channels: 9 (but must verify pin routing on schematic)

### 12.6 FMU-6X Features - SAMV71 Status

| Feature | SAMV71 Status | Notes |
|---------|---------------|-------|
| Ethernet | ✅ Hardware available | GMAC + KSZ8081 PHY on board - **can be enabled** |
| PX4IO | ❌ No coprocessor | Not needed for direct PWM |
| HW version detection | ⚠️ Not needed | Single board variant |
| Multiple SPI configs | ⚠️ Not needed | Fixed sensor config |
| Heater control | ❌ No heater hardware | - |

> **Ethernet Note**: The SAMV71-XULT has full Ethernet hardware:
> - GMAC peripheral in SAMV71 MCU
> - KSZ8081 Ethernet PHY
> - RJ45 connector with magnetics
>
> This could be enabled for MAVLink over Ethernet (like FMU-6X) by:
> 1. Enabling `CONFIG_SAMV7_EMAC` in NuttX defconfig
> 2. Configuring network stack
> 3. Adding `MAV_2_CONFIG 1000` for Ethernet MAVLink

---

## Appendix A: File Cross-Reference

| Component | FMU-6X Path | SAMV71 Path |
|-----------|-------------|-------------|
| Board config | `boards/px4/fmu-v6x/src/board_config.h` | `boards/microchip/samv71-xult-clickboards/src/board_config.h` |
| Init | `boards/px4/fmu-v6x/src/init.cpp` | `boards/microchip/samv71-xult-clickboards/src/init.c` |
| SPI | `boards/px4/fmu-v6x/src/spi.cpp` | `boards/microchip/samv71-xult-clickboards/src/spi.cpp` |
| I2C | `boards/px4/fmu-v6x/src/i2c.cpp` | `boards/microchip/samv71-xult-clickboards/src/i2c.cpp` |
| Timer | `boards/px4/fmu-v6x/src/timer_config.cpp` | `boards/microchip/samv71-xult-clickboards/src/timer_config.cpp` |
| MTD | `boards/px4/fmu-v6x/src/mtd.cpp` | Not implemented |
| Defaults | `boards/px4/fmu-v6x/init/rc.board_defaults` | `boards/microchip/samv71-xult-clickboards/init/rc.board_defaults` |
| Sensors | `boards/px4/fmu-v6x/init/rc.board_sensors` | `boards/microchip/samv71-xult-clickboards/init/rc.board_sensors` |
| defconfig | `boards/px4/fmu-v6x/nuttx-config/nsh/defconfig` | `boards/microchip/samv71-xult-clickboards/nuttx-config/nsh/defconfig` |

---

**Document End**

---

## Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | Nov 28, 2025 | Initial document |
| 1.1 | Nov 28, 2025 | Corrections: HRT is 16-bit+software (not chained 32-bit); Timer section clarifies PWM0/PWM1 not implemented in io_timer layer; rc.board_sensors corrected (ICM20689 commented, AK09916/DPS310 enabled); Executive summary updated to reflect disabled drivers |
| 1.2 | Nov 28, 2025 | Ethernet correction: SAMV71-XULT has GMAC + KSZ8081 PHY hardware (not "no hardware"); added Section 8.2 Ethernet Configuration with enable instructions |
| 1.3 | Nov 28, 2025 | Added Section 9.3-9.4: Work queue / BlockingList `PTHREAD_MUTEX_INITIALIZER` static init issue; documented `updateSubscriptions()` workaround in PWMSim; added BlockingList fix to P1 priority list |
| 1.4 | Nov 28, 2025 | Clarified `wq:rate_ctrl` work queue role in `updateSubscriptions()` - this is the high-priority work queue for motor control timing; updated impact analysis |
| 1.5 | Nov 28, 2025 | Added `wq:manager` (WorkQueueManager) documentation: startup chain, data structures, fixed vs unfixed components table; expanded crash call tree with full code path from `PWMSim::Run()` → `BlockingList::mutex()` with file:line references |

*Generated: November 28, 2025*
*Reference: SAMV71_PORTING_NOTES.md Sections 1-12*
*Reviewed against: io_timer_hw_description.h, hrt.c, rc.board_sensors, default.px4board*
