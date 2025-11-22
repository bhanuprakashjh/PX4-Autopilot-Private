# Session Context - Next Steps for SAMV71-XULT Development

**Date Created:** November 23, 2025
**Current Branch:** `samv7-custom`
**Last Commit:** `a70589d5ec` - "docs: Verify USB CDC/ACM with MAVProxy and create re-enablement roadmap"
**User:** bhanu1234

---

## 🎯 Current State Summary

### What's Working Now:
1. ✅ **Parameter Storage** - Manual construction fix applied, saving to SD card works
2. ✅ **SD Card I/O** - FAT32 mounted at /fs/microsd, read/write working
3. ✅ **USB CDC/ACM** - High-speed (480 Mbps) USB device on TARGET USB port
4. ✅ **MAVLink Communication** - Verified with MAVProxy: 13,784+ packets, 0 errors
5. ✅ **HRT** - High-resolution timer working (TC0 timer)
6. ✅ **dmesg** - Debug logging enabled and working

### What's Currently Disabled (From SD Card Debugging):
1. ❌ **dataman** - Disabled in rcS line 277 ("hangs when no storage available")
2. ❌ **navigator** - Disabled in rcS line 543 (requires dataman)
3. ❌ **payload_deliverer** - Disabled in rcS line 586 (dataman_client testing)
4. ❌ **logger** - Disabled by board rc.logging override (SD write bottleneck, errno 116)

### Manual Startup Required:
After each boot, must run:
```bash
# In NSH console (/dev/ttyACM0):
sercon
mavlink start -d /dev/ttyACM0

# Then on PC, connect to /dev/ttyACM1
```

---

## 🚀 What Was Just Accomplished

### USB CDC/ACM Implementation and Verification:

1. **Applied Critical USB Configurations:**
   - `CONFIG_USBDEV_DMA=y` - DMA support for all endpoints
   - `CONFIG_USBDEV_DUALSPEED=y` - High-speed USB (480 Mbps)
   - `CONFIG_SAMV7_USBHS_EP7DMA_WAR=y` - Silicon errata workaround

2. **Re-enabled MAVLink in rcS:**
   - Commented out `mavlink stop-all` commands (lines 51, 515)
   - Re-enabled USB CDC/ACM startup section (lines 306-314)
   - Re-enabled `mavlink boot_complete` (line 411)

3. **Tested with MAVProxy:**
   - Installed MAVProxy via pipx
   - Connected successfully to /dev/ttyACM1
   - Verified heartbeat streaming at 1 Hz
   - Confirmed bidirectional communication (13,784+ packets, 0 errors)
   - Verified MAVLink v2 protocol, PX4 autopilot detection

4. **Created Documentation:**
   - Updated `SAMV71_USB_CDC_IMPLEMENTATION.md` with MAVProxy test results
   - Created `SAMV71_FEATURE_RE_ENABLEMENT_ROADMAP.md` with 5-phase plan
   - Updated main `README.md` with verification status

5. **Committed and Pushed:**
   - Commit: `a70589d5ec`
   - Pushed to GitHub: `samv7-custom` branch

---

## 📋 Next Immediate Steps (Priority Order)

### **Phase 1: Re-enable dataman (READY TO START)**

**Goal:** Enable mission upload/download in QGroundControl

**File to Edit:** `ROMFS/px4fmu_common/init.d/rcS`

**Line 277-287 - Uncomment this section:**
```bash
# Currently commented as:
# TEMPORARILY DISABLED - dataman hangs when no storage available
# if param compare -s SYS_DM_BACKEND 1
# then
# 	dataman start -r
# else
# 	if param compare SYS_DM_BACKEND 0
# 	then
# 		dataman start $DATAMAN_OPT
# 	fi
# fi

# Change to (uncomment):
if param compare -s SYS_DM_BACKEND 1
then
	dataman start -r
else
	if param compare SYS_DM_BACKEND 0
	then
		# dataman start default (with optional board-specific path)
		dataman start $DATAMAN_OPT
	fi
fi
```

**Build Command:**
```bash
cd /media/bhanu1234/Development/PX4-Autopilot-Private
make clean
make microchip_samv71-xult-clickboards_default
```

