# Constrained Flash Build Analysis - SAMV71-XULT

## Current Configuration

**Board**: SAMV71Q21 (Cortex-M7 @ 150 MHz)
**Flash Memory**: 2 MB (2,097,152 bytes)
**SRAM**: 384 KB (393,216 bytes)
**Build Mode**: `CONFIG_BOARD_CONSTRAINED_FLASH=y`

## Current Memory Usage

```
Section         Used Size    Available    Percentage
─────────────────────────────────────────────────────
Flash (text):   880,812 B    2,097,152 B    42.0%
Data (data):        844 B      384 KB         0.2%
BSS (bss):       20,928 B      384 KB         5.3%
─────────────────────────────────────────────────────
Total SRAM:      21,772 B      393,216 B      5.5%
Flash Available: 1,216,340 B  (58.0% free)
```

**Status**: ✅ **Plenty of room for expansion** (58% flash free, 94.5% SRAM free)

---

## What You're MISSING with Constrained Flash Build

### 🔴 Critical Flight Features (DISABLED)

| Feature | Status | Impact | Flash Cost Est. |
|---------|--------|--------|-----------------|
| **DShot ESC Protocol** | ❌ Disabled | No digital ESC support | ~30 KB |
| **PWM Output** | ❌ Disabled | No motor control | ~20 KB |
| **ADC Battery Monitoring** | ❌ Disabled | No voltage/current sensing | ~15 KB |
| **Hardfault Logging** | ❌ Disabled | Can't debug crashes | ~10 KB |

**Total Critical Missing**: ~75 KB

### 🟡 Advanced EKF2 Features (DISABLED)

| Feature | Status | Impact | Flash Cost Est. |
|---------|--------|--------|-----------------|
| **Auxiliary Global Position** | ❌ Disabled | No RTK GPS fusion | ~25 KB |
| **Auxiliary Velocity** | ❌ Disabled | No optical flow fusion | ~20 KB |
| **Drag Fusion** | ❌ Disabled | Reduced wind estimation | ~15 KB |
| **External Vision** | ❌ Disabled | No VIO/SLAM fusion | ~30 KB |
| **GNSS Yaw** | ❌ Disabled | No dual-GPS heading | ~20 KB |
| **Range Finder** | ❌ Disabled | No lidar terrain following | ~25 KB |
| **Sideslip Fusion** | ❌ Disabled | Reduced fixed-wing accuracy | ~10 KB |

**Total EKF2 Advanced**: ~145 KB

### 🟢 Navigation Features (DISABLED)

| Feature | Status | Impact | Flash Cost Est. |
|---------|--------|--------|-----------------|
| **ADSB Collision Avoidance** | ❌ Disabled | No traffic avoidance | ~40 KB |
| **Airspeed Sensor** | ❌ Disabled | Fixed-wing only | ~20 KB |
| **Optical Flow** | ❌ Disabled | No vision-based positioning | ~30 KB |

**Total Navigation**: ~90 KB

### ⚪ Utility Features (DISABLED)

| Feature | Status | Impact | Flash Cost Est. |
|---------|--------|--------|-----------------|
| **dmesg** | ❌ Disabled | No boot log viewing (has bug anyway) | ~5 KB |
| **Module Help Text** | ❌ Disabled | No built-in command help | ~50 KB |
| **RGB LED Drivers** | ❌ Disabled | No fancy status LEDs | ~15 KB |
| **Tone Alarm** | ❌ Disabled | No audio feedback | ~10 KB |
| **Load Monitor** | ❌ Disabled | No CPU usage tracking | ~8 KB |
| **Payload Deliverer** | ❌ Disabled | No cargo drop | ~12 KB |
| **System Tests** | ❌ Disabled | No self-test commands | ~25 KB |
| **Performance Counters** | ❌ Disabled | No profiling | ~10 KB |

**Total Utility**: ~135 KB

### 📊 Additional Sensor Drivers (DISABLED)

Magnetometers not on your board (but excluded to save space):
- HMC5883, IIS2MDC, IST8308, IST8310, LIS3MDL, QMC5883L, RM3100, BMM350
- **Total**: ~80 KB

---

## Total Space Savings from Constrained Build

| Category | Space Saved |
|----------|-------------|
| Critical Flight Features | 75 KB |
| Advanced EKF2 | 145 KB |
| Navigation | 90 KB |
| Utility | 135 KB |
| Extra Sensors | 80 KB |
| **TOTAL SAVINGS** | **~525 KB** |

---

## What You STILL HAVE (Included)

### ✅ Essential Flight Systems

