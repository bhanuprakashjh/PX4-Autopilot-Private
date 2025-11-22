# SAMV71 Feature Re-enablement Roadmap

**Date:** November 23, 2025
**Current Status:** USB CDC/ACM working, basic MAVLink verified
**Goal:** Incrementally re-enable disabled services for full PX4 functionality

---

## Current System State

### ✅ Working Features:
- Parameter storage (manual construction fix)
- SD card I/O (read/write)
- USB CDC/ACM (high-speed, 480 Mbps)
- MAVLink communication (verified with MAVProxy)
- HRT (high-resolution timer)
- dmesg debugging tool

### ❌ Disabled Features (From SD Card Debugging):

| Feature | Disabled Where | Why Disabled | Impact on MAVLink |
|---------|---------------|--------------|-------------------|
| **dataman** | rcS line 277 | "hangs when no storage available" | ❌ No mission upload/download |
| **navigator** | rcS line 543 | Requires dataman | ❌ No mission execution |
| **payload_deliverer** | rcS line 586 | dataman_client testing | ⚠️ Minor - mission delivery only |
| **logger** | Board rc.logging | SD write bottleneck (errno 116) | ❌ No flight log recording |

---

## Phase 1: Re-enable dataman (Mission Storage)

**Priority:** HIGH
**Reason:** SD card is now working, dataman should no longer hang

### Changes Required:

**File:** `ROMFS/px4fmu_common/init.d/rcS`

**Line 277-287 - Uncomment dataman startup:**
```bash
# Before (DISABLED):
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

# After (ENABLED):
#
# Waypoint storage.
# REBOOTWORK this needs to start in parallel.
#
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

### Test Plan:

1. **Build and flash:**
   ```bash
   make clean
   make microchip_samv71-xult-clickboards_default
   # Flash firmware (user does this)
   ```

2. **After boot, verify dataman starts:**
   ```bash
   # In NSH console:
   ps | grep dataman
   # Should show dataman process running

   dmesg | grep dataman
   # Should show successful startup, no errors
   ```

3. **Test with MAVLink:**
   ```bash
   # Start MAVLink as usual
   sercon
   mavlink start -d /dev/ttyACM0

   # Check for mission init error - should be gone!
   dmesg | grep mission
   ```

4. **Test with QGroundControl:**
   - Connect to /dev/ttyACM1
   - Go to Plan tab
   - Try to create and upload a simple mission
   - Should succeed instead of failing

### Expected Results:
- ✅ dataman process appears in `ps` output
- ✅ No "offboard mission init failed" warning
- ✅ Mission upload/download works in QGC
- ✅ No system hangs or crashes

### Rollback if Failed:
If dataman causes issues:
```bash
# Re-comment the dataman section in rcS
# Rebuild and reflash
```

---

## Phase 2: Re-enable navigator (Mission Execution)

**Priority:** HIGH
**Depends on:** Phase 1 (dataman working)

### Changes Required:

**File:** `ROMFS/px4fmu_common/init.d/rcS`

**Line 543-544 - Uncomment navigator:**
```bash
# Before (DISABLED):
# TEMPORARILY DISABLED - navigator requires dataman which isn't available
# navigator start

# After (ENABLED):
#
# Start the navigator.
#
navigator start
```

### Test Plan:

1. **Build and flash** (after Phase 1 succeeds)

2. **Verify navigator starts:**
   ```bash
   ps | grep navigator
   # Should show navigator process

   dmesg | grep navigator
   # Should show successful startup
   ```

3. **Test mission execution:**
   - Upload a mission in QGC
   - Switch to AUTO mode
   - Verify mission progress is reported

### Expected Results:
- ✅ navigator process running
- ✅ Mission execution possible
- ✅ Mission progress updates via MAVLink

---

## Phase 3: Re-enable payload_deliverer (Optional)

**Priority:** LOW
**Depends on:** Phase 1 (dataman working)

### Changes Required:

**File:** `ROMFS/px4fmu_common/init.d/rcS`

**Line 586-587 - Uncomment payload_deliverer:**
```bash
# Before (DISABLED):
# TEMPORARILY DISABLED - testing dataman_client trigger
# payload_deliverer start

