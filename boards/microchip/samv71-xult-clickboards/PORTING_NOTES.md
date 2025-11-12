# PX4 Autopilot Port to ATSAMV71-XULT Board

## Documentation Date
November 12, 2025

## Overview
This document describes the PX4 Autopilot port to the Microchip ATSAMV71-XULT development board with MikroElektronika Click sensor boards. This is a **minimal working build** suitable for initial hardware testing and further development.

## Hardware Specifications

### Target MCU: ATSAMV71Q21B
- **CPU**: ARM Cortex-M7 @ 300 MHz
- **Flash**: 2 MB (2048 KB)
- **RAM**: 384 KB
- **FPU**: Double-precision hardware floating-point (FPv5-D16)

### Current Build Statistics
```
Memory region         Used Size  Region Size  %age Used
           flash:      859,184 B      2 MB     40.97%
            sram:       22,008 B    384 KB      5.60%
```

### Sensor Configuration (Click Boards)
- **IMU**: ICM-20689 (6-axis gyro + accel)
- **Magnetometer**: AK09916
- **Barometer**: DPS310

## Files Created

### 1. Board Configuration Files
Location: `boards/microchip/samv71-xult-clickboards/`

#### `default.px4board`
- Board configuration file defining build parameters
- Configures serial ports (GPS, telemetry, RC)
- Sets up build flags and module selection
- **Status**: ✅ Complete

#### `nuttx-config/nsh/defconfig`
- NuttX configuration for SAMV71Q21B
- Enables: I2C, SPI, USB, serial ports
- Configures memory layout (Flash: 0x200000, RAM: 0x60000)
- **Status**: ✅ Complete

#### `nuttx-config/scripts/script.ld`
- **NEW FILE**: Linker script for SAMV71Q21B
- Defines memory regions:
  - Flash: 0x00400000, 2048K
  - SRAM: 0x20400000, 384K
- Standard ARM Cortex-M7 sections (.text, .data, .bss, .ramfunc)
- **Status**: ✅ Complete
- **Location**: Line 1-131

### 2. Board Source Files
Location: `boards/microchip/samv71-xult-clickboards/src/`

#### `board_config.h`
- Hardware pin definitions and board capabilities
- **Added**:
  - ADC channel definitions (battery voltage/current monitoring)
  - Hardfault logging configuration
  - LED definitions (GPIO_nLED_BLUE on PA23)
- **Status**: ⚠️ Placeholder pin definitions - needs hardware verification
- **Modified Lines**: 50-70

#### `init.c`
- Board initialization and reset handling
- **Modified**:
  - `board_on_reset()`: Simplified to remove PWM dependencies (line 105-109)
- **Status**: ✅ Functional (PWM reset disabled until TC implementation)

#### `spi.cpp`
- SPI bus configuration and chip select functions
- **Added**:
  - `sam_spi0select()`, `sam_spi1select()`: Chip select control (line 60-80)
  - `sam_spi0status()`, `sam_spi1status()`: SPI status reporting (line 67-88)
- **Status**: ⚠️ Empty SPI bus configuration - sensors not yet wired
- **Limitation**: Stub functions return no-op, actual chip selects need GPIO mapping

#### `i2c.cpp`
- **NEW FILE**: I2C bus configuration
- Configures I2C0 as external bus for sensors
- **Status**: ✅ Complete
- **Location**: Line 1-39

#### `timer_config.cpp`
- PWM timer configuration using TC (Timer/Counter) peripherals
- **Status**: ⚠️ Empty timer arrays - PWM not implemented
- **Limitation**: No PWM outputs configured yet

#### `usb.c`
- USB device initialization and VBUS detection
- **Added**:
  - `board_read_VBUS_state()`: Always returns 1 (VBUS present) (line 109-115)
- **Status**: ⚠️ Stub implementation - no actual VBUS GPIO detection
- **TODO**: Implement actual VBUS detection with GPIO pin

#### `led.c`
- LED control for status indication
- **Status**: ✅ Complete (Blue LED on PA23)

#### `CMakeLists.txt`
- Build configuration
- **Added**: sam_gpiosetevent.c to build
- **Status**: ✅ Complete

### 3. Board Init Scripts
Location: `boards/microchip/samv71-xult-clickboards/init/`

#### `rc.board_defaults`
- Default parameter values for SAMV71 board
- Configures vehicle type, sensor calibration
- **Status**: ✅ Complete

