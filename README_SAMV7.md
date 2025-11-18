# SAMV71-XULT PX4 Custom Implementation

This repository contains custom PX4 implementations for the Microchip SAMV71-XULT-Clickboards development board.

## 🌿 Branch Structure

### `main` - Original/Upstream Code
- Standard PX4 baseline for SAMV71
- May have parameter storage and HRT timing issues
- Use for reference or to revert to original

### `samv7-custom` - Enhanced Implementation ⭐ **Recommended**
- ✅ **SD Card Parameter Storage** - Migrated from broken flash backend
- ✅ **Fixed HRT Implementation** - Reliable TC0 timer configuration
- ✅ **DMA Cache Coherency** - Proper memory management for SDIO
- ✅ **Comprehensive Documentation** - 20+ implementation guides
- ⚠️ **Known Issue:** Parameter writes blocked by SAMV7 C++ static init bug
- 🔧 **Solution Designed:** Lazy allocation fix ready for implementation

## 🚀 Quick Start

### Option 1: Build Enhanced Version (Recommended)

```bash
# Clone repository
git clone https://github.com/YOUR_USERNAME/PX4-Autopilot-Private.git
cd PX4-Autopilot-Private

# Switch to custom implementation
git checkout samv7-custom

# Update submodules (NuttX)
git submodule update --init --recursive

# Build firmware
make microchip_samv71-xult-clickboards_default

# Flash to board
make microchip_samv71-xult-clickboards_default upload
```

### Option 2: Build Original Version

```bash
# Clone repository
git clone https://github.com/YOUR_USERNAME/PX4-Autopilot-Private.git
cd PX4-Autopilot-Private

# Stay on main branch (or explicitly: git checkout main)

# Update submodules
git submodule update --init --recursive

# Build firmware
make microchip_samv71-xult-clickboards_default
```

### Switching Between Versions

```bash
# Switch to enhanced version
git checkout samv7-custom
git submodule update --init --recursive
make clean
make microchip_samv71-xult-clickboards_default

# Switch back to original
git checkout main
git submodule update --init --recursive
make clean
make microchip_samv71-xult-clickboards_default
```

## 📖 Documentation (samv7-custom branch)

### Implementation Guides
- **[SAMV7_PARAM_STORAGE_FIX.md](SAMV7_PARAM_STORAGE_FIX.md)** - Complete solution for parameter storage bug
- **[SAMV7_ACHIEVEMENTS.md](SAMV7_ACHIEVEMENTS.md)** - Full progress log and lessons learned
- **[SD_CARD_INTEGRATION_SUMMARY.md](SD_CARD_INTEGRATION_SUMMARY.md)** - SD card driver implementation
- **[CACHE_COHERENCY_GUIDE.md](CACHE_COHERENCY_GUIDE.md)** - DMA memory management for SAMV7

### Technical Deep-Dives
- **[RESEARCH_TOPICS.md](RESEARCH_TOPICS.md)** - Study guide for embedded concepts (atomic operations, RTOS, etc.)
- **[PARAMETER_STORAGE_STRATEGY.md](PARAMETER_STORAGE_STRATEGY.md)** - PX4 parameter architecture explained
- **[KCONFIG_DEBUG_OPTIONS_WARNING.md](KCONFIG_DEBUG_OPTIONS_WARNING.md)** - NuttX Kconfig debug hierarchy

### HRT Implementation Journey
- **[boards/microchip/samv71-xult-clickboards/HRT_IMPLEMENTATION_JOURNEY.md](boards/microchip/samv71-xult-clickboards/HRT_IMPLEMENTATION_JOURNEY.md)** - Complete debugging process
- **[boards/microchip/samv71-xult-clickboards/HRT_FIXES_APPLIED.md](boards/microchip/samv71-xult-clickboards/HRT_FIXES_APPLIED.md)** - Final working solution
- **[boards/microchip/samv71-xult-clickboards/HRT_APPROACH_COMPARISON.md](boards/microchip/samv71-xult-clickboards/HRT_APPROACH_COMPARISON.md)** - Design alternatives

### Investigation & Analysis
- **[LITTLEFS_INVESTIGATION.md](boards/microchip/samv71-xult-clickboards/LITTLEFS_INVESTIGATION.md)** - Alternative filesystems evaluated
- **[BOOT_ERROR_ANALYSIS.md](BOOT_ERROR_ANALYSIS.md)** - Boot sequence debugging
- **[CONSTRAINED_FLASH_ANALYSIS.md](CONSTRAINED_FLASH_ANALYSIS.md)** - Memory optimization strategies

