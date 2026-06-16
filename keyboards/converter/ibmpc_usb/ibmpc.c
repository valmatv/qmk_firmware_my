/*
Copyright 2010,2011,2012,2013,2019 Jun WAKO <wakojun@gmail.com>
Copyright 2021 Markus Fritsche <fritsche.markus@gmail.com>

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

/*
 * IBM PC keyboard protocol driver
 *
 * Ported from TMK tmk_core/protocol/ibmpc.cpp (C++ class → C functions).
 * Authoritative source: https://github.com/tmk/tmk_keyboard
 *
 * Key differences from vial-qmk port (bugs fixed):
 *   1. ISR timeout: > 10ms (was >= 3ms)
 *   2. AT parity check implemented (was TODO)
 *   3. XT_Clone ISR case (0b11000000) goes directly to done, no clock-wait in ISR
 *   4. XT_IBM-error-done (0b11100000) is a separate case
 *   5. ISR ERROR path calls clock_lo() to inhibit the device
 *   6. DONE uses goto END, does not fall through to ERROR
 *   7. host_send() returns -1 immediately if ISR is busy (no spin-wait)
 *   8. ibmpc_isr_debug captured in send error path
 *   9. XT_IBM guard (if !protocol) in 0b10100000 case
 */

#include <stdbool.h>
#include "ibmpc.h"
#include "debug.h"
#include "timer.h"
#include "wait.h"
#include "ringbuf.h"

#define WAIT(stat, us, err) do { \
    if (!wait_##stat(us)) { \
        ibmpc_error = err; \
        goto ERROR; \
    } \
} while (0)


/* --------------------------------------------------------------------------
 * Global state (volatile: accessed from both ISR and main loop)
 * -------------------------------------------------------------------------- */
volatile uint16_t ibmpc_isr_debug = 0;
volatile uint8_t  ibmpc_protocol  = IBMPC_PROTOCOL_NO;
volatile uint8_t  ibmpc_error     = IBMPC_ERR_NONE;

/* ring buffer for data received from the device */
#define RINGBUF_SIZE 16
static uint8_t  rbuf_data[RINGBUF_SIZE];
static ringbuf_t rb;

/* ISR shift register: initialised to 0x8000 (sentinel bit = idle marker).
 * Each falling clock edge shifts one data bit into the MSB.
 * Protocol is identified when the sentinel reaches a known position. */
static volatile uint16_t isr_state  = 0x8000;
static uint8_t            timer_start = 0;


/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void ibmpc_host_init(void)
{
    IBMPC_RST_HIZ();
    inhibit();
    IBMPC_INT_INIT();
    IBMPC_INT_OFF();
    ringbuf_init(&rb, rbuf_data, RINGBUF_SIZE);
}

void ibmpc_host_enable(void)
{
    IBMPC_INT_ON();
    idle();
}

void ibmpc_host_disable(void)
{
    IBMPC_INT_OFF();
    inhibit();
}

/*
 * Send one byte to the keyboard.
 * Returns the keyboard's ACK byte on success, -1 on failure.
 *
 * Returns -1 immediately (non-blocking) if the ISR is currently
 * receiving a byte — the caller should retry.
 */
int16_t ibmpc_host_send(uint8_t data)
{
    bool    parity = true;
    uint8_t retry  = 0;
    ibmpc_error = IBMPC_ERR_NONE;

    dprintf("w%02X ", data);

    /* Non-blocking: return -1 if ISR is mid-byte */
    if (isr_state != 0x8000) {
        dprintf("isr:%04X ", isr_state);
        return -1;
    }

    IBMPC_INT_OFF();

RETRY:
    inhibit();
    wait_us(200);   /* [5]p.54: hold clock low ≥ 100 µs */

    /* Request-to-Send + Start bit */
    data_lo();
    wait_us(200);
    clock_hi();                         /* release clock */
    WAIT(clock_lo, 10000, 1);           /* keyboard pulls clock low within 10ms */

    /* 8 data bits, LSB first */
    for (uint8_t i = 0; i < 8; i++) {
        wait_us(15);
        if (data & (1 << i)) {
            parity = !parity;
            data_hi();
        } else {
            data_lo();
        }
        WAIT(clock_hi, 50, 2);
        WAIT(clock_lo, 50, 3);
    }

    /* Parity bit (odd parity) */
    wait_us(15);
    if (parity) { data_hi(); } else { data_lo(); }
    WAIT(clock_hi, 50, 4);
    WAIT(clock_lo, 50, 5);

    /* Stop bit */
    wait_us(15);
    data_hi();

    /* ACK: keyboard pulls Data then Clock low */
    WAIT(data_lo, 300, 6);
    WAIT(data_hi, 300, 7);
    WAIT(clock_hi, 300, 8);

    ibmpc_host_isr_clear();

    idle();
    IBMPC_INT_ON();
    return ibmpc_host_recv_response();

ERROR:
    /* Retry for Zenith Z-150 AT start-bit false-alarm (error code 1) */
    if (ibmpc_error == 1 && retry++ < 10) {
        ibmpc_error = IBMPC_ERR_NONE;
        dprintf("R ");
        goto RETRY;
    }

    /* Capture ISR state before masking with IBMPC_ERR_SEND */
    ibmpc_isr_debug = isr_state;
    ibmpc_error |= IBMPC_ERR_SEND;
    inhibit();
    wait_ms(2);
    idle();
    IBMPC_INT_ON();
    return -1;
}