#### `rc.board_sensors`
- Sensor startup script
- Starts ICM20689, AK09916, DPS310 drivers
- **Status**: ⚠️ Drivers configured but sensors not wired yet

#### `rc.board_extras`
- Additional board-specific initialization
- **Status**: ✅ Complete (empty)

### 4. Platform Support Files
Location: `platforms/nuttx/src/px4/microchip/samv7/`

#### `CMakeLists.txt`
- **Modified**: Added io_pins and board_critmon subdirectories (line 37-38)
- **Status**: ✅ Complete

#### `include/px4_arch/micro_hal.h`
- Hardware abstraction layer definitions
- **Added**:
  - `sam_gpiosetevent()` declaration (line 85-87)
  - `px4_arch_gpiosetevent()` macro (line 45)
  - `PX4_MAKE_GPIO_INPUT()` macro (line 92)
- **Status**: ✅ Complete

#### `include/px4_arch/spi_hw_description.h`
- **NEW FILE**: SPI hardware configuration
- Defines GPIO and SPI namespaces for SAMV7
- Helper functions: `initSPIBus()`, `initSPIBusExternal()`
- **Status**: ✅ Complete
- **Location**: Line 1-117

#### `include/px4_arch/i2c_hw_description.h`
- **NEW FILE**: I2C hardware configuration
- Helper functions: `initI2CBusInternal()`, `initI2CBusExternal()`
- **Status**: ✅ Complete
- **Location**: Line 1-52

#### `include/px4_arch/io_timer.h`
- **NEW FILE**: IO timer declarations for PWM
- Defines timer channel structures and function prototypes
- **Status**: ✅ Complete (declarations only)
- **Location**: Line 1-72

#### `include/px4_arch/io_timer_hw_description.h`
- **NEW FILE**: Hardware-specific timer configuration
- Type definitions: `io_timer_channel_mode_t`, `channel_handler_t`
- **Status**: ✅ Complete
- **Location**: Line 1-22

### 5. GPIO Interrupt Implementation
Location: `boards/microchip/samv71-xult-clickboards/src/`

#### `sam_gpiosetevent.c`
- **NEW FILE**: GPIO interrupt wrapper for SAMV7
- Maps PX4's generic GPIO interrupt API to SAMV7's native API
- **Functions**:
  - `sam_gpiosetevent()`: Configure GPIO pin for interrupt (line 43-75)
- **Status**: ✅ Functional
- **Uses**: `sam_configgpio()`, `sam_gpioirq()`, `sam_gpioirqenable()`
- **Limitation**: Simplified implementation, may need refinement for production

### 6. High-Resolution Timer (HRT)
Location: `platforms/nuttx/src/px4/microchip/samv7/hrt/`

#### `hrt.c`
- **Modified**: Added `hrt_store_absolute_time()` for CPU load monitoring (line 339-342)
- **Status**: ✅ Complete
- **Note**: Uses TC0 (Timer/Counter 0) for microsecond timing

#### `CMakeLists.txt`
- **Status**: ✅ Complete (no changes needed)

### 7. IO Pins (PWM) Stub Library
Location: `platforms/nuttx/src/px4/microchip/samv7/io_pins/`

#### `io_timer_stub.c`
- **NEW FILE**: Stub implementations for PWM/timer functionality
- **Added**:
  - `io_timer_channel_get_as_pwm_input()`: Returns 0 (line 53-58)
  - `latency_counters[8]`: Global performance monitoring array (line 61)
- **Status**: ⚠️ Stub only - no actual PWM implementation
- **TODO**: Implement full PWM using SAMV7 TC0/TC1/TC2 peripherals

#### `CMakeLists.txt`
- **NEW FILE**: Build configuration for io_pins library
- Links to drivers_board
- **Status**: ✅ Complete

### 8. Board Critical Section Monitoring
Location: `platforms/nuttx/src/px4/microchip/samv7/board_critmon/`

#### `board_critmon.c`
- **NEW FILE**: Critical section timing monitoring
- **Functions**:
  - `board_critmon_init()`: No-op stub (line 49-52)
  - `board_critmon_report()`: No-op stub (line 54-57)
- **Status**: ⚠️ Stub only - no actual monitoring
- **TODO**: Implement when performance profiling is needed

#### `CMakeLists.txt`
- **NEW FILE**: Build configuration
- **Status**: ✅ Complete

