# SAMV71-XULT Current Status

**Date:** November 20, 2025
**Branch:** `samv7-custom`
**Build:** microchip_samv71-xult-clickboards_default
**Commit:** e90e0fbc5f + local safe-mode changes

---

## 🎯 Current Configuration: Safe Mode with Early Exit

This is a **known-good, stable baseline** configuration designed for:
- Immediate NSH prompt
- No background service interference
- Full debugging tool access
- Parameter storage testing

---

## ✅ Working Features

### Core System
- ✅ **NuttX Boot** - Clean boot to NSH prompt in <2 seconds
- ✅ **Manual Construction Fix** - Parameters storage layers properly initialized
- ✅ **Parameter Loading** - 376/1081 parameters load from compiled defaults
- ✅ **Parameter Set/Save** - `param set` and `param save` working correctly
- ✅ **Parameter Persistence** - Values persist across reboot (22-byte file vs 5-byte broken)

### Hardware
- ✅ **SD Card Mount** - `/fs/microsd` mounted (FAT32, 15.5GB)
- ✅ **SD Card I/O** - Manual read/write/append/delete all working
- ✅ **HRT (High-Resolution Timer)** - Both microsecond timers and deferred callbacks pass
- ✅ **I2C Bus** - `/dev/i2c0` available
- ✅ **USB Console** - Clean, responsive `/dev/ttyACM0`

### Debugging Tools
- ✅ **`dmesg` Command** - RAMLOG enabled, boot log capture working
- ✅ **`hrt_test` Command** - Timing verification passes
- ✅ **`top` Command** - Process monitoring functional
- ✅ **`free` Command** - Memory usage visible
- ✅ **NSH Shell** - Instant, responsive, no interference

### Safe Mode Features
- ✅ **Early Exit in rcS** - PX4 init stops after parameter import
- ✅ **MAVLink Disabled** - `MAV_0_CONFIG = 0` prevents USB conflicts
- ✅ **Logging Disabled** - Prevents SD bottleneck issues
- ✅ **Minimal Processes** - Only Idle task and work queues run (~99% idle)

---

## ❌ Known Issues

### SD Card Logging
- ❌ **Logger Service Fails** - `logger start` hangs with errno 116
- ❌ **HSMCI Driver Bottleneck** - Sustained writes cause timeout
- ❌ **Requires Power-Cycle** - NSH unresponsive after failed logger start
- ⚠️ **Manual I/O Works** - `echo`/`cat` succeed, only sustained logging fails

### Service Limitations (By Design in Safe Mode)
- ⚠️ **Commander** - Not started (safe mode)
- ⚠️ **Sensors** - Not started (safe mode)
- ⚠️ **MAVLink** - Disabled to prevent USB conflicts
- ⚠️ **Logging** - Disabled due to HSMCI bottleneck
- ⚠️ **Autopilot Stack** - Not running (safe mode for debugging)

### Minor Issues
- ⚠️ **SD Card Mount Warnings** - CMD1/CMD55 errors during init (expected, SD mounts successfully)
- ⚠️ **dmesg One-Shot** - First call shows all logs, subsequent calls show only new entries (RAMLOG FIFO behavior)

---

## 🔧 Current Build Configuration

### Modified Files

#### 1. **src/lib/parameters/parameters.cpp** ⭐ CRITICAL
Manual construction fix using aligned storage + placement new:
```cpp
// Aligned storage buffers
static uint64_t _firmware_defaults_storage[...];
static uint64_t _runtime_defaults_storage[...];
static uint64_t _user_config_storage[...];

// References to storage
static ConstLayer &firmware_defaults = *reinterpret_cast<ConstLayer*>(_firmware_defaults_storage);
static DynamicSparseLayer &runtime_defaults = *reinterpret_cast<DynamicSparseLayer*>(_runtime_defaults_storage);
DynamicSparseLayer &user_config = *reinterpret_cast<DynamicSparseLayer*>(_user_config_storage);

// Manual construction when heap is ready
void param_init() {
    if (!g_params_constructed) {
        new (&firmware_defaults) ConstLayer();
        new (&runtime_defaults) DynamicSparseLayer(&firmware_defaults);
        new (&user_config) DynamicSparseLayer(&runtime_defaults);
        g_params_constructed = true;
    }
    // ... rest of init ...
}
```

#### 2. **ROMFS/px4fmu_common/init.d/rcS** ⭐ SAFE MODE
Early exit after parameter import:
```bash
if [ $STORAGE_AVAILABLE = yes ]
then
    param select-backup $PARAM_BACKUP_FILE
fi

#
# SAFE MODE: skipping PX4 startup (SD debug)
#
exit 0
```

#### 3. **boards/microchip/samv71-xult-clickboards/init/rc.board_defaults**
```bash
#!/bin/sh
# board defaults
# board defaults restored for SD debugging safe mode
param set-default MAV_0_CONFIG 0
```

#### 4. **boards/microchip/samv71-xult-clickboards/init/rc.board_mavlink**
```bash
#!/bin/sh
# MAVLink disabled for safe mode
```

#### 5. **boards/microchip/samv71-xult-clickboards/init/rc.logging**
```bash
#!/bin/sh
# Logging disabled for safe mode
```

#### 6. **boards/microchip/samv71-xult-clickboards/nuttx-config/nsh/defconfig**
Added:
```
CONFIG_SYSTEMCMDS_DMESG=y
CONFIG_SYSTEMCMDS_HRT=y
CONFIG_RAMLOG_SYSLOG=y
```

