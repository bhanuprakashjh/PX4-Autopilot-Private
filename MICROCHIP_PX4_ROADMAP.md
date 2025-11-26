# Microchip PX4 Platform Roadmap
## SAMV71 → PIC32CZ CA70 → PIC32CZ CA80/CA90

**Created:** 2025-11-26
**Status:** Active Development
**Vision:** Build the most advanced, secure, and capable flight controller platform on Microchip silicon

---

## Executive Summary

This roadmap outlines the strategic path from our current SAMV71 implementation to the ultimate goal: a PIC32CZ CA90-based flight controller with hardware security, gigabit networking, and 8 MB of flash. By the end of this journey, we will have created a flight controller that **does things no Pixhawk can do**.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     THE MICROCHIP PX4 JOURNEY                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   PHASE 1              PHASE 2              PHASE 3              PHASE 4   │
│   SAMV71               PIC32CZ CA70         PIC32CZ CA80         CA90+HSM  │
│   Foundation           Drop-In Upgrade      Next-Gen Platform    Security  │
│                                                                             │
│   ┌─────────┐         ┌─────────┐          ┌─────────┐         ┌─────────┐│
│   │ 300 MHz │   ───▶  │ 300 MHz │   ───▶   │ 300 MHz │   ───▶  │ 300 MHz ││
│   │ 384KB   │         │ 512KB   │          │ 1MB+TCM │         │ 1MB+TCM ││
│   │ 2MB FL  │         │ 2MB FL  │          │ 8MB FL  │         │ 8MB+HSM ││
│   │ AEC-Q100│         │ Low Cost│          │ GbE     │         │ Secure  ││
│   └─────────┘         └─────────┘          └─────────┘         └─────────┘│
│                                                                             │
│   NOW (2025)          MID 2026             LATE 2026            2027+      │
│   In Progress         Pin-Compatible       New Design           Ultimate   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Current State: SAMV71-XULT (Phase 1)

### Code Structure

```
PX4-Autopilot-Private/
├── boards/microchip/samv71-xult-clickboards/
│   ├── default.px4board          # Build configuration
│   ├── nuttx-config/nsh/defconfig # NuttX kernel config
│   ├── init/                      # Boot scripts
│   │   ├── rc.board_defaults     # Board-specific params
│   │   ├── rc.board_sensors      # Sensor startup
│   │   └── rc.board_mavlink      # MAVLink config
│   └── src/
│       ├── board_config.h        # Pin definitions
│       ├── timer_config.cpp      # PWM config (STUB)
│       ├── spi.cpp               # SPI bus config
│       ├── i2c.cpp               # I2C bus config
│       ├── led.c                 # LED driver
│       ├── init.c                # Board init
│       └── usb.c                 # USB device init
│
└── platforms/nuttx/src/px4/microchip/samv7/
    ├── hrt/hrt.c                 # ✅ High-res timer (OPTIMIZED)
    ├── board_reset/              # ✅ Reset handling
    ├── board_critmon/            # ✅ Critical section monitor
    ├── version/                  # ✅ MCU version/identity
    ├── io_pins/io_timer_stub.c   # ❌ PWM timer (STUB)
    └── include/px4_arch/
        ├── io_timer.h            # Timer interface
        ├── io_timer_hw_description.h
        ├── spi_hw_description.h  # ✅ SPI definitions
        ├── i2c_hw_description.h  # ✅ I2C definitions
        └── micro_hal.h           # HAL interface
```

### Implementation Status

| Component | Status | Notes |
|-----------|--------|-------|
| **NuttX BSP** | ✅ Complete | Full SAMV71 support |
| **Boot/Init** | ✅ Working | USB console, SD card |
| **HRT Timer** | ✅ Optimized | 10x faster tick conversion |
| **FPU** | ✅ Enabled | Double-precision (FPv5-D16) |
| **TRNG** | ✅ Enabled | Hardware random number gen |
| **USB CDC** | ✅ Working | MAVLink over USB |
| **I2C** | ✅ Working | Sensors functional |
| **SPI** | ✅ Working | ICM20689 IMU verified |
| **SD Card** | ✅ Working | HSMCI with DMA |
| **Events** | ✅ Re-enabled | HRT fix resolved boot issue |
| **EKF2** | ✅ Enhanced | All advanced features enabled |
| **PWM Out** | ❌ STUB | Compiles but non-functional |
| **ADC** | ❌ Missing | No battery monitoring |
| **DShot** | ❌ Missing | No bidirectional ESC |
| **CAN-FD** | ❌ Disabled | Driver exists in NuttX |
| **Tone Alarm** | ❌ Missing | No audio feedback |