/*
 * Read one byte from the receive ring buffer.
 * Returns -1 if the buffer is empty.
 * Re-enables the ISR if it was disabled due to a full buffer.
 */
int16_t ibmpc_host_recv(void)
{
    int16_t ret = -1;

    /* If buffer was full and ISR was disabled, re-enable */
    if (ringbuf_is_full(&rb)) {
        ibmpc_host_isr_clear();
        IBMPC_INT_ON();
        idle();
    }

#if defined(__AVR__)
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
#endif
        ret = ringbuf_get(&rb);
#if defined(__AVR__)
    }
#endif

    if (ret != -1) dprintf("r%02X ", ret & 0xFF);
    return ret;
}

/*
 * Wait up to ~25ms for a response byte.
 * Used after sending a command.
 */
int16_t ibmpc_host_recv_response(void)
{
    uint8_t retry = 25;
    int16_t data  = -1;
    while (retry-- && (data = ibmpc_host_recv()) == -1) {
        wait_ms(1);
    }
    return data;
}

void ibmpc_host_isr_clear(void)
{
    ibmpc_isr_debug = 0;
    ibmpc_protocol  = 0;
    ibmpc_error     = 0;
    isr_state       = 0x8000;
    ringbuf_reset(&rb);
}

void ibmpc_host_set_led(uint8_t led)
{
    if (0xFA == ibmpc_host_send(0xED)) {
        ibmpc_host_send(led);
    }
}


/* --------------------------------------------------------------------------
 * Interrupt Service Routine
 *
 * NOTE: data line must be read within 5 µs of the falling clock edge.
 * On ATmega32U4 this is ~2.5 µs after ISR prologue with GCC 5.4.0.
 *
 * isr_state encoding (16-bit shift register, sentinel = 0x8000):
 *
 *   low-byte pattern after shift | protocol state
 *   ------------------------------|---------------------------------------
 *   0b00000000                    | midway (0-7 bits received)
 *   0b10000000                    | midway (8 bits received)
 *   0b01000000 (^1)               | AT/XT_IBM midway
 *   0b00100000                    | midway
 *   0b11000000                    | XT_Clone done
 *   0b11100000                    | XT_IBM error-done
 *   0b10100000 (^2)               | AT/XT_IBM ambiguity — check stop bit
 *   0b??010000                    | AT done (11 bits received)
 *   anything else                 | illegal
 *
 *   ^1: AT and XT_IBM are indistinguishable at this point
 *   ^2: AT (b0=1) and XT_IBM are indistinguishable; wait for AT stop-bit edge
 * -------------------------------------------------------------------------- */
