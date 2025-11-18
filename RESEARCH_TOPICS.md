# Research & Study Topics - SAMV7 Parameter Storage

**Context:** Deep-dive learning opportunities identified during SAMV7 parameter storage debugging
**Audience:** Developers working on embedded systems, PX4, or ARM Cortex-M7 platforms
**Priority Levels:** 🔴 Critical | 🟡 Important | 🟢 Nice-to-have

---

## 🔴 Critical Topics (Immediate Implementation Need)

### 1. Atomic Operations in Embedded Systems

**Why Important:** The lazy allocation fix requires race-free initialization.

**Study Topics:**
- `compare_exchange` semantics and ABA problem
- Memory ordering (acquire, release, relaxed, seq_cst)
- Lock-free vs wait-free algorithms
- When to use CAS vs mutexes in embedded

**Resources:**
- PX4 `px4::atomic` implementation in `src/lib/atomic/`
- PX4 `AtomicTransaction` class in `src/lib/parameters/atomic_transaction.h`
- C++11 `std::atomic` reference: https://en.cppreference.com/w/cpp/atomic/atomic
- Preshing on Programming: "Lock-Free Programming" series
- ARM Cortex-M7 LDREX/STREX instructions (ARMv7-M Architecture Reference)

**Practical Exercise:**
```cpp
// Implement a thread-safe lazy singleton using compare_exchange
class LazySingleton {
    static std::atomic<LazySingleton*> instance;
    static LazySingleton* getInstance();
};

// Compare your implementation to PX4's approach
```

**Questions to Answer:**
- What happens if two threads call `_ensure_allocated()` simultaneously?
- Why is `compare_exchange` needed instead of `if (_slots.load() == nullptr)`?
- What memory order should be used for the `_slots.load()` check?
- How does ARM Cortex-M7 implement atomic operations in hardware?

---

### 2. C++ Static Initialization Order Fiasco

**Why Important:** Root cause of the SAMV7 parameter storage bug.

**Study Topics:**
- Static initialization vs dynamic initialization phases
- Initialization order within a translation unit
- Initialization order across translation units (undefined!)
- Meyer's Singleton pattern (function-local static)
- NIFTY counter idiom