# After (ENABLED):
if param compare -s PD_GRIPPER_EN 1
then
	payload_deliverer start
fi
```

### Test Plan:
- Only enable if you need payload delivery features
- Not critical for basic MAVLink/mission functionality

---

## Phase 4: Re-enable logger (CAUTION!)

**Priority:** MEDIUM
**Risk:** HIGH - Previously caused errno 116 and NSH hangs
**Depends on:** Phase 1-2 working stably

### Changes Required:

**File:** `boards/microchip/samv71-xult-clickboards/init/rc.logging`

**Option A - Delete the override file (recommended):**
```bash
# Remove the board-specific rc.logging to use default
rm boards/microchip/samv71-xult-clickboards/init/rc.logging
```

**Option B - Modify to enable with conservative settings:**
```bash
#!/bin/sh
# Conservative logging settings for SAMV71
echo "INFO [logger] Enabling logging with conservative settings"

# Start logger with:
# -b 8: Small buffer (8 KB)
# -e: Log only when armed (reduces SD writes)
logger start -b 8 -e
```

**Option C - Enable only MAVLink logging (no SD writes):**
```bash
#!/bin/sh
# MAVLink-only logging (no SD card writes)
echo "INFO [logger] Enabling MAVLink logging only"

# Log to MAVLink backend only (no file writes)
param set SDLOG_BACKEND 2  # MAVLink only
logger start -b 8 -m mavlink
```

### Test Plan:

1. **Start with Option C** (MAVLink logging only - safest)
   - Build and flash
   - Verify logger starts without hanging NSH
   - Check memory usage with `free`
   - Monitor with `top` for CPU usage

2. **If Option C works, try Option B** (conservative SD logging)
   - Only logs when armed (less SD writes)
   - Monitor for errno 116 errors
   - Test arming/disarming cycle
   - Verify log files are created

3. **If all works, try Option A** (full logging)
   - Full logging capability
   - Monitor SD card write performance
   - Check for NSH responsiveness

### Warning Signs to Watch For:
- ⚠️ NSH becomes unresponsive
- ⚠️ "errno 116" errors
- ⚠️ System requires power cycle (not just reset)
- ⚠️ High CPU usage from logger (>50%)

### Rollback if Failed:
```bash
# Restore the disabled rc.logging
echo '#!/bin/sh' > boards/microchip/samv71-xult-clickboards/init/rc.logging
echo 'echo "INFO [logger] logging disabled for SD card debugging"' >> boards/microchip/samv71-xult-clickboards/init/rc.logging
# Rebuild and reflash
```

---

## Phase 5: QGroundControl Full Integration Test

**Priority:** MEDIUM
**Depends on:** Phase 1-2 complete, Phase 4 optional

### Test Checklist:

#### Connection Test:
- [ ] QGC auto-detects /dev/ttyACM1
- [ ] Parameters download successfully
- [ ] Firmware version shows correctly
- [ ] No connection drops

#### Parameters Tab:
- [ ] Can view all parameters
- [ ] Can modify parameters
- [ ] Can save parameters
- [ ] Parameters persist after reboot

#### Plan Tab (Mission Planning):
- [ ] Can create waypoint missions
- [ ] Can upload missions to vehicle
- [ ] Can download missions from vehicle
- [ ] Mission count shows correctly

#### Fly View:
- [ ] Real-time telemetry updates
- [ ] Attitude indicator works
- [ ] Can change flight modes
- [ ] Can arm/disarm (if sensors enabled)

#### Analyze Tab (Logs):
- [ ] Log list shows if logger enabled
- [ ] Can download logs
- [ ] Can view log playback

---

## Testing Strategy - Incremental Approach

### Week 1: Dataman + Navigator
1. Monday: Re-enable dataman, test basic startup
2. Tuesday: Test mission upload/download via MAVLink
3. Wednesday: Re-enable navigator, verify mission execution capability
4. Thursday: Extended testing with QGroundControl
5. Friday: Document results, commit if stable

### Week 2: Logger (Optional)
1. Monday: Enable MAVLink-only logging (Option C)
2. Tuesday: Test stability, monitor for issues
3. Wednesday: If stable, try conservative SD logging (Option B)
4. Thursday: Stress test with multiple arm/disarm cycles
5. Friday: Document results, commit if stable

### Week 3: Full Integration
1. Monday-Friday: Full QGroundControl integration testing
2. Test all tabs and features
3. Long-duration testing (hours, not minutes)
4. Document any issues or limitations

---

## Rollback Strategy

### If Any Phase Fails:

1. **Identify the failing component:**
   ```bash
   # Check dmesg for errors
   dmesg | grep -i error

   # Check process list
   ps

   # Check system health
   top
   free
   ```

2. **Revert to last known-good state:**
   ```bash
   # Use git to revert changes
   git diff  # See what changed
   git checkout <file>  # Revert specific file

   # Or use git stash to save changes
   git stash save "Phase X failed - reverting"
   ```

3. **Rebuild and reflash:**
   ```bash
   make clean
   make microchip_samv71-xult-clickboards_default
   # Flash (user does this)
   ```

4. **Document the failure:**
   - What was enabled
   - What symptoms appeared
   - Error messages from dmesg
   - System state before failure

---

## Success Criteria

### Phase 1 Success (dataman):
- ✅ dataman starts without errors
- ✅ No "offboard mission init failed" warning
- ✅ System stable for 1+ hour
- ✅ Mission upload works in QGC

### Phase 2 Success (navigator):
- ✅ navigator starts without errors
- ✅ Can upload and download missions
- ✅ Mission progress reported via MAVLink
- ✅ System stable for 1+ hour

### Phase 4 Success (logger):
- ✅ logger starts without hanging NSH
- ✅ No errno 116 errors
- ✅ Log files created (if SD logging enabled)
- ✅ System stable for multiple hours
- ✅ Can retrieve logs via MAVLink or SD card

### Phase 5 Success (QGC Integration):
- ✅ All QGC tabs functional
- ✅ No connection drops during testing
- ✅ Parameters, missions, logs all accessible
- ✅ Stable operation for full day

---

## Dependencies and Prerequisites

### Before Starting Any Phase:

1. **Current baseline must be working:**
   - USB CDC/ACM functional
   - MAVLink communication verified
   - Parameters saving correctly
   - SD card I/O working

2. **Have debugging tools ready:**
   - dmesg enabled
   - top command available
   - Second terminal for monitoring

3. **Backup current state:**
   ```bash
   # Create git commit of current working state
   git add -A
   git commit -m "Checkpoint before re-enabling features"

   # Or create a patch
   git diff > backup_before_reenable.patch
   ```

---

## Timeline Estimate

| Phase | Estimated Time | Risk Level |
|-------|---------------|------------|
| Phase 1: dataman | 1-2 days | Low |
| Phase 2: navigator | 1 day | Low |
| Phase 3: payload_deliverer | 1 hour | Very Low |
| Phase 4: logger | 3-5 days | **High** |
| Phase 5: QGC testing | 2-3 days | Low |
| **Total** | **1-2 weeks** | - |

**Note:** Phase 4 (logger) is high risk and may need extended debugging if SD write issues persist.

---

## Alternative Approach: Enable All at Once (Not Recommended)

If you want to try enabling everything at once:

### Risks:
- Hard to identify which component causes issues
- Longer debugging time if something fails
- May need to revert everything and start over

### If You Choose This:

1. Make all changes in one commit
2. Test thoroughly before committing
3. Be prepared to bisect which change caused issues
4. Have a full backup of current working state

---

## Next Immediate Step

**Recommended:** Start with Phase 1 (dataman)

1. Edit `ROMFS/px4fmu_common/init.d/rcS` line 277-287
2. Uncomment the dataman startup section
3. Clean build: `make clean && make microchip_samv71-xult-clickboards_default`
4. Flash firmware
5. Test and verify dataman starts successfully
6. Test mission upload in QGC or MAVProxy
7. If successful, document and commit
8. Proceed to Phase 2

---

**Document Version:** 1.0
**Last Updated:** November 23, 2025
**Status:** Ready for Phase 1 implementation
