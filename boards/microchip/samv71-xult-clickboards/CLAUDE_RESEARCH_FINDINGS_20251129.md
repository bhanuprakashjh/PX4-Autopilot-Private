# Claude Research Findings - November 29, 2025

**Purpose:** Deep dive analysis before coding sprint
**Focus Areas:** SD write hang, QSPI scaffolding, ADC mapping

---

## 1. SD Card Write Hang - Root Cause Analysis

### Current State
| Operation | Mode | Status |
|-----------|------|--------|
| SD Read | DMA | Working |
| SD Write | PIO | Hangs under sustained load (logger) |
| TX DMA | Disabled | Broken since 2015 (25+ fix attempts) |

### Root Cause: PIO Write Blocks System

**File:** `platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/sam_hsmci.c`
**Function:** `sam_sendsetup()` (lines 2279-2396)

```c
// Lines 2320-2321 - THE PROBLEM
sched_lock();                      // Disable scheduler
flags = enter_critical_section();  // Disable interrupts

// Lines 2326-2392 - Tight polling loop
while (remaining > 0) {
    sr = sam_getreg(priv, SAM_HSMCI_SR_OFFSET);
    // ... poll TXRDY, write data word by word
}

leave_critical_section(flags);
sched_unlock();
```

**Impact:**
- Entire system blocked during every SD write
- For small writes: ~milliseconds, tolerable
- For logger (sustained writes): System hangs
- Watchdog can't run, tasks can't run

### Why TX DMA Was Disabled

From driver header (lines 116-139):
```
TX DMA Investigation Summary:
After exhaustive debugging (Fixes #1-#25), TX DMA persistently fails with
UNRE (underrun) errors despite applying all fixes from Microchip's reference:
- Fix #22: Removed XDMAC chunk size override
- Fix #23: Disabled WRPROOF/RDPROOF
- Fix #24: Single DMA setup call matching Microchip CSP
- Fix #25: 1-bit bus initialization per SD specification

Error Pattern: TX DMA always fails with SR: 04100004 (UNRE bit set),
indicating HSMCI FIFO was empty when trying to send data.

NuttX History: TX DMA has been documented as broken since 2015
```

### Possible Solutions

#### Option A: Fix TX DMA (High Risk)
**Effort:** Unknown (already tried 25+ times)
**Approach:**
- Compare cache flush timing with Microchip CSP exactly
- Try different XDMAC chunk/burst configurations
- Investigate HSMCI FIFO threshold settings

**Risk:** High - extensive prior attempts failed

#### Option B: Chunked PIO with Yielding (Medium Risk)
**Effort:** 4-6 hours
**Approach:**
- Modify `sam_sendsetup()` to write in chunks (e.g., 512 bytes)
- Release scheduler/interrupts between chunks
- Allow other tasks to run during large writes

**Code Concept:**
```c
#define PIO_CHUNK_SIZE 512  // Write this many bytes before yielding

static int sam_sendsetup_chunked(...)
{
    while (remaining > 0) {
        size_t chunk = MIN(remaining, PIO_CHUNK_SIZE);

        sched_lock();
        flags = enter_critical_section();

        // Write one chunk
        while (chunk > 0) { /* existing polling logic */ }

        leave_critical_section(flags);
        sched_unlock();

        // Yield to other tasks
        if (remaining > 0) {
            sched_yield();  // Or nxsig_usleep(1)
        }
    }
}
```

**Risk:** Medium - may cause slight performance drop, SD card timing issues

#### Option C: Re-enable WRPROOF Mode (Low Risk)
**Effort:** 2 hours
**Approach:**
- WRPROOF mode stops SD clock when HSMCI FIFO empty
- This prevents underruns and allows interrupts
- Was disabled in Fix #23, may have been wrong

**Code Change:**
```c
// In defconfig, enable:
CONFIG_SAMV7_HSMCI_WRPROOF=y

// In sam_hsmci.c, verify HSMCU_PROOF_BITS includes WRPROOF
```

**Risk:** Low - hardware feature designed for this purpose

#### Option D: Use Interrupt-Driven TX (Medium Effort)
**Effort:** 8-10 hours
**Approach:**
- Register TXRDY interrupt handler
- Write data in interrupt context
- Main thread can continue executing

**Risk:** Medium-High - significant driver restructuring

### Recommended Approach

**Phase 1 (Immediate):** Try Option C (WRPROOF)
- Quick to test, low risk
- May solve the problem with minimal code change

**Phase 2 (If Phase 1 fails):** Option B (Chunked PIO)
- Moderate effort, known to work in principle
- Trades some performance for stability

**Phase 3 (Long term):** Option A (Fix TX DMA)
- Only if chunked PIO has issues
- May require Microchip support

---

## 2. QSPI Parameter Storage

### Existing Documentation
Full implementation plan in: `legacy documents/QSPI_FLASH_TODO.md`

### Hardware
- **Chip:** SST26VF064B (8MB QSPI flash)
- **Interface:** QSPI0 on SAMV71
- **Status:** Present on board, unused

### Implementation Summary

#### defconfig additions:
```
CONFIG_SAMV7_QSPI=y
CONFIG_SAMV7_QSPI_DMA=y
CONFIG_SAMV7_QSPI0=y
CONFIG_MTD=y
CONFIG_MTD_SST26=y
```

