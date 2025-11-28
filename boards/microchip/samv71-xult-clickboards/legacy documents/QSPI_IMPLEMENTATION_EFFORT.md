# QSPI Flash Implementation - Realistic Effort Estimate

**Hardware:** SST26VF064B 8MB QSPI Flash (already on SAMV71-XULT board)
**Date:** November 24, 2025
**Experience Level Assumed:** Intermediate (familiar with PX4 and NuttX)

---

## Quick Answer

**Total Time: 6-12 hours** (depending on issues encountered)

**Breakdown:**
- ✅ **Easy parts:** ~4 hours (configuration, known patterns)
- ⚠️ **Medium parts:** ~2-4 hours (debugging, pin verification)
- ❌ **Hard parts:** ~0-4 hours (if unexpected issues arise)

**Success Probability:**
- 80% chance it works first try after configuration
- 15% chance needs debugging (add 2-4 hours)
- 5% chance hits blocking issue (add 4+ hours)

---

## Detailed Effort Breakdown

### Phase 1: NuttX QSPI Configuration (1-2 hours)

**Task:** Enable QSPI peripheral and SST26 driver in NuttX

**Complexity:** 🟢 **Easy** - Just configuration

**Steps:**

1. **Edit NuttX defconfig** (30 min)
   ```bash
   # File: boards/microchip/samv71-xult-clickboards/nuttx-config/nsh/defconfig

   # Add these lines:
   CONFIG_SAMV7_QSPI=y
   CONFIG_SAMV7_QSPI0=y
   CONFIG_SAMV7_QSPI_DMA=y
   CONFIG_QSPI=y
   CONFIG_MTD=y
   CONFIG_MTD_SST26=y
   CONFIG_MTD_BYTE_WRITE=y
   CONFIG_MTD_PARTITION=y
   CONFIG_FS_LITTLEFS=y  # Or other filesystem
   ```

2. **Configure QSPI pins in board.h** (30 min)
   ```bash
   # File: boards/.../nuttx-config/include/board.h

   # Add QSPI pin definitions (need to verify from schematic):
   #define GPIO_QSPI0_CS     (GPIO_QSPI0_CS_1)
   #define GPIO_QSPI0_IO0    (GPIO_QSPI0_IO0_1)
   #define GPIO_QSPI0_IO1    (GPIO_QSPI0_IO1_1)
   #define GPIO_QSPI0_IO2    (GPIO_QSPI0_IO2_1)
   #define GPIO_QSPI0_IO3    (GPIO_QSPI0_IO3_1)
   #define GPIO_QSPI0_SCK    (GPIO_QSPI0_SCK_1)
   ```

3. **Build and test compile** (30 min)
   ```bash
   make microchip_samv71-xult-clickboards_default
   # Check for compile errors
   ```

**Potential Issues:**
- ⚠️ Pin mapping unclear from schematic (add 1 hour to research)
- ⚠️ Config option conflicts (add 30 min)

**Likelihood of Issues:** 30%

---

### Phase 2: Board Initialization Code (2-3 hours)

**Task:** Write code to initialize QSPI and SST26 flash

**Complexity:** 🟡 **Medium** - Need to write C code, follow patterns

**Steps:**

1. **Create QSPI initialization function** (1 hour)
   ```c
   // File: boards/.../src/init.c or new qspi.c

   #include <nuttx/spi/qspi.h>
   #include <nuttx/mtd/mtd.h>

   static int sam_qspi_init(void)
   {
       struct qspi_dev_s *qspi;
       struct mtd_dev_s *mtd;

       // Initialize QSPI peripheral
       qspi = sam_qspi_initialize(0);
       if (!qspi) {
           return -ENODEV;
       }

       // Initialize SST26 flash
       mtd = sst26_initialize(qspi, false);
       if (!mtd) {
           return -ENODEV;
       }

       // Register MTD device
       register_mtddriver("/dev/qspiflash0", mtd, 0755, NULL);

       return OK;
   }
   ```

2. **Call from board_app_initialize()** (30 min)
   ```c
   // In board_app_initialize() or px4_platform_configure()
   #ifdef CONFIG_SAMV7_QSPI
       ret = sam_qspi_init();
       if (ret < 0) {
           syslog(LOG_ERR, "QSPI init failed: %d\n", ret);
       }
   #endif
   ```

