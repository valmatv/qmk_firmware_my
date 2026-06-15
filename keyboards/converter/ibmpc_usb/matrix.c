/*
Copyright 2011 Jun Wako <wakojun@gmail.com>,
2021 Markus Fritsche <fritsche.markus@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdint.h>
#include <stdbool.h>
#include "print.h"
#include "util.h"
#include "debug.h"
#include "timer.h"
#include "led.h"
#include "ibmpc_usb.h"
#include "ibmpc.h"
#include "matrix.h"

#define ROW(code)  ((code >> 4) & 0x07)
#define COL(code)  (code & 0x0F)

static matrix_row_t matrix[MATRIX_ROWS];

static int8_t process_cs1(uint8_t code);
static int8_t process_cs2(uint8_t code);
static int8_t process_cs3(uint8_t code);

/* Wait up to wait_ms for next byte from keyboard. */
static int16_t read_wait(uint16_t wait_ms)
{
    uint16_t start = timer_read();
    int16_t code;
    while ((code = ibmpc_host_recv()) == -1 && timer_elapsed(start) < wait_ms);
    return code;
}

#define ID_STR(id)  (id == 0xFFFE ? "_????" : \
                    (id == 0xFFFD ? "_Z150" : \
                    (id == 0x0000 ? "_AT84" : \
                     "")))

static uint16_t read_keyboard_id(void)
{
    uint16_t id = 0;
    int16_t  code;

    /* Zenith Z-150 AT: already identified by protocol flag, skip F2 */
    if (ibmpc_protocol == IBMPC_PROTOCOL_AT_Z150) return 0xFFFD;

    /* Send Read ID command */
    code = ibmpc_host_send(0xF2);
    if (code == -1) { id = 0xFFFF; goto DONE; }     /* XT or no keyboard */
    if (code != 0xFA) { id = 0xFFFE; goto DONE; }   /* broken PS/2 */

    /* First ID byte — up to 500 ms (TechRef [8] 4-41) */
    code = read_wait(500);
    if (code == -1) { id = 0x0000; goto DONE; }     /* AT 84-key */
    id = (uint16_t)(code & 0xFF) << 8;

    /* Second ID byte — mouse responds with one byte 0x00 → id = 0x00FF */
    code = read_wait(500);
    id |= code & 0xFF;

DONE:
    return id;
}

__attribute__((weak))
void matrix_init_user(void) {}

void matrix_clear(void)
{
    for (uint8_t i = 0; i < MATRIX_ROWS; i++) matrix[i] = 0x00;
}

void matrix_init(void)
{
    debug_enable = true;

    for (uint8_t i = 0; i < MATRIX_ROWS; i++) matrix[i] = 0x00;

    ibmpc_host_init();
    ibmpc_host_enable();
}

uint8_t         current_protocol = 0;
uint16_t        keyboard_id      = 0x0000;
keyboard_kind_t keyboard_kind    = NONE;