#### Pin Configuration (from QSPI_FLASH_TODO.md):
```c
#define GPIO_QSPI0_CS     (GPIO_QSPI0_CS_1)    /* PA11 */
#define GPIO_QSPI0_IO0    (GPIO_QSPI0_IO0_1)   /* PA13 - MOSI */
#define GPIO_QSPI0_IO1    (GPIO_QSPI0_IO1_1)   /* PA12 - MISO */
#define GPIO_QSPI0_IO2    (GPIO_QSPI0_IO2_1)   /* PA17 - WP */
#define GPIO_QSPI0_IO3    (GPIO_QSPI0_IO3_1)   /* PD31 - HOLD */
#define GPIO_QSPI0_SCK    (GPIO_QSPI0_SCK_1)   /* PA14 */
```

#### Scaffolding Tasks:
1. Add QSPI config to defconfig (behind flag)
2. Add pin definitions to board.h
3. Add MTD init stub to init.c
4. Add partition configuration
5. Add Kconfig option for storage location

### Estimated Effort: 3-4 hours

---

## 3. ADC/AFEC Pin Mapping

### Current State
```c
// board_config.h - PLACEHOLDERS
#define ADC_BATTERY_VOLTAGE_CHANNEL  0
#define ADC_BATTERY_CURRENT_CHANNEL  1
#define BOARD_NUMBER_BRICKS          1
```

AFEC is **NOT enabled** in defconfig.

### SAMV71-XULT ADC Options

SAMV71 has two AFEC (Analog Front-End Controller) peripherals:
- **AFEC0:** 12 channels (AD0-AD11)
- **AFEC1:** 12 channels (AD0-AD11)

### Pins Available on SAMV71-XULT Headers

From SAMV71-XULT User Guide, Arduino-compatible headers:
| Arduino Pin | SAMV71 Pin | AFEC Channel |
|-------------|------------|--------------|
| A0 | PD30 | AFEC0_AD0 |
| A1 | PA21 | AFEC0_AD1 |
| A2 | PA3 | - (used for I2C) |
| A3 | PA4 | - (used for I2C) |
| A4 | PC31 | AFEC1_AD6 |
| A5 | PC30 | AFEC1_AD5 |

### mikroBUS Socket ADC
| Socket | Pin | SAMV71 | AFEC |
|--------|-----|--------|------|
| mikroBUS1 | AN | PD30 | AFEC0_AD0 |
| mikroBUS2 | AN | PC13 | AFEC1_AD1 |

### Recommended Configuration

```c
// board_config.h
/* AFEC Configuration for Battery Monitoring */
#define ADC_BATTERY_VOLTAGE_CHANNEL  0   // AFEC0_AD0 (Arduino A0 / mikroBUS1 AN)
#define ADC_BATTERY_CURRENT_CHANNEL  1   // AFEC0_AD1 (Arduino A1)
#define ADC_RSSI_CHANNEL            -1   // Not connected
#define ADC_5V_RAIL_CHANNEL         -1   // Not available on dev board
```

### defconfig additions:
```
CONFIG_SAMV7_AFEC=y
CONFIG_SAMV7_AFEC0=y
CONFIG_SAMV7_AFEC0_RES=12  # 12-bit resolution
CONFIG_ADC=y
```

### Calibration Needed
- `BAT_V_DIV` - Voltage divider ratio (depends on external circuit)
- `BAT_A_PER_V` - Current sensor scaling
- Need external voltage divider for >3.3V battery measurement

### Estimated Effort: 2-3 hours

---

## 4. Summary & Recommended Execution Order

### Day 1 (Nov 29)
1. **SD Write Fix Attempt #1:** Enable WRPROOF mode (2 hrs)
   - Quick test, may solve issue immediately

2. **QSPI Scaffolding:** Add config options (3 hrs)
   - Enable behind feature flag
   - Ready for Monday hardware test

3. **ADC Scaffolding:** Add AFEC config (2 hrs)
   - Enable behind feature flag
   - Map channels based on available pins

### Day 2 (Nov 30)
4. **SD Write Fix Attempt #2:** Chunked PIO (4-6 hrs)
   - If WRPROOF doesn't work
   - Implement yielding between chunks

5. **Testing & Documentation** (2 hrs)
   - Verify build succeeds
   - Update test checklist for Monday

---

## 5. Files to Modify

### SD Write Fix
| File | Changes |
|------|---------|
| `nuttx-config/nsh/defconfig` | Enable WRPROOF |
| `sam_hsmci.c` (NuttX) | Chunked PIO if needed |

### QSPI Scaffolding
| File | Changes |
|------|---------|
| `nuttx-config/nsh/defconfig` | QSPI config (commented) |
| `nuttx-config/include/board.h` | QSPI pin defines |
| `src/init.c` | MTD init stub |
| `src/board_config.h` | Feature flag |

### ADC Scaffolding
| File | Changes |
|------|---------|
| `nuttx-config/nsh/defconfig` | AFEC config (commented) |
| `src/board_config.h` | ADC channel defines, feature flag |

---

## 6. Test Plan for Monday

### SD Write Test
```bash
# Start logger
nsh> logger on

# Wait 10 minutes
# Check for hangs

# Verify log files
nsh> ls /fs/microsd/log/
nsh> ulog_info <latest.ulg>
```

### QSPI Test (if enabled)
```bash
# Check MTD device
nsh> ls /dev/mtd*

# Test parameter storage
nsh> param set TEST_QSPI 123
nsh> param save
nsh> reboot
nsh> param show TEST_QSPI
```

### ADC Test (if enabled)
```bash
# Check ADC readings
nsh> adc test

# Check battery topic
nsh> listener battery_status
```

---

**Document Created:** November 29, 2025
**Author:** Claude (AI Assistant)
**Status:** Ready for implementation