### Enabled PX4 Features (Current)

```
# Core Flight Stack
CONFIG_MODULES_COMMANDER=y           # Flight state machine
CONFIG_MODULES_EKF2=y                # State estimation
CONFIG_MODULES_MC_ATT_CONTROL=y      # Attitude control
CONFIG_MODULES_MC_POS_CONTROL=y      # Position control
CONFIG_MODULES_MC_RATE_CONTROL=y     # Rate control
CONFIG_MODULES_CONTROL_ALLOCATOR=y   # Motor mixing
CONFIG_MODULES_NAVIGATOR=y           # Mission execution
CONFIG_MODULES_FLIGHT_MODE_MANAGER=y # Flight modes
CONFIG_MODULES_LAND_DETECTOR=y       # Landing detection

# Sensors & Drivers
CONFIG_DRIVERS_IMU_INVENSENSE_ICM20689=y  # 6-axis IMU
CONFIG_DRIVERS_MAGNETOMETER_AKM_AK09916=y # Magnetometer
CONFIG_DRIVERS_BAROMETER_DPS310=y         # Barometer
CONFIG_DRIVERS_GPS=y                      # GPS module
CONFIG_DRIVERS_RC_INPUT=y                 # RC receiver

# Advanced EKF2 Features (Leveraging DP-FPU)
CONFIG_EKF2_AUX_GLOBAL_POSITION=y    # Secondary GPS
CONFIG_EKF2_AUXVEL=y                 # Aux velocity sources
CONFIG_EKF2_DRAG_FUSION=y            # Aerodynamic drag
CONFIG_EKF2_EXTERNAL_VISION=y        # SLAM/VIO support
CONFIG_EKF2_GNSS_YAW=y               # Dual-antenna heading
CONFIG_EKF2_RANGE_FINDER=y           # Lidar/rangefinder

# System
CONFIG_MODULES_BATTERY_STATUS=y      # Battery monitoring
CONFIG_MODULES_DATAMAN=y             # Waypoint storage
CONFIG_MODULES_LOGGER=y              # Flight logging
CONFIG_MODULES_MAVLINK=y             # Ground station comms
CONFIG_MODULES_LOAD_MON=y            # CPU monitoring
CONFIG_MODULES_EVENTS=y              # Event system
```

---

## Phase 1: SAMV71 Tier-1 Completion

### Goal: Full-Featured Flight Controller

Make SAMV71 a **production-ready** PX4 platform with all features necessary for autonomous flight.

### Remaining Tasks

#### 1. PWM Output (io_timer) - CRITICAL
**Effort:** 8-12 hours | **Priority:** P0

```c
// File: platforms/nuttx/src/px4/microchip/samv7/io_pins/io_timer.c

// SAMV71 Timer Channel Mapping
// TC0: CH0 (TIOA0/PA0), CH1 (TIOB0/PA1), CH2 (TIOA1/PA15)
// TC1: CH0 (TIOA2/PA26), CH1 (TIOB2/PA27), CH2 (TIOA3/PC25)

// Key functions to implement:
int io_timer_init_timer(unsigned timer, io_timer_channel_mode_t mode);
int io_timer_set_rate(unsigned timer, unsigned rate);
int io_timer_set_ccr(unsigned channel, uint16_t value);
int io_timer_channel_init(unsigned channel, io_timer_channel_mode_t mode,
                          channel_handler_t channel_handler, void *context);
```

**Test Plan:**
- [ ] Oscilloscope: Verify 50 Hz PWM, 1000-2000 µs pulses
- [ ] Connect servo: Verify movement matches command
- [ ] QGC: Motor test widget functional

#### 2. ADC (AFEC) - HIGH
**Effort:** 4-8 hours | **Priority:** P1

