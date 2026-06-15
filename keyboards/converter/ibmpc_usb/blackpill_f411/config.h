// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/*
 * IBM PC USB Converter — WeAct BlackPill STM32F411 pin assignments
 *
 * Wiring:
 *   PS/2 Clock → A0  (EXTI line, 5V-tolerant on F411)
 *   PS/2 Data  → A1  (5V-tolerant on F411)
 *   XT Reset 0 → A2
 *   XT Reset 1 → A3
 */
#define IBMPC_CLOCK_PIN   A0
#define IBMPC_DATA_PIN    A1
#define IBMPC_RST_PIN0    A2
#define IBMPC_RST_PIN1    A3
