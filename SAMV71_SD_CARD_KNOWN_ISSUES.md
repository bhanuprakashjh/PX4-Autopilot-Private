# SAMV71 SD Card DMA - Known Issues and Future Fixes

**Board:** Microchip SAMV71-XULT
**Date:** November 22, 2025
**Status:** ⚠️ **POTENTIAL DATA INTEGRITY ISSUES**
**Current State:** SD card operational but error interrupts are being masked

---

## Executive Summary

While the SD card DMA is now **functionally working** after Fix #21, there are **3 critical error interrupts** that were enabled (Fix #16) but whose **root causes were not fixed** (Fix #17 just masks them).

**Current approach:** Log errors but don't abort DMA transfers
**Risk:** Silent data corruption or incomplete transfers
**Recommendation:** Investigate whether these errors actually occur under load

---

## Background - What Was Done

### Fix #16: Enable Error Interrupts
**File:** `sam_hsmci.c:242-247`

We enabled proper DMA error interrupts:
```c
#define HSMCI_DMARECV_INTS \
  (HSMCI_INT_XFRDONE | HSMCI_INT_UNRE | HSMCI_INT_OVRE | \
   HSMCI_INT_DTOE | HSMCI_INT_DCRCE)

#define HSMCI_DMASEND_INTS \
  (HSMCI_INT_XFRDONE | HSMCI_INT_UNRE | HSMCI_INT_OVRE | \
   HSMCI_INT_DTOE | HSMCI_INT_DCRCE)
```

**Before:** Only RXRDY/TXRDY (PIO interrupts) - wrong for DMA
**After:** Proper error detection enabled

### Fix #17: Mask Errors Instead of Fixing Them
**File:** `sam_hsmci.c:1594-1633`

We changed error handling to **not abort** DMA when errors occur:
```c
if ((pending & HSMCI_DATA_ERRORS) != 0) {
    mcerr("ERROR: enabled: %08x pending: %08x\n", enabled, pending);

    if (!priv->dmabusy) {
        /* PIO mode - abort on errors as before */
        sam_endtransfer(priv, SDIOWAIT_ERROR);
        continue;
    } else {
        /* DMA mode - log but DON'T abort! */
        mcerr("INFO: Data error during DMA - letting DMA complete\n");
    }
}
```

**Reason:** DCRCE was firing (possibly spuriously) and aborting all transfers
**Solution:** Let DMA complete even with errors, match Microchip's approach
**Problem:** This is a WORKAROUND, not a fix for the underlying errors

---

## The 3 Unresolved Error Interrupts

### Issue #1: OVRE (Overrun Error) - RX Path

#### What It Means
```
SD Card --[data]--> HSMCI FIFO --[DMA]--> Memory
                       ▲
                       |
                    256 words

OVRE fires when:
- SD card sends data
- FIFO becomes full (all 256 words occupied)
- New data arrives but FIFO has no space
- Data is LOST or OVERWRITTEN
```