**Flash Command (USER DOES THIS):**
```bash
openocd -f interface/cmsis-dap.cfg -f target/atsamv.cfg \
  -c "program build/microchip_samv71-xult-clickboards_default/microchip_samv71-xult-clickboards_default.elf verify reset exit"
```

**Verification After Boot:**
```bash
# In NSH console:
ps | grep dataman
# Should show dataman process

dmesg | grep dataman
# Should show successful startup, no errors

# Start MAVLink
sercon
mavlink start -d /dev/ttyACM0

# Check for mission init error (should be GONE)
dmesg | grep mission
# Should NOT show "offboard mission init failed"
```

**Expected Outcome:**
- ✅ dataman process running
- ✅ No "offboard mission init failed" warning
- ✅ Mission upload/download works in QGC

**Rollback if Failed:**
Re-comment the dataman section, rebuild, reflash.

---

### **Phase 2: Re-enable navigator**

**Depends on:** Phase 1 success

**File:** `ROMFS/px4fmu_common/init.d/rcS`

**Line 543-544:**
```bash
# Change from:
# TEMPORARILY DISABLED - navigator requires dataman which isn't available
# navigator start

# To:
navigator start
```

---

### **Phase 3: Test with QGroundControl**

**After Phase 1-2 complete:**

1. Start QGC on PC
2. Connect to /dev/ttyACM1 (auto-detect or manual)
3. Test Parameters tab
4. Test Plan tab (mission upload/download)
5. Verify all features working

---

### **Phase 4: Re-enable logger (OPTIONAL, HIGH RISK)**

**WARNING:** Logger previously caused errno 116 and NSH hangs

**Conservative Approach - MAVLink logging only (no SD writes):**

Edit: `boards/microchip/samv71-xult-clickboards/init/rc.logging`
```bash
#!/bin/sh
# MAVLink-only logging (no SD card writes)
echo "INFO [logger] Enabling MAVLink logging only"
logger start -b 8 -m mavlink
```

**Test carefully:**
- Monitor for NSH responsiveness
- Check memory with `free`
- Watch for errno 116 errors
- Test arming/disarming if applicable

---

## 🔧 Important Technical Context

### USB CDC/ACM Architecture:
- **SAMV71 internal device:** `/dev/ttyACM0` (created by sercon)
- **PC external device:** `/dev/ttyACM1` (USB CDC/ACM)
- **DEBUG USB (EDBG):** `/dev/ttyACM0` on PC (NSH console)
- **TARGET USB:** `/dev/ttyACM1` on PC (MAVLink)

### Connection Sequence (CRITICAL):
1. PC opens `/dev/ttyACM1` first (MAVProxy or QGC)
2. THEN SAMV71 can open `/dev/ttyACM0`
3. This is USB CDC/ACM connection-oriented behavior (ENOTCONN otherwise)

### MAVLink Startup Sequence:
```bash
# On PC - start first:
mavproxy.py --master=/dev/ttyACM1 --baudrate=2000000
# OR start QGroundControl

# On SAMV71 - then start:
sercon
mavlink start -d /dev/ttyACM0
```

### Build Configuration:
- **Target:** `microchip_samv71-xult-clickboards_default`
- **Flash:** 1020 KB / 2 MB (48.61%)
- **SRAM:** 27 KB / 384 KB (6.90%)
- **CONSTRAINED_FLASH:** Enabled
- **USB Speed:** High-speed (480 Mbps)

### Key Files Modified:
1. `src/lib/parameters/parameters.cpp` - Manual construction fix
2. `boards/microchip/samv71-xult-clickboards/nuttx-config/nsh/defconfig` - USB configs
3. `boards/microchip/samv71-xult-clickboards/default.px4board` - Serial mappings
4. `boards/microchip/samv71-xult-clickboards/init/rc.board_defaults` - MAV_0_CONFIG
5. `boards/microchip/samv71-xult-clickboards/init/rc.board_mavlink` - MAVLink enabled
6. `boards/microchip/samv71-xult-clickboards/init/rc.logging` - Logger disabled
7. `ROMFS/px4fmu_common/init.d/rcS` - MAVLink re-enabled, dataman/navigator disabled

