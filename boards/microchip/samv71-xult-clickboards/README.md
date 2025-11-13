# PX4 on Microchip ATSAMV71-XULT Development Board

This directory contains the PX4 Autopilot port for the Microchip ATSAMV71Q21 microcontroller using the SAMV71-XULT development board with Click sensor boards.

## Hardware Overview

**Development Board:** SAMV71-XULT (Atmel/Microchip SAMV71 Xplained Ultra)

**Microcontroller:** ATSAMV71Q21
- **CPU:** ARM Cortex-M7 @ 300 MHz
- **Flash:** 2 MB
- **RAM:** 384 KB SRAM
- **FPU:** Yes (double precision)
- **MPU:** Yes
- **Caches:** I-Cache and D-Cache

**Supported Sensors:**
- **AK09916** - Magnetometer (I2C, address 0x0C)
- **DPS310** - Barometer (I2C, address 0x77)
- **ICM20689** - IMU (SPI - requires configuration)

**Interfaces:**
- I2C (TWIHS0) - Bus 0 for sensor boards
- SPI (SPI0/SPI1) - Available but not yet configured
- UART1 - Serial console (115200 baud)
- USB Device - CDC/ACM virtual serial port
- microSD card slot

## Current Status

### ✅ Working Features
- [x] NuttX 11.0.0 boots successfully
- [x] PX4 firmware initialization
- [x] ROMFS with CROMFS compression
- [x] Parameter system (394 parameters)
- [x] Logger module
- [x] Commander (flight controller)
- [x] Sensors module
- [x] EKF2 state estimator
- [x] MAVLink communication
- [x] Flight control modules (MC attitude, position, rate control)
- [x] Navigator module
- [x] I2C bus (TWIHS0)
- [x] Serial console (UART1)
- [x] USB CDC/ACM
- [x] microSD card support
- [x] NSH shell with scripting

### ⚠️ Partially Implemented
- [ ] SPI buses (hardware available, needs pin mapping configuration)
- [ ] Flash parameter storage (MTD partitions need setup)
- [ ] PWM outputs (timers available, needs configuration)
- [ ] ADC (hardware available, needs channel mapping)

### 🚧 Not Implemented Yet
- [ ] Additional UART ports
- [ ] Ethernet (hardware available)
- [ ] CAN bus
- [ ] Hardware timers for input capture

## Build Instructions

### Prerequisites

**Required tools:**
- ARM GCC toolchain (arm-none-eabi-gcc 13.2.1 or compatible)
- Python 3.6+
- Git
- CMake 3.15+
- Make or Ninja build system

**Install on Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install -y \
    git \
    cmake \
    build-essential \
    python3 \
    python3-pip \
    ninja-build \
    gcc-arm-none-eabi \
    gdb-multiarch \
    openocd
```

**Install Python dependencies:**
```bash
pip3 install --user -r Tools/setup/requirements.txt
```

### Clone and Build

1. **Clone the repository:**
```bash
git clone <your-repo-url>
cd PX4-Autopilot-Private
git submodule update --init --recursive
```

2. **Build the firmware:**
```bash
make microchip_samv71-xult-clickboards_default
```

This will:
- Configure NuttX for SAMV71
- Build PX4 modules
- Generate ROMFS with initialization scripts
- Create firmware binary

**Build artifacts:**
- `build/microchip_samv71-xult-clickboards_default/microchip_samv71-xult-clickboards_default.elf`
- `build/microchip_samv71-xult-clickboards_default/microchip_samv71-xult-clickboards_default.bin`
- `build/microchip_samv71-xult-clickboards_default/microchip_samv71-xult-clickboards_default.px4`

3. **Clean build (if needed):**
```bash
make microchip_samv71-xult-clickboards_default clean
make microchip_samv71-xult-clickboards_default
```

## Flashing

### Hardware Setup

1. Connect SAMV71-XULT board to PC via USB (J-Link debugger port)
2. Power the board (USB or external power)
3. Connect serial console to UART1 (or use USB CDC/ACM after first boot)

### Using OpenOCD

**Flash the firmware:**
```bash
cd /path/to/PX4-Autopilot-Private
openocd -f interface/cmsis-dap.cfg -f target/atsamv.cfg \
    -c "program build/microchip_samv71-xult-clickboards_default/microchip_samv71-xult-clickboards_default.elf verify reset exit"