```c
// File: boards/microchip/samv71-xult-clickboards/src/adc.cpp

// SAMV71 AFEC Configuration
// AFEC0: General purpose ADC
// AFEC1: Battery monitoring

// Channels:
// AFEC0_CH0: VBAT (voltage divider 10:1)
// AFEC0_CH1: IBAT (current sense 0.01 ohm)
// AFEC0_CH2: 5V rail (divider 2:1)

// Leverage SAMV71's unique ADC features:
// - Differential input mode for better noise rejection
// - Programmable gain (0.5x to 4x)
// - Hardware offset correction
```

**Test Plan:**
- [ ] QGC: Battery voltage displays correctly
- [ ] QGC: Current reading (if sensor present)
- [ ] Logger: Verify ADC samples in .ulg file

#### 3. CAN-FD (MCAN) - HIGH
**Effort:** 4-8 hours | **Priority:** P2

```
# Enable in defconfig:
CONFIG_SAMV7_MCAN0=y
CONFIG_CAN=y
CONFIG_CAN_EXTID=y

# Enable in px4board:
CONFIG_DRIVERS_UAVCANV1=y
```

**Test Plan:**
- [ ] Connect CAN analyzer: Verify bus traffic
- [ ] Connect UAVCAN GPS: Verify data received
- [ ] Verify CAN-FD speeds (up to 8 Mbps data phase)

#### 4. DShot Protocol - MEDIUM
**Effort:** 8-12 hours | **Priority:** P3

```c
// File: platforms/nuttx/src/px4/microchip/samv7/dshot/dshot.c

// DShot150: 6.67 µs bit period
// DShot300: 3.33 µs bit period
// DShot600: 1.67 µs bit period

// Implementation: Use TC in waveform mode with DMA
// XDMAC provides scatter-gather for efficient frame transmission
```

#### 5. Tone Alarm - LOW
**Effort:** 2-4 hours | **Priority:** P4

```c
// Use PWM output channel for speaker
// Configure TC for audio frequency generation
// Implement standard PX4 tone sequences
```

### Phase 1 Timeline

```
┌─────────────────────────────────────────────────────────────────┐
│                    PHASE 1: SAMV71 COMPLETION                   │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  WEEK 1           WEEK 2           WEEK 3           WEEK 4     │
│  ┌─────────┐     ┌─────────┐     ┌─────────┐     ┌─────────┐  │
│  │io_timer │     │  ADC    │     │ CAN-FD  │     │ Testing │  │
│  │  PWM    │     │ Battery │     │ DShot   │     │ Polish  │  │
│  └─────────┘     └─────────┘     └─────────┘     └─────────┘  │
│                                                                 │
│  Deliverables:                                                  │
│  • Motors spin        • Battery % shown   • UAVCAN works       │
│  • Servo test works   • Current sense     • DShot300 verified  │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Phase 1 Success Criteria

- [ ] **Boot Time:** < 5 seconds to armed-ready
- [ ] **PWM:** 6 channels functional, verified with scope
- [ ] **ADC:** Battery voltage within 2% accuracy
- [ ] **CAN:** UAVCAN GPS works at CAN-FD speeds
- [ ] **CPU:** > 30% idle during hover simulation
- [ ] **Flash:** < 60% usage with all features
- [ ] **QGC:** Full connection, arm/disarm, motor test
- [ ] **HITL:** Complete flight simulation working

---

## Phase 2: PIC32CZ CA70 Migration

### Goal: Cost-Optimized Drop-In Replacement

The PIC32CZ CA70 is **pin-compatible** with SAMV71 but offers:
- 60% lower cost
- 512 KB RAM (vs 384 KB)
- Existing NuttX support

### Why CA70?

| Factor | SAMV71 | PIC32CZ CA70 |
|--------|--------|--------------|
| Price (10k) | ~$15 | ~$6 (60% less) |
| RAM | 384 KB | 512 KB (+33%) |
| Pin Compatible | - | ✅ Yes |
| NuttX Support | Full | Full (2024 port) |
| Code Changes | - | Minimal |

### Migration Tasks

**Effort:** 8-16 hours total

1. **Create New Board Config** (2-4 hours)
   ```
   boards/microchip/pic32cz-ca70-curiosity/
   ├── default.px4board      # Copy from SAMV71, adjust
   ├── nuttx-config/         # Use NuttX CA70 BSP
   └── src/                   # Reuse SAMV71 code
   ```

2. **Test Hardware Compatibility** (4-8 hours)
   - Verify all SAMV71 drivers work unchanged
   - Test I2C, SPI, UART, USB
   - Verify timer configuration

3. **Optimize for Extra RAM** (2-4 hours)
   - Increase sensor buffers
   - Enable larger EKF2 state
   - Add more waypoint storage

### Phase 2 Timeline

```
┌─────────────────────────────────────────────────────────────────┐
│               PHASE 2: PIC32CZ CA70 MIGRATION                   │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Start: After SAMV71 Tier-1 complete                           │
│  Duration: 2 weeks                                              │
│                                                                 │
│  WEEK 1              WEEK 2                                     │
│  ┌─────────────┐    ┌─────────────┐                            │
│  │ Board Config│    │ Testing     │                            │
│  │ Basic Port  │    │ Optimization│                            │
│  └─────────────┘    └─────────────┘                            │
│                                                                 │
│  Deliverables:                                                  │
│  • PIC32CZ CA70 board flies                                     │
│  • 60% cost reduction achieved                                  │
│  • RAM optimizations active                                     │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## Phase 3: PIC32CZ CA80 Platform