---

## 📚 Documentation Files

### Current Session Created:
1. **SAMV71_USB_CDC_IMPLEMENTATION.md** - Complete USB implementation guide
2. **SAMV71_FEATURE_RE_ENABLEMENT_ROADMAP.md** - 5-phase incremental plan
3. **SAMV71_USB_CDC_KNOWN_ISSUES.md** - Initial USB debugging
4. **SAMV71_USB_CONFIGURATION_VERIFICATION.md** - Config checklist
5. **SAMV71_USB_HARMONY_VS_NUTTX_COMPARISON.md** - NuttX vs Harmony USB

### Reference Documents:
- **SAMV7_PARAM_STORAGE_FIX.md** - Manual construction fix (CRITICAL)
- **KNOWN_GOOD_SAFE_MODE_CONFIG.md** - Baseline configuration
- **CURRENT_STATUS.md** - System status (may be outdated)
- **DOCUMENTATION_INDEX.md** - Central hub
- **README.md** - Main repository README (updated)

---

## 🐛 Known Issues and Workarounds

### Issue 1: Manual Startup Required
**Problem:** `sercon` and `mavlink start` needed after every boot
**Root Cause:** `rc.serial_port` has `SERIAL_DEV=none` instead of `/dev/ttyACM0`
**Workaround:** Manual commands work perfectly
**Fix:** Investigate PX4's serial port generation system (Tools/serial/generate_config.py)

### Issue 2: ENOTCONN Error
**Problem:** `mavlink start -d /dev/ttyACM0` fails if PC not connected
**Solution:** Always start MAVProxy/QGC on PC first, then start MAVLink on SAMV71

### Issue 3: param 65535 invalid
**Symptom:** `ERROR [parameters] get: param 65535 invalid`
**Cause:** MAVLink offboard mission feature references missing parameter
**Impact:** Harmless warning, MAVLink works fine
**Related:** dataman disabled (mission features not available)

### Issue 4: Logger SD Write Bottleneck (UNRESOLVED)
**Symptom:** `logger start` causes errno 116, NSH becomes unresponsive
**Cause:** HSMCI driver bottleneck with sustained writes
**Workaround:** Logger disabled via board rc.logging override
**Future:** Try MAVLink-only logging, or conservative SD settings

---

## 🚨 User Preferences and Constraints

### Important Notes:
1. **User handles all firmware flashing** - Don't auto-flash via scripts
2. **User prefers incremental approach** - One feature at a time
3. **Documentation first** - Document before committing
4. **User is hands-on** - Comfortable with NSH, command line
5. **Avoid breaking changes** - Always have rollback plan

### User's Development Flow:
1. Make changes
2. Build firmware: `make clean && make microchip_samv71-xult-clickboards_default`
3. Flash manually (user does this)
4. Test in NSH console
5. Document results
6. Commit if successful
7. Push to GitHub

---

## 💡 AI Assistant Tips for Next Session

### When Resuming:
1. Read this file first to understand current state
2. Check git log for any commits after `a70589d5ec`
3. Ask user what they want to work on (Phase 1, 2, or QGC testing)
4. Reference the roadmap: `SAMV71_FEATURE_RE_ENABLEMENT_ROADMAP.md`

### When Making Changes:
1. Always read files before editing
2. Show user what will change before applying
3. Explain why each change is needed
4. Include rollback instructions
5. Update documentation after testing

### When Troubleshooting:
1. Check `dmesg` output first
2. Use `ps` to verify processes running
3. Check `top` and `free` for resource issues
4. Look at NSH console for error messages
5. Reference previous debugging docs (SD card, parameter storage)

### Documentation Style:
1. Use markdown with clear sections
2. Include code blocks with syntax highlighting
3. Add verification steps and test plans
4. Document both success and failure scenarios
5. Include commit message templates
6. Always add "Generated with Claude Code" footer

### Git Workflow:
1. Check status before committing: `git status`
2. Stage specific files: `git add <file>`
3. Write detailed commit messages with:
   - What was changed
   - Why it was changed
   - Test results
   - Next steps
