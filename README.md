# PX4 Drone Autopilot

[![Releases](https://img.shields.io/github/release/PX4/PX4-Autopilot.svg)](https://github.com/PX4/PX4-Autopilot/releases) [![DOI](https://zenodo.org/badge/22634/PX4/PX4-Autopilot.svg)](https://zenodo.org/badge/latestdoi/22634/PX4/PX4-Autopilot)

[![Build Targets](https://github.com/PX4/PX4-Autopilot/actions/workflows/build_all_targets.yml/badge.svg?branch=main)](https://github.com/PX4/PX4-Autopilot/actions/workflows/build_all_targets.yml) [![SITL Tests](https://github.com/PX4/PX4-Autopilot/workflows/SITL%20Tests/badge.svg?branch=master)](https://github.com/PX4/PX4-Autopilot/actions?query=workflow%3A%22SITL+Tests%22)

[![Discord Shield](https://discordapp.com/api/guilds/1022170275984457759/widget.png?style=shield)](https://discord.gg/dronecode)

This repository holds the [PX4](http://px4.io) flight control solution for drones, with the main applications located in the [src/modules](https://github.com/PX4/PX4-Autopilot/tree/main/src/modules) directory. It also contains the PX4 Drone Middleware Platform, which provides drivers and middleware to run drones.

PX4 is highly portable, OS-independent and supports Linux, NuttX and MacOS out of the box.

---

## Custom Board Support: ATSAMV71-XULT

This repository includes **custom PX4 implementations** for the **Microchip ATSAMV71-XULT development board** with comprehensive fixes and documentation.

### 🌿 Branch Structure

This repository maintains two branches for SAMV71-XULT development:

#### **`main`** - Original/Baseline Implementation (You are here)
Standard PX4 baseline for SAMV71 with basic hardware support.
- **Status:** Reference implementation
- **Use Case:** Starting point, comparison baseline
- **Documentation:** [boards/microchip/samv71-xult-clickboards/PORTING_NOTES.md](boards/microchip/samv71-xult-clickboards/PORTING_NOTES.md)

#### **`samv7-custom`** - Enhanced Implementation ⭐ **Recommended for Development**
Fully working implementation with critical bug fixes and comprehensive documentation.

**✅ Major Fixes Implemented:**
- **Parameter Storage Fix** - Manual construction pattern fixes C++ static initialization bug
- **SD Card Integration** - Complete FAT32 support with parameter persistence
- **HRT Implementation** - Reliable TC0 timer configuration
- **DMA Cache Coherency** - Proper memory management for SDIO
- **Safe Mode Configuration** - Stable baseline for incremental development

**✅ Current Status (November 2025):**
- Flash: 920 KB / 2 MB (43.91%)
- SRAM: 18 KB / 384 KB (4.75%)
- Parameter set/save/persistence: Working
- SD card I/O: Working
- HRT self-test: Passing
- Debugging tools: dmesg, hrt_test enabled

**📚 Complete Documentation Suite (30+ documents):**

Navigate all documentation starting from these entry points:
- **[DOCUMENTATION_INDEX.md](https://github.com/bhanuprakashjh/PX4-Autopilot-Private/blob/samv7-custom/DOCUMENTATION_INDEX.md)** - Central navigation hub
- **[CURRENT_STATUS.md](https://github.com/bhanuprakashjh/PX4-Autopilot-Private/blob/samv7-custom/CURRENT_STATUS.md)** - Latest system state
- **[README_SAMV7.md](https://github.com/bhanuprakashjh/PX4-Autopilot-Private/blob/samv7-custom/README_SAMV7.md)** - Branch overview
- **[SAMV7_PARAM_STORAGE_FIX.md](https://github.com/bhanuprakashjh/PX4-Autopilot-Private/blob/samv7-custom/SAMV7_PARAM_STORAGE_FIX.md)** - Critical fixes
- **[KNOWN_GOOD_SAFE_MODE_CONFIG.md](https://github.com/bhanuprakashjh/PX4-Autopilot-Private/blob/samv7-custom/KNOWN_GOOD_SAFE_MODE_CONFIG.md)** - Reference config

### 🚀 Quick Start - Switch to Enhanced Branch

**⚠️ Note:** For active development, use the `samv7-custom` branch which has all fixes and documentation.

```bash
# Clone repository
git clone https://github.com/bhanuprakashjh/PX4-Autopilot-Private.git
cd PX4-Autopilot-Private

# Switch to enhanced branch (recommended)
git checkout samv7-custom
git submodule update --init --recursive

# Build firmware
make microchip_samv71-xult-clickboards_default

# Flash via OpenOCD
openocd -f interface/cmsis-dap.cfg -f target/atsamv.cfg \
  -c "program build/microchip_samv71-xult-clickboards_default/microchip_samv71-xult-clickboards_default.elf verify reset exit"
```

### Hardware Configuration
- **MCU:** ATSAM V71Q21 (ARM Cortex-M7 @ 300MHz)
- **Flash:** 2MB
- **SRAM:** 384KB
- **SD Card:** FAT32 support (tested with 16GB)
- **Sensors:** I2C bus ready for ICM20689 IMU, magnetometer, barometer

### 🎯 Recommended: Use samv7-custom Branch

The **samv7-custom** branch contains:
- ✅ All critical bug fixes
- ✅ Working parameter storage
- ✅ SD card integration
- ✅ Comprehensive documentation (30+ files)
- ✅ Debugging tools enabled
- ✅ Known-good baseline configuration

**View the enhanced branch:** [Switch to samv7-custom](https://github.com/bhanuprakashjh/PX4-Autopilot-Private/tree/samv7-custom)

---

* Official Website: http://px4.io (License: BSD 3-clause, [LICENSE](https://github.com/PX4/PX4-Autopilot/blob/main/LICENSE))
* [Supported airframes](https://docs.px4.io/main/en/airframes/airframe_reference.html) ([portfolio](https://px4.io/ecosystem/commercial-systems/)):
  * [Multicopters](https://docs.px4.io/main/en/frames_multicopter/)
  * [Fixed wing](https://docs.px4.io/main/en/frames_plane/)
  * [VTOL](https://docs.px4.io/main/en/frames_vtol/)
  * [Autogyro](https://docs.px4.io/main/en/frames_autogyro/)
  * [Rover](https://docs.px4.io/main/en/frames_rover/)
  * many more experimental types (Blimps, Boats, Submarines, High Altitude Balloons, Spacecraft, etc)
* Releases: [Downloads](https://github.com/PX4/PX4-Autopilot/releases)

## Releases

Release notes and supporting information for PX4 releases can be found on the [Developer Guide](https://docs.px4.io/main/en/releases/).

## Building a PX4 based drone, rover, boat or robot

The [PX4 User Guide](https://docs.px4.io/main/en/) explains how to assemble [supported vehicles](https://docs.px4.io/main/en/airframes/airframe_reference.html) and fly drones with PX4. See the [forum and chat](https://docs.px4.io/main/en/#getting-help) if you need help!


## Changing Code and Contributing

This [Developer Guide](https://docs.px4.io/main/en/development/development.html) is for software developers who want to modify the flight stack and middleware (e.g. to add new flight modes), hardware integrators who want to support new flight controller boards and peripherals, and anyone who wants to get PX4 working on a new (unsupported) airframe/vehicle.

Developers should read the [Guide for Contributions](https://docs.px4.io/main/en/contribute/).
See the [forum and chat](https://docs.px4.io/main/en/#getting-help) if you need help!


## Weekly Dev Call

The PX4 Dev Team syncs up on a [weekly dev call](https://docs.px4.io/main/en/contribute/).

> **Note** The dev call is open to all interested developers (not just the core dev team). This is a great opportunity to meet the team and contribute to the ongoing development of the platform. It includes a QA session for newcomers. All regular calls are listed in the [Dronecode calendar](https://www.dronecode.org/calendar/).


## Maintenance Team

See the latest list of maintainers on [MAINTAINERS](MAINTAINERS.md) file at the root of the project.

For the latest stats on contributors please see the latest stats for the Dronecode ecosystem in our project dashboard under [LFX Insights](https://insights.lfx.linuxfoundation.org/foundation/dronecode). For information on how to update your profile and affiliations please see the following support link on how to [Complete Your LFX Profile](https://docs.linuxfoundation.org/lfx/my-profile/complete-your-lfx-profile). Dronecode publishes a yearly snapshot of contributions and achievements on its [website under the Reports section](https://dronecode.org).

## Supported Hardware

For the most up to date information, please visit [PX4 User Guide > Autopilot Hardware](https://docs.px4.io/main/en/flight_controller/).

## Project Governance

The PX4 Autopilot project including all of its trademarks is hosted under [Dronecode](https://www.dronecode.org/), part of the Linux Foundation.

<a href="https://www.dronecode.org/" style="padding:20px" ><img src="https://dronecode.org/wp-content/uploads/sites/24/2020/08/dronecode_logo_default-1.png" alt="Dronecode Logo" width="110px"/></a>
<div style="padding:10px">&nbsp;</div>
