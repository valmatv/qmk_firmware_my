/*
Copyright 2010,2011,2012,2013,2019 Jun WAKO <wakojun@gmail.com>

This software is licensed with a Modified BSD License.
All of this is supposed to be Free Software, Open Source, DFSG-free,
GPL-compatible, and OK to use in both free and proprietary applications.
Additions and corrections to this file are welcome.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in
  the documentation and/or other materials provided with the
  distribution.

* Neither the name of the copyright holders nor the names of
  contributors may be used to endorse or promote products derived
  from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include <stdbool.h>
#include "wait.h"
#include "print.h"
#include "ch.h"
#include "hal.h"
#include "gpio.h"

#define IBMPC_ACK         0xFA
#define IBMPC_RESEND      0xFE
#define IBMPC_SET_LED     0xED

#define IBMPC_PROTOCOL_NO       0
#define IBMPC_PROTOCOL_AT       0x10
#define IBMPC_PROTOCOL_AT_Z150  0x11
#define IBMPC_PROTOCOL_XT       0x20
#define IBMPC_PROTOCOL_XT_IBM   0x21
#define IBMPC_PROTOCOL_XT_CLONE 0x22
#define IBMPC_PROTOCOL_XT_ERROR 0x23

/* Error codes — match TMK ibmpc.hpp and promicro/ibmpc.h exactly */
#define IBMPC_ERR_NONE        0
#define IBMPC_ERR_PARITY      0x01
#define IBMPC_ERR_PARITY_AA   0x02
#define IBMPC_ERR_SEND        0x10
#define IBMPC_ERR_TIMEOUT     0x20
#define IBMPC_ERR_FULL        0x40
#define IBMPC_ERR_ILLEGAL     0x80

#define IBMPC_LED_SCROLL_LOCK 0
#define IBMPC_LED_NUM_LOCK    1
#define IBMPC_LED_CAPS_LOCK   2

extern volatile uint16_t ibmpc_isr_debug;
extern volatile uint8_t  ibmpc_protocol;
extern volatile uint8_t  ibmpc_error;

void    ibmpc_host_init(void);
void    ibmpc_host_enable(void);
void    ibmpc_host_disable(void);
int16_t ibmpc_host_send(uint8_t data);
int16_t ibmpc_host_recv_response(void);
int16_t ibmpc_host_recv(void);
void    ibmpc_host_isr_clear(void);
void    ibmpc_host_set_led(uint8_t usb_led);
void    ibmpc_interrupt_service_routine(void);
void    palCallback(void *arg);


/*--------------------------------------------------------------------
 * Platform-specific inline functions (ChibiOS PAL)
 * PS/2 is open-collector; use OUTPUT_OPENDRAIN so driving HIGH releases
 * the line (wired-AND with the device's open-collector output).
 *------------------------------------------------------------------*/

static inline void clock_lo(void)
{
    palSetLineMode(IBMPC_CLOCK_PIN, PAL_MODE_OUTPUT_OPENDRAIN);
    palWriteLine(IBMPC_CLOCK_PIN, PAL_LOW);
}

static inline void clock_hi(void)
{
    palSetLineMode(IBMPC_CLOCK_PIN, PAL_MODE_OUTPUT_OPENDRAIN);
    palWriteLine(IBMPC_CLOCK_PIN, PAL_HIGH);
}

static inline bool clock_in(void)
{
    palSetLineMode(IBMPC_CLOCK_PIN, PAL_MODE_INPUT);
    wait_us(1);
    return palReadLine(IBMPC_CLOCK_PIN);
}

static inline void data_lo(void)
{
    palSetLineMode(IBMPC_DATA_PIN, PAL_MODE_OUTPUT_OPENDRAIN);
    palWriteLine(IBMPC_DATA_PIN, PAL_LOW);
}

static inline void data_hi(void)
{
    palSetLineMode(IBMPC_DATA_PIN, PAL_MODE_OUTPUT_OPENDRAIN);
    palWriteLine(IBMPC_DATA_PIN, PAL_HIGH);
}

static inline bool data_in(void)
{
    palSetLineMode(IBMPC_DATA_PIN, PAL_MODE_INPUT);
    wait_us(1);
    return palReadLine(IBMPC_DATA_PIN);
}

static inline uint16_t wait_clock_lo(uint16_t us)
{
    while ( clock_in() && us) { asm(""); wait_us(1); us--; }
    return us;
}
static inline uint16_t wait_clock_hi(uint16_t us)
{
    while (!clock_in() && us) { asm(""); wait_us(1); us--; }
    return us;
}
static inline uint16_t wait_data_lo(uint16_t us)
{
    while ( data_in() && us) { asm(""); wait_us(1); us--; }
    return us;
}
static inline uint16_t wait_data_hi(uint16_t us)
{
    while (!data_in() && us) { asm(""); wait_us(1); us--; }
    return us;
}

/* idle: release clock and data so the device can send */
static inline void idle(void)
{
    clock_hi();
    data_hi();
}

/* inhibit: pull clock low — pauses AT device; soft-resets XT */
static inline void inhibit(void)
{
    clock_lo();
    data_hi();
}

/* XT inhibit-data: pull data low while clock is high */
static inline void inhibit_xt(void)
{
    clock_hi();
    data_lo();
}

/* XT hard-reset: drive RST pins low for 500 ms */
#define IBMPC_RST_HIZ() do { \
    gpio_write_pin_low(IBMPC_RST_PIN0); \
    gpio_set_pin_input(IBMPC_RST_PIN0); \
    gpio_write_pin_low(IBMPC_RST_PIN1); \
    gpio_set_pin_input(IBMPC_RST_PIN1); \
} while (0)

#define IBMPC_RST_LO() do { \
    gpio_write_pin_low(IBMPC_RST_PIN0); \
    gpio_set_pin_output(IBMPC_RST_PIN0); \
    gpio_write_pin_low(IBMPC_RST_PIN1); \
    gpio_set_pin_output(IBMPC_RST_PIN1); \
} while (0)

/* PAL interrupt: falling edge on clock line triggers palCallback */
#define IBMPC_INT_INIT() do { \
    palSetLineMode(IBMPC_CLOCK_PIN, PAL_MODE_INPUT); \
} while (0)

#define IBMPC_INT_ON() do { \
    palEnableLineEvent(IBMPC_CLOCK_PIN, PAL_EVENT_MODE_FALLING_EDGE); \
    palSetLineCallback(IBMPC_CLOCK_PIN, palCallback, NULL); \
} while (0)

#define IBMPC_INT_OFF() do { \
    palDisableLineEvent(IBMPC_CLOCK_PIN); \
} while (0)