void ibmpc_interrupt_service_routine(void)
{
    uint8_t dbit = data_in();

    /* Timeout: if a byte takes longer than 10ms to arrive, abort */
    uint8_t t = (uint8_t)timer_read();
    if (isr_state == 0x8000) {
        timer_start = t;
    } else if ((uint8_t)(t - timer_start) > 10) {
        /* > 10ms — authoritative value from TMK commit 3c7ca5b6 */
        ibmpc_isr_debug = isr_state;
        ibmpc_error     = IBMPC_ERR_TIMEOUT;
        goto ERROR;
    }

    /* Shift new bit into MSB */
    isr_state = isr_state >> 1;
    if (dbit) isr_state |= 0x8000;

    switch (isr_state & 0xFF) {
        case 0b00000000:
        case 0b10000000:
        case 0b01000000:    /* ^1 */
        case 0b00100000:
            /* midway — keep receiving */
            goto NEXT;

        case 0b11000000:
            /* XT_Clone done: 9 bits (start=0, start=1, b0..b6) shifted in.
             * Go directly to DONE — no clock-wait inside ISR. */
            ibmpc_isr_debug = isr_state;
            isr_state       = isr_state >> 8;
            ibmpc_protocol  = IBMPC_PROTOCOL_XT_CLONE;
            goto DONE;

        case 0b11100000:
            /* XT_IBM error-done */
            ibmpc_isr_debug = isr_state;
            isr_state       = isr_state >> 8;
            ibmpc_protocol  = IBMPC_PROTOCOL_XT_ERROR;
            goto DONE;

        case 0b10100000:    /* ^2: AT (b0=1) vs XT_IBM ambiguity */
            {
                if (!ibmpc_protocol) {
                    uint8_t us = 100;
                    /* Wait for the AT stop-bit's rising then falling edge */
#if defined(__AVR__)
                    while (!(IBMPC_CLOCK_PIN & (1 << IBMPC_CLOCK_BIT)) && us) { wait_us(1); us--; }
                    while (  IBMPC_CLOCK_PIN & (1 << IBMPC_CLOCK_BIT)  && us) { wait_us(1); us--; }
#else
                    while (!clock_in() && us) { wait_us(1); us--; }
                    while ( clock_in() && us) { wait_us(1); us--; }
#endif
                    if (us) {
                        /* Found a stop-bit edge: this is AT, still midway */
                        goto NEXT;
                    }
                    /* No stop-bit edge: XT_IBM done */
                } else if (ibmpc_protocol & IBMPC_PROTOCOL_AT) {
                    /* Already identified as AT — b0=1 midway, stop bit comes next */
                    goto NEXT;
                } else if (ibmpc_protocol == IBMPC_PROTOCOL_XT_IBM) {
                    /* Already identified as XT_IBM — skip the wait */
                }
                ibmpc_isr_debug = isr_state;
                isr_state       = isr_state >> 8;
                ibmpc_protocol  = IBMPC_PROTOCOL_XT_IBM;
                goto DONE;
            }

        case 0b00010000:
        case 0b10010000:
        case 0b01010000:
        case 0b11010000:
            /* AT done: 11 bits (start + 8 data + parity + stop) */
            ibmpc_isr_debug = isr_state;

            /* Special case: 0xAA with inverted parity → XT/AT auto-switching signal.
             * isr_state layout: st pr b7 b6 b5 b4 b3 b2 | b1 b0 0 *1 0 0 0 0
             * AA with bad parity = 0xAA90 */
            if (isr_state == 0xAA90) {
                ibmpc_error = IBMPC_ERR_PARITY_AA;
                goto ERROR;
            }

            /* Odd parity check over data bits b0..b7 and the parity bit */
            {
                /* isr_state: st pr b7..b2 | b1 b0 0 *1 0 0 0 0 */
                uint8_t p = (isr_state & 0x4000) ? 1 : 0;  /* parity bit */
                p ^= (isr_state >> 6);                       /* XOR with b0..b7 packed in high byte */
                while (p & 0xFE) {
                    p = (p >> 1) ^ (p & 0x01);
                }
                if (p == 0) {
                    ibmpc_error = IBMPC_ERR_PARITY;
                    goto ERROR;
                }
            }

            /* Stop bit: high = normal AT; low = Zenith Z-150 AT variant */
            if (isr_state & 0x8000) {
                ibmpc_protocol = IBMPC_PROTOCOL_AT;
            } else {
                /* Zenith Z-150 AT drives stop bit low */
                ibmpc_protocol = IBMPC_PROTOCOL_AT_Z150;
            }
            isr_state = isr_state >> 6;
            goto DONE;

        case 0b01100000:
        case 0b00110000:
        case 0b10110000:
        case 0b01110000:
        case 0b11110000:
        default:
            /* Illegal state */
            ibmpc_protocol  = 0;
            ibmpc_isr_debug = isr_state;
            ibmpc_error     = IBMPC_ERR_ILLEGAL;
            goto ERROR;
    }

DONE:
    /* Store received byte in ring buffer */
    if (!ringbuf_put(&rb, isr_state & 0xFF)) {
        /* Buffer full — overflow */
        ibmpc_error = IBMPC_ERR_FULL;
    }

    if (ringbuf_is_full(&rb)) {
        /* Disable ISR and hold clock low to pause the device */
        IBMPC_INT_OFF();
        clock_lo();
    }
    goto END;

ERROR:
    /* Inhibit the device by pulling clock low */
    clock_lo();

END:
    isr_state = 0x8000;
NEXT:
    return;
}

/* --------------------------------------------------------------------------
 * Platform ISR / callback dispatch
 * -------------------------------------------------------------------------- */
#if defined(__AVR__)
ISR(IBMPC_INT_VECT)
{
    ibmpc_interrupt_service_routine();
}
#else
/* ChibiOS PAL callback — called from PAL interrupt context */
void palCallback(void *arg)
{
    (void)arg;
    ibmpc_interrupt_service_routine();
}
#endif