| System | Status | Purpose |
|--------|--------|---------|
| **HRT (High Res Timer)** | ✅ Enabled | Microsecond timing for control loops |
| **EKF2 (Core)** | ✅ Enabled | State estimation (IMU+GPS+Mag+Baro) |
| **MC Att Control** | ✅ Enabled | Multicopter attitude control |
| **MC Pos Control** | ✅ Enabled | Multicopter position control |
| **MC Rate Control** | ✅ Enabled | Multicopter rate control |
| **Commander** | ✅ Enabled | Mode management & arming |
| **Navigator** | ✅ Enabled | Mission & position navigation |
| **Flight Mode Manager** | ✅ Enabled | Flight mode switching |
| **Land Detector** | ✅ Enabled | Automatic landing detection |
| **Control Allocator** | ✅ Enabled | Motor mixing |
| **MAVLink** | ✅ Enabled | GCS communication |
| **Logger** | ✅ Enabled | Flight log recording |
| **Battery Status** | ✅ Enabled | Battery monitoring framework |
| **Sensors Module** | ✅ Enabled | Sensor aggregation |
| **Manual Control** | ✅ Enabled | Stick input processing |
| **RC Update** | ✅ Enabled | RC receiver processing |
| **Dataman** | ✅ Enabled | Mission storage |

### ✅ Sensor Drivers

| Sensor | Type | Status |
|--------|------|--------|
| **ICM-20689** | 6-axis IMU | ✅ Enabled (needs SPI driver) |
| **AK09916** | Magnetometer | ✅ Enabled (needs I2C fix) |
| **DPS310** | Barometer | ✅ Enabled (needs I2C fix) |
| **GPS** | GNSS | ✅ Enabled (needs UART config) |

### ✅ System Commands

| Command | Purpose | Status |
|---------|---------|--------|
| `param` | Parameter management | ✅ Enabled |
| `top` | CPU usage | ✅ Enabled |
| `ver` | Version info | ✅ Enabled |
| `nshterm` | NSH terminal | ✅ Enabled |
| `tune_control` | Buzzer control | ✅ Enabled |
| `bsondump` | Log export | ✅ Enabled |

---

## Memory Requirements for FULL Featured Build

### Estimated Full PX4 Build Sizes

**Based on typical PX4 boards (e.g., Pixhawk 4, FMUv5)**:

| Configuration | Flash Usage | SRAM Usage | Required Flash | Required SRAM |
|---------------|-------------|------------|----------------|---------------|
| **Minimal (Current)** | 881 KB (42%) | 22 KB (5.5%) | 1 MB | 64 KB |
| **Standard Multicopter** | ~1.2 MB (57%) | ~80 KB (20%) | 1.5 MB | 128 KB |
| **Full Featured** | ~1.5 MB (71%) | ~120 KB (30%) | 2 MB | 256 KB |
| **Everything Enabled** | ~1.8 MB (86%) | ~180 KB (45%) | 2 MB | 384 KB |

### SAMV71Q21 Capacity

| Resource | Capacity | Current Usage | Full Build Est. | Headroom |
|----------|----------|---------------|-----------------|----------|
| **Flash** | 2 MB | 881 KB (42%) | ~1.4 MB (67%) | 33% free |
| **SRAM** | 384 KB | 22 KB (5.5%) | ~100 KB (26%) | 74% free |

**Conclusion**: ✅ **SAMV71Q21 has MORE than enough memory for a full-featured PX4 build**

---

## Recommended Build Configurations

### Option 1: Current "Constrained" (Conservative)
```
Flash: 881 KB / 2048 KB (42%)
SRAM:   22 KB / 384 KB (5.5%)
Status: ✅ Safe, plenty of room
```

**Pros**:
- Fast compilation
- Minimal bloat
- Stable

**Cons**:
- Missing advanced features
- No PWM/DShot yet
- Limited diagnostics

### Option 2: "Standard Multicopter" (Recommended)
```
Estimated Flash: ~1.2 MB / 2048 KB (57%)
Estimated SRAM:  ~80 KB / 384 KB (20%)
Status: ✅ Recommended for flight
```

**Enable**:
- ✅ PWM/DShot output
- ✅ ADC battery monitoring
- ✅ Hardfault logging
- ✅ Optical flow (if sensor added)
- ✅ Range finder support
- ✅ Module help text
- ✅ System tests
- ✅ Load monitoring

**Disable** (not needed for multicopter):
- ❌ Fixed-wing modules (airspeed, VTOL)
- ❌ ADSB
- ❌ Advanced EKF features (unless needed)

### Option 3: "Full Featured" (Maximum Capability)
```
Estimated Flash: ~1.5 MB / 2048 KB (71%)
Estimated SRAM:  ~120 KB / 384 KB (30%)
Status: ✅ Still fits comfortably
```

**Enable Everything**:
- ✅ All EKF2 fusion modes
- ✅ All sensor drivers
- ✅ All navigation features
- ✅ All diagnostic tools
- ✅ Fixed-wing & VTOL support
- ✅ ADSB collision avoidance

---

## How to Upgrade from Constrained to Full Build

### Step 1: Remove Constrained Flag

Edit `boards/microchip/samv71-xult-clickboards/default.px4board`:

```diff
- CONFIG_BOARD_CONSTRAINED_FLASH=y
+ # CONFIG_BOARD_CONSTRAINED_FLASH is not set
- CONFIG_BOARD_NO_HELP=y
+ # CONFIG_BOARD_NO_HELP is not set
```