uint8_t matrix_scan(void)
{
    static enum {
        INIT,
        WAIT_SETTLE,
        AT_RESET,
        XT_RESET,
        XT_RESET_WAIT,
        XT_RESET_DONE,
        WAIT_AA,
        WAIT_AABF,
        WAIT_AABFBF,
        READ_ID,
        SETUP,
        LOOP,
        ERROR,
    } state = INIT;
    static uint16_t init_time;

    /* ----------------------------------------------------------------
     * Error handling — runs every scan cycle
     * ---------------------------------------------------------------- */
    if (ibmpc_error) {
        xprintf("\n%u ERR:%02X ISR:%04X ", timer_read(), ibmpc_error, ibmpc_isr_debug);

        /* Recv error (not send/full) in LOOP → reinitialize */
        if (!(ibmpc_error & (IBMPC_ERR_SEND | IBMPC_ERR_FULL))) {
            if (state == LOOP) {
                xprintf("[RST] ");
                state = ERROR;
            }
        }

        ibmpc_error     = IBMPC_ERR_NONE;
        ibmpc_isr_debug = 0;
    }

    /* ----------------------------------------------------------------
     * Protocol change detection
     * ---------------------------------------------------------------- */
    if (ibmpc_protocol && ibmpc_protocol != current_protocol) {
        xprintf("\n%u PRT:%02X ISR:%04X ", timer_read(), ibmpc_protocol, ibmpc_isr_debug);

        /* AT↔XT protocol flip is unexpected; reinitialize from LOOP */
        if (((current_protocol & IBMPC_PROTOCOL_XT) && (ibmpc_protocol & IBMPC_PROTOCOL_AT)) ||
            ((current_protocol & IBMPC_PROTOCOL_AT) && (ibmpc_protocol & IBMPC_PROTOCOL_XT))) {
            xprintf("\nERR:%02X ISR:%04X ", ibmpc_error, ibmpc_isr_debug);
            if (state == LOOP) {
                xprintf("[CHG] ");
                state = ERROR;
            }
        }

        current_protocol = ibmpc_protocol;
        ibmpc_isr_debug  = 0;
    }

    /* ----------------------------------------------------------------
     * State machine
     * ---------------------------------------------------------------- */
    switch (state) {
        case INIT:
            xprintf("I%u ", timer_read());
            keyboard_kind    = NONE;
            keyboard_id      = 0x0000;
            current_protocol = 0;

            matrix_clear();

            init_time = timer_read();
            state = WAIT_SETTLE;
            ibmpc_host_enable();
            break;

        case WAIT_SETTLE:
            /* Drain any spurious bytes while waiting for the keyboard to settle */
            while (ibmpc_host_recv() != -1) ;

            if (timer_elapsed(init_time) > 3000) {
                state = AT_RESET;
            }
            break;

        case AT_RESET:
            xprintf("A%u ", timer_read());

            if (0xFA == ibmpc_host_send(0xFF)) {
                state = WAIT_AA;
            } else {
                state = XT_RESET;
            }
            break;

        case XT_RESET:
            /* XT: hold Clock low (soft reset) and pull RST pins low (hard reset) */
            ibmpc_host_disable();
            IBMPC_RST_LO();

            init_time = timer_read();
            state = XT_RESET_WAIT;
            break;

        case XT_RESET_WAIT:
            if (timer_elapsed(init_time) > 500) {
                state = XT_RESET_DONE;
            }
            break;

        case XT_RESET_DONE:
            IBMPC_RST_HIZ();
            ibmpc_host_isr_clear();
            ibmpc_host_enable();

            xprintf("X%u ", timer_read());
            init_time = timer_read();
            state = WAIT_AA;
            break;

        case WAIT_AA:
            /*
             * Wait for BAT completion code (0xAA).
             * AT 84-key: POR+BAT 900–9900 ms (TechRef [8] 4-7)
             * AT 101/102: POR+BAT 450–2500 ms (TechRef [8] 4-39)
             * Terminal keyboards send AA BF BF before ID.
             */
            if (ibmpc_host_recv() != -1) {
                xprintf("W%u ", timer_read());
                init_time = timer_read();
                state = WAIT_AABF;
            }
            break;

        case WAIT_AABF:
            if (timer_elapsed(init_time) > 500) {
                state = READ_ID;
                break;
            }
            if (ibmpc_host_recv() != -1) {
                xprintf("W%u ", timer_read());
                init_time = timer_read();
                state = WAIT_AABFBF;
            }
            break;

        case WAIT_AABFBF:
            if (timer_elapsed(init_time) > 500) {
                state = READ_ID;
                break;
            }
            if (ibmpc_host_recv() != -1) {
                xprintf("W%u ", timer_read());
                state = READ_ID;
            }
            break;

        case READ_ID:
            keyboard_id = read_keyboard_id();
            xprintf("R%u ", timer_read());

            if (0x0000 == keyboard_id) {            /* Code Set 2 AT 84-key */
                keyboard_kind = PC_AT;
            } else if (0xFFFF == keyboard_id) {     /* Code Set 1 XT (no F2 response) */
                keyboard_kind = PC_XT;
            } else if (0xFFFE == keyboard_id) {     /* Broken PS/2 */
                keyboard_kind = PC_AT;
            } else if (0xFFFD == keyboard_id) {     /* Zenith Z-150 AT */
                keyboard_kind = PC_AT;
            } else if (0xAB85 == keyboard_id ||     /* IBM 122-key Model M, NCD N-97 */
                       0xAB86 == keyboard_id ||     /* Cherry G80-2551, IBM 1397000 */
                       0xAB92 == keyboard_id) {     /* IBM 5576-001 */
                if ((0xFA == ibmpc_host_send(0xF0)) &&
                    (0xFA == ibmpc_host_send(0x03))) {
                    keyboard_kind = PC_TERMINAL;
                } else {
                    keyboard_kind = PC_AT;
                }
            } else if (0xAB90 == keyboard_id ||     /* IBM 5576-002 */
                       0xAB91 == keyboard_id) {     /* IBM 5576-003 / Televideo DEC */
                xprintf("\n5576_CS82h:");
                keyboard_kind = PC_AT;
                if ((0xFA == ibmpc_host_send(0xF0)) &&
                    (0xFA == ibmpc_host_send(0x82))) {
                    xprintf("OK ");
                } else {
                    xprintf("NG ");
                    if (0xAB91 == keyboard_id) {
                        /* May be Televideo DEC — try Code Set 3 */
                        if ((0xFA == ibmpc_host_send(0xF0)) &&
                            (0xFA == ibmpc_host_send(0x03))) {
                            xprintf("OK ");
                            keyboard_kind = PC_TERMINAL;
                        } else {
                            xprintf("NG ");
                        }
                    }
                }
            } else if (0xBFB0 == keyboard_id) {     /* IBM RT Keyboard */
                keyboard_kind = PC_TERMINAL;
            } else if (0xAB00 == (keyboard_id & 0xFF00)) {  /* Code Set 2 PS/2 */
                keyboard_kind = PC_AT;
            } else if (0xBF00 == (keyboard_id & 0xFF00)) {  /* Code Set 3 Terminal */
                keyboard_kind = PC_TERMINAL;
            } else if (0x7F00 == (keyboard_id & 0xFF00)) {  /* Code Set 3 Terminal (1394204) */
                keyboard_kind = PC_TERMINAL;
            } else {
                xprintf("\nUnknown ID: Report to TMK ");
                if ((0xFA == ibmpc_host_send(0xF0)) &&
                    (0xFA == ibmpc_host_send(0x02))) {
                    keyboard_kind = PC_AT;
                } else if ((0xFA == ibmpc_host_send(0xF0)) &&
                           (0xFA == ibmpc_host_send(0x03))) {
                    keyboard_kind = PC_TERMINAL;
                } else {
                    keyboard_kind = PC_AT;
                }
            }

            xprintf("\nID:%04X(%s%s) ", keyboard_id,
                    KEYBOARD_KIND_STR(keyboard_kind), ID_STR(keyboard_id));

            state = SETUP;
            break;

        case SETUP:
            xprintf("S%u ", timer_read());
            switch (keyboard_kind) {
                case PC_XT:
                    break;
                case PC_AT:
                    led_set(host_keyboard_leds());
                    break;
                case PC_TERMINAL:
                    ibmpc_host_send(0xF8);   /* all keys: make/break */
                    led_set(host_keyboard_leds());
                    break;
                default:
                    break;
            }
            state = LOOP;
            xprintf("L%u ", timer_read());
            /* fall through */

        case LOOP: {
            int16_t code = ibmpc_host_recv();
            if (code == -1) break;

            /* Error/overrun codes: CS1 = 0xFF, CS2/CS3 = 0x00, buffer full = 0xFF */
            if (code == 0x00 || code == 0xFF) {
                matrix_clear();
                clear_keyboard();
                xprintf("\n[CLR] ");
                break;
            }

            switch (keyboard_kind) {
                case PC_XT:
                    if (process_cs1((uint8_t)code) == -1) state = ERROR;
                    break;
                case PC_AT:
                    if (process_cs2((uint8_t)code) == -1) state = ERROR;
                    break;
                case PC_TERMINAL:
                    if (process_cs3((uint8_t)code) == -1) state = ERROR;
                    break;
                default:
                    break;
            }
            break;
        }

        case ERROR:
            clear_keyboard();
            state = INIT;
            break;

        default:
            break;
    }
    return 1;
}