### Goal: Next-Generation Flight Controller

The CA80 requires a **new board design** but enables capabilities impossible on SAMV71:

| Feature | SAMV71 | PIC32CZ CA80 | Gain |
|---------|--------|--------------|------|
| Flash | 2 MB | 8 MB | **4x** |
| RAM | 384 KB | 1 MB | **2.6x** |
| TCM | None | 256 KB | **New** |
| Ethernet | 10/100 | **Gigabit** | **10x** |
| ADC | 2 Msps | 4.6875 Msps | **2.3x** |

### New Capabilities Unlocked

#### 1. On-Chip Flight Logging
```
8 MB Flash enables:
- Store 100+ minutes of high-rate logs on-chip
- No SD card required for short flights
- Faster boot (no SD init delay)
- More reliable (no card failures)
```

#### 2. Video + Telemetry over Gigabit
```
Gigabit Ethernet enables:
- 1080p video from companion computer
- Full telemetry simultaneously
- Sub-millisecond latency
- IEEE 1588 time sync for swarms
```

#### 3. TCM for Real-Time Guarantees
```
256 KB TCM enables:
- Rate controllers in TCM = zero-latency
- Guaranteed execution time
- No cache misses in critical paths
- ECC protection against bit flips
```

#### 4. Larger State Estimation
```
1 MB RAM enables:
- Full optical flow fusion
- Larger terrain maps
- More sensor fusion sources
- Complex mission logic
```

### NuttX Porting Tasks

**Effort:** 40-80 hours

```
1. CA80 BSP Creation (16-24 hours)
   - Clone CA70 BSP as starting point
   - Add GbE driver (GMAC)
   - Configure 8 MB flash layout
   - Set up TCM memory regions

2. Peripheral Drivers (16-24 hours)
   - SERCOM (UART/SPI/I2C) configuration
   - ADC (different from AFEC)
   - TC/PWM mapping
   - USB HS verification

3. PX4 Platform Layer (8-16 hours)
   - io_timer for CA80 TC
   - ADC driver update
   - HRT timer adaptation
   - Memory layout optimization

4. Testing & Optimization (8-16 hours)
   - Verify all peripherals
   - Performance benchmarks
   - Flight testing
```

### Phase 3 PX4 Enhancements

```
# New features enabled by CA80 hardware:

# Gigabit Ethernet MAVLink
CONFIG_MODULES_MAVLINK_ETHERNET=y    # New
CONFIG_MAVLINK_UDP_BUFFER=65536      # 64KB buffers

# On-chip logging
CONFIG_MODULES_LOGGER_FLASH=y        # New
CONFIG_LOGGER_FLASH_SIZE=4194304     # 4 MB for logs

# TCM optimization
CONFIG_FLIGHT_CRITICAL_IN_TCM=y      # New
CONFIG_EKF2_IN_TCM=y                 # New

# Larger state estimation
CONFIG_EKF2_OPTICAL_FLOW=y           # Requires RAM
CONFIG_EKF2_TERRAIN=y                # Terrain following
CONFIG_NAVIGATOR_RTCM=y              # RTK corrections
```