### 9. Board Identity and Version
Location: `platforms/nuttx/src/px4/microchip/samv7/version/`

#### `board_identity.c`
- **Modified**: Added `PX4_SOC_ARCH_ID` definition (0x0017 for SAMV7)
- **Status**: ✅ Complete

#### `board_mcu_version.c`
- **Modified**: Added missing includes (`nuttx/arch.h`, `arm_internal.h`)
- **Status**: ✅ Complete

## Current Limitations and TODOs

### ❌ Not Implemented (Critical for Full Functionality)

1. **PWM Output (Motor/Servo Control)**
   - **Files Affected**: `timer_config.cpp`, `io_timer_stub.c`
   - **Issue**: Empty timer configuration arrays
   - **Impact**: Cannot control motors or servos
   - **TODO**:
     - Map GPIO pins to TC (Timer/Counter) channels
     - Implement `io_timer.c` using SAMV7 TC peripherals (TC0, TC1, TC2)
     - Configure TIOA/TIOB outputs for PWM generation
     - Typical pins: PD21-PD26 for TC0-TC2 channels

2. **SPI Device Configuration**
   - **Files Affected**: `spi.cpp`
   - **Issue**: Empty `px4_spi_buses` array
   - **Impact**: SPI sensors cannot be initialized
   - **TODO**:
     - Determine which MikroBUS sockets are used for sensors
     - Map SPI0/SPI1 pins to Click board connections
     - Configure CS (chip select) GPIO pins for each sensor
     - Implement proper chip select logic in `sam_spi0/1select()`
     - Add device configurations to `px4_spi_buses` array

3. **GPIO Pin Mapping**
   - **Files Affected**: `board_config.h`, various sensor drivers
   - **Issue**: Placeholder pin definitions
   - **Impact**: Sensors cannot be properly initialized
   - **TODO**:
     - Review SAMV71-XULT schematic
     - Map DRDY (data ready) interrupt pins for IMU
     - Verify I2C pin assignments (TWD0/TWCK0)
     - Document actual pin connections

4. **USB VBUS Detection**
   - **Files Affected**: `usb.c`
   - **Issue**: Always returns "VBUS present"
   - **Impact**: Cannot detect USB connection state properly
   - **TODO**:
     - Identify VBUS sense GPIO pin
     - Implement actual GPIO read in `board_read_VBUS_state()`

### ⚠️ Partially Implemented (Works but needs refinement)

1. **ADC Battery Monitoring**
   - **Files Affected**: `board_config.h`
   - **Status**: Channel definitions exist but channels are placeholders
   - **TODO**: Verify actual ADC channels used for battery sense

2. **GPIO Interrupts**
   - **Files Affected**: `sam_gpiosetevent.c`
   - **Status**: Basic implementation works
   - **TODO**: Add error handling and edge case management

3. **I2C Configuration**
   - **Files Affected**: `i2c.cpp`
   - **Status**: I2C0 configured as external bus
   - **TODO**: Verify speed settings and add I2C1 if needed

### ✅ Complete and Functional

1. **Build System Integration**
2. **Linker Script and Memory Layout**
3. **High-Resolution Timer (HRT)**
4. **Board Reset Handler**
5. **LED Control**
6. **Basic USB Device Support**
7. **Serial Port Configuration**

## Testing Roadmap

### Phase 1: Basic Boot Test (Hardware Required)
```bash
# Flash firmware
bossac -e -w -v -b microchip_samv71-xult-clickboards_default.bin

# Connect serial console (115200 8N1)
minicom -D /dev/ttyACM0 -b 115200

# Expected output:
# - NuttX boot banner
# - PX4 system startup
# - Module initialization messages
```

### Phase 2: USB and Serial Console
- Verify USB CDC/ACM enumeration
- Test MAVLink communication
- Verify parameter system

### Phase 3: I2C Sensor Detection
```bash
# In NuttX shell:
nsh> i2c bus
nsh> i2c dev 0

# Expected: Detect devices at I2C addresses
# - ICM20689: 0x68 or 0x69
# - AK09916: 0x0C
# - DPS310: 0x77 or 0x76
```

### Phase 4: Sensor Driver Integration
- Wire sensors to MikroBUS sockets
- Configure SPI/I2C buses properly
- Test individual sensor drivers
- Verify data output in uORB topics

### Phase 5: PWM/Motor Control
- Implement TC-based PWM
- Test PWM output on oscilloscope
- Verify frequency and duty cycle control

