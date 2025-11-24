# QSPI Flash Storage Implementation - Future Task

**Status:** Not Started
**Priority:** Medium
**Hardware:** SST26VF064B (8MB QSPI Flash) - Already present on SAMV71-XULT board
**Purpose:** Professional MTD storage for `/fs/mtd_params` and `/fs/mtd_caldata`

## Executive Summary

The SAMV71-XULT development board includes an **SST26VF064B 8MB QSPI flash chip** that is currently unused. Implementing MTD storage on this chip would provide the same professional-grade parameter and calibration storage used by Pixhawk and other commercial flight controllers.

## Hardware Specifications

### SST26VF064B QSPI Flash
- **Capacity:** 8MB (64 Mbit)
- **Interface:** QSPI (Quad SPI) via SAMV71 QSPI peripheral
- **Speed:** Up to 104MHz SPI clock
- **Location:** On-board, connected to QSPI0
- **Datasheet:** https://www.microchip.com/en-us/product/SST26VF064B

### Key Features
- ✅ **Unlimited write endurance** (~100,000 erase cycles per sector)
- ✅ **No erase-before-write** required for page programming
- ✅ **Byte-level writes** within pages
- ✅ **Hardware write protection** for data integrity
- ✅ **Fast access** - Quad SPI faster than SD card
- ✅ **Separate from firmware** - Cannot corrupt code flash
- ✅ **8MB capacity** - Far more than needed for params/caldata

## Current Status vs Future with QSPI Flash

| Feature | Current (SD Card) | With QSPI Flash |
|---------|------------------|----------------|
| **Parameter Storage** | /fs/microsd/params | /fs/mtd_params |
| **Calibration Data** | /fs/microsd/params | /fs/mtd_caldata |
| **Speed** | ~10-20 MB/s | ~50-100 MB/s |
| **Reliability** | Medium (card can fail) | High (no moving parts) |
| **Persistence** | Lost if SD removed | Always available |
| **Write Endurance** | Limited (~10K cycles) | ~100K cycles/sector |
| **Boot Time** | ~1-2s SD init | ~100ms flash init |

## Implementation Plan

### Phase 1: Enable QSPI Peripheral in NuttX

**File:** `boards/microchip/samv71-xult-clickboards/nuttx-config/nsh/defconfig`

Add QSPI configuration:
```
CONFIG_SAMV7_QSPI=y
CONFIG_SAMV7_QSPI_DMA=y
CONFIG_SAMV7_QSPI0=y
CONFIG_QSPI=y
CONFIG_QSPI_CMDDATA=y
```

**File:** `boards/microchip/samv71-xult-clickboards/nuttx-config/include/board.h`

Add QSPI pin definitions:
```c
/* QSPI Configuration for SST26VF064B */
#define GPIO_QSPI0_CS     (GPIO_QSPI0_CS_1)    /* PA11 */
#define GPIO_QSPI0_IO0    (GPIO_QSPI0_IO0_1)   /* PA13 - MOSI/IO0 */
#define GPIO_QSPI0_IO1    (GPIO_QSPI0_IO1_1)   /* PA12 - MISO/IO1 */
#define GPIO_QSPI0_IO2    (GPIO_QSPI0_IO2_1)   /* PA17 - WP/IO2 */
#define GPIO_QSPI0_IO3    (GPIO_QSPI0_IO3_1)   /* PD31 - HOLD/IO3 */
#define GPIO_QSPI0_SCK    (GPIO_QSPI0_SCK_1)   /* PA14 */
```

### Phase 2: Add SST26 MTD Driver

**File:** `boards/microchip/samv71-xult-clickboards/nuttx-config/nsh/defconfig`

Enable SST26 driver:
```
CONFIG_MTD=y
CONFIG_MTD_BYTE_WRITE=y
CONFIG_MTD_PARTITION=y
CONFIG_MTD_SST26=y
```

### Phase 3: Initialize QSPI Flash in Board Init

**File:** `boards/microchip/samv71-xult-clickboards/src/init.c`