### Phase 3 Timeline

```
┌─────────────────────────────────────────────────────────────────┐
│               PHASE 3: PIC32CZ CA80 PLATFORM                    │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Start: After CA70 validation                                   │
│  Duration: 2-3 months                                           │
│                                                                 │
│  MONTH 1            MONTH 2            MONTH 3                  │
│  ┌─────────────┐   ┌─────────────┐   ┌─────────────┐           │
│  │ NuttX BSP   │   │ PX4 Platform│   │ Testing &   │           │
│  │ GbE Driver  │   │ New Features│   │ Optimization│           │
│  └─────────────┘   └─────────────┘   └─────────────┘           │
│                                                                 │
│  Deliverables:                                                  │
│  • CA80 boots PX4                                               │
│  • Gigabit Ethernet MAVLink works                               │
│  • On-chip logging functional                                   │
│  • TCM-optimized flight code                                    │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## Phase 4: PIC32CZ CA90 + HSM Security

### Goal: Military/Enterprise Grade Security

The CA90 adds a **Hardware Security Module** - making this the first flight controller with:
- True secure boot (tamper-proof)
- Hardware crypto acceleration
- Non-extractable key storage
- Firmware authentication

### Security Features

```
┌─────────────────────────────────────────────────────────────────┐
│                    CA90 HSM CAPABILITIES                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  SECURE BOOT                                                    │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ ROM Bootloader → Verify HSM → Verify PX4 → Run         │   │
│  │     (fixed)        (signed)    (signed)     (trusted)   │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
│  CRYPTO ACCELERATION                                            │
│  • AES-128/256: >1,280 Mbps (100x faster than software)        │
│  • SHA-256: Hardware accelerated                                │
│  • RSA/ECC: Key operations in hardware                          │
│  • TRNG: True random for key generation                         │
│                                                                 │
│  KEY STORAGE                                                    │
│  • Keys stored in HSM, never exposed to main CPU                │
│  • Resistant to physical attacks                                │
│  • Secure provisioning at factory                               │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Security-Enhanced PX4

```
# New security modules for CA90:

# Encrypted MAVLink
CONFIG_MAVLINK_ENCRYPTION=y          # AES-GCM encrypted links
CONFIG_MAVLINK_KEY_HSM=y             # Keys in HSM

# Secure Parameters
CONFIG_PARAM_ENCRYPTION=y            # Encrypted param storage
CONFIG_PARAM_SIGNATURE=y             # Signed parameter sets

# Secure Boot
CONFIG_SECURE_BOOT=y                 # Verify firmware signature
CONFIG_ANTI_ROLLBACK=y               # Prevent downgrade attacks

# Drone Authentication
CONFIG_DRONE_IDENTITY=y              # Hardware-backed identity
CONFIG_REMOTE_ID_SIGNED=y            # Signed Remote ID broadcasts
```

### Use Cases

1. **Defense/Military**
   - Encrypted command links
   - Tamper-proof firmware
   - Mission data protection

2. **Enterprise/Commercial**
   - Fleet authentication
   - Secure firmware updates
   - Regulatory compliance (Remote ID)

3. **Critical Infrastructure**
   - Power line inspection
   - Pipeline monitoring
   - Secure data handling

### Phase 4 Timeline

