/****************************************************************************
 *
 *   Copyright (C) 2024 PX4 Development Team. All rights reserved.
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
 * @file board_mcu_version.c
 * Implementation of SAMV7 based SoC version API
 */

#include <px4_platform_common/px4_config.h>
#include <px4_platform_common/defines.h>
#include <nuttx/arch.h>
#include "hardware/sam_chipid.h"
#include "arm_internal.h"

/* SAMV7 CHIPID values from datasheet */
#define SAMV71_CIDR_ARCH_MASK	0x0FF00000
#define SAMV71_CIDR_ARCH_SAMV71	0x01100000

#define SAMV7_CIDR_EPROC_CORTEXM7	(0 << 5)

/* Extract fields from CIDR */
#define CHIPID_CIDR_VERSION(x)	((x) & 0x1F)
#define CHIPID_CIDR_EPROC(x)	(((x) >> 5) & 0x7)
#define CHIPID_CIDR_NVPSIZ(x)	(((x) >> 8) & 0xF)
#define CHIPID_CIDR_SRAMSIZ(x)	(((x) >> 16) & 0xF)
#define CHIPID_CIDR_ARCH(x)	(((x) >> 20) & 0xFF)

/* MCU revision codes */
enum MCU_REV {
	MCU_REV_SAMV71_A = 0x00,
	MCU_REV_SAMV71_B = 0x01,
	MCU_REV_SAMV71_C = 0x02,
};

int board_mcu_version(char *rev, const char **revstr, const char **errata)
{
	uint32_t cidr = getreg32(SAM_CHIPID_CIDR);
	uint32_t arch = CHIPID_CIDR_ARCH(cidr);
	uint32_t version = CHIPID_CIDR_VERSION(cidr);
	uint32_t eproc __attribute__((unused)) = CHIPID_CIDR_EPROC(cidr);

	const char *chip_errata = NULL;

	/* Identify chip family */
	if (arch == 0x11) {  /* SAMV71 family */
		*revstr = "SAMV71";

	} else if (arch == 0x10) {  /* SAME70 family */
		*revstr = "SAME70";

	} else if (arch == 0x12) {  /* SAMV70 family */
		*revstr = "SAMV70";

	} else if (arch == 0x13) {  /* SAMS70 family */
		*revstr = "SAMS70";

	} else {
		*revstr = "SAMV7x";
	}

	/* Identify revision */
	switch (version) {
	case MCU_REV_SAMV71_A:
		*rev = 'A';
		break;

	case MCU_REV_SAMV71_B:
		*rev = 'B';
		break;

	case MCU_REV_SAMV71_C:
		*rev = 'C';
		break;

	default:
		*rev = '?';
		break;
	}

	/* Check for known errata */
	if (errata) {
		*errata = chip_errata;
	}

	return 0;
}
