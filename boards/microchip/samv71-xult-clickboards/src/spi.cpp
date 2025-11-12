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

#include <px4_arch/spi_hw_description.h>
#include <drivers/drv_sensor.h>
#include <nuttx/spi/spi.h>

/* SPI bus configuration for SAMV71-XULT with Click sensor boards
 *
 * NOTE: Pin assignments are placeholders. The actual SPI pin assignments depend on:
 * 1. Which mikroBUS socket the sensors are plugged into
 * 2. The SAMV71-XULT schematic showing SPI routing to mikroBUS sockets
 *
 * This needs to be updated once the hardware is available and schematics are reviewed.
 *
 * SAMV71-XULT has:
 * - SPI0: Typically on PA25-PA28 (MISO, MOSI, SPCK, NPCS0-3)
 * - SPI1: Typically on PC26-PC29 (MISO, MOSI, SPCK, NPCS0-3)
 *
 * For Click boards sensor connections, we'll likely use SPI0.
 */

/* Temporarily empty SPI configuration - sensors not yet wired */
constexpr px4_spi_bus_t px4_spi_buses[SPI_BUS_MAX_BUS_ITEMS] = {
};

/* SPI chip select and status functions required by NuttX SAMV7 */
extern "C" {

void sam_spi0select(uint32_t devid, bool selected)
{
	/* No SPI devices configured yet - stub */
	(void)devid;
	(void)selected;
}

uint8_t sam_spi0status(struct spi_dev_s *dev, uint32_t devid)
{
	/* No SPI devices configured yet - stub */
	(void)dev;
	(void)devid;
	return 0;
}

void sam_spi1select(uint32_t devid, bool selected)
{
	/* No SPI devices configured yet - stub */
	(void)devid;
	(void)selected;
}

uint8_t sam_spi1status(struct spi_dev_s *dev, uint32_t devid)
{
	/* No SPI devices configured yet - stub */
	(void)dev;
	(void)devid;
	return 0;
}

} // extern "C"
