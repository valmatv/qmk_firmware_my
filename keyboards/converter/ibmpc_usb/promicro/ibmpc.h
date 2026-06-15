/*
Copyright 2010,2011,2012,2013,2019 Jun WAKO <wakojun@gmail.com>

This software is licensed with a Modified BSD License.
*/

#pragma once

#include <stdbool.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/atomic.h>
#include "wait.h"
#include "print.h"
#include "config.h"  /* brings in IBMPC_CLOCK_PORT/DDR/PIN/BIT etc. */

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

/* Error codes — match TMK ibmpc.hpp exactly */
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


/*--------------------------------------------------------------------
 * Platform-specific inline functions (AVR register access)
 * These are called from ibmpc.c and from the ISR.
 *------------------------------------------------------------------*/

static inline void clock_lo(void)
{
    IBMPC_CLOCK_PORT &= ~(1 << IBMPC_CLOCK_BIT);
    IBMPC_CLOCK_DDR  |=  (1 << IBMPC_CLOCK_BIT);
}

static inline void clock_hi(void)
{
    /* input with pull-up */
    IBMPC_CLOCK_DDR  &= ~(1 << IBMPC_CLOCK_BIT);
    IBMPC_CLOCK_PORT |=  (1 << IBMPC_CLOCK_BIT);
}

static inline bool clock_in(void)
{
    IBMPC_CLOCK_DDR  &= ~(1 << IBMPC_CLOCK_BIT);
    IBMPC_CLOCK_PORT |=  (1 << IBMPC_CLOCK_BIT);
    wait_us(1);
    return IBMPC_CLOCK_PIN & (1 << IBMPC_CLOCK_BIT);
}

static inline void data_lo(void)
{
    IBMPC_DATA_PORT &= ~(1 << IBMPC_DATA_BIT);
    IBMPC_DATA_DDR  |=  (1 << IBMPC_DATA_BIT);
}

static inline void data_hi(void)
{
    /* input with pull-up */
    IBMPC_DATA_DDR  &= ~(1 << IBMPC_DATA_BIT);
    IBMPC_DATA_PORT |=  (1 << IBMPC_DATA_BIT);
}

/* Called first in ISR — must be fast; no wait_us here */
static inline bool data_in(void)
{
    IBMPC_DATA_DDR  &= ~(1 << IBMPC_DATA_BIT);
    IBMPC_DATA_PORT |=  (1 << IBMPC_DATA_BIT);
    return IBMPC_DATA_PIN & (1 << IBMPC_DATA_BIT);
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
    IBMPC_RST_PORT &= ~(1 << IBMPC_RST_BIT0); \
    IBMPC_RST_DDR  &= ~(1 << IBMPC_RST_BIT0); \
    IBMPC_RST_PORT &= ~(1 << IBMPC_RST_BIT1); \
    IBMPC_RST_DDR  &= ~(1 << IBMPC_RST_BIT1); \
} while (0)

#define IBMPC_RST_LO() do { \
    IBMPC_RST_PORT &= ~(1 << IBMPC_RST_BIT0); \
    IBMPC_RST_DDR  |=  (1 << IBMPC_RST_BIT0); \
    IBMPC_RST_PORT &= ~(1 << IBMPC_RST_BIT1); \
    IBMPC_RST_DDR  |=  (1 << IBMPC_RST_BIT1); \
} while (0)

/* External interrupt on falling edge of clock (INT1 = PD1) */
#define IBMPC_INT_INIT() do { \
    EICRA |= ((1 << ISC11) | (0 << ISC10)); \
} while (0)

#define IBMPC_INT_ON() do { \
    EIFR  |= (1 << INTF1); \
    EIMSK |= (1 << INT1);  \
} while (0)

#define IBMPC_INT_OFF() do { \
    EIMSK &= ~(1 << INT1); \
} while (0)

#define IBMPC_INT_VECT  INT1_vect
