# SAMV71-XULT PX4 Documentation Index

**Branch:** `samv7-custom`
**Last Updated:** November 23, 2025
**Status:** USB CDC/ACM Verified - Ready for Service Re-enablement

---

## 📚 Quick Navigation

### 🚀 Getting Started
- **[README_SAMV7.md](README_SAMV7.md)** - Main README with branch overview and quick start
- **[SESSION_CONTEXT_NEXT_STEPS.md](SESSION_CONTEXT_NEXT_STEPS.md)** - **⭐ SESSION RESUMPTION** - Read this first!

### 📋 Project Planning & Roadmap
- **[SAMV71_PROJECT_PLAN.md](SAMV71_PROJECT_PLAN.md)** - **⭐ SPRINT PLAN** - Sprint 3-10 breakdown with timeline
- **[SAMV71_COMPLETE_TASK_LIST.md](SAMV71_COMPLETE_TASK_LIST.md)** - **⭐ MASTER LIST** - All working/disabled features and remaining tasks
- **[SAMV71_FEATURE_RE_ENABLEMENT_ROADMAP.md](SAMV71_FEATURE_RE_ENABLEMENT_ROADMAP.md)** - **⭐ NEXT STEPS** - 5-phase service re-enablement plan

### 🔧 Core Implementation & Fixes
1. **[SAMV7_PARAM_STORAGE_FIX.md](SAMV7_PARAM_STORAGE_FIX.md)** - **⭐ CRITICAL** Manual construction fix for parameter storage
2. **[SAMV7_VTABLE_CORRUPTION_ANALYSIS.md](SAMV7_VTABLE_CORRUPTION_ANALYSIS.md)** - Root cause analysis of vtable corruption
3. **[KNOWN_GOOD_SAFE_MODE_CONFIG.md](KNOWN_GOOD_SAFE_MODE_CONFIG.md)** - **⭐ REFERENCE** Complete safe-mode configuration guide
4. **[CURRENT_STATUS.md](CURRENT_STATUS.md)** - Current state, working features, and known issues

### 🛠️ Hardware & Drivers
- **[SAMV71_USB_CDC_IMPLEMENTATION.md](SAMV71_USB_CDC_IMPLEMENTATION.md)** - **⭐ NEW** USB CDC/ACM with MAVLink verification
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

### For New Contributors (Start Here!)
1. **[README_SAMV7.md](README_SAMV7.md)** - Branch overview and quick start
2. **[SAMV71_COMPLETE_TASK_LIST.md](SAMV71_COMPLETE_TASK_LIST.md)** - Complete feature status and roadmap
3. **[KNOWN_GOOD_SAFE_MODE_CONFIG.md](KNOWN_GOOD_SAFE_MODE_CONFIG.md)** - Reference baseline configuration

### For Resuming Work
1. **[SESSION_CONTEXT_NEXT_STEPS.md](SESSION_CONTEXT_NEXT_STEPS.md)** - Current state and immediate next steps
2. **[SAMV71_FEATURE_RE_ENABLEMENT_ROADMAP.md](SAMV71_FEATURE_RE_ENABLEMENT_ROADMAP.md)** - Phase-by-phase plan
3. **[SAMV71_PROJECT_PLAN.md](SAMV71_PROJECT_PLAN.md)** - Sprint breakdown with timeline

### For Understanding the Fixes
1. **[SAMV7_VTABLE_CORRUPTION_ANALYSIS.md](SAMV7_VTABLE_CORRUPTION_ANALYSIS.md)** - What went wrong
2. **[SAMV7_PARAM_STORAGE_FIX.md](SAMV7_PARAM_STORAGE_FIX.md)** - How we fixed it
3. **[SAMV7_ACHIEVEMENTS.md](SAMV7_ACHIEVEMENTS.md)** - Complete journey

### For Development Work
1. **[SAMV71_COMPLETE_TASK_LIST.md](SAMV71_COMPLETE_TASK_LIST.md)** - All tasks and their status
2. **[DOCS_DMESG_HRT_ENABLE.md](DOCS_DMESG_HRT_ENABLE.md)** - Available debugging tools
3. **[SAMV71_USB_CDC_IMPLEMENTATION.md](SAMV71_USB_CDC_IMPLEMENTATION.md)** - MAVLink communication guide

---

## 📊 Documentation Statistics

- **Total Documentation Files:** 35+
- **Project Planning Docs:** 4 ⭐ NEW
- **Core Implementation Docs:** 4
- **Hardware/Driver Docs:** 5 (USB CDC/ACM added)
- **Debugging Guides:** 3
- **Technical Deep Dives:** 4
- **Board-Specific Docs:** 4

---

## 🔍 Finding Information

**Search by Topic:**
- **Planning/Roadmap:** `SAMV71_PROJECT_PLAN.md`, `SAMV71_COMPLETE_TASK_LIST.md`, `SAMV71_FEATURE_RE_ENABLEMENT_ROADMAP.md`
- **Session Resumption:** `SESSION_CONTEXT_NEXT_STEPS.md` ⭐ START HERE
- **USB/MAVLink:** `SAMV71_USB_CDC_IMPLEMENTATION.md`, `KNOWN_GOOD_SAFE_MODE_CONFIG.md`
- **Parameters:** `SAMV7_PARAM_STORAGE_FIX.md`, `PARAMETER_STORAGE_STRATEGY.md`
- **SD Card:** `SD_CARD_INTEGRATION_SUMMARY.md`, `KNOWN_GOOD_SAFE_MODE_CONFIG.md`
- **Boot Issues:** `BOOT_ERROR_ANALYSIS.md`, `attention_gemini.md`
- **HRT/Timers:** `HRT_IMPLEMENTATION_SUMMARY.md`, `DOCS_DMESG_HRT_ENABLE.md`
- **Safe Mode:** `KNOWN_GOOD_SAFE_MODE_CONFIG.md`, `CURRENT_STATUS.md`
- **Memory/Cache:** `CACHE_COHERENCY_GUIDE.md`, `CONSTRAINED_FLASH_ANALYSIS.md`

---

**For the latest status and next steps:**
- **Session resumption:** [SESSION_CONTEXT_NEXT_STEPS.md](SESSION_CONTEXT_NEXT_STEPS.md)
- **Complete roadmap:** [SAMV71_COMPLETE_TASK_LIST.md](SAMV71_COMPLETE_TASK_LIST.md)
- **Current state:** [CURRENT_STATUS.md](CURRENT_STATUS.md)
