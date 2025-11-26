# PIC32CZ CA80/CA90 Analysis for PX4 Flight Controllers

## Executive Summary

The **PIC32CZ CA80/CA90** is Microchip's newest ARM Cortex-M7 MCU family, designed as a successor to the SAMV71. It offers significant improvements in memory, security, and connectivity while maintaining the same CPU architecture.

**Key Finding:** PIC32CZ CA80 is the natural upgrade path from SAMV71, but requires NuttX porting work before PX4 can run on it.

---

## Quick Comparison Table

| Feature | PIC32CZ CA80 | SAMV71Q21B | STM32H753II (FMU-v6X) | Winner |
|---------|--------------|------------|----------------------|--------|
| **CPU Core** | Cortex-M7 | Cortex-M7 | Cortex-M7 | Tie |
| **Clock Speed** | 300 MHz | 300 MHz | 480 MHz | STM32H7 |
| **Flash** | **8 MB** | 2 MB | 2 MB | **PIC32CZ** |
| **SRAM** | **1 MB** | 384 KB | 1 MB | PIC32CZ/STM32 |
| **TCM (Tightly Coupled)** | **256 KB** | None | 128 KB | **PIC32CZ** |
| **FPU** | Double-precision | Double-precision | Double-precision | Tie |
| **L1 Cache** | 16 KB I + 16 KB D (ECC) | 16 KB I + 16 KB D | 16 KB I + 16 KB D | Tie |
| **Ethernet** | **10/100/1000 (GbE)** | 10/100 | 10/100 | **PIC32CZ** |
| **USB** | High-Speed (480 Mbps) | High-Speed (480 Mbps) | HS needs ULPI | PIC32CZ/SAMV71 |
| **CAN-FD** | Yes (2x) | Yes (2x) | Yes (2x) | Tie |
| **ADC** | 12-bit, **4.6875 Msps** | 12-bit, 2 Msps | 16-bit, 3.6 Msps | Mixed |
| **Hardware AES** | Yes (via HSM on CA90) | Yes | Yes | Tie |
| **Hardware Security Module** | **YES (CA90 only)** | NO | NO | **PIC32CZ CA90** |
| **Touch Controller** | **YES (PTC)** | NO | NO | **PIC32CZ** |
| **Automotive Qualified** | Industrial | **AEC-Q100** | NO | SAMV71 |
| **NuttX Support** | Partial (CA70 only) | Full | Full | SAMV71/STM32 |
| **PX4 Support** | **NONE** | In Development | Production | STM32H7 |
| **Price (10k qty)** | $14.80 | ~$10-15 | ~$15-20 | Similar |

---

## PIC32CZ Family Overview

### PIC32CZ CA70 (Budget Option)
- Pin-compatible with SAMV71
- 512 KB SRAM (vs 384 KB SAMV71)
- **60% lower cost** than SAMV71
- NuttX support available (recent port)
- No HSM, no Gigabit Ethernet

### PIC32CZ CA80 (Standard)
- Up to 8 MB Flash, 1 MB SRAM
- 256 KB TCM with ECC
- Gigabit Ethernet with AVB/IEEE 1588
- Peripheral Touch Controller (PTC)
- **No HSM**

### PIC32CZ CA90 (Security)
- Same as CA80 plus:
- **Integrated Hardware Security Module (HSM)**
- Hardware crypto acceleration (AES, RSA, ECC, SHA)
- Secure key storage
- Secure boot support
- $0.74 premium over CA80

---

## PIC32CZ CA80 Unique Advantages

### 1. Massive Flash (8 MB)

**What:** 4x more flash than SAMV71 or STM32H7

**PX4 Impact:**
- Full logging to internal flash (no SD card needed for short flights)
- Multiple firmware images for A/B updates
- Store parameters, calibration, terrain data internally
- Room for ML/vision processing code

**Comparison:**
| MCU | Flash | PX4 Binary | Headroom |
|-----|-------|------------|----------|
| PIC32CZ CA80 | 8 MB | ~1 MB | **7 MB (87%)** |
| SAMV71Q21B | 2 MB | ~1 MB | 1 MB (50%) |
| STM32H753II | 2 MB | ~1.9 MB | 100 KB (5%) |

---

### 2. Large SRAM (1 MB) with 256 KB TCM

**What:** 2.6x more RAM than SAMV71, plus tightly coupled memory

**TCM Benefits:**
- Zero-wait-state access (faster than cache)
- Deterministic timing for real-time code
- ECC protected (error detection/correction)
- Ideal for critical flight control loops

**PX4 Impact:**
- Larger EKF2 state vectors
- More waypoints in memory
- Bigger sensor buffers
- Complex flight modes without RAM constraints

---

### 3. Gigabit Ethernet (10/100/1000)

**What:** 10x faster than SAMV71/STM32's 10/100 Ethernet

**Features:**
- IEEE 1588 Precision Time Protocol (PTP)
- Audio Video Bridging (AVB)
- Dedicated DMA engine

