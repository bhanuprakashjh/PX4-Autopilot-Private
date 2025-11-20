# SAMV71-XULT PX4 Documentation Index

**Branch:** `samv7-custom`
**Last Updated:** November 20, 2025
**Status:** Safe Mode - Stable Baseline with Debugging Tools

---

## 📚 Quick Navigation

### 🚀 Getting Started
- **[README_SAMV7.md](README_SAMV7.md)** - Main README with branch overview and quick start

### 🔧 Core Implementation & Fixes
1. **[SAMV7_PARAM_STORAGE_FIX.md](SAMV7_PARAM_STORAGE_FIX.md)** - **⭐ CRITICAL** Manual construction fix for parameter storage
2. **[SAMV7_VTABLE_CORRUPTION_ANALYSIS.md](SAMV7_VTABLE_CORRUPTION_ANALYSIS.md)** - Root cause analysis of vtable corruption
3. **[KNOWN_GOOD_SAFE_MODE_CONFIG.md](KNOWN_GOOD_SAFE_MODE_CONFIG.md)** - **⭐ REFERENCE** Complete safe-mode configuration guide
4. **[CURRENT_STATUS.md](CURRENT_STATUS.md)** - **⭐ LATEST** Current state, working features, and known issues

### 🛠️ Hardware & Drivers
- **[SD_CARD_INTEGRATION_SUMMARY.md](SD_CARD_INTEGRATION_SUMMARY.md)** - SD card driver implementation
- **[HRT_IMPLEMENTATION_SUMMARY.md](HRT_IMPLEMENTATION_SUMMARY.md)** - High-resolution timer implementation
- **[CACHE_COHERENCY_GUIDE.md](CACHE_COHERENCY_GUIDE.md)** - DMA memory management for SAMV7
- **[I2C_VERIFICATION_GUIDE.md](I2C_VERIFICATION_GUIDE.md)** - I2C bus testing procedures

### 🐛 Debugging & Tools
- **[DOCS_DMESG_HRT_ENABLE.md](DOCS_DMESG_HRT_ENABLE.md)** - Enabling dmesg and hrt_test commands
- **[BOOT_ERROR_ANALYSIS.md](BOOT_ERROR_ANALYSIS.md)** - Boot sequence debugging
- **[attention_gemini.md](attention_gemini.md)** - Session-by-session progress log

### 📖 Technical Deep Dives
- **[SAMV7_ACHIEVEMENTS.md](SAMV7_ACHIEVEMENTS.md)** - Complete development journey and lessons learned
- **[RESEARCH_TOPICS.md](RESEARCH_TOPICS.md)** - Study guide (atomic operations, RTOS, etc.)
- **[PARAMETER_STORAGE_STRATEGY.md](PARAMETER_STORAGE_STRATEGY.md)** - PX4 parameter architecture explained
- **[KCONFIG_DEBUG_OPTIONS_WARNING.md](KCONFIG_DEBUG_OPTIONS_WARNING.md)** - NuttX Kconfig debug hierarchy

### 📋 Board-Specific Documentation
Located in `boards/microchip/samv71-xult-clickboards/`:
- **[HRT_IMPLEMENTATION_JOURNEY.md](boards/microchip/samv71-xult-clickboards/HRT_IMPLEMENTATION_JOURNEY.md)** - Complete HRT debugging process
- **[HRT_FIXES_APPLIED.md](boards/microchip/samv71-xult-clickboards/HRT_FIXES_APPLIED.md)** - Final working HRT solution
- **[HRT_APPROACH_COMPARISON.md](boards/microchip/samv71-xult-clickboards/HRT_APPROACH_COMPARISON.md)** - Design alternatives
- **[LITTLEFS_INVESTIGATION.md](boards/microchip/samv71-xult-clickboards/LITTLEFS_INVESTIGATION.md)** - Alternative filesystems evaluation

### 🧪 Testing & Integration
- **[SAMV71_HIL_TESTING_GUIDE.md](SAMV71_HIL_TESTING_GUIDE.md)** - Hardware-in-the-loop testing
- **[SAMV71_HIL_QUICKSTART.md](SAMV71_HIL_QUICKSTART.md)** - HIL quick start guide
- **[TESTING_GUIDE.md](TESTING_GUIDE.md)** - General testing procedures

### 📝 Legacy & Development Notes
- **[CONSTRAINED_FLASH_ANALYSIS.md](CONSTRAINED_FLASH_ANALYSIS.md)** - Memory optimization strategies
- **[SPI_IMPLEMENTATION_PLAN.md](SPI_IMPLEMENTATION_PLAN.md)** - SPI driver planning
- **[CONTEXT_FOR_CLAUDE.md](CONTEXT_FOR_CLAUDE.md)** - AI assistant context
- **[SESSION_CONTEXT.md](SESSION_CONTEXT.md)** - Session notes
- **[CODEX_SESSION_NOTES.md](CODEX_SESSION_NOTES.md)** - Additional session notes

---

## 🎯 Recommended Reading Order

### For New Contributors
1. Start with **[README_SAMV7.md](README_SAMV7.md)**
2. Read **[CURRENT_STATUS.md](CURRENT_STATUS.md)** for latest state
3. Review **[KNOWN_GOOD_SAFE_MODE_CONFIG.md](KNOWN_GOOD_SAFE_MODE_CONFIG.md)** as reference baseline

### For Understanding the Fixes
1. **[SAMV7_VTABLE_CORRUPTION_ANALYSIS.md](SAMV7_VTABLE_CORRUPTION_ANALYSIS.md)** - What went wrong
2. **[SAMV7_PARAM_STORAGE_FIX.md](SAMV7_PARAM_STORAGE_FIX.md)** - How we fixed it
3. **[SAMV7_ACHIEVEMENTS.md](SAMV7_ACHIEVEMENTS.md)** - Complete journey

### For Development Work
1. **[CURRENT_STATUS.md](CURRENT_STATUS.md)** - Current capabilities and limitations
2. **[DOCS_DMESG_HRT_ENABLE.md](DOCS_DMESG_HRT_ENABLE.md)** - Available debugging tools
3. **[attention_gemini.md](attention_gemini.md)** - Latest changes and next steps

---

## 📊 Documentation Statistics

- **Total Documentation Files:** 30+
- **Core Implementation Docs:** 4
- **Hardware/Driver Docs:** 4
- **Debugging Guides:** 3
- **Technical Deep Dives:** 4
- **Board-Specific Docs:** 4

---

## 🔍 Finding Information

**Search by Topic:**
- **Parameters:** `SAMV7_PARAM_STORAGE_FIX.md`, `PARAMETER_STORAGE_STRATEGY.md`
- **SD Card:** `SD_CARD_INTEGRATION_SUMMARY.md`, `KNOWN_GOOD_SAFE_MODE_CONFIG.md`
- **Boot Issues:** `BOOT_ERROR_ANALYSIS.md`, `attention_gemini.md`
- **HRT/Timers:** `HRT_IMPLEMENTATION_SUMMARY.md`, `DOCS_DMESG_HRT_ENABLE.md`
- **Safe Mode:** `KNOWN_GOOD_SAFE_MODE_CONFIG.md`, `CURRENT_STATUS.md`
- **Memory/Cache:** `CACHE_COHERENCY_GUIDE.md`, `CONSTRAINED_FLASH_ANALYSIS.md`

---

**For the latest status, always check:** [CURRENT_STATUS.md](CURRENT_STATUS.md) and [attention_gemini.md](attention_gemini.md)
