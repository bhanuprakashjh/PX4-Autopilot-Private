# SPI Implementation Plan for SAMV71-XULT-Clickboards (with DMA)

## Executive Summary

**Goal**: Enable DMA-based SPI bus communication to support ICM-20689 IMU (6-axis accelerometer + gyroscope)

**Timeline Estimate**: 4-6 hours implementation + 2-4 hours testing

**Priority**: HIGH - IMU is critical for flight control (accelerometer and gyroscope required for attitude estimation)

**Hardware**: SAMV71Q21 has SPI0 and SPI1 peripherals with XDMAC support

**DMA Configuration**: Following production PX4 board standards (all Pixhawk boards use SPI DMA)

**Success Criteria**:
- SPI bus initialization successful with DMA enabled
- ICM-20689 WHO_AM_I register reads correctly (0x98)
- Accelerometer and gyroscope data streaming to uORB topics
- Data rate: 1000 Hz (typical for flight control IMU)
- DMA transfers active for reads > 8 bytes (verified in logs)

---

## 1. SAMV71 SPI Hardware Overview

### 1.1 SPI Peripheral Capabilities

**SAMV71Q21 has TWO SPI controllers**:
- **SPI0**: Synchronous Serial Controller 0
- **SPI1**: Synchronous Serial Controller 1