**PX4 Impact:**
- High-bandwidth video streaming from companion computers
- Real-time MAVLink with minimal latency
- Multi-drone swarm coordination
- Ground station with full telemetry + video

---

### 4. Hardware Security Module (CA90 Only)

**What:** Dedicated security co-processor with:
- AES, RSA, ECC, SHA hardware acceleration
- Secure key storage (non-extractable)
- Secure boot chain verification
- True Random Number Generator
- Firmware authentication

**CA90 HSM Crypto Performance:**
| Algorithm | Throughput |
|-----------|------------|
| AES-128 | >1,280 Mbps |
| SHA-256 | Hardware accelerated |
| RSA/ECC | Hardware accelerated |

**PX4 Impact:**
- Encrypted MAVLink communication
- Secure firmware updates (only signed code runs)
- Protected parameters and calibration
- Drone authentication (anti-spoofing)
- Military/commercial security requirements

**Note:** CA90 requires NDA for HSM documentation.

---

### 5. Peripheral Touch Controller (PTC)

**What:** Integrated capacitive touch sensing

**PX4 Impact:**
- Touch-based arming gestures
- Touch buttons for mode selection
- Proximity sensing for safety
- No external touch IC needed

---

### 6. Faster ADC (4.6875 Msps)

**What:** 2x faster sampling than SAMV71's AFEC

**PX4 Impact:**
- Better current/voltage waveform analysis
- Faster ESC telemetry decoding
- Higher-precision power measurements

---

## Migration Path: SAMV71 → PIC32CZ

Microchip provides an official migration guide (AN5891).

### Compatible Aspects
| Feature | Compatibility |
|---------|---------------|
| CPU Core | Identical (Cortex-M7) |
| Instruction Set | Binary compatible |
| Pin Layout | Different (requires new PCB) |
| DMA | Similar (XDMAC) |
| Timers | Similar |
| UART/SPI/I2C | SERCOM-based (different register model) |

### Key Differences Requiring Code Changes

1. **Serial Communications:** SAMV71 uses dedicated USART/SPI/TWI peripherals; PIC32CZ uses SERCOM (configurable)

2. **ADC:** SAMV71 AFEC vs PIC32CZ ADC (different register interface)

3. **GPIO:** Different register model for pin configuration

4. **Clock System:** Different oscillator/PLL configuration

5. **DMA:** Similar XDMAC but register differences

### Estimated Porting Effort

| Component | SAMV71 → PIC32CZ CA80 | Notes |
|-----------|----------------------|-------|
| NuttX BSP | 40-80 hours | CA70 port exists, CA80 needs extension |
| PX4 Board Config | 8-16 hours | New board definition |
| HRT Timer | 2-4 hours | Similar TC peripheral |
| PWM Output | 8-16 hours | Different timer mapping |
| ADC Driver | 4-8 hours | Different ADC peripheral |
| SPI/I2C | 4-8 hours | SERCOM configuration |
| **Total** | **66-132 hours** | ~2-4 weeks full-time |

---

## NuttX Status

### PIC32CZ CA70 (Supported)
- Port submitted to NuttX in 2024
- Tested on PIC32CZ CA70 Curiosity board
- Working: USB CDC/ACM, SD card, buttons
- Binary compatible with SAMV71

### PIC32CZ CA80/CA90 (NOT Supported)
- No current NuttX support
- Would require new board support package
- CA80-specific peripherals (GbE, PTC) need drivers
- HSM integration for CA90 requires NDA

---

## PX4 Feasibility Assessment

### Pros
1. Massive memory (8 MB flash, 1 MB RAM) eliminates constraints
2. Gigabit Ethernet for advanced applications
3. HSM for commercial/military security requirements
4. Future-proof platform from Microchip
5. Migration guide from SAMV71 available
6. Same Cortex-M7 core = code reuse

### Cons
1. **No NuttX support for CA80/CA90** (major blocker)
2. No current PX4 port (ground-up effort)
3. Different peripheral model from SAMV71
4. 300 MHz (vs 480 MHz STM32H7)
5. HSM requires NDA (CA90)
6. New PCB design required

### Verdict

**Short-term (0-6 months):** Continue SAMV71 development. It has working NuttX/PX4 support.

**Medium-term (6-18 months):** Consider PIC32CZ CA70 as drop-in replacement when SAMV71 goes EOL. It's pin-compatible and has NuttX support.

**Long-term (18+ months):** PIC32CZ CA80/CA90 is the ideal target for next-gen flight controllers with:
- Security requirements (CA90 HSM)
- High-bandwidth networking (GbE)
- Large code/data requirements (8 MB flash)

---

## Recommended Strategy

