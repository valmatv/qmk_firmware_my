// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/*
 * IBM PC USB Converter — WeAct BlackPill STM32F401 pin assignments
 *
 * Wiring:
 *   PS/2 Clock → A0  (EXTI line, 5V-tolerant on F401)
 *   PS/2 Data  → A1  (5V-tolerant on F401)
 *   XT Reset 0 → A2
 *   XT Reset 1 → A3
 */
/* 4 KB flash backing → 2 KB logical EEPROM, enough for 5 VIA layers with 128-key matrix */
#define WEAR_LEVELING_BACKING_SIZE 4096

#define IBMPC_CLOCK_PIN   A0
#define IBMPC_DATA_PIN    A1
#define IBMPC_RST_PIN0    A2
#define IBMPC_RST_PIN1    A3