```

**Alternative - using make (if openocd is configured):**
```bash
make microchip_samv71-xult-clickboards_default upload
```

### Serial Console

**Connect to serial console:**
```bash
# Using UART1 (115200 baud, 8N1)
minicom -D /dev/ttyUSB0 -b 115200

# Or using screen
screen /dev/ttyUSB0 115200

# Or using USB CDC/ACM (after boot)
minicom -D /dev/ttyACM0 -b 115200
```

## Testing

### Basic System Test

After flashing and booting, you should see:

```
NuttShell (NSH) NuttX-11.0.0
nsh>
```

**Run these commands to verify functionality:**

```bash
# Show version information
ver all

# Check running tasks
top

# Show system messages
dmesg

# Check parameter system
param show | head -20

# Check module status
logger status
commander status
sensors status

# List available commands
?

# Check I2C bus
i2c bus

# Show filesystem
ls /
ls /fs/microsd
```

### Expected Output

**Successful boot shows:**
- NuttX boots without hard faults
- PX4 initializes all modules
- Logger starts and creates `/fs/microsd/log` directory
- Commander enters disarmed state
- Sensors module initializes (no sensors detected if hardware not connected)
- 394/1083 parameters loaded

**Expected warnings (normal without sensor hardware):**
- `WARN [SPI_I2C] ak09916: no instance started (no device on bus?)`
- `WARN [SPI_I2C] dps310: no instance started (no device on bus?)`
- `ERROR [gps] invalid device (-d) /dev/ttyS2` (UART not configured)

## Development

### Adding New Sensors

**I2C Sensors:**

1. Edit `boards/microchip/samv71-xult-clickboards/init/rc.board_sensors`
2. Add sensor startup command:
```bash
# Example for new I2C sensor
my_sensor start -I -b 0 -a 0x1E
```
3. Rebuild and flash

**SPI Sensors:**

1. Configure SPI buses in `boards/microchip/samv71-xult-clickboards/src/spi.cpp`:
```cpp
constexpr px4_spi_bus_t px4_spi_buses[SPI_BUS_MAX_BUS_ITEMS] = {
    initSPIBus(SPI::Bus::SPI0, {
        initSPIDevice(DRV_IMU_DEVTYPE_ICM20689, SPI::CS{GPIO::PortA, GPIO::Pin11}),
    }),
};
```

2. Update chip select functions in `spi.cpp`
3. Add sensor to `rc.board_sensors`:
```bash
icm20689 start -s -b 0 -c 1
```

### Enabling Additional Modules

**Edit board configuration:**
```bash
boards/microchip/samv71-xult-clickboards/default.px4board
```

Add module configuration:
```cmake
CONFIG_MODULES_MY_MODULE=y
```

**Or edit NuttX config:**
```bash
boards/microchip/samv71-xult-clickboards/nuttx-config/nsh/defconfig
```

### Debugging

**Using OpenOCD and GDB:**

Terminal 1 - Start OpenOCD:
```bash
openocd -f interface/cmsis-dap.cfg -f target/atsamv.cfg
```

Terminal 2 - Connect GDB:
```bash
arm-none-eabi-gdb build/microchip_samv71-xult-clickboards_default/microchip_samv71-xult-clickboards_default.elf
(gdb) target remote :3333
(gdb) monitor reset halt
(gdb) load
(gdb) break main
(gdb) continue
```

**Analyzing crashes:**

If hard fault occurs, use crash info to find location:
```bash
# Get PC (program counter) address from crash dump
arm-none-eabi-addr2line -e build/microchip_samv71-xult-clickboards_default/microchip_samv71-xult-clickboards_default.elf 0x00420abc
```

## Known Issues

### C++ Static Initialization Bug

**Issue:** SAMV7 toolchain has a bug where brace-initialized static variables with pointer/object initializers are zero-initialized instead of using specified values.

**Affected code:**
- Parameter system layer initialization
- WorkQueueManager initialization

**Workaround applied:** Null pointer checks in `DynamicSparseLayer::get()` to return default values instead of dereferencing null parent pointers.

**Location:** `src/lib/parameters/DynamicSparseLayer.h:126-128`

**Proper fix (future):** Use placement new with aligned storage for explicit runtime initialization (commented out due to compilation issues).

### Missing Features

**Flash Parameter Storage:**
- MTD partitions `/fs/mtd_caldata` and `/fs/mtd_params` need configuration
- Currently parameters stored only in RAM (lost on reboot)
- Requires NuttX MTD driver setup for internal flash

**PWM Outputs:**
- Hardware timers available (TC0-TC2)
- Pin mapping needs configuration in `timer_config.cpp`
- PWM module not yet enabled

## Contributing

### Testing Checklist

Before submitting changes, verify:

- [ ] Clean build succeeds
- [ ] Firmware boots without crashes
- [ ] Logger starts successfully
- [ ] Commander initializes
- [ ] Sensors module runs
- [ ] Parameters load correctly
- [ ] No new hard faults introduced
- [ ] Serial console accessible
- [ ] USB CDC/ACM works

### Code Style

Follow PX4 coding standards:
- Use tabs for indentation
- 120 character line limit
- C++14 standard
- Run `make format` before committing

### Submitting Changes

1. Create feature branch: `git checkout -b feature/my-improvement`
2. Make changes and test thoroughly
3. Commit with descriptive message
4. Push and create pull request

### Areas Needing Contribution

**High Priority:**
1. SPI bus configuration (enable ICM20689 and other SPI sensors)
2. Flash MTD partitions (persistent parameter storage)
3. PWM output configuration (motor control)
4. Additional UART ports (GPS, telemetry)

**Medium Priority:**
1. ADC channel mapping (battery monitoring)
2. Ethernet support
3. CAN bus support
4. Hardware timer input capture (PPM, S.BUS)

**Low Priority:**
1. Performance optimization
2. Additional sensor drivers
3. Board-specific features

## Resources

**Documentation:**
- [PX4 Developer Guide](https://docs.px4.io/main/en/)
- [NuttX Documentation](https://nuttx.apache.org/docs/latest/)
- [SAMV71 Datasheet](https://www.microchip.com/en-us/product/ATSAMV71Q21)
- [SAMV71-XULT User Guide](https://www.microchip.com/en-us/development-tool/ATSAMV71-XULT)

**Hardware:**
- SAMV71-XULT schematic available from Microchip website
- mikroBUS Click boards for additional sensors
- Arduino headers for prototyping

## License

This port follows PX4 licensing:
- PX4 stack: BSD 3-Clause License
- NuttX RTOS: Apache License 2.0

## Support

For questions and issues:
1. Check existing GitHub issues
2. Review PX4 Discuss forum
3. Consult NuttX mailing list for RTOS-specific questions

## Changelog

### 2025-11-13 - Initial Working Port

**Added:**
- Complete PX4 port to ATSAMV71Q21
- NuttX 11.0.0 configuration
- I2C sensor support (AK09916, DPS310)
- USB CDC/ACM support
- microSD card support
- CROMFS for ROMFS compression
- NSH scripting with if/then/else support
- I2C tools for debugging

**Fixed:**
- C++ static initialization bug in parameter system
- NSH scripting disabled issue
- If/then/else flow control disabled
- Missing unset command
- Sensor startup command syntax

**Known Limitations:**
- SPI not configured (ICM20689 disabled)
- Flash parameter storage not implemented
- PWM outputs not configured
- Only UART1 enabled for console

---

**Maintainer:** [Your Name]
**Last Updated:** 2025-11-13
**PX4 Version:** 1.17.0
**NuttX Version:** 11.0.0