```
┌─────────────────────────────────────────────────────────────────┐
│               PHASE 4: CA90 SECURITY PLATFORM                   │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Start: After CA80 stable                                       │
│  Duration: 3-4 months                                           │
│                                                                 │
│  Prerequisites:                                                 │
│  • Sign Microchip HSM NDA                                       │
│  • Obtain CA90 development board                                │
│  • Complete HSM training                                        │
│                                                                 │
│  MONTH 1-2          MONTH 3            MONTH 4                  │
│  ┌─────────────┐   ┌─────────────┐   ┌─────────────┐           │
│  │ HSM Driver  │   │ Secure Boot │   │ MAVLink     │           │
│  │ Integration │   │ Key Mgmt    │   │ Encryption  │           │
│  └─────────────┘   └─────────────┘   └─────────────┘           │
│                                                                 │
│  Deliverables:                                                  │
│  • Secure boot verified                                         │
│  • Encrypted MAVLink functional                                 │
│  • Key provisioning workflow                                    │
│  • Security certification prep                                  │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## Complete Feature Matrix

### PX4 Feature Enhancement by Phase

| Feature | Phase 1 (SAMV71) | Phase 2 (CA70) | Phase 3 (CA80) | Phase 4 (CA90) |
|---------|------------------|----------------|----------------|----------------|
| **Core Flight** |
| EKF2 | ✅ Enhanced | ✅ Same | ✅ TCM Optimized | ✅ Same |
| Attitude Control | ✅ | ✅ | ✅ TCM | ✅ TCM |
| Position Control | ✅ | ✅ | ✅ | ✅ |
| Navigator | ✅ | ✅ | ✅ Extended | ✅ Secure |
| **Motor Output** |
| PWM | 🔨 Implementing | ✅ | ✅ | ✅ |
| DShot | 🔨 Planned | ✅ | ✅ | ✅ |
| **Sensors** |
| IMU (I2C/SPI) | ✅ | ✅ | ✅ | ✅ |
| Baro | ✅ | ✅ | ✅ | ✅ |
| Mag | ✅ | ✅ | ✅ | ✅ |
| GPS | ✅ | ✅ | ✅ | ✅ |
| ADC/Battery | 🔨 Implementing | ✅ | ✅ Fast ADC | ✅ |
| **Communication** |
| USB MAVLink | ✅ | ✅ | ✅ | ✅ Encrypted |
| Serial MAVLink | ✅ | ✅ | ✅ | ✅ Encrypted |
| CAN/UAVCAN | 🔨 Planned | ✅ | ✅ | ✅ |
| Ethernet | ❌ | ❌ | ✅ Gigabit | ✅ Gigabit |
| **Storage** |
| SD Card | ✅ | ✅ | ✅ | ✅ |
| On-chip Flash Log | ❌ | ❌ | ✅ 4 MB | ✅ 4 MB |
| QSPI Flash | ❌ | ❌ | ✅ Optional | ✅ Optional |
| **Security** |
| TRNG | ✅ | ✅ | ✅ | ✅ |
| Software Crypto | Optional | Optional | Optional | ❌ Use HSM |
| Hardware Crypto | ✅ AES/SHA | ✅ | ✅ | ✅ HSM |
| Secure Boot | ❌ | ❌ | ❌ | ✅ |
| Encrypted MAVLink | ❌ | ❌ | ❌ | ✅ |
| **Advanced** |
| External Vision | ✅ Ready | ✅ | ✅ | ✅ |
| Optical Flow | ❌ RAM | ✅ | ✅ | ✅ |
| Terrain Follow | ❌ RAM | ✅ | ✅ | ✅ |
| Swarm Sync | ❌ | ❌ | ✅ PTP | ✅ PTP |

### Hardware Comparison

| Spec | SAMV71 | CA70 | CA80 | CA90 |
|------|--------|------|------|------|
| CPU | Cortex-M7 300 MHz | Same | Same | Same |
| Flash | 2 MB | 2 MB | **8 MB** | **8 MB** |
| RAM | 384 KB | **512 KB** | **1 MB** | **1 MB** |
| TCM | None | None | **256 KB** | **256 KB** |
| Ethernet | 10/100 | 10/100 | **Gigabit** | **Gigabit** |
| ADC | 2 Msps | 2 Msps | **4.7 Msps** | **4.7 Msps** |
| USB | HS Built-in | HS Built-in | HS Built-in | HS Built-in |
| CAN-FD | 2x | 2x | 2x | 2x |
| HSM | ❌ | ❌ | ❌ | **✅** |
| Touch | ❌ | ❌ | **PTC** | **PTC** |
| Temp Range | **-40 to +125°C** | -40 to +85°C | -40 to +85°C | -40 to +85°C |
| Auto Grade | **AEC-Q100** | Industrial | Industrial | Industrial |
| Price (10k) | ~$15 | **~$6** | ~$15 | ~$16 |
| NuttX | ✅ Full | ✅ Full | 🔨 Port | 🔨 Port |

---

## Why We Will Be The Best

### 1. No Competitor Has This Path

```
┌─────────────────────────────────────────────────────────────────┐
│                    COMPETITIVE LANDSCAPE                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Pixhawk (STM32):                                              │
│  ✗ Limited to 2 MB flash                                       │
│  ✗ No hardware security module                                 │
│  ✗ No gigabit ethernet                                         │
│  ✗ 10/100 Mbps max networking                                  │
│  ✗ Supply chain issues (ST chip shortages)                     │
│                                                                 │
│  Microchip (Our Platform):                                     │
│  ✓ 8 MB flash - unlimited features                             │
│  ✓ Hardware Security Module option                             │
│  ✓ Gigabit Ethernet for video + telemetry                      │
│  ✓ 256 KB TCM for real-time guarantees                         │
│  ✓ Industrial supply chain (no shortages)                      │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 2. Unique Value Propositions