matrix_row_t matrix_get_row(uint8_t row)
{
    return matrix[row];
}

void matrix_print(void)
{
    print("r/c 0123456789ABCDEF\n");
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        xprintf("%02X: %016b\n", row, bitrev16(matrix_get_row(row)));
    }
}

bool matrix_is_on(uint8_t row, uint8_t col)
{
    return (matrix[row] & (1 << col));
}

static uint8_t to_unimap(uint8_t code)
{
    uint8_t row = ROW(code);
    uint8_t col = COL(code);
    switch (keyboard_kind) {
        case PC_XT:       return pgm_read_byte(&unimap_cs1[row][col]);
        case PC_AT:       return pgm_read_byte(&unimap_cs2[row][col]);
        case PC_TERMINAL: return pgm_read_byte(&unimap_cs3[row][col]);
        default:          return UNIMAP_NO;
    }
}

static void matrix_make(uint8_t code)
{
    uint8_t newcode = to_unimap(code);
    if (!matrix_is_on(ROW(newcode), COL(newcode))) {
        matrix[ROW(newcode)] |= 1 << COL(newcode);
    }
}

static void matrix_break(uint8_t code)
{
    uint8_t newcode = to_unimap(code);
    if (matrix_is_on(ROW(newcode), COL(newcode))) {
        matrix[ROW(newcode)] &= ~(1 << COL(newcode));
    }
}