Add initialization code:
```c
#include <nuttx/spi/qspi.h>
#include <nuttx/mtd/mtd.h>

#ifdef CONFIG_SAMV7_QSPI
static int sam_qspi_init(void)
{
    struct qspi_dev_s *qspi;
    struct mtd_dev_s *mtd;
    int ret;

    /* Get QSPI0 device */
    qspi = sam_qspi_initialize(0);
    if (!qspi) {
        PX4_ERR("Failed to initialize QSPI0");
        return -ENODEV;
    }

    /* Initialize SST26 flash */
    mtd = sst26_initialize(qspi);
    if (!mtd) {
        PX4_ERR("Failed to initialize SST26 flash");
        return -ENODEV;
    }

    /* Register MTD driver */
    ret = register_mtddriver("/dev/qspiflash0", mtd, 0755, NULL);
    if (ret < 0) {
        PX4_ERR("Failed to register MTD driver: %d", ret);
        return ret;
    }

    PX4_INFO("SST26VF064B QSPI flash initialized (8MB)");
    return OK;
}
#endif
```

Call from `board_app_initialize()`:
```c
#ifdef CONFIG_SAMV7_QSPI
    ret = sam_qspi_init();
    if (ret < 0) {
        PX4_WARN("QSPI flash initialization failed: %d", ret);
    }
#endif
```

### Phase 4: Create MTD Partitions

**File:** `boards/microchip/samv71-xult-clickboards/src/mtd.cpp` (new file)

Define MTD partitions following PX4 convention:
```cpp
#include <nuttx/config.h>
#include <board_config.h>
#include <nuttx/spi/spi.h>
#include <px4_platform_common/px4_manifest.h>

// QSPI flash on SAMV71-XULT
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
            .nblocks = 512  // 64KB for parameters (128 bytes/block)
        },
        {
            .type = MTD_CALDATA,
            .path = "/fs/mtd_caldata",
            .nblocks = 512  // 64KB for calibration data
        },
        {
            .type = MTD_WAYPOINTS,
            .path = "/fs/mtd_waypoints",
            .nblocks = 16384  // 2MB for mission waypoints (rest available)
        }
    },
};

static const px4_mtd_manifest_t board_mtd_config = {
    .nconfigs = 1,
    .entries = {
        &qspi_flash,
    }
};

static const px4_mft_entry_s mtd_mft = {
    .type = MTD,
    .pmft = (void *) &board_mtd_config,
};

static const px4_mft_s mft = {
    .nmft = 1,
    .mfts = {
        &mtd_mft,
    }
};

const px4_mft_s *board_get_manifest(void)
{
    return &mft;
}
```

**File:** `boards/microchip/samv71-xult-clickboards/src/CMakeLists.txt`

Add to build:
```cmake
px4_add_board_library(
    SRCS
        init.c
        led.c
        i2c.cpp
        spi.cpp
        sam_hsmci.c
        usb.c
        timer_config.cpp
        sam_gpiosetevent.c
        mtd.cpp  # <-- Add this
)
```

### Phase 5: Update Board Configuration

**File:** `boards/microchip/samv71-xult-clickboards/src/board_config.h`

Enable flash-based params:
```c
/* MTD Parameter Storage on QSPI Flash */
#define FLASH_BASED_PARAMS
#define FLASH_BASED_DATAMAN
```

## Testing Procedure

### Step 1: Build with QSPI Support
```bash
make microchip_samv71-xult-clickboards_default
```

### Step 2: Flash and Verify Boot
```bash
openocd -f interface/cmsis-dap.cfg -c "adapter speed 500" -f target/atsamv.cfg \
  -c "program build/microchip_samv71-xult-clickboards_default/microchip_samv71-xult-clickboards_default.elf verify reset exit"
```

Check boot log for:
```
[init] SST26VF064B QSPI flash initialized (8MB)
[dataman] Using /fs/mtd_params for parameter storage
[dataman] Using /fs/mtd_caldata for calibration data
```

### Step 3: Test Parameter Operations
```bash
nsh> param set TEST_PARAM 123
nsh> param save
nsh> reboot
# After reboot
nsh> param get TEST_PARAM
Value: 123.000000
```

### Step 4: Verify MTD Devices
```bash
nsh> ls /dev/mtd*
/dev/mtd0      # Full QSPI flash
/dev/mtd0p0    # Partition 0: /fs/mtd_params
/dev/mtd0p1    # Partition 1: /fs/mtd_caldata
/dev/mtd0p2    # Partition 2: /fs/mtd_waypoints

nsh> ls /fs/mtd_params
parameters.bson
parameters_backup.bson
```