#### Why It Shouldn't Happen
- **RDPROOF bit** (Fix #15): Ensures previous read completes before next
- **Hardware flow control**: HSMCI should assert flow control to pause card
- **DMA transfer rate**: Should keep FIFO drained faster than card fills it
- **FIFO threshold**: DMA should start before FIFO fills

#### What Happens If It Occurs
- ❌ **Data loss** - Incoming data discarded
- ❌ **File corruption** - Missing bytes in file
- ❌ **CRC errors** - File system detects corruption on next read
- ❌ **Silent failure** - Transfer completes but data is wrong

#### Current Status
- ✅ Interrupt enabled (Fix #16)
- ✅ RDPROOF bit set and preserved (Fix #15)
- ⚠️ Root cause not investigated
- ⚠️ Currently masked by Fix #17 (doesn't abort)

#### Investigation Required
1. Check if OVRE actually fires during normal operation
2. If yes, measure DMA transfer rate vs SD card data rate
3. Verify CSIZE=1, MBSIZE=1 are optimal (Fix #21)
4. Check FIFO threshold configuration
5. Verify hardware handshaking timing

---

### Issue #2: UNRE (Underrun Error) - TX Path

#### What It Means
```
Memory --[DMA]--> HSMCI FIFO --[data]--> SD Card
                       ▲
                       |
                    256 words

UNRE fires when:
- HSMCI tries to transmit to card
- FIFO is empty (no data available)
- HSMCI sends garbage/zeros
- Card writes corrupted data
```

#### Why It Shouldn't Happen
- **WRPROOF bit** (Fix #15): Ensures previous write completes before next
- **DMA fill rate**: Should keep FIFO filled faster than HSMCI drains it
- **Hardware handshaking**: HSMCI requests data only when needed

#### What Happens If It Occurs
- ❌ **Corrupted writes** - Card receives zeros or garbage
- ❌ **File corruption** - Written file contains wrong data
- ❌ **Silent failure** - Write appears successful but data is corrupted
- ❌ **Permanent data loss** - Once written to card, data is permanently wrong

#### Current Status
- ✅ Interrupt enabled (Fix #16)
- ✅ WRPROOF bit set and preserved (Fix #15)
- ⚠️ Root cause not investigated
- ⚠️ Currently masked by Fix #17 (doesn't abort)

#### Investigation Required
1. Check if UNRE actually fires during write operations
2. If yes, measure DMA fill rate vs HSMCI transmit rate
3. Verify memory bandwidth is sufficient
4. Check if write operations have different CSIZE/MBSIZE requirements
5. Test under heavy system load (multiple DMA channels active)

---

### Issue #3: DTOE (Data Timeout Error) - Both Paths

#### What It Means
```
Timeline:
Time 0: Command sent to card
Time 1: Card starts sending data
Time 2: Data transfer in progress...
Time 3: Card STOPS sending data (unexpectedly)
Time 4: HSMCI waits for more data...
Time 5: Timeout expires (DTOE fires)
```

#### Why It Shouldn't Happen
- Normal transfers complete without timeout
- Card should finish what it started
- Clock configuration should be correct

#### What Happens If It Occurs
- ❌ **Incomplete transfer** - Only partial data received
- ❌ **File corruption** - File shorter than expected
- ❌ **File system errors** - FAT table corruption
- ❌ **Unpredictable behavior** - Some transfers work, others don't

#### Possible Causes
1. **Card removed mid-transfer** - Physical disconnection
2. **Card malfunction** - Defective SD card
3. **Clock configuration issue** - HSMCI clock too fast for card
4. **Voltage issue** - Power supply instability
5. **Signal integrity** - Poor electrical connection
6. **Command error** - Card rejected command but we didn't notice

#### Current Status
- ✅ Interrupt enabled (Fix #16)
- ⚠️ Root cause not investigated
- ⚠️ Currently masked by Fix #17 (doesn't abort)

#### Investigation Required
1. Check if DTOE actually fires during normal operation
2. If yes, verify SD card clock configuration (HSMCI_MR register)
3. Test with multiple SD cards (rule out defective card)
4. Check power supply voltage and stability
5. Verify command response handling (R1, R3, R7, etc.)

---

## DCRCE (Data CRC Error) - Partially Addressed

### What It Means
- Card sends CRC checksum with each data block
- HSMCI calculates CRC independently
- If they don't match, DCRCE fires
- Indicates **data corruption** during transfer

### Why We See It
During our debugging, we observed:
```
sam_hsmci_interrupt: ERROR: enabled: 08200000 pending: 08200000
                                    ^^^^^^^^   ^^^^^^^^
                                    XFRDONE+DCRCE (both fired!)
```

### Current Handling (Fix #17)
```c
/* DMA mode - log but DON'T abort! */
mcerr("INFO: Data error during DMA - letting DMA complete\n");
```

### Why This Might Be Acceptable
1. **Microchip's approach**: Also logs CRC errors but completes transfer
2. **Spurious CRC errors**: Timing issues in FIFO can cause false positives
3. **Application decides**: Higher layer can re-read if CRC error occurred
4. **Still transfers data**: DMA completes, application checks error status

### Why This Might Be Wrong
1. **Real corruption**: CRC error might indicate actual data corruption
2. **Silent failure**: File system accepts corrupted data
3. **Cascading errors**: One corrupted block corrupts entire file system

### Investigation Required
1. Monitor DCRCE frequency during normal operation
2. If frequent, investigate:
   - FIFO timing (Fix #13 uses FIFO, should help)
   - FERRCTRL configuration (Fix #14)
   - Clock configuration
   - Signal integrity

---

## Testing Recommendations

### Phase 1: Detection
**Goal:** Determine if errors actually occur

**Add enhanced logging to `sam_hsmci.c` interrupt handler:**
```c
if ((pending & HSMCI_DATA_ERRORS) != 0) {
    mcerr("ERROR: enabled: %08x pending: %08x\n", enabled, pending);

    /* Log each error individually */
    if ((pending & HSMCI_INT_OVRE) != 0) {
        mcerr("  >>> OVRE: FIFO OVERRUN - data lost!\n");
    }
    if ((pending & HSMCI_INT_UNRE) != 0) {
        mcerr("  >>> UNRE: FIFO UNDERRUN - garbage sent!\n");
    }
    if ((pending & HSMCI_INT_DTOE) != 0) {
        mcerr("  >>> DTOE: Data timeout - incomplete transfer!\n");
    }
    if ((pending & HSMCI_INT_DCRCE) != 0) {
        mcerr("  >>> DCRCE: CRC error - data corrupted!\n");
    }
}
```

**Test scenarios:**
1. **Basic operation:**
   ```bash
   mount -t vfat /dev/mmcsd0 /fs/microsd
   echo "test" > /fs/microsd/test.txt
   cat /fs/microsd/test.txt
   ```
   **Expected:** No error interrupts

2. **Large file write:**
   ```bash
   dd if=/dev/zero of=/fs/microsd/large.bin bs=1024 count=1024
   ```
   **Expected:** No error interrupts

3. **Rapid read/write cycles:**
   ```bash
   for i in 1 2 3 4 5; do
     echo "test $i" > /fs/microsd/file$i.txt
     cat /fs/microsd/file$i.txt
   done
   ```
   **Expected:** No error interrupts

4. **Concurrent operations:**
   - SD card read/write while flight controller active
   - Multiple sensors using DMA simultaneously
   - High CPU load scenarios

   **Expected:** No error interrupts (true test of DMA bandwidth)

### Phase 2: Investigation (If Errors Detected)

**If OVRE occurs:**
1. Measure time between DMA transfers (add timestamps)
2. Check XDMAC channel priority
3. Verify memory bandwidth sufficient
4. Consider larger CSIZE/MBSIZE (currently 1/1 from Fix #21)

**If UNRE occurs:**
1. Test write-only workload
2. Check if memory cache conflicts exist
3. Verify D-cache flush for write buffers
4. Profile DMA memory access patterns

**If DTOE occurs:**
1. Lower SD card clock frequency
2. Test multiple SD cards
3. Add pull-up resistors to SD signals
4. Check power supply voltage during transfer
5. Verify CMD/DAT line signal integrity with oscilloscope

**If DCRCE occurs frequently:**
1. Lower SD card clock frequency
2. Verify FERRCTRL benefit (Fix #14)
3. Check if FIFO timing improved with Fix #13
4. Verify D-cache operations (Fix #1)

### Phase 3: Fixes (If Root Cause Identified)

**Potential fixes might include:**

1. **DMA Configuration:**
   ```c
   /* Try CSIZE=4, MBSIZE=4 for better throughput */
   regval |= XDMACH_CC_CSIZE_4;
   regval |= XDMACH_CC_MBSIZE_4;
   ```

2. **Clock Configuration:**
   ```c
   /* Reduce SD clock if signal integrity issues */
   /* Current: 25 MHz high-speed mode */
   /* Try: 12.5 MHz for stability */
   ```

3. **FIFO Threshold:**
   ```c
   /* Adjust DMA trigger threshold */
   /* Earlier trigger = less chance of overrun */
   ```

4. **Channel Priority:**
   ```c
   /* Increase XDMAC channel priority for HSMCI */
   /* Ensure SD card gets bandwidth when needed */
   ```

---

## Risk Assessment

### Current Risk Level: ⚠️ **MEDIUM**

**Why not HIGH:**
- ✅ SD card appears to work in testing
- ✅ Basic read/write operations successful
- ✅ File system not corrupted in limited testing
- ✅ Microchip also uses "log but continue" approach

**Why not LOW:**
- ❌ Unknown if errors actually occur under load
- ❌ No stress testing performed
- ❌ Production workload not tested
- ❌ Silent failures possible

### Production Readiness

**For low-duty cycle applications:** ✅ **Acceptable**
- Parameter storage (infrequent writes)
- Configuration files (rare access)
- Occasional data logging

**For high-duty cycle applications:** ⚠️ **Needs validation**
- Continuous data logging
- High-frequency sensor data recording
- Mission-critical parameter updates
- Flight logs during active operations

### Recommended Actions Before Production

1. ✅ **Add detailed error logging** (Phase 1)
2. ✅ **Perform stress testing** (Phase 2)
3. ✅ **Monitor for 24+ hours** under typical workload
4. ⚠️ **Only proceed if NO errors detected**
5. ⚠️ **If errors detected, investigate and fix** (Phase 3)

---

## Comparison with Microchip CSP

### What Microchip Does
**Reference:** `csp/peripheral/hsmci_6449/templates/plib_hsmci.c.ftl` (lines 117-142)

```c
if ((intFlags & (HSMCI_SR_XFRDONE_Msk | HSMCI_DATA_ERROR)) != 0U) {
    if ((intFlags & HSMCI_DATA_ERROR) != 0U) {
        // Just LOG errors to errorStatus flags
        if ((intFlags & HSMCI_SR_DCRCE_Msk) != 0U) {
            errorStatus |= HSMCI_DATA_CRC_ERROR;  // Flag only!
        }
        if ((intFlags & HSMCI_SR_DTOE_Msk) != 0U) {
            errorStatus |= HSMCI_DATA_TIMEOUT_ERROR;
        }
        // etc. for OVRE, UNRE
    }

    isDataInProgress = false;
    xferStatus |= HSMCI_XFER_STATUS_DATA_COMPLETED;  // Mark as completed!

    // Callback with status - APPLICATION decides what to do
}
```

**Key point:** Microchip provides error flags to the **application**, which then decides:
- Retry the transfer?
- Ignore the error?
- Fail the operation?

### What NuttX Does (After Fix #17)
```c
/* DMA mode - log but DON'T abort! */
mcerr("INFO: Data error during DMA - letting DMA complete\n");
```

**Key difference:** NuttX **doesn't provide error status to higher layers**
- Application has no way to know error occurred
- Cannot retry on error
- Cannot validate data integrity

### What We Should Add

**Proper error reporting to application layer:**

1. **Add error status to struct:**
   ```c
   struct sam_dev_s {
       // ... existing fields ...
       uint32_t data_errors;  /* Accumulated data error flags */
   };
   ```

2. **Store errors in interrupt handler:**
   ```c
   if ((pending & HSMCI_DATA_ERRORS) != 0) {
       priv->data_errors |= pending;  /* Accumulate errors */
       mcerr("INFO: Data error during DMA - letting DMA complete\n");
   }
   ```

3. **Application can check after transfer:**
   ```c
   /* After transfer completes */
   if (priv->data_errors & HSMCI_INT_DCRCE) {
       /* Retry read due to CRC error */
   }
   ```

**This would match Microchip's approach exactly.**

---

## Files That Would Need Modification

### 1. Enhanced Error Logging
**File:** `platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/sam_hsmci.c`
**Function:** `sam_interrupt()` (line ~1594)
**Change:** Add individual error logging for OVRE, UNRE, DTOE, DCRCE

### 2. Error Status Reporting
**File:** `platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/sam_hsmci.c`
**Struct:** `sam_dev_s` (line ~368)
**Change:** Add `uint32_t data_errors` field

**Function:** `sam_interrupt()` (line ~1594)
**Change:** Store error flags in `priv->data_errors`

**Function:** Application layer (higher level)
**Change:** Check error status after transfers, implement retry logic

### 3. DMA Configuration (If Needed)
**File:** `platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/sam_xdmac.c`
**Function:** `sam_txcc()`, `sam_rxcc()` (lines ~801, ~971)
**Change:** Adjust CSIZE/MBSIZE if OVRE/UNRE detected

### 4. Clock Configuration (If Needed)
**File:** `platforms/nuttx/NuttX/nuttx/arch/arm/src/samv7/sam_hsmci.c`
**Function:** `sam_clock()` or initialization
**Change:** Reduce clock frequency if signal integrity issues

---

## Summary Table

| Error | Meaning | Impact | Current Status | Investigation Priority |
|-------|---------|--------|----------------|----------------------|
| **OVRE** | FIFO overrun (RX) | Data loss | Masked (Fix #17) | 🔴 **HIGH** |
| **UNRE** | FIFO underrun (TX) | Corrupted writes | Masked (Fix #17) | 🔴 **HIGH** |
| **DTOE** | Data timeout | Incomplete transfer | Masked (Fix #17) | 🟡 **MEDIUM** |
| **DCRCE** | CRC error | Data corruption | Masked (Fix #17) | 🟡 **MEDIUM** (may be spurious) |

---

## Conclusion

The SD card DMA is **functionally working** but has **unresolved error conditions** that are currently **masked rather than fixed**.

**Immediate next steps:**
1. ✅ Add enhanced error logging (Phase 1)
2. ✅ Run stress tests to see if errors actually occur
3. ⚠️ Only use in production if NO errors detected
4. ⚠️ If errors occur, investigate root cause (Phase 2)

**Long-term recommendation:**
- Implement proper error status reporting to application layer (match Microchip)
- Add retry logic at application level
- Monitor error rates in production deployments
- Fix root causes if errors occur frequently

---

**Document Version:** 1.0
**Last Updated:** November 22, 2025
**Author:** Claude (Anthropic)
**Status:** ⚠️ Investigation Required
**Related:** SAMV71_SD_CARD_DMA_SUCCESS.md, SAMV71_HSMCI_MICROCHIP_COMPARISON_FIXES.md