void led_set(uint8_t usb_led)
{
    if (keyboard_kind == NONE) return;
    if (keyboard_kind == PC_XT) return;

    led_t led = {.raw = usb_led};
    uint8_t ibmpc_led = 0;
    if (led.scroll_lock) ibmpc_led |= (1 << IBMPC_LED_SCROLL_LOCK);
    if (led.num_lock)    ibmpc_led |= (1 << IBMPC_LED_NUM_LOCK);
    if (led.caps_lock)   ibmpc_led |= (1 << IBMPC_LED_CAPS_LOCK);
    ibmpc_host_set_led(ibmpc_led);
}


/*******************************************************************************
 * XT: Scan Code Set 1
 *
 * E0-escaped codes are translated into the unused range 0x54–0x7F of the matrix.
 ******************************************************************************/
static uint8_t cs1_e0code(uint8_t code)
{
    switch (code) {
        case 0x37: return 0x54; /* Print Screen */
        case 0x46: return 0x55; /* Ctrl+Pause */
        case 0x5B: return 0x5A; /* Left GUI */
        case 0x5C: return 0x5B; /* Right GUI */
        case 0x5D: return 0x5C; /* Application */
        case 0x20: return 0x5D; /* Mute */
        case 0x2E: return 0x5E; /* Volume Down */
        case 0x30: return 0x5F; /* Volume Up */
        case 0x48: return 0x60; /* Up */
        case 0x4B: return 0x61; /* Left */
        case 0x50: return 0x62; /* Down */
        case 0x4D: return 0x63; /* Right */
        case 0x1C: return 0x6F; /* Keypad Enter */
        case 0x52: return 0x71; /* Insert */
        case 0x53: return 0x72; /* Delete */
        case 0x47: return 0x74; /* Home */
        case 0x4F: return 0x75; /* End */
        case 0x49: return 0x77; /* Page Up */
        case 0x51: return 0x78; /* Page Down */
        case 0x1D: return 0x7A; /* Right Ctrl */
        case 0x38: return 0x7C; /* Right Alt */
        case 0x35: return 0x7F; /* Keypad / */
        /* Shared cells */
        case 0x5E: return 0x70; /* Power (KANA) */
        case 0x5F: return 0x79; /* Sleep (HENKAN) */
        case 0x63: return 0x7B; /* Wake (MUHENKAN) */
        default:
            xprintf("!CS1_E0_%02X!\n", code);
            return code;
    }
}

