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
 *    the distribution and/or other materials provided with the
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
 * @file timer_config.cpp
 *
 * Configuration data for the SAMV71 PWM servo driver.
 *
 * SAMV71-XULT PWM Output Configuration using TC (Timer/Counter)
 * TC0 provides PWM channels for motor outputs
 *
 * IMPORTANT: This is a STUB configuration that allows compilation but
 * PWM outputs will NOT function until proper hardware pin mappings are added.
 *
 * TODO: Map TC0 timer channels to specific GPIO pins based on actual
 * hardware connections on your SAMV71-XULT board.
 */

#include <px4_arch/io_timer_hw_description.h>
#include <px4_arch/io_timer.h>

/* STUB Timer Configuration
 * This configuration compiles but does not provide functional PWM
 * Real implementation requires:
 * 1. Determining which TC channels are wired to accessible pins
 * 2. Configuring GPIO alternate functions for TC outputs
 * 3. Setting up proper timer parameters
 */

// Empty timer configuration - allows compilation
// PWM functionality requires proper timer setup
constexpr io_timers_t io_timers[MAX_IO_TIMERS] = {
};

constexpr timer_io_channels_t timer_io_channels[MAX_TIMER_IO_CHANNELS] = {
};