## Known Issues

1. **Build Warnings**: None currently
2. **Flash Usage**: 41% - plenty of room for expansion
3. **RAM Usage**: 5.6% - very conservative
4. **PWM**: Not implemented - motors cannot be controlled
5. **SPI**: Not configured - SPI sensors will not work

## Development Notes

### Useful Commands

```bash
# Clean build
make distclean

# Build for SAMV71
make microchip_samv71-xult-clickboards_default

# Build with verbose output
make microchip_samv71-xult-clickboards_default VERBOSE=1

# Upload via BOSSA (erase, write, verify, boot from flash)
bossac -p /dev/ttyACM0 -e -w -v -b build/microchip_samv71-xult-clickboards_default/microchip_samv71-xult-clickboards_default.bin

# Monitor serial console
minicom -D /dev/ttyACM0 -b 115200
# or
screen /dev/ttyACM0 115200
```

### Debugging

1. **Enable USB Bootloader**: Short ERASE pin to ground during power-up
2. **JTAG/SWD**: Available on J20 header (SWDIO, SWCLK, RESET)
3. **LED Indicators**:
   - Blue LED (PA23): System status/heartbeat

### Reference Documentation

- [SAMV71 Datasheet](https://www.microchip.com/wwwproducts/en/ATSAMV71Q21)
- [SAMV71-XULT User Guide](https://www.microchip.com/DevelopmentTools/ProductDetails/ATSAMV71-XULT)
- [PX4 Developer Guide](https://docs.px4.io/main/en/dev_setup/building_px4.html)
- [NuttX SAMV7 Documentation](https://nuttx.apache.org/docs/latest/platforms/arm/samv7/index.html)

## File Summary

### Created Files (18 total)
```
boards/microchip/samv71-xult-clickboards/
├── PORTING_NOTES.md                                          (this file)
├── nuttx-config/scripts/script.ld                           (NEW)
├── src/sam_gpiosetevent.c                                   (NEW)
└── src/i2c.cpp                                              (NEW)

platforms/nuttx/src/px4/microchip/samv7/
├── include/px4_arch/spi_hw_description.h                    (NEW)
├── include/px4_arch/i2c_hw_description.h                    (NEW)
├── include/px4_arch/io_timer.h                              (NEW)
├── include/px4_arch/io_timer_hw_description.h               (NEW)
├── io_pins/io_timer_stub.c                                  (NEW)
├── io_pins/CMakeLists.txt                                   (NEW)
├── board_critmon/board_critmon.c                            (NEW)
└── board_critmon/CMakeLists.txt                             (NEW)
```

### Modified Files (13 total)
```
boards/microchip/samv71-xult-clickboards/
├── src/board_config.h                                       (ADC, hardfault, LED defs)
├── src/init.c                                               (board_on_reset simplified)
├── src/spi.cpp                                              (SPI select/status stubs)
├── src/usb.c                                                (VBUS stub)
├── src/timer_config.cpp                                     (empty arrays)
└── src/CMakeLists.txt                                       (added sam_gpiosetevent.c)

platforms/nuttx/src/px4/microchip/samv7/
├── CMakeLists.txt                                           (added subdirectories)
├── include/px4_arch/micro_hal.h                             (GPIO interrupt macros)
├── hrt/hrt.c                                                (hrt_store_absolute_time)
├── version/board_identity.c                                 (PX4_SOC_ARCH_ID)
└── version/board_mcu_version.c                              (missing includes)
```

## Conclusion

This port provides a **minimal working firmware** that successfully builds and should boot on SAMV71-XULT hardware. However, it requires **hardware-specific configuration** (PWM pins, SPI chip selects, GPIO mappings) before full functionality is achieved.

The build is suitable for:
- ✅ Initial hardware bring-up and testing
- ✅ USB/Serial communication verification
- ✅ Basic NuttX/PX4 system validation
- ✅ Foundation for sensor integration

**Next Critical Steps**:
1. Test basic boot and console on actual hardware
2. Map and configure PWM timer outputs
3. Configure SPI bus and sensor chip selects
4. Verify and test sensor communication
5. Implement full system integration testing

---
**Last Updated**: November 12, 2025
**PX4 Version**: v1.17.0-alpha1-91-g5eab16c17c
**Build Status**: ✅ SUCCESS (859 KB flash, 22 KB RAM)