### Build Statistics
```
Memory region         Used Size  Region Size  %age Used
           flash:      920 KB         2 MB     43.91%
            sram:       18 KB       384 KB      4.75%
```

---

## 🧪 Verification Tests

### Test 1: SD Card I/O ✅
```bash
nsh> echo "Hello from SAMV71 SD card test" > /fs/microsd/test.txt
nsh> cat /fs/microsd/test.txt
Hello from SAMV71 SD card test

nsh> echo "Line 1: Parameter fix working" > /fs/microsd/test.txt
nsh> echo "Line 2: SD card stable" >> /fs/microsd/test.txt
nsh> cat /fs/microsd/test.txt
Line 1: Parameter fix working
Line 2: SD card stable

nsh> rm /fs/microsd/test.txt
```
**Result:** ✅ PASSED

### Test 2: Parameter Storage ✅
```bash
nsh> param set CAL_ACC0_ID 123456
nsh> param show CAL_ACC0_ID
CAL_ACC0_ID [16,47] : 123456

nsh> param save
nsh> ls -l /fs/microsd/params
 -rw-rw-rw-      22 /fs/microsd/params

nsh> reboot

# After reboot:
nsh> param show CAL_ACC0_ID
CAL_ACC0_ID [16,47] : 123456
```
**Result:** ✅ PASSED (file size 22 bytes, previously 5 bytes when broken)

### Test 3: HRT Verification ✅
```bash
nsh> hrt_test
[TEST PASSED] Microsecond timer working
[TEST PASSED] HRT deferred callbacks working
```
**Result:** ✅ PASSED

### Test 4: System Monitoring ✅
```bash
nsh> top
  PID GROUP PRI POLICY   TYPE    NPX STATE   EVENT      SIGMASK        STACK USED  FILLED   COMMAND
    0     0   0 FIFO     Kthread - Ready              0000000000000000  2000   368   18.4%  Idle Task
    1     1 100 RR       Kthread - Waiting  Semaphore 0000000000000000  1984   688   34.6%  hpwork
    2     2 100 RR       Task    - Running            0000000000000000 12208  2784   22.8%  nsh_main
```
**Result:** ✅ PASSED (~99% idle, minimal processes)

### Test 5: dmesg Output ✅
```bash
nsh> dmesg
[Shows complete boot log including SD card init, parameter loading, etc.]
```
**Result:** ✅ PASSED

---

## 🚀 Next Steps (Planned)

### Phase 1: MAVLink on Hardware UART
1. Connect FTDI adapter to TELEM1 (USART0)
2. Set `MAV_0_CONFIG = 101` (TELEM1 at 57600 baud)
3. Test MAVLink communication without USB interference
4. Verify NSH remains responsive

### Phase 2: Incremental Service Re-enablement
1. Remove early exit from `rcS` (allow PX4 init to continue)
2. Re-enable commander and sensors
3. Test with `top` and `dmesg` after each change
4. Monitor for console interference

### Phase 3: SD Logging (Conditional)
1. Only after MAVLink stable on UART
2. Consider lower bandwidth options: `logger start -m drop`
3. Alternative: Fix HSMCI driver bottleneck
4. Test incrementally with frequent monitoring

### Phase 4: Full Autopilot Stack
1. Restore `SYS_AUTOSTART` to desired airframe ID
2. Enable full sensor stack
3. Test autopilot functionality
4. Consider HITL/Gazebo integration

---

## 📊 System Health Indicators

Use these commands after any changes:

```bash
# Quick health check
top                    # Should show ~99% idle in safe mode
free                   # Check memory usage
dmesg                  # Look for errors or warnings
hrt_test               # Verify timing still works
param show CAL_ACC0_ID # Verify parameters persist (if set)

# SD card verification
ls -l /fs/microsd/     # Should list files
echo "test" > /fs/microsd/test.txt && cat /fs/microsd/test.txt

# Process monitoring
ps                     # List all processes
```

---

## 🔍 Troubleshooting

### If NSH Becomes Unresponsive
1. **Power-cycle the board** (not just reset)
2. Check if logger was started (safe mode prevents this)
3. Verify MAVLink is disabled (`MAV_0_CONFIG = 0`)

### If Parameters Don't Save
1. Check file size: `ls -l /fs/microsd/params` (should be >5 bytes)
2. Verify manual construction is applied in `parameters.cpp`
3. Check SD card is mounted: `mount`

### If Boot Hangs
1. Check for early exit in `rcS` (safe mode)
2. Verify board scripts are correctly configured
3. Review with `dmesg` after successful boot

---

## 📚 Related Documentation

- **[KNOWN_GOOD_SAFE_MODE_CONFIG.md](KNOWN_GOOD_SAFE_MODE_CONFIG.md)** - Complete configuration reference
- **[SAMV7_PARAM_STORAGE_FIX.md](SAMV7_PARAM_STORAGE_FIX.md)** - Parameter fix details
- **[DOCS_DMESG_HRT_ENABLE.md](DOCS_DMESG_HRT_ENABLE.md)** - Debugging tools setup
- **[attention_gemini.md](attention_gemini.md)** - Session-by-session progress log

---

**Last Updated:** November 20, 2025
**Build Timestamp:** Nov 20, 15:53
**Status:** ✅ Stable - Ready for incremental service re-enablement
