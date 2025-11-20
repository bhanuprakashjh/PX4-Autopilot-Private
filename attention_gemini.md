1. Confirmed PX4 wasn’t calling constructors early: `__libc_init_array` only appeared via `lib_cxx_initialize` in `nxtask_startup`, so the heap-order problem had to be application-side, not an OS bug.
2. Decided to keep the placement-new fix in `param_init()` permanently; it avoids SAMV7’s fragile heap state during early constructor runs.
3. Identified console lockups as a resource fight (MAVLink blasting binary data over /dev/ttyACM0) compounded by SD logger stalls; NSH was getting starved, not crashing.
4. Pivoted to “safe mode”: disabled MAVLink, logging, and SYS_AUTOSTART via board scripts while leaving PX4’s kernel/uORB/params stack intact.
5. Verified SD-card initialization still spews CMD1/CMD55 warnings but eventually mounts; that’s acceptable as long as it prints “SD card initialized.”
6. Restored the original rcS flow (removed the hard `exit 0`) but kept the safe-mode defaults so PX4 can continue initializing without heavy services.
7. Enabled RAMLOG in NuttX (`CONFIG_RAMLOG_SYSLOG`, `/dev/ramlog`) and re-enabled the NSH `dmesg` command so we can capture boot logs post-startup.
8. Resolved `dmesg: command not found` by ensuring both the ROMFS scripts and NuttX config matched (syslog dev path, NSH command, RAM log).
9. Bumped `CONFIG_SYSTEMCMDS_TESTS` so the PX4 `systemcmds/tests` module builds, pulling in `hrt_test` for on-target timing verification.
10. Worked around ccache’s tmpdir permissions by pointing it to `/tmp/ccache_tmp`; builds now complete without “failed to create temporary file” errors.
11. Flashed the new firmware and confirmed `dmesg` outputs the boot log; noted that subsequent invocations only show new entries because RAMLOG consumes buffers.
12. Ran `hrt_test`; both the microsecond timers and HRT deferred callbacks passed, confirming the high-resolution timer hardware and px4::hrt layer are healthy.
13. Captured the process in `DOCS_DMESG_HRT_ENABLE.md` so we can recreate this state if future work breaks it.
14. Current state: NSH is instant, `dmesg`/`hrt_test` work, MAVLink and logger remain off, SD card responds to reads/writes, and we have a known-good “managed safe mode.”
15. Immediate next step: manually exercise `/fs/microsd` (create/delete a file) to confirm writes succeed before re-enabling SD logging.
16. Once SD writes look reliable, relocate MAVLink to a hardware UART (`MAV_0_CONFIG 101`, `MAV_0_RATE 57600`) using an FTDI adapter so USB stays dedicated to NSH.
17. After MAVLink is stable on the UART, re-enable logging (`SDLOG_MODE 1`) and ensure `dmesg` stays clean—if the SD driver regresses, turn logging off again.
18. Finally, restore `SYS_AUTOSTART` to the desired airframe ID so commander/sensors start automatically; do this only after MAVLink and logging no longer disrupt the console.
19. With that in place, we can iterate on sensor bring-up (board_sensors script), then revisit HITL/Gazebo or real hardware testing as needed.
20. Throughout, keep using `top`, `dmesg`, and `hrt_test` after each change—those are now our quick health checks before committing to the next stage.
21. Manual SD write/read probes succeed (`echo`/`cat`), but any sustained logging (`logger start`) still times out with errno 116, so SD logging must stay disabled until we address the HSMCI bottleneck.
22. `SDLOG_MODE` remains at -1 and the board script `rc.logging` still prints “logging disabled” to guarantee PX4 doesn’t auto-start the logger after reboot.
23. Each forced logger test required a power-cycle to recover NSH, confirming the failure mode is a blocking HSMCI driver rather than a PX4 bug.
24. `dmesg` now only dumps new entries after the first call, which is expected behavior with RAMLOG being treated as a FIFO.
25. Plan is to relocate MAVLink to TELEM1 (`MAV_0_CONFIG 101`, FTDI on USART1) next, leaving logging off so we don’t reintroduce USB console conflicts.
26. Once MAVLink is confirmed stable on the UART we’ll revisit SD logging with lower bandwidth options (e.g., `logger start -m drop`) or driver fixes.