### Step 2: Enable Missing Critical Features

Add to `default.px4board`:

```bash
# Motor Output
CONFIG_DRIVERS_DSHOT=y
CONFIG_DRIVERS_PWM_OUT=y

# ADC for Battery Monitoring
CONFIG_DRIVERS_ADC_BOARD_ADC=y

# Diagnostics
CONFIG_SYSTEMCMDS_HARDFAULT_LOG=y
CONFIG_SYSTEMCMDS_PERF=y
CONFIG_SYSTEMCMDS_SYSTEM_TIME=y
```

### Step 3: Enable Advanced EKF2 (Optional)

```bash
# Advanced Sensor Fusion
CONFIG_EKF2_AUX_GLOBAL_POSITION=y  # RTK GPS
CONFIG_EKF2_RANGE_FINDER=y         # Lidar terrain following
CONFIG_EKF2_EXTERNAL_VISION=y      # VIO/SLAM
```

### Step 4: Rebuild and Verify

```bash
make microchip_samv71-xult-clickboards_default
# Check flash usage - should still be < 1.5 MB
```

---

## Comparison with Common PX4 Boards

| Board | MCU | Flash | SRAM | PX4 Build Size |
|-------|-----|-------|------|----------------|
| **Pixhawk 1 (FMUv2)** | STM32F427 | 2 MB | 256 KB | ~1.4 MB (70%) |
| **Pixhawk 4 (FMUv5)** | STM32F765 | 2 MB | 512 KB | ~1.5 MB (75%) |
| **CUAV V5+ (FMUv5)** | STM32F765 | 2 MB | 512 KB | ~1.6 MB (80%) |
| **Holybro Durandal** | STM32H743 | 2 MB | 1 MB | ~1.7 MB (85%) |
| **SAMV71-XULT** | SAMV71Q21 | **2 MB** | **384 KB** | **881 KB (42%)** ✅ |

**Analysis**: SAMV71-XULT has:
- ✅ Same flash as Pixhawk boards (2 MB)
- ✅ More SRAM than Pixhawk 1 (384 KB vs 256 KB)
- ✅ **Currently using HALF the flash of typical PX4 builds**
- ✅ **Plenty of room to enable all missing features**

---

## Recommended Next Steps

### Phase 1: Enable Critical Features (Now)
1. Keep `CONSTRAINED_FLASH=y` for now (good for development)
2. Implement missing drivers:
   - ✅ I2C initialization (done!)
   - ⚠️ SPI for ICM-20689
   - ⚠️ PWM output for motors
   - ⚠️ UART for GPS/RC
   - ⚠️ QSPI for flash storage

### Phase 2: Remove Constrained Flag (After Drivers Work)
1. Disable `CONSTRAINED_FLASH`
2. Enable PWM/DShot
3. Enable ADC battery monitoring
4. Enable hardfault logging
5. Enable module help text
6. **Expected size**: ~1.2 MB (still 40% free!)

### Phase 3: Add Advanced Features (Optional)
1. Enable advanced EKF2 fusion modes
2. Enable optical flow support
3. Enable range finder support
4. Enable system tests
5. **Expected size**: ~1.5 MB (still 25% free!)

---

## Flash Usage Breakdown (Current Build)

```
Component                Size (KB)    Percentage
─────────────────────────────────────────────────
Core Flight Control      ~350 KB      40%
EKF2 State Estimator     ~200 KB      23%
MAVLink Protocol         ~120 KB      14%
Sensor Drivers           ~80 KB       9%
Navigation               ~60 KB       7%
Logger                   ~40 KB       5%
System Libraries         ~20 KB       2%
─────────────────────────────────────────────────
Total                    ~881 KB      100%
```

---

## Conclusion

**You have PLENTY of memory for a full-featured build!**

| Metric | Current | Full Build | Max Capacity |
|--------|---------|------------|--------------|
| Flash Usage | 881 KB (42%) | ~1.4 MB (67%) | 2 MB (100%) |
| Flash Free | 1,216 KB (58%) | ~700 KB (33%) | 0 KB |
| SRAM Usage | 22 KB (5.5%) | ~100 KB (26%) | 384 KB (100%) |
| SRAM Free | 362 KB (94.5%) | ~284 KB (74%) | 0 KB |

**Recommendations**:

1. **For Development**: Keep constrained build (fast compilation, easy debugging)
2. **For Flight Testing**: Remove constrained flag, enable PWM/DShot + ADC
3. **For Production**: Full build with all features (~1.4 MB, still 33% free)

**Your SAMV71Q21 is NOT memory-constrained**. You're being conservative, which is smart for initial development, but you can easily enable everything once drivers are working!

---

**Document Version**: 1.0
**Last Updated**: 2025-11-16
**Current Build**: microchip_samv71-xult-clickboards_default (constrained)
**Flash Available**: 58% free (1.2 MB unused)
**Recommendation**: Remove `CONSTRAINED_FLASH` after driver implementation complete
