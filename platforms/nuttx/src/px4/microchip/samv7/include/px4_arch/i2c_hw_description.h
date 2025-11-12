/****************************************************************************
 *
 *   Copyright (c) 2024 PX4 Development Team. All rights reserved.
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

#pragma once

#include <px4_platform_common/i2c.h>
#include <sam_gpio.h>

/* SAMV7 I2C Hardware Description
 * SAMV7 has up to 3 TWIHS (Two-Wire Interface High Speed) peripherals
 * TWIHS0 - I2C0
 * TWIHS1 - I2C1
 * TWIHS2 - I2C2
 */

/* I2C bus timing configuration - derived from MCK */
#define I2C_SPEED_STANDARD  100000  /* 100 kHz */
#define I2C_SPEED_FAST      400000  /* 400 kHz */

#if defined(CONFIG_I2C)

static constexpr px4_i2c_bus_t initI2CBusInternal(int bus)
{
	px4_i2c_bus_t ret{};
	ret.bus = bus;
	ret.is_external = false;
	return ret;
}

static constexpr px4_i2c_bus_t initI2CBusExternal(int bus)
{
	px4_i2c_bus_t ret{};
	ret.bus = bus;
	ret.is_external = true;
	return ret;
}
#endif // CONFIG_I2C