4. Always push after committing
5. Use branch: `samv7-custom`

---

## 🔍 Quick Reference Commands

### Build and Flash:
```bash
cd /media/bhanu1234/Development/PX4-Autopilot-Private
make clean
make microchip_samv71-xult-clickboards_default
# User flashes manually
```

### Start MAVLink (After Boot):
```bash
# In NSH console:
sercon
mavlink start -d /dev/ttyACM0

# On PC:
mavproxy.py --master=/dev/ttyACM1 --baudrate=2000000
# OR start QGroundControl
```

### Debug Commands (NSH):
```bash
ps                    # List processes
top                   # CPU/memory usage
free                  # Memory stats
dmesg                 # Boot log and errors
mavlink status        # MAVLink statistics
param show MAV_*      # MAVLink parameters
ls -l /fs/microsd/    # SD card contents
```

### Git Commands:
```bash
git status
git log --oneline -5
git diff
git add <file>
git commit -m "message"
git push origin samv7-custom
```

---

## 📞 Questions to Ask User When Resuming

1. "What would you like to work on next?"
   - Phase 1: Re-enable dataman
   - Phase 2: Re-enable navigator
   - Phase 4: Re-enable logger
   - Phase 5: Test with QGroundControl
   - Something else

2. "Did you test anything since the last session?"
   - Check if they flashed new firmware
   - Check if they encountered any issues
   - Check if they made any changes

3. "Do you want to follow the roadmap incrementally, or try something different?"

---

## 🎯 Success Criteria for Next Phase

### Phase 1 (dataman) Success:
- [ ] dataman process appears in `ps` output
- [ ] No "offboard mission init failed" warning in dmesg
- [ ] System stable for 1+ hour
- [ ] Can upload mission in QGroundControl
- [ ] No system hangs or crashes

### Phase 2 (navigator) Success:
- [ ] navigator process appears in `ps` output
- [ ] Mission execution possible
- [ ] Mission progress updates via MAVLink
- [ ] System stable for 1+ hour

### QGC Integration Success:
- [ ] QGC connects to /dev/ttyACM1
- [ ] Parameters download successfully
- [ ] Plan tab functional (mission upload/download)
- [ ] Fly view shows telemetry
- [ ] No connection drops

---

## 🔗 External Resources

- **PX4 Documentation:** https://docs.px4.io/
- **MAVLink Protocol:** https://mavlink.io/
- **SAMV71 Datasheet:** SAM E70/S70/V70/V71 Family Datasheet
- **Silicon Errata:** DS80000767J (EP7 DMA workaround)
- **NuttX SAMV71 Docs:** https://nuttx.apache.org/docs/latest/platforms/arm/samv7/

---

## 📝 Session History Summary

### Session 1-5: Parameter Storage Fix
- Identified C++ static initialization bug
- Implemented manual construction pattern
- Parameters now save/load correctly

### Session 6-10: SD Card Integration
- Fixed DMA cache coherency issues
- Applied 21 fixes from Microchip reference
- SD card I/O fully working

### Session 11-15: USB CDC/ACM Implementation
- Added critical USB configurations
- Re-enabled MAVLink in rcS
- Tested with MAVProxy successfully
- **Current session ends here** ⭐

### Next Session: Feature Re-enablement
- Start with Phase 1 (dataman)
- Incremental approach
- Test between each phase

---

**Last Updated:** November 23, 2025
**Session End Time:** After commit `a70589d5ec`
**Next Session:** Start with Phase 1 (dataman re-enablement)
**Status:** ✅ Ready for incremental feature re-enablement

---

## 🎬 Quick Start for Next Session

```bash
# 1. Navigate to project
cd /media/bhanu1234/Development/PX4-Autopilot-Private

# 2. Check current branch and status
git status
git log --oneline -3

# 3. Read this file for context
cat SESSION_CONTEXT_NEXT_STEPS.md

# 4. Read the roadmap
cat SAMV71_FEATURE_RE_ENABLEMENT_ROADMAP.md

# 5. Ask user what they want to work on
# 6. Follow the appropriate phase from the roadmap
```

---

**This file should be read first by any AI assistant resuming this project!**