**Resources:**
- "Effective C++" by Scott Meyers - Item 4
- C++ FAQ: Static Initialization Order Fiasco
- GCC Bugzilla: Static init issues on ARM (bug #70013)
- PX4 codebase: Search for "static.*Layer" patterns

**Practical Exercise:**
```cpp
// File: a.cpp
struct A {
    A() { printf("A constructed\n"); }
};
static A a;

// File: b.cpp
struct B {
    B() { printf("B constructed\n"); }
};
static B b;

// Question: What order do A and B construct?
// Answer: Undefined! Could be either order.
```

**Questions to Answer:**
- Why does `malloc()` fail during static init on SAMV7 but not on other platforms?
- What is the initialization order of `firmware_defaults`, `runtime_defaults`, `user_config`?
- Could Meyer's Singleton pattern solve this? Why or why not?
- How can you guarantee initialization order in C++?

---

### 3. Placement New Operator

**Why Important:** Attempted fix that failed; need to understand why.

**Study Topics:**
- Placement new syntax: `new (address) Type(args)`
- When destructors need to be called manually
- Alignment requirements
- Object lifetime and undefined behavior
- Double-destruction bugs

**Resources:**
- C++ Placement New: https://en.cppreference.com/w/cpp/language/new
- "Effective C++" by Scott Meyers - Item 52
- PX4's failed fix attempt in this project (learn from failure!)

**Practical Exercise:**
```cpp
// Allocate buffer
char buffer[sizeof(MyClass)];

// Placement new
MyClass* obj = new (buffer) MyClass();

// When to call destructor?
obj->~MyClass();  // Manual destruction needed!

// What happens if you call destructor on failed static init object?
```

**Questions to Answer:**
- Why did calling the destructor cause "sam_reserved: PANIC"?
- What was the pointer value in `_slots` before destruction?
- Can placement new be used on already-constructed objects?
- What happens if you `free()` an invalid pointer on embedded systems?

---

## 🟡 Important Topics (Understanding System Behavior)

### 4. NuttX RTOS Heap Implementation

**Why Important:** Understanding why SAMV7 heap isn't ready during static init.

**Study Topics:**
- NuttX heap initialization sequence
- `kmm_initialize()` call timing
- Memory region definitions (SRAM, SDRAM, CCM)
- `CONFIG_ARCH_USE_MPU` and protected builds
- Early allocations vs normal allocations

**Resources:**
- NuttX source: `nuttx/mm/kmm_heap/`
- NuttX source: `nuttx/arch/arm/src/armv7-m/arm_initialize.c`
- SAMV7 memory map: `nuttx/arch/arm/src/samv7/samv7_allocateheap.c`
- NuttX documentation: Memory Management

**Practical Exercise:**
1. Trace through `arm_initialize()` step-by-step
2. Identify when `kmm_initialize()` is called
3. Determine when global constructors run relative to heap init
4. Compare to other ARM platforms (STM32H7, i.MX RT)

**Questions to Answer:**
- At what point in boot does the heap become usable on SAMV7?
- Why do other PX4 boards (STM32) not have this issue?
- Could we move heap init earlier in the boot sequence?
- What is the minimum heap size needed for DynamicSparseLayer?

---

### 5. BSON (Binary JSON) Format

**Why Important:** Parameter files use BSON serialization.

**Study Topics:**
- BSON specification (bsonspec.org)
- Document structure (header + elements + footer)
- Type encoding (int32, double, string, etc.)
- PX4's BSON implementation

**Resources:**
- BSON Spec: http://bsonspec.org/spec.html
- PX4 BSON encoder: `src/lib/parameters/bson_encoder.c`
- PX4 BSON decoder: `src/lib/parameters/bson_decoder.c`

**Practical Exercise:**
```bash
# Examine a parameter file
hexdump -C /fs/microsd/params

# Expected structure:
# [4 bytes: document length]
# [elements: type + name + value]
# [1 byte: 0x00 terminator]

# Why is broken file only 5 bytes?
# Answer: Length (4) + terminator (1) = 5 (no elements!)
```

**Questions to Answer:**
- What is the overhead per parameter in BSON format?
- How does BSON handle parameter name length limits?
- Why BSON instead of binary struct or JSON?
- How are float parameters encoded (precision)?

---

### 6. SD Card Protocols (SD vs MMC, SDIO vs SPI)

**Why Important:** Understanding SD card initialization errors in boot log.

**Study Topics:**
- SD Card specification (SD Association)
- MMC (MultiMediaCard) vs SD differences
- SDIO 4-bit mode vs SPI mode
- CMD1 (MMC) vs CMD41/ACMD41 (SD)
- Card initialization state machine

**Resources:**
- SD Specifications Part 1: Physical Layer Simplified Specification
- SAMV7 HSMCI (High Speed Multimedia Card Interface) datasheet
- NuttX HSMCI driver: `nuttx/arch/arm/src/samv7/sam_hsmci.c`

**Practical Exercise:**
```
# Analyze boot errors:
sam_waitresponse: ERROR: cmd: 00008101 events: 009b0001 SR: 04100024
mmcsd_cardidentify: ERROR: CMD1 RECVR3: -22

# CMD1 = SEND_OP_COND (MMC-specific)
# Error: Card doesn't respond (it's SD, not MMC!)
# Driver should try ACMD41 next for SD cards
```

**Questions to Answer:**
- Why does the driver try MMC first if SD is more common?
- What is the performance difference between SDIO and SPI modes?
- How does card detection work (GPIO vs SDIO DAT3 pull-up)?
- What is the 1000ms delay for in `samv71_sdcard_initialize()`?

---

### 7. PX4 Parameter Layer Architecture

**Why Important:** Understanding how parameters flow through the system.

**Study Topics:**
- Layer hierarchy (firmware → runtime → user)
- Layer delegation pattern (`get()` falls through to parent)
- When layers are populated (compile-time, init scripts, user commands)
- `param_import()` vs `param_load_default()`
- Parameter notification system (uORB topic)

**Resources:**
- PX4 source: `src/lib/parameters/parameters.cpp`
- PX4 source: `src/lib/parameters/ParamLayer.h`
- PX4 docs: Parameter Synchronization

**Practical Exercise:**
```
# Trace a param_get() call:
1. user_config.get(PARAM_ID)
2. Not found? → runtime_defaults.get(PARAM_ID)
3. Not found? → firmware_defaults.get(PARAM_ID)
4. Return value

# Why is this better than a single parameter array?
```

**Questions to Answer:**
- What happens when you `param reset` a single parameter?
- How does `param diff` determine "changed" parameters?
- What is the role of `params_active` and `params_unsaved` bitsets?
- When does autosave trigger (timeout, count threshold)?

---

## 🟢 Nice-to-Have Topics (Optimization & Future Work)

### 8. Lock-Free Data Structures

**Why Important:** DynamicSparseLayer uses lock-free patterns; could be optimized.

**Study Topics:**
- Lock-free queues (MPMC, MPSC, SPMC, SPSC)
- Lock-free hash tables
- Lock-free skip lists
- RCU (Read-Copy-Update) patterns
- Hazard pointers for memory reclamation

**Resources:**
- "The Art of Multiprocessor Programming" by Herlihy & Shavit
- Preshing blog: "Lock-Free Programming" series
- CppCon talks on lock-free programming

**Practical Exercise:**
- Analyze PX4's `AtomicTransaction` class
- Compare to `std::scoped_lock` and mutexes
- Benchmark parameter access latency with/without locks

---

### 9. Flash Memory Management on Microcontrollers

**Why Important:** Understanding why flash backend was broken, in case we need it later.

**Study Topics:**
- Flash sector/block organization on ARM
- Wear leveling strategies
- Flash file systems (LittleFS, SPIFFS, FatFS on flash)
- Parameter storage best practices
- Error correction codes (ECC)

**Resources:**
- SAMV71 datasheet: Flash Memory Controller (EEFC)
- LittleFS design doc: https://github.com/littlefs-project/littlefs/blob/master/DESIGN.md
- PX4 flashparams: `src/lib/parameters/flashparams/`

**Questions to Answer:**
- Why were the original flash addresses wrong (0x00420000)?
- What is the correct sector map for SAMV71Q21?
- How many erase cycles before flash wears out?
- Should parameters use last sectors or first sectors?

---

### 10. ARM Cortex-M7 Memory Protection Unit (MPU)

**Why Important:** May affect memory allocation and static initialization.

**Study Topics:**
- MPU regions and permissions
- NuttX protected vs flat builds
- Kernel vs user space in NuttX
- SRAM partitioning (kernel heap vs user heap)
- Early memory allocation restrictions

**Resources:**
- ARM Cortex-M7 Technical Reference Manual - Chapter on MPU
- NuttX documentation: Memory Protection
- SAMV7 memory regions in `defconfig`

**Practical Exercise:**
- Check if SAMV71 build uses MPU (`CONFIG_ARM_MPU`)
- Compare flat vs protected build parameter behavior
- Analyze if MPU restrictions affect static init malloc

---

### 11. Embedded Debugging Techniques

**Why Important:** Skills needed to diagnose similar issues in future.

**Study Topics:**
- GDB for ARM targets
- OpenOCD scripting
- SWD/JTAG protocols
- Hardfault analysis (fault registers, stack unwinding)
- printf-style debugging vs trace tools
- Performance profiling with perf counters

**Resources:**
- OpenOCD User's Guide: https://openocd.org/doc/html/
- GDB for ARM: "The Definitive Guide to ARM Cortex-M"
- PX4 hardfault logger: `src/systemcmds/hardfault_log/`

**Practical Exercise:**
```bash
# Connect GDB to running target
arm-none-eabi-gdb build/.../firmware.elf
target extended-remote :3333

# Set breakpoint in param_set
break param_set_internal

# Examine storage layer state
print user_config._slots
print user_config._n_slots
```

---

### 12. Parameter Calibration Systems

**Why Important:** Understanding why `CAL_*` parameters are critical.

**Study Topics:**
- IMU calibration (accelerometer, gyroscope, magnetometer)
- Parameter interdependencies
- Calibration procedures in QGroundControl
- Factory calibration vs user calibration
- Sensor fusion parameter tuning

**Resources:**
- PX4 User Guide: Sensor Calibration
- QGroundControl source: Calibration wizards
- PX4 commander module: `src/modules/commander/commander.cpp`

**Practical Exercise:**
- Run `commander calibrate accel` and watch parameter changes
- Identify which parameters are set during calibration
- Understand parameter persistence requirements

---

### 13. Real-Time Operating System Concepts

**Why Important:** NuttX is an RTOS; understanding priorities and scheduling matters.

**Study Topics:**
- Priority inversion and priority inheritance
- Work queues (high priority, low priority)
- Rate-monotonic scheduling
- Stack size determination
- Interrupt vs thread context

**Resources:**
- NuttX documentation: Task Scheduling
- "Real-Time Concepts for Embedded Systems" by Li & Yao
- PX4 work queue manager: `src/lib/work_queue/`

**Practical Exercise:**
```bash
# View task priorities
nsh> ps

# Compare priorities:
# hpwork: 249 (high)
# lpwork: 50 (low)
# param task: varies

# When can parameter autosave preempt sensor reads?
```

---

### 14. Build System Architecture (CMake + PX4)

**Why Important:** Understanding how config propagates from `.px4board` to binary.

**Study Topics:**
- PX4 board definition system
- CMake ExternalData for configs
- Parameter generation from metadata
- ROMFS generation for init scripts
- Conditional compilation patterns

**Resources:**
- PX4 build system docs: https://docs.px4.io/main/en/dev_setup/building_px4.html
- PX4 source: `platforms/common/cmake/`
- PX4 board configs: `boards/*/default.px4board`

**Practical Exercise:**
```bash
# Trace config processing
cmake --trace-expand . 2>&1 | grep PARAM_FILE

# Examine generated files
cat build/.../romfs/init.d/rc.board_defaults

# Understand parameter generation
cat build/.../px4_parameters.cpp
```

---

## Study Plan Recommendations

### For Immediate Implementation (This Week)

Focus on **Critical** topics 1-3:
1. Read about atomic operations (2 hours)
2. Implement `_ensure_allocated()` with `compare_exchange` (4 hours)
3. Test on hardware (2 hours)
4. Debug any race conditions (2-4 hours)

### For System Understanding (Next Week)

Study **Important** topics 4-7:
- 4. NuttX heap (1 day) - understand the environment
- 5. BSON format (2 hours) - verify parameter files
- 6. SD protocols (2 hours) - clean up boot errors
- 7. PX4 parameter layers (4 hours) - master the architecture

### For Long-Term Expertise (Monthly)

Explore **Nice-to-Have** topics 8-14:
- Pick one topic per week as a "deep dive" study session
- Build example projects to practice concepts
- Contribute improvements to PX4 based on learnings

---

## Recommended Books

1. **"Effective C++" by Scott Meyers**
   - Static initialization, object lifetime, placement new

2. **"The Art of Multiprocessor Programming" by Herlihy & Shavit**
   - Lock-free algorithms, atomic operations, memory models

3. **"Real-Time Concepts for Embedded Systems" by Li & Yao**
   - RTOS fundamentals, scheduling, priorities

4. **"The Definitive Guide to ARM Cortex-M" by Joseph Yiu**
   - ARM architecture, MPU, interrupts, debugging

5. **"Embedded Systems Architecture" by Daniele Lacamera**
   - Memory management, flash, bootloaders, RTOS

---

## Online Courses & Resources

1. **ARM Education:**
   - Free courses on Cortex-M architecture
   - https://www.arm.com/resources/education

2. **PX4 Developer Guide:**
   - Official documentation and tutorials
   - https://docs.px4.io/main/en/development/

3. **NuttX Documentation:**
   - RTOS concepts and driver development
   - https://nuttx.apache.org/docs/latest/

4. **Preshing on Programming:**
   - Excellent blog on lock-free and concurrent programming
   - https://preshing.com/

5. **Embedded Artistry:**
   - Embedded systems best practices
   - https://embeddedartistry.com/

---

## Practice Projects

### Project 1: Thread-Safe Lazy Singleton
Implement `_ensure_allocated()` in isolation, test with multiple threads.

### Project 2: BSON Parser
Write a simple BSON decoder to inspect parameter files manually.

### Project 3: Parameter Backend Simulator
Create a user-space test program that simulates PX4 parameter layers.

### Project 4: Static Init Tester
Create minimal test cases for static initialization order issues.

### Project 5: Lock-Free Queue
Implement a single-producer-single-consumer lock-free queue for sensor data.

---

## Conclusion

These topics represent a deep well of knowledge that will make you an expert in embedded systems, real-time operating systems, and PX4 architecture. Prioritize based on immediate needs, but don't neglect the foundational topics that build long-term understanding.

**Remember:** The best learning happens through doing. Implement the fixes, break things, fix them again, and understand *why* each solution works or fails.
