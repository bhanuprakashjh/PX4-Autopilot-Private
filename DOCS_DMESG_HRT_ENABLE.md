# SAMV71 Safe-Mode Instrumentation Notes

This log tracks the changes required to enable `dmesg` and `hrt_test` while keeping the SAMV71 port in its SD-debug “safe mode”.

## Target State

* `ROMFS/px4fmu_common/init.d/rcS` runs the full startup script (no hard exit), but board defaults keep heavy services disabled:
  * `SYS_AUTOSTART=0`, `MAV_0_CONFIG=0`, `SDLOG_MODE=-1`.
* RAM log (`/dev/ramlog`) is the syslog backend so `dmesg` can dump boot logs after the fact.
* NSH `dmesg` command is enabled; first call shows the full boot buffer, subsequent calls only show new lines (RAMLOG consumes entries).
* `systemcmds/tests` + `systemcmds/tests/hrt_test` are built so `hrt_test` runs on hardware.

## Source Changes

1. **rcS:** removed the early “SAFE MODE: exit 0” block to allow normal PX4 init (board defaults continue to suppress MAVLink/logging/autostart).
2. **Board defaults (`boards/microchip/samv71-xult-clickboards/init`):**
   * `rc.board_defaults` sets `SYS_AUTOSTART 0`, `MAV_0_CONFIG 0`, `SDLOG_MODE -1`.
   * `rc.board_mavlink`/`rc.logging`/`rc.serial_port` keep MAVLink and logger disabled so safe-mode behavior persists without killing rcS entirely.
3. **NuttX defconfig (`boards/.../nuttx-config/nsh/defconfig`):**
   * Enabled RAMLOG + SYSLOG (`CONFIG_RAMLOG=y`, `CONFIG_RAMLOG_SYSLOG=y`, `CONFIG_SYSLOG_DEVPATH="/dev/ramlog"`, `CONFIG_RAMLOG_BUFSIZE=8192`).
   * Kept `CONFIG_SYSTEMCMDS_DMESG`/`CONFIG_SYSTEMCMDS_HRT` set so the commands are compiled in.
   * Moved ccache temp dir to `/tmp/ccache_tmp` during builds to avoid permission errors.
4. **Board config (`default.px4board`):** added `CONFIG_SYSTEMCMDS_TESTS=y` so the PX4 “tests” module (and `hrt_test`) are linked for this target.

## Verification Checklist

After flashing the build (`make ... upload`), run:

```
nsh> dmesg             # dumps boot log from /dev/ramlog
nsh> top               # expect ~99% idle (safe mode)
nsh> ls /fs/microsd    # confirm SD card mount responds
nsh> hrt_test start    # verifies HRT timing & callbacks
```

Example `hrt_test` output:

```
INFO [hrt_test] Elapsed time 65970699789 in 1 sec (usleep)
INFO [hrt_test] Elapsed time 65970700160 in 1 sec (sleep)
INFO [hrt_test] HRT_CALL 1
INFO [hrt_test] HRT_CALL - 1
INFO [hrt_test] HRT_CALL + 1
goodbye
```

Typical `dmesg` output (first call):

```
find_blockdriver: ERROR: Failed to find /dev/ram0
...
[boot] SD card initialized
[boot] Parameters will be stored on /fs/microsd/params
[boot] Board initialization complete
```

The `find_blockdriver` warnings are expected: rcS probes `/dev/ram0` before any RAM disk exists. The important part is that the SD driver eventually prints “SD card initialized”; intermittent CMD1/CMD55 errors are normal on this port.

## Next Steps (outside this document)

* Reassign MAVLink to a hardware UART (keep USB for NSH).
* Validate SD writes manually before re-enabling the logger.
* Gradually lift safe-mode overrides (SYS_AUTOSTART, logging, etc.) once MAVLink and storage are stable.