3. **Add to CMakeLists.txt** (15 min)
   ```cmake
   # If created separate qspi.c file
   px4_add_board_library(
       SRCS
           ...
           qspi.c  # Add this
   )
   ```

4. **Build and flash** (30 min)

**Potential Issues:**
- ⚠️ QSPI driver API mismatch (need to check NuttX version) - add 1 hour
- ⚠️ SST26 driver parameters unclear - add 30 min
- ⚠️ Initialization order issues - add 1 hour

**Likelihood of Issues:** 40%

---

### Phase 3: PX4 MTD Manifest (1-2 hours)

**Task:** Create PX4 MTD partition manifest

**Complexity:** 🟢 **Easy** - Copy existing pattern

**Steps:**

1. **Create mtd.cpp** (45 min)
   ```cpp
   // File: boards/.../src/mtd.cpp

   #include <px4_platform_common/px4_manifest.h>

   static const px4_mft_device_t qspi0 = {
       .bus_type = px4_mft_device_t::SPI,
       .devid    = SPIDEV_FLASH(0)
   };

   static const px4_mtd_entry_t qspi_flash = {
       .device = &qspi0,
       .npart = 2,
       .partd = {
           {
               .type = MTD_PARAMETERS,
               .path = "/fs/mtd_params",
               .nblocks = 512  // 64KB
           },
           {
               .type = MTD_CALDATA,
               .path = "/fs/mtd_caldata",
               .nblocks = 512  // 64KB
           }
       },
   };

   static const px4_mtd_manifest_t board_mtd_config = {
       .nconfigs = 1,
       .entries = { &qspi_flash }
   };

   static const px4_mft_entry_s mtd_mft = {
       .type = MTD,
       .pmft = (void *) &board_mtd_config,
   };

   static const px4_mft_s mft = {
       .nmft = 1,
       .mfts = { &mtd_mft }
   };

   const px4_mft_s *board_get_manifest(void)
   {
       return &mft;
   }
   ```

2. **Add to CMakeLists.txt** (15 min)

3. **Build and test** (30 min)

**Potential Issues:**
- ⚠️ Device ID mismatch - add 30 min
- ⚠️ Block size calculation wrong - add 1 hour

**Likelihood of Issues:** 20%

---

### Phase 4: Testing and Debugging (2-4 hours)

**Task:** Verify QSPI flash is working

**Complexity:** 🟡 **Medium** - Depends on issues found

**Testing Steps:**

1. **Boot and check dmesg** (15 min)
   ```bash
   nsh> dmesg | grep -i qspi
   # Should see: QSPI initialized, SST26 detected
   ```

2. **Check MTD devices** (15 min)
   ```bash
   nsh> ls /dev/mtd*
   /dev/mtd0      # QSPI flash
   /dev/mtd0p0    # /fs/mtd_params
   /dev/mtd0p1    # /fs/mtd_caldata
   ```

3. **Test raw writes** (30 min)
   ```bash
   nsh> dd if=/dev/zero of=/dev/mtd0 bs=512 count=1
   nsh> dd if=/dev/mtd0 of=/dev/null bs=512 count=1
   # Should succeed
   ```

4. **Test filesystem** (30 min)
   ```bash
   nsh> mkfs.littlefs /dev/mtd0p0
   nsh> mount -t littlefs /dev/mtd0p0 /fs/mtd_params
   nsh> echo "test" > /fs/mtd_params/test.txt
   nsh> cat /fs/mtd_params/test.txt
   ```

5. **Test parameter storage** (30 min)
   ```bash
   nsh> param set TEST_PARAM 123
   nsh> param save
   # Should save to /fs/mtd_params instead of SD card
   nsh> reboot
   nsh> param get TEST_PARAM
   # Should return 123
   ```

**Common Issues and Fixes:**

| Issue | Symptom | Fix | Time |
|-------|---------|-----|------|
| **Pin conflict** | QSPI init fails | Check schematic, verify pins | 1-2 hours |
| **Wrong flash detected** | SST26 not recognized | Verify part number, check driver | 1 hour |
| **DMA issues** | Timeouts on read/write | Disable QSPI DMA, use polling | 30 min |
| **Flash not formatted** | Mount fails | Format with mkfs.littlefs | 15 min |
| **Parameter system doesn't use MTD** | Still uses SD card | Check manifest, verify partition paths | 1 hour |