```
┌─────────────────────────────────────────────────────────────────┐
│                    MICROCHIP PX4 ROADMAP                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  NOW (2025)          NEAR (2026)          FUTURE (2027+)       │
│  ┌─────────┐        ┌─────────┐          ┌─────────┐          │
│  │ SAMV71  │───────▶│PIC32CZ  │─────────▶│PIC32CZ  │          │
│  │ Q21B    │        │ CA70    │          │ CA80/90 │          │
│  └─────────┘        └─────────┘          └─────────┘          │
│                                                                 │
│  Features:          Features:            Features:             │
│  • Working PX4      • Pin-compatible     • 8MB Flash           │
│  • Full NuttX       • 512KB RAM          • 1MB RAM             │
│  • AEC-Q100         • 60% cheaper        • GbE, HSM            │
│                     • NuttX ready        • Needs port          │
│                                                                 │
│  Effort: 0          Effort: Low          Effort: High          │
│  (already done)     (reuse SAMV71 BSP)   (new port)           │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## Comparison Summary

```
┌────────────────────────────────────────────────────────────────┐
│              PIC32CZ CA80 vs SAMV71 vs STM32H753II             │
├────────────────────────────────────────────────────────────────┤
│                                                                │
│  PIC32CZ CA80 WINS:                                           │
│  ✓ Flash: 8 MB (4x more)                                      │
│  ✓ RAM: 1 MB + 256KB TCM                                      │
│  ✓ Ethernet: Gigabit (10x faster)                             │
│  ✓ Security: HSM option (CA90)                                │
│  ✓ Touch: Integrated PTC                                      │
│  ✓ ADC: 4.6875 Msps                                           │
│                                                                │
├────────────────────────────────────────────────────────────────┤
│                                                                │
│  SAMV71 WINS:                                                 │
│  ✓ PX4 Support: In development                                │
│  ✓ NuttX Support: Full                                        │
│  ✓ Automotive: AEC-Q100 qualified                             │
│  ✓ USB PHY: Integrated                                        │
│  ✓ Availability: Production                                   │
│                                                                │
├────────────────────────────────────────────────────────────────┤
│                                                                │
│  STM32H753II WINS:                                            │
│  ✓ Clock Speed: 480 MHz (60% faster)                          │
│  ✓ PX4 Support: Production-ready                              │
│  ✓ Ecosystem: Mature, documented                              │
│  ✓ Crypto Modes: GCM/CCM/HMAC                                 │
│                                                                │
└────────────────────────────────────────────────────────────────┘
```

---

## Development Resources

### Hardware
- [PIC32CZ CA80 Curiosity Ultra Board](https://www.microchip.com/en-us/development-tool/ev51s73a) - $150
- [PIC32CZ CA90 Curiosity Ultra Board](https://www.microchip.com/en-us/development-tool/ev16w43a) - $175

### Documentation
- [PIC32CZ CA80/CA90 Datasheet (PDF)](https://ww1.microchip.com/downloads/aemDocuments/documents/MCU32/ProductDocuments/DataSheets/PIC32CZ-CA80-CA90-Family-Data-Sheet-DS60001749.pdf)
- [Migration Guide: SAMV71 to PIC32CZ CA80 (AN5891)](https://ww1.microchip.com/downloads/aemDocuments/documents/MCU32/ApplicationNotes/ApplicationNotes/AN5891-Migration-Guide-from-SAM-E70-S70-V70-V71-to-PIC32CZ-CA80-CA9x-MCU-DS00005891.pdf)
- [NuttX SAMV7/PIC32CZ CA70 Documentation](https://nuttx.apache.org/docs/latest/platforms/arm/samv7/index.html)

### Software
- MPLAB X IDE
- MPLAB Harmony v3
- NuttX (for CA70, needs port for CA80)

---

## Conclusion

**PIC32CZ CA80 is the future** of Microchip's Cortex-M7 lineup, offering compelling advantages over both SAMV71 and STM32H7 in memory, networking, and security. However, the lack of NuttX/PX4 support makes it a **medium-to-long term target**.

**Recommendation:**
1. Complete SAMV71 PX4 port first (current work)
2. Monitor NuttX community for CA80 progress
3. When SAMV71 reaches Tier-1 status, evaluate CA80 port
4. For security-critical applications, plan for CA90 with HSM

---

**Document Version:** 1.0
**Date:** 2025-11-26
**Author:** Claude Code Analysis

## Sources

- [Microchip PIC32CZ CA Product Page](https://www.microchip.com/en-us/products/microcontrollers/32-bit-mcus/pic32-sam/pic32cz-ca)
- [CNX Software - PIC32CZ CA HSM Overview](https://www.cnx-software.com/2023/10/12/microchip-pic32cz-ca-300-mhz-arm-cortex-m7-mcu-hardware-security-module-hsm/)
- [NuttX SAMV7/PIC32CZ Documentation](https://nuttx.apache.org/docs/latest/platforms/arm/samv7/index.html)
- [Microchip Migration Guide AN5891](https://onlinedocs.microchip.com/oxy/GUID-89A2670A-012B-4B42-867E-FA7E7D6FFC4A-en-US-1/index.html)
- [PX4 Flight Controller Documentation](https://docs.px4.io/main/en/flight_controller/)
