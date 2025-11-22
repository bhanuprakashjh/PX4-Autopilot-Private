# SAMV71 SD DMA Debug Snapshot (Nov 21, 2025)

## Current Repo State
- Branch `samv7-custom`
- Latest build artifacts: `build/microchip_samv71-xult-clickboards_default/{*.elf,*.bin,*.px4}`
- Equipment: SAMV71-XULT board running PX4 (NuttX 11.0.0).

## What Works
- HSMCI clocks/command path OK (CMD0/8/55 sequences run).
- SDIO DMA path re-enabled (TX + RX) with cache clean/invalidate.
- DMA start is deferred until after CMD17/CMD24 is issued (`sam_sendcmd` -> `sam_begindma`).
- XDMAC PERIDs for HSMCI forced to 0 (matching hardware request line).

## What Still Fails
- SCR read (8 bytes) still times out: `sam_dmacallback result=-4`, `SR=0x08000027`. Need to reprogram `HSMCI_BLKR` right before CMD51 so BCNT/BLKLEN survive.
- 512-byte read/write: DMA cancels immediately (`result=-4`) even though `RXRDY/TXRDY` go high. XDMAC CUBC/CBC never decrement – descriptor still running multi-beat bursts that overflow the FIFO.

## Next Steps (to implement)
1. **BLKR reinit** – Write `HSMCI_BLKR_BCNT(1) | BLKLEN(block)` immediately before CMD17/CMD51 so zero-length reads don’t occur.
2. **Single-beat TX descriptor** – After `sam_dmatxsetup`, patch the head descriptor (`g_xdmach[ch]->llhead`) so:
   - `cubc = buflen / 4` (words), `cbc = 1` micro-block,
   - `cc` enforces `CHKSIZE_1`, `DWIDTH_WORD`, `PERID(0)`.
3. **Telemetry** – Add CUBC/CBC dump inside `sam_eventtimeout` to prove whether DMA advanced.
4. Rebuild/flash, run `mount -t vfat /dev/mmcsd0 /fs/microsd`, capture new `dmesg`.

Use this as the starting context for the next session.
