// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/*
 * IBM PC USB Converter — Pro Micro (ATmega32U4) pin assignments
 *
 * Wiring:
 *   PS/2 Clock → D1  (INT1 on ATmega32U4)
 *   PS/2 Data  → D0
 *   XT Reset 0 → B6
 *   XT Reset 1 → B7
 */

/* Clock line: must be an External Interrupt pin (INT1 = PD1) */
#define IBMPC_CLOCK_PORT  PORTD
#define IBMPC_CLOCK_PIN   PIND   /* AVR PIN input register, not QMK pin id */
#define IBMPC_CLOCK_DDR   DDRD
#define IBMPC_CLOCK_BIT   1

/* Data line */
#define IBMPC_DATA_PORT   PORTD
#define IBMPC_DATA_PIN    PIND
#define IBMPC_DATA_DDR    DDRD
#define IBMPC_DATA_BIT    0

/* XT keyboard hard-reset lines */
#define IBMPC_RST_PORT    PORTB
#define IBMPC_RST_PINREG  PINB
#define IBMPC_RST_DDR     DDRB
#define IBMPC_RST_BIT0    6
#define IBMPC_RST_BIT1    7