## Benefits of Implementation

### Operational Benefits
1. **Faster Boot** - No SD card initialization delay
2. **More Reliable** - No SD card corruption risk
3. **Always Available** - Parameters persist even if SD card removed
4. **Professional Grade** - Same storage approach as Pixhawk boards

### Development Benefits
1. **Debugging** - Can store logs/dumps in QSPI even if SD fails
2. **Calibration** - IMU calibration data survives SD card swaps
3. **Configuration** - Critical settings always available
4. **Testing** - Reduces dependency on external storage

### Production Benefits
1. **Cost Reduction** - Can operate without SD card for basic missions
2. **Robustness** - One less mechanical failure point
3. **Speed** - Faster parameter access during flight
4. **Capacity** - 8MB available for future features (logs, maps, etc.)

## Comparison with Other PX4 Boards

### Pixhawk 6X (STM32H7)
- Uses external FRAM on SPI (FM25V02A - 32KB)
- Uses external EEPROM on I2C (24LC64T - 8KB)
- **SAMV71-XULT has 8MB QSPI flash - 250x more capacity!**

### Pixhawk 4 (STM32F7)
- Uses external FRAM on SPI
- **SAMV71-XULT QSPI is faster (Quad SPI vs regular SPI)**

### Our Advantage
- Larger capacity (8MB vs typical 32-64KB)
- Single chip instead of multiple (FRAM + EEPROM)
- Faster interface (QSPI vs SPI/I2C)
- Already on board (no additional hardware cost)

## Estimated Implementation Time

| Phase | Description | Time Estimate |
|-------|-------------|---------------|
| 1 | NuttX QSPI config | 1 hour |
| 2 | SST26 driver setup | 1 hour |
| 3 | Board initialization | 2 hours |
| 4 | MTD partitions | 2 hours |
| 5 | Testing & debugging | 3 hours |
| **Total** | | **9 hours** |

## Dependencies

### Hardware
- ✅ SST26VF064B chip present on SAMV71-XULT board
- ✅ QSPI pins routed and accessible
- ✅ No additional hardware needed

### Software
- ✅ NuttX SAMV7 QSPI driver exists in tree
- ✅ SST26 MTD driver exists in NuttX
- ✅ PX4 MTD manifest framework exists
- ⚠️ Need to verify QSPI pin mappings in SAMV71-XULT schematic

### Documentation
- [SST26VF064B Datasheet](https://www.microchip.com/en-us/product/SST26VF064B)
- [SAMV71 Datasheet](https://www.microchip.com/en-us/product/ATSAMV71Q21) - Chapter 44: QSPI
- [SAMV71-XULT User Guide](https://www.microchip.com/en-us/development-tool/ATSAMV71-XULT)
- NuttX: `arch/arm/src/samv7/sam_qspi.c`
- NuttX: `drivers/mtd/sst26.c`

## Risks and Mitigation

| Risk | Impact | Mitigation |
|------|--------|------------|
| Pin conflicts with other peripherals | Medium | Verify schematic, QSPI uses dedicated pins |
| QSPI driver bugs in NuttX | High | Test thoroughly, fallback to SD card |
| SST26 driver compatibility | Medium | Driver exists in NuttX, well-tested |
| Flash wear-out | Low | 100K erase cycles = 274 years @ 1 erase/day |

## Alternative: Keep Using SD Card

**Current SD card storage is perfectly functional:**
- ✅ Parameters load/save successfully
- ✅ Reliable operation in testing
- ✅ Large capacity for logs
- ✅ Easy to access data (removable)
- ✅ No implementation effort needed

**QSPI flash is an optimization, not a requirement.**

Implement QSPI flash when:
- Building production hardware
- Need guaranteed parameter persistence
- Want professional-grade reliability
- Have time for 9-hour implementation

## Conclusion

The SAMV71-XULT board has excellent hardware for MTD storage (8MB QSPI flash), and implementing it would provide professional-grade parameter storage matching or exceeding commercial flight controllers. However, **the current SD card storage is fully functional** and adequate for development and testing.

**Recommendation:** Document this as future work, implement when moving from development to production hardware, or when SD card reliability becomes a concern.

---

**Created:** November 24, 2025
**Status:** Documented - Not Yet Implemented
**Next Action:** Prioritize based on project phase (development vs production)