static int8_t process_cs1(uint8_t code)
{
    static enum { INIT, cE0, cE1, E1_1D, E1_9D } state = INIT;

    switch (state) {
        case INIT:
            switch (code) {
                case 0xE0: state = cE0; break;
                case 0xE1: state = cE1; break;
                default:
                    if (code < 0x80) matrix_make(code);
                    else             matrix_break(code & 0x7F);
                    break;
            }
            break;

        case cE0:
            switch (code) {
                case 0x2A: case 0xAA:
                case 0x36: case 0xB6:
                    /* ignore fake shift */
                    state = INIT;
                    break;
                default:
                    if (code < 0x80) matrix_make(cs1_e0code(code));
                    else             matrix_break(cs1_e0code(code & 0x7F));
                    state = INIT;
                    break;
            }
            break;

        case cE1:
            switch (code) {
                case 0x1D: state = E1_1D; break;
                case 0x9D: state = E1_9D; break;
                default:   state = INIT;  break;
            }
            break;

        case E1_1D:
            if (code == 0x45) matrix_make(0x55); /* Pause make */
            state = INIT;
            break;

        case E1_9D:
            if (code == 0xC5) matrix_break(0x55); /* Pause break */
            state = INIT;
            break;

        default:
            state = INIT;
            break;
    }
    return 0;
}


/*******************************************************************************
 * AT/PS2: Scan Code Set 2
 ******************************************************************************/
static uint8_t cs2_e0code(uint8_t code)
{
    switch (code) {
        case 0x11: if (0xAB90 == keyboard_id || 0xAB91 == keyboard_id)
                       return 0x13; /* Hiragana (5576) → KANA */
                   else
                       return 0x0F; /* Right Alt */
        case 0x41: if (0xAB90 == keyboard_id || 0xAB91 == keyboard_id)
                       return 0x7C; /* Keypad , (5576) → Keypad * */
                   else
                       return (code & 0x7F);
        case 0x14: return 0x19; /* Right Ctrl */
        case 0x1F: return 0x17; /* Left GUI */
        case 0x27: return 0x1F; /* Right GUI */
        case 0x2F: return 0x27; /* Apps */
        case 0x4A: return 0x60; /* Keypad / */
        case 0x5A: return 0x62; /* Keypad Enter */
        case 0x69: return 0x5C; /* End */
        case 0x6B: return 0x53; /* Cursor Left */
        case 0x6C: return 0x2F; /* Home */
        case 0x70: return 0x39; /* Insert */
        case 0x71: return 0x37; /* Delete */
        case 0x72: return 0x3F; /* Cursor Down */
        case 0x74: return 0x47; /* Cursor Right */
        case 0x75: return 0x4F; /* Cursor Up */
        case 0x77: return 0x00; /* Unicomp New Model M Pause/Break fix */
        case 0x7A: return 0x56; /* Page Down */
        case 0x7D: return 0x5E; /* Page Up */
        case 0x7C: return 0x7F; /* Print Screen */
        case 0x7E: return 0x00; /* Ctrl Pause */
        case 0x21: return 0x65; /* Volume Down */
        case 0x32: return 0x6E; /* Volume Up */
        case 0x23: return 0x6F; /* Mute */
        case 0x10: return 0x08; /* WWW search → F13 */
        case 0x18: return 0x10; /* WWW favourites → F14 */
        case 0x20: return 0x18; /* WWW refresh → F15 */
        case 0x28: return 0x20; /* WWW stop → F16 */
        case 0x30: return 0x28; /* WWW forward → F17 */
        case 0x38: return 0x30; /* WWW back → F18 */
        case 0x3A: return 0x38; /* WWW home → F19 */
        case 0x40: return 0x40; /* My Computer → F20 */
        case 0x48: return 0x48; /* Email → F21 */
        case 0x2B: return 0x50; /* Calculator → F22 */
        case 0x34: return 0x08; /* Play/Pause → F13 */
        case 0x3B: return 0x10; /* Stop → F14 */
        case 0x15: return 0x18; /* Previous Track → F15 */
        case 0x4D: return 0x20; /* Next Track → F16 */
        case 0x50: return 0x28; /* Media Select → F17 */
        case 0x5E: return 0x50; /* ACPI Wake → F22 */
        case 0x3F: return 0x57; /* ACPI Sleep → F23 */
        case 0x37: return 0x5F; /* ACPI Power → F24 */
        /* DEC LK411 */
        case 0x03: return 0x18; /* Help → F15 */
        case 0x04: return 0x08; /* F13 */
        case 0x0B: return 0x20; /* Do → F16 */
        case 0x0C: return 0x10; /* F14 */
        case 0x0D: return 0x19; /* LCompose → LGUI */
        case 0x79: return 0x6D; /* KP- → PCMM */
        case 0x83: return 0x28; /* F17 */
        default:   return (code & 0x7F);
    }
}

