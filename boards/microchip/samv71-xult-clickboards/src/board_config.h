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

/**
 * @file board_config.h
 *
 * SAMV71-XULT with Click sensor boards internal definitions
 */

#pragma once

/****************************************************************************************************
 * Included Files
 ****************************************************************************************************/

#include <px4_platform_common/px4_config.h>
#include <nuttx/compiler.h>
#include <stdint.h>

#include <sam_gpio.h>

/****************************************************************************************************
 * Definitions
 ****************************************************************************************************/

/* GPIOs ***********************************************************************************/

/* LEDs - SAMV71-XULT has two LEDs:
 *   PA23 - Yellow LED0
 *   PC9  - Yellow LED1
 */

#define GPIO_nLED_BLUE       /* PA23 */  (GPIO_OUTPUT|GPIO_CFG_PULLUP|GPIO_OUTPUT_SET|GPIO_PORT_PIOA|GPIO_PIN23)

/* Only one LED available on SAMV71-XULT - PA23 (Blue LED)
 * Driver will use drv_board_led.h defaults:
 * LED_BLUE=0, LED_AMBER=1, LED_RED=1, LED_GREEN=3
 * Board only implements LED_BLUE (index 0)
 */

#define BOARD_HAS_CONTROL_STATUS_LEDS      1
#define BOARD_ARMED_STATE_LED  LED_BLUE

#define  FLASH_BASED_PARAMS

/* ADC Channels ***********************************************************************************/

/* ADC is not yet configured for SAMV71-XULT
 * Define placeholder channels to allow battery_status module to compile
 * These will need to be properly configured once ADC hardware mapping is done
 */
#define ADC_BATTERY_VOLTAGE_CHANNEL  0
#define ADC_BATTERY_CURRENT_CHANNEL  1
#define BOARD_NUMBER_BRICKS          1
#define BOARD_ADC_BRICK_VALID        1  /* Brick 1 valid (placeholder - all bricks considered valid) */

/* I2C Buses ***********************************************************************************/

/* SAMV71-XULT I2C Configuration:
 * I2C0 (TWIHS0): All sensors on mikroBUS sockets and Arduino headers
 *   PA3 - TWD0 (SDA)
 *   PA4 - TWCK0 (SCL)
 */

#define PX4_NUMBER_I2C_BUSES 1
#define BOARD_NUMBER_I2C_BUSES 1

/* PWM Timer Configuration ***********************************************************************************/

/* SAMV71-XULT PWM Configuration using TC (Timer/Counter):
 * TC0 - 6 PWM channels for motor outputs
 */

#define DIRECT_PWM_OUTPUT_CHANNELS  6

/* High-resolution timer */
#define HRT_TIMER               0  /* use TC0 channel 0 for the HRT */
#define HRT_TIMER_CHANNEL       0  /* use capture/compare channel 0 */

/* USB ***********************************************************************************/

/* SAMV71 has USB high-speed device */

/* This board provides a DMA pool */
#define BOARD_DMA_ALLOC_POOL_SIZE 5120

/* This board provides the board_on_reset interface */
#define BOARD_HAS_ON_RESET 1

/* Hardfault log path for crash dumps - stored in flash */
#define HARDFAULT_ULOG_PATH "/fs/microsd"
#define HARDFAULT_MAX_ULOG_FILE_LEN 80  /* Maximum length for ULog filename */

#define PX4_GPIO_INIT_LIST { \
		GPIO_nLED_BLUE,           \
	}

#define BOARD_ENABLE_CONSOLE_BUFFER

#define BOARD_NUM_IO_TIMERS 3

__BEGIN_DECLS

/****************************************************************************************************
 * Public Types
 ****************************************************************************************************/

/****************************************************************************************************
 * Public data
 ****************************************************************************************************/

#ifndef __ASSEMBLY__

/****************************************************************************************************
 * Public Functions
 ****************************************************************************************************/

extern void sam_usbinitialize(void);

extern void board_peripheral_reset(int ms);

#include <px4_platform_common/board_common.h>

#endif /* __ASSEMBLY__ */

__END_DECLS
