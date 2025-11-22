# SAMV71-XULT PX4 Custom Implementation

This repository contains custom PX4 implementations for the Microchip SAMV71-XULT-Clickboards development board.

## 🌿 Branch Structure

### `main` - Original/Upstream Code
- Standard PX4 baseline for SAMV71
- May have parameter storage and HRT timing issues
- Use for reference or to revert to original

### `samv7-custom` - Enhanced Implementation ⭐ **Recommended**
- ✅ **SD Card DMA Fully Working** - Complete DMA transfer support (Fixes #1-#21)
- ✅ **SD Card Parameter Storage** - Read/write operations verified
- ✅ **Fixed HRT Implementation** - Reliable TC0 timer configuration
- ✅ **DMA Cache Coherency** - Proper memory management for SDIO
- ✅ **Hardware Handshaking** - XDMAC properly synchronized with HSMCI
- ✅ **Comprehensive Documentation** - 25+ implementation guides including complete DMA debugging journey

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

### SD Card DMA Implementation (✅ COMPLETE)
- **[SAMV71_SD_CARD_DMA_SUCCESS.md](SAMV71_SD_CARD_DMA_SUCCESS.md)** - ⭐ Complete DMA fix documentation with proof of success
- **[SAMV71_HSMCI_MICROCHIP_COMPARISON_FIXES.md](SAMV71_HSMCI_MICROCHIP_COMPARISON_FIXES.md)** - Fixes #12-#19 detailed analysis
- **[SAMV71_SD_CARD_KNOWN_ISSUES.md](SAMV71_SD_CARD_KNOWN_ISSUES.md)** - Unresolved error interrupts (OVRE/UNRE/DTOE)
- **[SAMV7_HSMCI_SD_CARD_DEBUG_INVESTIGATION.md](SAMV7_HSMCI_SD_CARD_DEBUG_INVESTIGATION.md)** - Complete 4-day debugging journey

### Implementation Guides
- **[SAMV7_PARAM_STORAGE_FIX.md](SAMV7_PARAM_STORAGE_FIX.md)** - Complete solution for parameter storage bug
- **[SAMV7_ACHIEVEMENTS.md](SAMV7_ACHIEVEMENTS.md)** - Full progress log and lessons learned
- **[SD_CARD_INTEGRATION_SUMMARY.md](SD_CARD_INTEGRATION_SUMMARY.md)** - SD card driver implementation (PIO mode)
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

- ✅ **SD Card DMA** - Hardware-synchronized DMA transfers working perfectly
- ✅ **SD Card Storage** - 15.5GB FAT32 mounted at `/fs/microsd`
- ✅ **Parameter Persistence** - Save/load verified with DMA writes
- ✅ **File I/O** - Read/write operations verified (echo, cat, ls all working)
- ✅ **HRT Self-Test** - Passing consistently with TC0 timer
- ✅ **Boot Sequence** - Clean boot to NSH prompt
- ✅ **Parameters Load** - 376/1081 from compiled defaults
- ✅ **I2C Bus** - `/dev/i2c0` ready for sensors
- ✅ **Documentation** - 30,000+ lines of implementation guides

### Proof of Success
```bash
nsh> echo "SD card DMA is working!" > /fs/microsd/test.txt
nsh> cat /fs/microsd/test.txt
SD card DMA is working!
```

## ⚠️ Known Issues (samv7-custom branch)

### Potential DMA Error Interrupts (Under Investigation)

**Issue:** Three error interrupts enabled but root causes not yet investigated:
1. **OVRE (Overrun Error)** - FIFO overflow during RX (could indicate data loss)
2. **UNRE (Underrun Error)** - FIFO underrun during TX (could indicate corrupted writes)
3. **DTOE (Data Timeout Error)** - Transfer timeout (could indicate incomplete data)

**Current Status:**
- ✅ Error interrupts enabled (Fix #16)
- ✅ Errors logged but don't abort transfers (Fix #17, matches Microchip approach)
- ⚠️ Unknown if these errors actually occur under normal operation
- ⚠️ Root causes not yet investigated

**Impact:**
- SD card works perfectly in testing
- No errors observed during basic read/write operations
- Need stress testing under heavy load to verify

**Next Steps:**
- Add enhanced error logging to detect if errors occur
- Stress test with large files and rapid read/write cycles
- Investigate root cause if errors detected
- See [SAMV71_SD_CARD_KNOWN_ISSUES.md](SAMV71_SD_CARD_KNOWN_ISSUES.md) for full details

**Risk Level:** ⚠️ Medium - Works now, but needs validation under production load

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

1. ✅ ~~**SD Card DMA Fix**~~ - **COMPLETE!** All 21 fixes applied and tested
2. **Validate DMA Under Load** - Stress test with continuous logging
3. **Monitor Error Interrupts** - Add logging for OVRE/UNRE/DTOE detection
4. **Full System Integration** - Test complete autopilot stack with SD logging
5. **Sensor Integration** - Add ICM20689 IMU support
6. **Production Testing** - 24+ hour stability test with data logging
7. **Upstream Contribution** - Submit HSMCI DMA fixes to NuttX/PX4

## 📋 TODO List

### SD Card DMA
- [x] ~~Fix D-cache invalidation (Fix #1)~~
- [x] ~~Fix BLKR register handling (Fix #2)~~
- [x] ~~Implement cached block parameters (Fix #4)~~
- [x] ~~Guard sam_notransfer() (Fix #12)~~
- [x] ~~Always use FIFO (Fix #13)~~
- [x] ~~CFG register - FERRCTRL instead of LSYNC (Fix #14)~~
- [x] ~~Preserve WRPROOF/RDPROOF bits (Fix #15)~~
- [x] ~~Remove SWREQ, enable hardware handshaking (Fix #21)~~
- [x] ~~Verify file I/O working~~
- [ ] Add enhanced error logging for OVRE/UNRE/DTOE
- [ ] Stress test with large file writes
- [ ] Monitor for 24+ hours under typical workload

### System Integration
- [ ] Re-enable dataman service
- [ ] Re-enable logger service with SD card backend
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