| Market | Our Advantage | Competitor Weakness |
|--------|---------------|---------------------|
| **Defense** | HSM secure boot, encrypted links | No hardware security |
| **Enterprise** | Fleet authentication, GbE video | Low bandwidth, no auth |
| **Industrial** | AEC-Q100 (SAMV71), reliable supply | Consumer-grade parts |
| **Research** | 8 MB flash = any algorithm | Flash-constrained |
| **Swarms** | GbE + IEEE 1588 sync | No precision timing |

### 3. The Killer Features

1. **"Flash is free"** - 8 MB means never disable a feature
2. **"Security by hardware"** - HSM can't be hacked in software
3. **"Video + Control"** - GbE handles both simultaneously
4. **"Deterministic"** - TCM guarantees timing
5. **"Future-proof"** - Clear upgrade path for 10+ years

---

## Investment Summary

### Total Effort Estimate

| Phase | Effort | Duration | Key Deliverable |
|-------|--------|----------|-----------------|
| **Phase 1** | 32-48 hours | 4 weeks | SAMV71 Tier-1 complete |
| **Phase 2** | 8-16 hours | 2 weeks | CA70 cost-optimized board |
| **Phase 3** | 40-80 hours | 8-12 weeks | CA80 next-gen platform |
| **Phase 4** | 60-100 hours | 12-16 weeks | CA90 secure platform |
| **Total** | 140-244 hours | 6-9 months | Full platform suite |

### Resource Requirements

**Hardware:**
- SAMV71-XULT: ✅ Have
- PIC32CZ CA70 Curiosity: $99
- PIC32CZ CA80 Curiosity: $150
- PIC32CZ CA90 Curiosity: $175

**Software:**
- PX4-Autopilot: ✅ Open source
- NuttX: ✅ Open source
- MPLAB X: ✅ Free
- Microchip HSM SDK: Requires NDA

**Knowledge:**
- ARM Cortex-M7 expertise: ✅ Have
- NuttX internals: ✅ Have
- PX4 architecture: ✅ Have
- HSM programming: Need training

---

## Conclusion

This roadmap transforms Microchip silicon from "alternative platform" to **the most capable flight controller architecture available**. By Phase 4, we will have:

1. **8 MB flash** - Room for any feature
2. **1 MB RAM + 256 KB TCM** - Complex algorithms, guaranteed timing
3. **Gigabit Ethernet** - Video + telemetry + swarm sync
4. **Hardware Security Module** - Military-grade protection
5. **Clear migration path** - SAMV71 → CA70 → CA80 → CA90

**The endgame:** A flight controller platform that does things **no Pixhawk can physically do**, backed by Microchip's industrial supply chain and 15+ year product commitment.

---

**Document Version:** 1.0
**Date:** 2025-11-26
**Author:** Microchip PX4 Development Team

---

## Quick Reference: Build Commands

```bash
# Phase 1: SAMV71
cd /media/bhanu1234/Development/PX4-Autopilot-Private
make microchip_samv71-xult-clickboards_default

# Flash via OpenOCD
openocd -f interface/cmsis-dap.cfg -f target/atsamv.cfg \
  -c "program build/microchip_samv71-xult-clickboards_default/*.elf verify reset exit"

# Test on hardware
# Connect USB, open QGC
# Run: nsh> top
# Run: nsh> listener sensor_combined
```