## ✅ What's Working (samv7-custom branch)

- ✅ **SD Card Driver** - 15.5GB FAT32 mounted at `/fs/microsd`
- ✅ **HRT Self-Test** - Passing consistently with TC0 timer
- ✅ **Boot Sequence** - Clean boot to NSH prompt
- ✅ **Parameters Load** - 376/1081 from compiled defaults
- ✅ **File I/O** - Read/write operations verified
- ✅ **I2C Bus** - `/dev/i2c0` ready for sensors
- ✅ **Documentation** - 21,000+ lines of implementation guides

## ⚠️ Known Issues (samv7-custom branch)

### SAMV7 C++ Static Initialization Bug

**Problem:**
- `param set` fails with "failed to store param" error
- `param save` creates 5-byte files (BSON header only, no data)
- Root cause: `malloc()` fails during C++ static construction phase
- Result: Parameter storage layers have zero allocated slots

**Status:** ✅ Solution designed, implementation pending

**Solution:** Lazy allocation in `DynamicSparseLayer.h`
- Defer `malloc()` from constructor to first use
- Thread-safe initialization using `compare_exchange`
- Complete implementation plan in [SAMV7_PARAM_STORAGE_FIX.md](SAMV7_PARAM_STORAGE_FIX.md)

**Impact:** Parameters load correctly but cannot be modified or saved yet.

**Workaround:** None currently - awaiting lazy allocation implementation.

## 🔧 Build Information

### Hardware Requirements
- **Board:** Microchip SAMV71-XULT-Clickboards
- **MCU:** ATSAM V71Q21 (Cortex-M7 @ 300MHz)
- **Flash:** 2MB (43.9% used by firmware)
- **SRAM:** 384KB (4.75% used at boot)
- **SD Card:** FAT32 formatted (tested with 16GB card)

### Software Requirements
- **Toolchain:** GCC 13.2.1 ARM (or newer)
- **CMake:** 3.16+
- **Python:** 3.6+
- **Git:** For submodule management

### Build Statistics
```
Memory region         Used Size  Region Size  %age Used
           flash:      920KB         2 MB     43.9%
            sram:       18KB       384 KB      4.75%
```

## 🎯 Next Steps (Development Roadmap)

1. **Implement Lazy Allocation** - Fix parameter storage (2-4 hours)
2. **Test Parameter Persistence** - Verify set/save/reboot cycle
3. **Re-enable Boot Services** - Restore dataman, logger, mavlink
4. **Full System Integration** - Test complete autopilot stack
5. **Sensor Integration** - Add ICM20689 IMU support
6. **Upstream Contribution** - Submit lazy allocation fix to PX4

## 📋 TODO List

- [ ] Implement `_ensure_allocated()` in `DynamicSparseLayer.h`
- [ ] Add thread-safe lazy initialization
- [ ] Test `param set` functionality
- [ ] Test `param save` and verify file size > 5 bytes
- [ ] Verify parameter persistence across reboot
- [ ] Re-enable dataman service
- [ ] Re-enable logger service
- [ ] Re-enable mavlink autostart
- [ ] Full autopilot stack testing

## 🤝 Contributing

This is a personal development repository for SAMV71 board support. If you find the solutions useful:

1. The **lazy allocation fix** should be contributed upstream to PX4
2. The **HRT implementation** may help other SAMV7 ports
3. The **documentation** can guide similar embedded debugging efforts

## 📚 Learning Resources

See [RESEARCH_TOPICS.md](RESEARCH_TOPICS.md) for:
- Atomic operations and lock-free programming
- C++ static initialization issues
- NuttX RTOS concepts
- PX4 parameter architecture
- ARM Cortex-M7 specifics
- Embedded debugging techniques

## 🔗 References

- **PX4 Autopilot:** https://px4.io/
- **PX4 Documentation:** https://docs.px4.io/main/en/
- **NuttX RTOS:** https://nuttx.apache.org/
- **SAMV71 Datasheet:** Microchip DS60001527E
- **PX4 GitHub:** https://github.com/PX4/PX4-Autopilot

## 📝 License

Same as PX4-Autopilot (BSD 3-Clause)

## 🙏 Acknowledgments

- **PX4 Development Team** - For comprehensive autopilot framework
- **NuttX Community** - For SAMV7 RTOS support
- **Microchip** - For SAMV71-XULT development board
- **Existing Code Comments** - Especially the SAMV7 workaround notes

---

**Status:** Active Development | **Last Updated:** November 2025 | **Board:** SAMV71-XULT-Clickboards
