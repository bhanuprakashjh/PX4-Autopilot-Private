/****************************************************************************
 *
 *   Copyright (c) 2025 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/**
 * @file littlefs_mount.c
 * Mount and optionally format LittleFS filesystems
 *
 * @author PX4 Development Team
 */

#include <px4_platform_common/px4_config.h>
#include <px4_platform_common/module.h>
#include <px4_platform_common/getopt.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

__EXPORT int littlefs_mount_main(int argc, char *argv[]);

static void usage(const char *reason)
{
	if (reason) {
		PX4_ERR("%s", reason);
	}

	PRINT_MODULE_DESCRIPTION(
		R"DESCR_STR(
### Description
Mount a LittleFS filesystem, optionally formatting it first.

### Examples
Format and mount /dev/mtdparams:
$ littlefs_mount -d /dev/mtdparams -m /fs/mtd_params -f

Mount existing filesystem:
$ littlefs_mount -d /dev/mtdparams -m /fs/mtd_params
)DESCR_STR");

	PRINT_MODULE_USAGE_NAME("littlefs_mount", "command");
	PRINT_MODULE_USAGE_PARAM_STRING('d', NULL, "<device>", "MTD device path", false);
	PRINT_MODULE_USAGE_PARAM_STRING('m', NULL, "<path>", "Mount point path", false);
	PRINT_MODULE_USAGE_PARAM_FLAG('f', "Format before mounting", true);
}

int littlefs_mount_main(int argc, char *argv[])
{
	const char *device = NULL;
	const char *mountpoint = NULL;
	bool format = false;

	int myoptind = 1;
	int ch;
	const char *myoptarg = NULL;

	while ((ch = px4_getopt(argc, argv, "d:m:f", &myoptind, &myoptarg)) != EOF) {
		switch (ch) {
		case 'd':
			device = myoptarg;
			break;

		case 'm':
			mountpoint = myoptarg;
			break;

		case 'f':
			format = true;
			break;

		default:
			usage("unrecognized option");
			return 1;
		}
	}

	if (!device || !mountpoint) {
		usage("missing required arguments");
		return 1;
	}

	/* Create mount point if it doesn't exist */
	struct stat st;
	if (stat(mountpoint, &st) != 0) {
		/* Create parent directory first */
		char parent[256];
		strncpy(parent, mountpoint, sizeof(parent) - 1);
		parent[sizeof(parent) - 1] = '\0';

		char *last_slash = strrchr(parent, '/');
		if (last_slash && last_slash != parent) {
			*last_slash = '\0';
			mkdir(parent, 0777);
		}

		if (mkdir(mountpoint, 0777) != 0 && errno != EEXIST) {
			PX4_ERR("Failed to create mount point %s: %s", mountpoint, strerror(errno));
			return 1;
		}
	}

	int ret;

	if (format) {
		PX4_INFO("Formatting and mounting %s at %s", device, mountpoint);
		ret = nx_mount(device, mountpoint, "littlefs", 0, "forceformat");
	} else {
		PX4_INFO("Mounting %s at %s", device, mountpoint);
		ret = nx_mount(device, mountpoint, "littlefs", 0, NULL);
	}

	if (ret < 0) {
		PX4_ERR("Mount failed: %s (ret=%d)", strerror(-ret), ret);

		if (!format && ret == -EFAULT) {
			PX4_WARN("Filesystem appears corrupt or unformatted. Try with -f to format");
		}

		return 1;
	}

	PX4_INFO("Successfully mounted %s", device);
	return 0;
}