static uint8_t translate_5576_cs2(uint8_t code)
{
    switch (code) {
        case 0x11: return 0x0F; /* Zenmen → RALT */
        case 0x13: return 0x11; /* Kanji → LALT */
        case 0x0E: return 0x54; /* @ */
        case 0x54: return 0x5B; /* [ */
        case 0x5B: return 0x5D; /* ] */
        case 0x5C: return 0x6A; /* JYEN */
        case 0x5D: return 0x6A; /* JYEN */
        case 0x62: return 0x0E; /* Han/Zen → `~ */
        case 0x7C: return 0x77; /* Keypad * */
    }
    return code;
}

static uint8_t translate_5576_cs2_e0(uint8_t code)
{
    switch (code) {
        case 0x11: return 0x13; /* Hiragana → KANA */
        case 0x41: return 0x7C; /* Keypad ' */
    }
    return code;
}

static int8_t process_cs2(uint8_t code)
{
    static enum {
        INIT,
        cF0,
        cE0,
        E0_F0,
        cE1,
        E1_14,
        E1_F0,
        E1_F0_14,
        E1_F0_14_F0,
    } state = INIT;

    switch (state) {
        case INIT:
            if (0xAB90 == keyboard_id || 0xAB91 == keyboard_id)
                code = translate_5576_cs2(code);
            switch (code) {
                case 0xE0: state = cE0; break;
                case 0xF0: state = cF0; break;
                case 0xE1: state = cE1; break;
                case 0x83: matrix_make(0x02); state = INIT; break; /* F7 */
                case 0x84: matrix_make(0x7F); state = INIT; break; /* Alt PrintScreen */
                case 0xAA: case 0xFC:                               /* self-test */
                default:
                    state = INIT;
                    if (code < 0x80) {
                        matrix_make(code);
                    } else {
                        matrix_clear();
                        xprintf("!CS2_INIT!\n");
                        return -1;
                    }
                    break;
            }
            break;

        case cE0:
            if (0xAB90 == keyboard_id || 0xAB91 == keyboard_id)
                code = translate_5576_cs2_e0(code);
            switch (code) {
                case 0x12: case 0x59: /* ignore fake shift */
                    state = INIT; break;
                case 0xF0: state = E0_F0; break;
                default:
                    state = INIT;
                    if (code < 0x80) {
                        matrix_make(cs2_e0code(code));
                    } else {
                        matrix_clear();
                        xprintf("!CS2_E0!\n");
                        return -1;
                    }
                    break;
            }
            break;

        case cF0:
            if (0xAB90 == keyboard_id || 0xAB91 == keyboard_id)
                code = translate_5576_cs2(code);
            switch (code) {
                case 0x83: matrix_break(0x02); state = INIT; break; /* F7 */
                case 0x84: matrix_break(0x7F); state = INIT; break; /* Alt PrintScreen */
                default:
                    state = INIT;
                    if (code < 0x80) {
                        matrix_break(code);
                    } else {
                        matrix_clear();
                        xprintf("!CS2_F0!\n");
                        return -1;
                    }
                    break;
            }
            break;

        case E0_F0:
            if (0xAB90 == keyboard_id || 0xAB91 == keyboard_id)
                code = translate_5576_cs2_e0(code);
            switch (code) {
                case 0x12: case 0x59:
                    state = INIT; break;
                default:
                    state = INIT;
                    if (code < 0x80) {
                        matrix_break(cs2_e0code(code));
                    } else {
                        matrix_clear();
                        xprintf("!CS2_E0_F0!\n");
                        return -1;
                    }
                    break;
            }
            break;

        case cE1:
            switch (code) {
                case 0x14: state = E1_14; break;
                case 0xF0: state = E1_F0; break;
                default:   state = INIT;  break;
            }
            break;

        case E1_14:
            if (code == 0x77) matrix_make(0x00);  /* Pause make */
            state = INIT;
            break;

        case E1_F0:
            if (code == 0x14) state = E1_F0_14;
            else              state = INIT;
            break;

        case E1_F0_14:
            if (code == 0xF0) state = E1_F0_14_F0;
            else              state = INIT;
            break;

        case E1_F0_14_F0:
            if (code == 0x77) matrix_break(0x00); /* Pause break */
            state = INIT;
            break;

        default:
            state = INIT;
            break;
    }
    return 0;
}