**Key Features**:
- Master/Slave mode (we'll use Master)
- Up to 50 MHz clock (ICM-20689 supports 10 MHz max)
- Hardware chip select (CS) support with up to 4 CS lines per bus
- DMA capable (optional, can be added later)
- 8-bit or 16-bit transfers
- CPOL/CPHA configurable (ICM-20689 uses Mode 0/3)

### 1.2 Pin Assignments (from NuttX board.h)

**SPI0 Pins** (mikroBUS and Arduino headers):
```
Pin Function    GPIO   Usage
─────────────────────────────────────────
MOSI (Master Out) PD21   SPI0_MOSI   - Data to sensors
MISO (Master In)  PD20   SPI0_MISO   - Data from sensors
SCK  (Clock)      PD22   SPI0_SPCK   - Clock line
CS0              PC26   SPI0_NPCS0  - Chip select 0 (can use GPIO instead)
CS1              PB02   SPI0_NPCS1  - Chip select 1 (optional)
```

**SPI1 Pins** (expansion connector):
```
Pin Function    GPIO   Usage
─────────────────────────────────────────
MOSI             PC27   SPI1_MOSI
MISO             PC26   SPI1_MISO
SCK              PC24   SPI1_SPCK
CS0-3            Various - see datasheet
```

**For ICM-20689**: We'll use SPI0 with GPIO-controlled chip select (more flexible than hardware CS)

### 1.3 Clock Configuration

**MCK (Master Clock)**: 150 MHz (from PLLA)

**SPI Clock Calculation**:
```
SPI_CLK = MCK / SCBR
```

For ICM-20689 (max 10 MHz):
```
SCBR = 150 MHz / 10 MHz = 15
Actual SPI_CLK = 150 MHz / 15 = 10 MHz ✅
```

---

## 2. ICM-20689 IMU Sensor Details

### 2.1 Hardware Characteristics

**Manufacturer**: TDK InvenSense
**Type**: 6-axis IMU (3-axis gyro + 3-axis accel)
**Interface**: SPI only (no I2C support)

**SPI Configuration**:
- **Mode**: SPI Mode 0 (CPOL=0, CPHA=0) or Mode 3 (CPOL=1, CPHA=1)
- **Max Clock**: 10 MHz
- **Chip Select**: Active low
- **Data Order**: MSB first

**WHO_AM_I Register**:
- **Address**: 0x75 (read-only)
- **Value**: 0x98
- **Usage**: Sensor detection and verification

### 2.2 Register Access

**Read Operation** (single register):
```
CS low
TX: [0x80 | register_addr] [dummy_byte]
RX: [dummy_byte] [data]
CS high
```

**Write Operation** (single register):
```
CS low
TX: [register_addr] [data]
RX: [dummy] [dummy]
CS high
```

**Burst Read** (multiple registers):
```
CS low
TX: [0x80 | start_addr] [dummy] [dummy] [dummy] ...
RX: [dummy] [data1] [data2] [data3] ...
CS high
```

### 2.3 Key Registers

| Register | Address | Description |
|----------|---------|-------------|
| WHO_AM_I | 0x75 | Device ID (0x98) |
| PWR_MGMT_1 | 0x6B | Power management |
| GYRO_CONFIG | 0x1B | Gyro full scale |
| ACCEL_CONFIG | 0x1C | Accel full scale |
| ACCEL_XOUT_H | 0x3B | Accel X high byte (start of sensor data) |
| TEMP_OUT_H | 0x41 | Temperature high byte |
| GYRO_XOUT_H | 0x43 | Gyro X high byte |

---

## 3. NuttX SPI Driver Status

### 3.1 Current Configuration

From `nuttx-config/nsh/defconfig`:
```
CONFIG_SPI=y                 ✅ SPI framework enabled
CONFIG_SPI_EXCHANGE=y        ✅ SPI exchange API enabled
CONFIG_SAMV7_SPI0=y          ✅ SPI0 hardware enabled
CONFIG_SAMV7_SPI1=y          ✅ SPI1 hardware enabled
CONFIG_SAMV7_XDMAC=y         ✅ DMA controller enabled
```

**DMA Status**: ❌ NOT YET ENABLED
```
# CONFIG_SAMV7_SPI_DMA is not set        ← Need to enable this
# CONFIG_SAMV7_SPI_DMATHRESHOLD not set  ← Need to set this
```

**Status**: ⚠️ NuttX SPI driver is compiled but DMA not configured yet

### 3.2 NuttX SPI Driver Functions

NuttX provides these SPI functions:
```c
struct spi_dev_s *sam_spibus_initialize(int port);  // Initialize SPI bus
void sam_spi_select(struct spi_dev_s *dev, uint32_t devid, bool selected);  // CS control
uint32_t sam_spi_send(struct spi_dev_s *dev, uint32_t wd);  // Send/receive byte
void sam_spi_exchange(struct spi_dev_s *dev, const void *txbuffer,
                      void *rxbuffer, size_t nwords);  // Bulk transfer
```

**Location**: `platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/sam_spi.c`

---

## 4. PX4 SPI Integration Architecture

### 4.1 Required Files

We need to create/modify these files:

#### **File 1**: `boards/microchip/samv71-xult-clickboards/src/spi.cpp` (NEW)
**Purpose**: Define SPI bus configuration and device mapping
**Reference**: `boards/px4/fmu-v5/src/spi.cpp`

#### **File 2**: `boards/microchip/samv71-xult-clickboards/src/board_config.h` (MODIFY)
**Purpose**: Add SPI-related GPIO definitions

#### **File 3**: `boards/microchip/samv71-xult-clickboards/src/init.c` (MODIFY)
**Purpose**: Initialize SPI buses at boot

#### **File 4**: `boards/microchip/samv71-xult-clickboards/init/rc.board_sensors` (MODIFY)
**Purpose**: Uncomment ICM-20689 startup command

### 4.2 PX4 SPI Framework

PX4 uses a hardware abstraction layer for SPI:

```cpp
// High-level API (platform-independent)
#include <px4_arch/spi_hw_description.h>

// Defines:
px4_spi_bus_all_hw_t px4_spi_buses_all_hw[] = {
    initSPIBus(SPI::Bus::SPI0, {
        initSPIDevice(DRV_IMU_DEVTYPE_ICM20689,
                      SPI::CS{GPIO::PortA, GPIO::Pin11},
                      SPI::DRDY{GPIO::PortA, GPIO::Pin12}),
    }),
};
```

This gets translated to NuttX calls at runtime.

---

## 5. Step-by-Step Implementation Plan

### Phase 0: Enable SPI DMA in NuttX (15 minutes)

**CRITICAL**: This must be done FIRST before any other SPI configuration!

#### Step 0.1: Enable DMA for SPI

**File**: `boards/microchip/samv71-xult-clickboards/nuttx-config/nsh/defconfig`

**Add after line 145** (after `CONFIG_SAMV7_SPI1=y`):

```bash
# SPI DMA Configuration (matching production PX4 boards)
CONFIG_SAMV7_SPI_DMA=y
CONFIG_SAMV7_SPI_DMATHRESHOLD=8
```

**Explanation**:
- `CONFIG_SAMV7_SPI_DMA=y` - Enables DMA for all SPI buses
- `CONFIG_SAMV7_SPI_DMATHRESHOLD=8` - Use DMA for transfers > 8 bytes (same as Pixhawk 4/5/6)

**Why this matters**:
- ICM-20689 reads are 14 bytes (accel + gyro + temp) → **WILL USE DMA**
- Small register reads (1-2 bytes) will use polling (more efficient)
- Matches all production PX4 boards (FMUv2 through FMUv6)

#### Step 0.2: Rebuild NuttX Configuration

After modifying defconfig, NuttX must be reconfigured:

```bash
# Clean NuttX build
make microchip_samv71-xult-clickboards_default distclean

# Rebuild with new DMA config
make microchip_samv71-xult-clickboards_default
```

**Expected output**: Build succeeds, DMA-related code gets compiled into SPI driver

#### Step 0.3: Verify DMA Configuration

After build completes, verify DMA is enabled:

```bash
grep -A 2 "CONFIG_SAMV7_SPI_DMA" \
  build/microchip_samv71-xult-clickboards_default/NuttX/nuttx/.config
```

**Expected output**:
```
CONFIG_SAMV7_SPI_DMA=y
CONFIG_SAMV7_SPI_DMATHRESHOLD=8
```

---

### Phase 1: Hardware Pin Configuration (30 minutes)

#### Step 1.1: Define SPI GPIO Pins

**File**: `boards/microchip/samv71-xult-clickboards/src/board_config.h`

**Add after line 73** (after LED definitions):

```c
/* SPI Buses ***********************************************************************************/

/* SAMV71-XULT SPI0 Configuration:
 * SPI0: ICM-20689 IMU on mikroBUS socket 1
 *   PD21 - MOSI (SPI0_MOSI)
 *   PD20 - MISO (SPI0_MISO)
 *   PD22 - SCK  (SPI0_SPCK)
 *   PA11 - CS   (GPIO, active low)
 *   PA12 - DRDY (Data Ready interrupt, optional for now)
 */

#define PX4_NUMBER_SPI_BUSES 1

/* ICM-20689 on SPI0 - GPIO chip select */
#define GPIO_SPI0_CS_ICM20689  (GPIO_OUTPUT | GPIO_CFG_DEFAULT | GPIO_OUTPUT_SET | \
                                GPIO_PORT_PIOA | GPIO_PIN11)

/* ICM-20689 Data Ready line (interrupt) - configure as input for now */
#define GPIO_SPI0_DRDY_ICM20689 (GPIO_INPUT | GPIO_CFG_PULLUP | \
                                 GPIO_PORT_PIOA | GPIO_PIN12)
```

#### Step 1.2: Add SPI GPIOs to Init List

**File**: `boards/microchip/samv71-xult-clickboards/src/board_config.h`

**Modify PX4_GPIO_INIT_LIST** (around line 124):

```c
#define PX4_GPIO_INIT_LIST { \
        GPIO_nLED_BLUE,           \
        GPIO_SPI0_CS_ICM20689,    \
        GPIO_SPI0_DRDY_ICM20689,  \
    }
```

---

### Phase 2: Create SPI Bus Configuration (45 minutes)

#### Step 2.1: Create spi.cpp File

**File**: `boards/microchip/samv71-xult-clickboards/src/spi.cpp` (NEW)

```cpp
/****************************************************************************
 * boards/microchip/samv71-xult-clickboards/src/spi.cpp
 *
 * SPI bus configuration for SAMV71-XULT-Clickboards
 ****************************************************************************/

#include <px4_arch/spi_hw_description.h>
#include <drivers/drv_sensor.h>
#include <nuttx/spi/spi.h>

/* SPI bus 0: Internal sensors on mikroBUS */
constexpr px4_spi_bus_t px4_spi_buses[SPI_BUS_MAX_BUS_ITEMS] = {
    initSPIBus(SPI::Bus::SPI0, {
        initSPIDevice(DRV_IMU_DEVTYPE_ICM20689, SPI::CS{GPIO::PortA, GPIO::Pin11}),
    }),
};

static constexpr bool unused = validateSPIConfig(px4_spi_buses);
```

**Key Points**:
- Defines one SPI bus (SPI0)
- Maps ICM-20689 to chip select on PA11
- PX4 framework will handle chip select toggling

#### Step 2.2: Add spi.cpp to CMakeLists.txt

**File**: `boards/microchip/samv71-xult-clickboards/src/CMakeLists.txt`

**Add** (around line 40, after other source files):

```cmake
px4_add_board_module(
    MODULE drivers__board
    SRCS
        init.c
        led.c
        spi.cpp        # <-- ADD THIS LINE
)
```

---

### Phase 3: Initialize SPI at Boot (30 minutes)

#### Step 3.1: Add SPI Initialization Code

**File**: `boards/microchip/samv71-xult-clickboards/src/init.c`

**Add after I2C initialization** (around line 199):

```c
    /* Initialize SPI buses - must be after px4_platform_init */
#ifdef CONFIG_SAMV7_SPI0
    syslog(LOG_ERR, "[boot] Initializing SPI bus 0 (SPI0) with DMA...\n");
    struct spi_dev_s *spi0 = sam_spibus_initialize(0);
    if (spi0 == NULL) {
        syslog(LOG_ERR, "[boot] ERROR: Failed to initialize SPI bus 0\n");
        led_on(LED_RED);
    } else {
        syslog(LOG_ERR, "[boot] SPI bus 0 initialized successfully\n");

        /* Configure SPI mode and speed */
        SPI_SETMODE(spi0, SPIDEV_MODE0);  /* ICM20689 supports mode 0 or 3 */
        SPI_SETBITS(spi0, 8);              /* 8-bit transfers */
        SPI_SETFREQUENCY(spi0, 10000000);  /* 10 MHz max for ICM20689 */

#ifdef CONFIG_SAMV7_SPI_DMA
        syslog(LOG_ERR, "[boot] SPI0 configured: Mode 0, 8-bit, 10 MHz, DMA enabled (threshold: %d bytes)\n",
               CONFIG_SAMV7_SPI_DMATHRESHOLD);
#else
        syslog(LOG_ERR, "[boot] SPI0 configured: Mode 0, 8-bit, 10 MHz, polling mode\n");
#endif
    }
#endif
```

#### Step 3.2: Add Required Headers

**File**: `boards/microchip/samv71-xult-clickboards/src/init.c`

**Add near top** (around line 59, after i2c_master.h):

```c
#include <nuttx/spi/spi.h>
```

**Add external function declaration** (near line 148, after sam_usbinitialize):

```c
extern struct spi_dev_s *sam_spibus_initialize(int bus);
```

---

### Phase 4: Enable ICM-20689 Driver (15 minutes)

#### Step 4.1: Uncomment Sensor Startup

**File**: `boards/microchip/samv71-xult-clickboards/init/rc.board_sensors`

**Change line 10** from:
```bash
# icm20689 start -s -b 0
```

**To**:
```bash
icm20689 start -s -b 0
```

**Explanation**:
- `-s` = SPI mode
- `-b 0` = SPI bus 0

---

### Phase 5: Build and Test (1 hour)

#### Step 5.1: Clean Build

```bash
make clean
make microchip_samv71-xult-clickboards_default
```

**Expected**: Compilation succeeds, spi.cpp compiles without errors

#### Step 5.2: Flash Firmware

```bash
make microchip_samv71-xult-clickboards_default upload
```

#### Step 5.3: Connect to Console

```bash
screen /dev/ttyACM0 57600
```

#### Step 5.4: Check Boot Log

**Look for**:
```
[boot] Initializing SPI bus 0 (SPI0) with DMA...
[boot] SPI bus 0 initialized successfully
[boot] SPI0 configured: Mode 0, 8-bit, 10 MHz, DMA enabled (threshold: 8 bytes)
```

**Verify DMA is active**:
- Message should say "DMA enabled" not "polling mode"
- Threshold should be 8 bytes (matching Pixhawk boards)

**Then look for ICM-20689 startup**:
```
INFO  [icm20689] SPI device ID: 0x98
INFO  [icm20689] on SPI bus 0
```

#### Step 5.5: Manual Driver Test

If driver doesn't autostart, test manually:

```bash
nsh> icm20689 start -s -b 0
nsh> icm20689 status
```

**Expected output**:
```
Running on SPI bus 0
Device ID: 0x98
Gyro range: ±2000 dps
Accel range: ±16g
Sample rate: 1000 Hz
```

#### Step 5.6: Verify Data Streaming

```bash
nsh> listener sensor_accel 10
```

**Expected output** (10 samples):
```
TOPIC: sensor_accel
    timestamp: 123456789
    device_id: 5701776
    x: 0.123 (m/s²)
    y: -0.456 (m/s²)
    z: -9.81 (m/s²)  <-- Should be ~-9.81 if board is level
    temperature: 25.3
    error_count: 0
```

```bash
nsh> listener sensor_gyro 10
```

**Expected output**:
```
TOPIC: sensor_gyro
    timestamp: 123456790
    device_id: 5701776
    x: 0.001 (rad/s)  <-- Should be near 0 if board is stationary
    y: -0.002 (rad/s)
    z: 0.000 (rad/s)
    temperature: 25.3
    error_count: 0
```

---

## 6. Comparison: I2C vs SPI Implementation

### What We Learned from I2C

| Aspect | I2C Implementation | SPI Implementation |
|--------|-------------------|-------------------|
| **Initialization** | Required `i2c_register()` | No registration needed |
| **Device Node** | Creates `/dev/i2c0` | No device node (drivers use directly) |
| **Timing** | Must init after `px4_platform_init()` | Same - init after platform |
| **GPIO Setup** | None (hardware handles SDA/SCL) | Must configure CS pins as GPIO |
| **Chip Select** | N/A | Manual GPIO control or hardware CS |
| **Speed** | 100 kHz - 400 kHz | Up to 50 MHz |
| **DMA Usage** | ❌ NO (interrupt-driven) | ✅ YES (for transfers > 8 bytes) |
| **Complexity** | Lower | Higher (need CS + DMA management) |

### DMA Configuration Comparison

| Board Type | I2C DMA | SPI DMA | Reason |
|------------|---------|---------|--------|
| **STM32 Pixhawks (FMUv2-6)** | ❌ NO | ✅ YES | SPI is high-bandwidth, I2C is low-bandwidth |
| **iMXRT Pixhawks (FMUv6xrt)** | ✅ YES | ✅ YES | High-performance chip, DMA for all |
| **SAMV71-XULT (This board)** | ❌ NO | ✅ YES | Following STM32 standard |

### SPI-Specific Challenges

1. **Chip Select Management**: Must manually control CS GPIO (unlike I2C which has no CS)
2. **Timing Sensitivity**: Higher speeds = stricter timing requirements
3. **Mode Configuration**: Must match sensor's CPOL/CPHA (Mode 0 for ICM-20689)
4. **No Device Detection**: Can't scan bus like I2C (`i2cdetect`), must try to read WHO_AM_I

---

## 7. Troubleshooting Guide

### Issue: SPI Initialization Fails

**Symptoms**:
```
ERROR: Failed to initialize SPI bus 0
```

**Causes**:
1. NuttX SPI driver not enabled in defconfig
2. Pin conflict with another peripheral
3. Clock not configured

**Solutions**:
1. Verify `CONFIG_SAMV7_SPI0=y` in `nuttx-config/nsh/defconfig`
2. Check pin assignments in `nuttx-config/include/board.h`
3. Verify MCK is 150 MHz in boot log

---

### Issue: ICM-20689 Not Detected

**Symptoms**:
```
ERROR [icm20689] WHO_AM_I mismatch: expected 0x98, got 0x00 or 0xFF
```

**Causes**:
1. SPI wiring issue (MOSI/MISO swapped)
2. Chip select not working
3. Wrong SPI mode
4. Sensor not powered

**Debug Steps**:

**1. Verify SPI Bus Exists**:
```bash
nsh> ls /dev/spi*
# Should show nothing - SPI doesn't create device nodes
# This is normal for NuttX SPI
```

**2. Test Chip Select GPIO**:
```bash
nsh> gpio write pa11 0  # CS low
nsh> gpio write pa11 1  # CS high
```
Use multimeter to verify PA11 toggles.

**3. Try Manual WHO_AM_I Read**:
Add debug code in `init.c`:
```c
uint8_t tx[2] = {0x80 | 0x75, 0x00};  // Read WHO_AM_I (0x75)
uint8_t rx[2] = {0, 0};

/* Select ICM20689 */
sam_gpiowrite(GPIO_SPI0_CS_ICM20689, false);
SPI_EXCHANGE(spi0, tx, rx, 2);
sam_gpiowrite(GPIO_SPI0_CS_ICM20689, true);

syslog(LOG_ERR, "[boot] ICM20689 WHO_AM_I: 0x%02x (expect 0x98)\n", rx[1]);
```

**4. Try Different SPI Modes**:
ICM-20689 supports both Mode 0 and Mode 3:
```c
SPI_SETMODE(spi0, SPIDEV_MODE3);  // Try mode 3 if mode 0 fails
```

---

### Issue: Data Stream Has Errors

**Symptoms**:
```
error_count: 123  (non-zero)
```

**Causes**:
1. SPI clock too fast
2. Noise on SPI lines
3. CS timing issue

**Solutions**:
1. Reduce SPI speed to 1 MHz for testing:
```c
SPI_SETFREQUENCY(spi0, 1000000);  // 1 MHz
```

2. Add delays between transfers (if needed)

3. Check hardware: use logic analyzer on PD20/PD21/PD22

---

## 8. Optional Enhancements (Future Work)

### 8.1 ~~DMA Support~~ ✅ INCLUDED IN BASE PLAN

**Status**: ✅ **DMA is now part of the base implementation** (Phase 0)

**Reason**: ALL production PX4 boards use SPI DMA, so we're following industry standard from day one

---

### 8.2 Data Ready Interrupt

**Benefit**: Precise timing, eliminates polling

**Current**: ICM-20689 polled at 1 kHz
**With DRDY**: Interrupt-driven, exact sample timing

**Implementation**:
1. Configure PA12 as external interrupt
2. Modify PX4 sensor driver to use DRDY
3. Read sensor data only when interrupt fires

**Effort**: 2-3 hours

**Priority**: MEDIUM (improves timing accuracy)

---

### 8.3 Multiple SPI Devices

**Use Case**: Add barometer, external magnetometer on SPI

**Implementation**: Add more devices to `spi.cpp`:
```cpp
initSPIBus(SPI::Bus::SPI0, {
    initSPIDevice(DRV_IMU_DEVTYPE_ICM20689, SPI::CS{GPIO::PortA, GPIO::Pin11}),
    initSPIDevice(DRV_BARO_DEVTYPE_MS5611, SPI::CS{GPIO::PortA, GPIO::Pin13}),
}),
```

**Effort**: 30 minutes per device

**Priority**: LOW (depends on hardware availability)

---

## 9. Success Criteria Checklist

Use this checklist to verify SPI implementation is complete:

### NuttX DMA Configuration
- [ ] `CONFIG_SAMV7_SPI_DMA=y` added to defconfig
- [ ] `CONFIG_SAMV7_SPI_DMATHRESHOLD=8` added to defconfig
- [ ] NuttX rebuilt with `make distclean && make`
- [ ] Verified DMA config in `.config` file

### Hardware Configuration
- [ ] SPI0 pins defined in `board_config.h`
- [ ] CS GPIO configured as output, initialized high
- [ ] DRDY GPIO configured as input (optional)
- [ ] GPIOs added to `PX4_GPIO_INIT_LIST`

### Software Integration
- [ ] `spi.cpp` created with bus configuration
- [ ] `spi.cpp` added to `CMakeLists.txt`
- [ ] `init.c` calls `sam_spibus_initialize(0)`
- [ ] SPI mode, bits, frequency configured
- [ ] Boot log prints DMA status

### Build System
- [ ] Firmware compiles without errors
- [ ] No warnings about undefined SPI symbols
- [ ] Flash usage still acceptable (< 70%)

### Boot Verification
- [ ] Boot log shows "SPI bus 0 initialized successfully"
- [ ] Boot log shows "DMA enabled (threshold: 8 bytes)"
- [ ] No SPI-related errors in boot log
- [ ] ICM-20689 driver starts without errors

### Functional Testing
- [ ] `icm20689 status` shows "Running on SPI bus 0"
- [ ] WHO_AM_I reads as 0x98
- [ ] `listener sensor_accel` shows valid data (~9.81 m/s² on Z axis)
- [ ] `listener sensor_gyro` shows near-zero values when stationary
- [ ] Error counts remain at 0
- [ ] Temperature reading is reasonable (20-30°C)

### Performance Validation
- [ ] Accelerometer updates at 1000 Hz
- [ ] Gyroscope updates at 1000 Hz
- [ ] No CPU overload (check with `top`)
- [ ] Data latency < 2 ms

---

## 10. Timeline and Milestones

| Phase | Task | Time | Cumulative |
|-------|------|------|------------|
| 0 | Enable SPI DMA in NuttX | 15 min | 0.25 hr |
| 1 | Pin configuration | 30 min | 0.75 hr |
| 2 | Create spi.cpp | 45 min | 1.5 hr |
| 3 | Init code in init.c | 30 min | 2.0 hr |
| 4 | Enable ICM-20689 | 15 min | 2.25 hr |
| 5 | Build & test | 60 min | 3.25 hr |
| - | **Subtotal (if no issues)** | **3.25 hr** | - |
| - | Buffer for debugging | 1-3 hr | **4-6 hr** |

**Realistic Estimate**: 4-6 hours total

**Best Case**: 3 hours (if everything works first try)

**Worst Case**: 8 hours (if major debugging needed)

---

## 11. Risk Assessment

| Risk | Probability | Impact | Mitigation |
|------|------------|--------|------------|
| **SPI pins not accessible** | Low | High | Verify schematic before starting |
| **ICM-20689 not populated** | Medium | High | Check hardware documentation |
| **MOSI/MISO swapped** | Medium | Medium | Follow NuttX board.h carefully |
| **Clock speed too fast** | Low | Low | Start with 1 MHz, increase to 10 MHz |
| **CS not working** | Low | Medium | Test CS GPIO manually before SPI |
| **Mode mismatch** | Low | Low | ICM-20689 supports mode 0 and 3 |

---

## 12. Next Steps After SPI Works

Once SPI and ICM-20689 are working:

1. **Verify EKF2 Fusion** ✅
   - Check `ekf2 status` shows valid accelerometer/gyroscope
   - Verify attitude estimation working

2. **Implement PWM Output** ⚠️ (Next priority)
   - TC timer-based PWM for motor control
   - 6 channels (DIRECT_PWM_OUTPUT_CHANNELS)

3. **Test HIL Simulation** ✅
   - Can do NOW - simulated sensors via MAVLink
   - Real IMU will improve accuracy

4. **Add Remaining Sensors** (Lower priority)
   - Barometer (DPS310 on I2C already working!)
   - Magnetometer (AK09916 on I2C already working!)
   - GPS (needs UART)

---

## 13. References

### Documentation
- SAMV71 Datasheet: Section 42 (SPI)
- ICM-20689 Datasheet: Register map and SPI timing
- NuttX SPI Driver: `nuttx/arch/arm/src/samv7/sam_spi.c`
- PX4 SPI Framework: `platforms/common/include/px4_platform_common/spi.h`

### Example Boards
- `boards/px4/fmu-v5/src/spi.cpp` - STM32-based SPI config
- `boards/px4/fmu-v6x/src/spi.cpp` - Advanced multi-bus example

### Related Files (SAMV71-XULT)
- `boards/microchip/samv71-xult-clickboards/src/board_config.h`
- `boards/microchip/samv71-xult-clickboards/nuttx-config/include/board.h`
- `boards/microchip/samv71-xult-clickboards/nuttx-config/nsh/defconfig`

---

## 14. Summary

### What We're Doing
Implementing **DMA-based SPI bus support** to enable ICM-20689 IMU communication, which provides critical accelerometer and gyroscope data for flight control.

### Why It Matters
Without IMU data, EKF2 cannot estimate attitude (roll/pitch/yaw), making flight impossible. This is the highest priority missing feature.

### Why DMA Matters
- **100% of production PX4 boards** use SPI DMA
- Reduces CPU load from ~10% to ~2% for IMU reads
- Allows CPU to focus on control algorithms
- Required for high-bandwidth sensor operations
- Industry standard for flight controllers

### Key Differences from I2C
- SPI is faster (10 MHz vs 400 kHz)
- SPI **uses DMA** (I2C does not on STM32 boards)
- SPI requires manual chip select control
- SPI doesn't have device scanning (must read WHO_AM_I)
- SPI has no bus registration (simpler than I2C)

### Success Looks Like
```bash
nsh> icm20689 status
Running on SPI bus 0
Device ID: 0x98
Gyro: ±2000 dps, Accel: ±16g
Sample rate: 1000 Hz
Status: OK

nsh> listener sensor_accel 3
  timestamp: 123456789
  x: 0.05 m/s²
  y: 0.02 m/s²
  z: -9.81 m/s²  ← Gravity!
  error_count: 0
```

---

**Document Version**: 2.0 (Updated with DMA configuration)
**Last Updated**: 2025-11-16
**Target Platform**: SAMV71-XULT-Clickboards
**Estimated Implementation Time**: 4-6 hours
**Priority**: HIGH (required for flight)
**DMA Mode**: ENABLED (matching all production PX4 boards)

**Production Standard Implementation**: This plan now matches the configuration used on all Pixhawk flight controllers (FMUv2 through FMUv6)

**Ready to proceed with implementation!**