**Likelihood of Issues:** 50% (at least one debugging session needed)

---

## Realistic Time Estimates

### Best Case Scenario (Everything Works First Try):
- **Phase 1:** 1 hour (configuration)
- **Phase 2:** 2 hours (initialization code)
- **Phase 3:** 1 hour (MTD manifest)
- **Phase 4:** 2 hours (testing)
- **Total:** **6 hours**

### Expected Case (Minor Issues):
- **Phase 1:** 1.5 hours (+ pin mapping research)
- **Phase 2:** 3 hours (+ some API debugging)
- **Phase 3:** 1.5 hours (+ block size fixes)
- **Phase 4:** 3 hours (+ DMA or filesystem debugging)
- **Total:** **9 hours**

### Worst Case (Major Issues):
- **Phase 1:** 2 hours (+ config conflicts)
- **Phase 2:** 4 hours (+ initialization order issues)
- **Phase 3:** 2 hours (+ manifest debugging)
- **Phase 4:** 4 hours (+ pin conflicts or hardware issues)
- **Total:** **12 hours**

**Most Likely:** **8-10 hours** for someone familiar with PX4/NuttX

---

## What Makes This Easier

### ✅ You Have:

1. **Hardware already present** - SST26VF064B on board
2. **NuttX drivers exist** - sam_qspi.c and sst26.c already in tree
3. **Reference implementations** - Other PX4 boards use similar patterns
4. **Working SD card storage** - Can fall back if QSPI fails
5. **Stable platform** - System boots reliably, easy to test iterations

### ✅ Documentation Available:

1. **SST26VF064B Datasheet** - Flash chip specs
2. **SAMV71 Datasheet** - QSPI peripheral chapter
3. **NuttX QSPI Driver** - Source code well-commented
4. **PX4 MTD Manifest** - Examples from other boards
5. **SAMV71-XULT User Guide** - Pin mappings

---

## What Makes This Harder

### ⚠️ Potential Challenges:

1. **Pin Verification** - Need to confirm QSPI pins from schematic
   - Time to resolve: 1-2 hours
   - Probability: 30%

2. **DMA Configuration** - QSPI DMA might have same issues as HSMCI
   - Time to resolve: 2-4 hours
   - Probability: 20%
   - Mitigation: Use polling mode first

3. **Flash ID Reading** - SST26 driver might not auto-detect
   - Time to resolve: 1-2 hours
   - Probability: 15%

4. **Filesystem Format** - LittleFS might need tuning
   - Time to resolve: 1 hour
   - Probability: 25%

5. **Parameter System Integration** - MTD mount might not be automatic
   - Time to resolve: 1-2 hours
   - Probability: 30%

---

## Skills Required

### Must Have:
- ✅ C/C++ programming
- ✅ Basic embedded Linux/NuttX concepts
- ✅ Comfortable with command line and make
- ✅ Can read schematics (for pin verification)
- ✅ Patience for debugging

### Nice to Have:
- Understanding of SPI/QSPI protocol
- Experience with MTD/flash filesystems
- Familiarity with PX4 architecture
- NuttX configuration knowledge

**Your Level:** Appears to be **intermediate-advanced** based on SD card debugging success
**Estimated Difficulty:** **Medium** for you

---

## Step-by-Step Implementation Plan

### Session 1: Configuration (1-2 hours)

**Goal:** Get QSPI compiled into firmware