/*******************************************************************************
 * Terminal: Scan Code Set 3
 ******************************************************************************/
static uint8_t translate_5576_cs3(uint8_t code)
{
    switch (code) {
        case 0x13: return 0x5D; /* JYEN */
        case 0x5C: return 0x51; /* RO */
        case 0x76: return 0x7E; /* Keypad ' */
        case 0x7E: return 0x76; /* Keypad Dup */
    }
    return code;
}

static uint8_t translate_televideo_dec_cs3(uint8_t code)
{
    switch (code) {
        case 0x08: return 0x76;
        case 0x8D: return 0x77;
        case 0x8E: return 0x67;
        case 0x8F: return 0x7F;
        case 0x90: return 0x7B;
        case 0x6E: return 0x65;
        case 0x65: return 0x6D;
        case 0x67: return 0x62;
        case 0x6D: return 0x64;
        case 0x64: return 0x6E;
        case 0x84: return 0x7C;
        case 0x87: return 0x02;
        case 0x88: return 0x7E;
        case 0x89: return 0x0C;
        case 0x8A: return 0x03;
        case 0x8B: return 0x04;
        case 0x8C: return 0x05;
        case 0x85: return 0x08;
        case 0x86: return 0x10;
        case 0x91: return 0x01;
        case 0x92: return 0x09;
        case 0x77: return 0x58;
        case 0x57: return 0x5C;
        case 0x5C: return 0x53;
        case 0x7C: return 0x68;
    }
    return code;
}

static int8_t process_cs3(uint8_t code)
{
    static enum { READY, cF0 } state = READY;

    /* BAT/ID bytes — treat as reset signal */
    switch (code) {
        case 0xAA: case 0xFC: case 0xBF: case 0xAB:
            state = READY;
            xprintf("!CS3_RESET!\n");
            return -1;
    }

    switch (state) {
        case READY:
            if (0xAB92 == keyboard_id) code = translate_5576_cs3(code);
            if (0xAB91 == keyboard_id) code = translate_televideo_dec_cs3(code);
            switch (code) {
                case 0xF0: state = cF0; break;
                case 0x83: matrix_make(0x02); break; /* PrintScreen */
                case 0x84: matrix_make(0x7F); break; /* Keypad * */
                case 0x85: matrix_make(0x68); break; /* Muhenkan */
                case 0x86: matrix_make(0x78); break; /* Henkan */
                case 0x87: matrix_make(0x00); break; /* Hiragana */
                case 0x8B: matrix_make(0x01); break; /* Left GUI */
                case 0x8C: matrix_make(0x09); break; /* Right GUI */
                case 0x8D: matrix_make(0x0A); break; /* Application */
                default:
                    if (code < 0x80) {
                        matrix_make(code);
                    } else {
                        xprintf("!CS3_READY!\n");
                    }
                    break;
            }
            break;

        case cF0:
            state = READY;
            if (0xAB92 == keyboard_id) code = translate_5576_cs3(code);
            if (0xAB91 == keyboard_id) code = translate_televideo_dec_cs3(code);
            switch (code) {
                case 0x83: matrix_break(0x02); break;
                case 0x84: matrix_break(0x7F); break;
                case 0x85: matrix_break(0x0B); break;
                case 0x86: matrix_break(0x06); break;
                case 0x87: matrix_break(0x00); break;
                case 0x8B: matrix_break(0x01); break;
                case 0x8C: matrix_break(0x09); break;
                case 0x8D: matrix_break(0x0A); break;
                default:
                    if (code < 0x80) {
                        matrix_break(code);
                    } else {
                        xprintf("!CS3_F0!\n");
                    }
                    break;
            }
            break;

        default:
            state = READY;
            break;
    }
    return 0;
}