1. Enable QSPI in defconfig
2. Add pin definitions to board.h
3. Build and verify compilation
4. Flash and check boot (QSPI won't work yet, just checking no conflicts)

**Checkpoint:** Firmware builds and boots normally

---

### Session 2: Initialization (2-3 hours)

**Goal:** Initialize QSPI hardware

1. Write sam_qspi_init() function
2. Add to board initialization
3. Flash and check dmesg for QSPI messages

**Checkpoint:** Boot log shows "QSPI0 initialized"

---

### Session 3: Flash Detection (1-2 hours)

**Goal:** Detect SST26 flash chip

1. Call sst26_initialize()
2. Register MTD device
3. Flash and check `/dev/mtd0` exists

**Checkpoint:** `/dev/mtd0` device present

---

### Session 4: PX4 Integration (1-2 hours)

**Goal:** Create PX4 MTD partitions

1. Create mtd.cpp manifest
2. Define partitions for params/caldata
3. Flash and check `/dev/mtd0p0`, `/dev/mtd0p1` exist

**Checkpoint:** MTD partitions visible

---

### Session 5: Testing (2-4 hours)

**Goal:** Verify everything works

1. Format partitions
2. Mount filesystems
3. Test parameter save to MTD
4. Verify persistence across reboot

**Checkpoint:** `param save` uses `/fs/mtd_params`

---

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| **Pin conflicts with other peripherals** | Low (20%) | High | Check schematic carefully |
| **QSPI DMA issues** | Medium (40%) | Medium | Use polling mode initially |
| **SST26 not detected** | Low (15%) | High | Verify part number, check driver compatibility |
| **Filesystem corruption** | Low (10%) | Medium | Test extensively before production |
| **Parameter system doesn't switch to MTD** | Medium (30%) | Low | Manual configuration needed |

---

## Comparison with Alternative Approaches

### Option 1: Enable QSPI Flash (This Plan)
- **Effort:** 6-12 hours
- **Result:** Professional MTD storage
- **Risk:** Medium
- **Benefit:** Production-ready, unlimited writes

### Option 2: Use Internal Flash (Progmem)
- **Effort:** 8-16 hours (harder, dual-bank issues)
- **Result:** Limited storage, write cycles
- **Risk:** High (dual-bank flash bug exists)
- **Benefit:** No external dependencies

### Option 3: Stay with SD Card
- **Effort:** 0 hours
- **Result:** Current functionality
- **Risk:** None
- **Benefit:** Already working, adequate for development

---

## Recommendation

### For Your Current Stage:

**Effort Level:** Medium (6-12 hours)

**Should You Do It Now?**

**❌ No, if:**
- You want to start QGC/HIL testing immediately
- You're still in early development phase
- You need to test flight control algorithms
- Time to fly is more important than storage optimization

**✅ Yes, if:**
- You're preparing for production hardware
- You need factory calibration protection
- You want autonomous missions with persistent waypoints
- You have 1-2 days available for implementation

### Suggested Timeline:

**Week 1-2 (Now):**
- ✅ Focus on QGC connection and HIL testing
- ✅ Get basic flight control working
- ✅ Test with current SD card storage

**Week 3-4 (Later):**
- ⚠️ Implement QSPI flash for professional storage
- ⚠️ Add factory calibration protection
- ⚠️ Test persistent missions

---

## Success Probability

Based on your experience with SD card debugging:

**Probability of Success:**
- **Within 6-8 hours:** 70%
- **Within 8-10 hours:** 85%
- **Within 12 hours:** 95%

**Blocking Issues (5%):**
- Hardware defect (flash chip damaged)
- Schematic error (pins not connected as documented)
- NuttX driver incompatibility (rare)

---

## Bottom Line

### Effort Summary:
- **Minimum:** 6 hours (perfect execution)
- **Expected:** 8-10 hours (typical for your skill level)
- **Maximum:** 12 hours (if debugging needed)

### Complexity:
- **Configuration:** 🟢 Easy
- **Code Writing:** 🟡 Medium
- **Debugging:** 🟡 Medium
- **Testing:** 🟢 Easy

### Is It Worth It?

**For QGC/HIL:** ❌ **No** - Not needed at all
**For Development:** ⚠️ **Maybe** - Nice to have, not essential
**For Production:** ✅ **Yes** - Highly recommended

### My Recommendation:

**Defer QSPI implementation until:**
1. ✅ You've tested QGC connection
2. ✅ You've run HIL simulation
3. ✅ You've tested basic flight algorithms
4. ✅ You're ready for production hardware

**Then implement QSPI in a dedicated 1-2 day session.**

---

**Document Version:** 1.0
**Date:** November 24, 2025
**Estimated By:** Based on SAMV71-XULT hardware and NuttX driver analysis
**Confidence:** High (80%) - Assumes hardware functional and documentation accurate
