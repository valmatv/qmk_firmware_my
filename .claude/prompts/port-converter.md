# Port a TMK converter to QMK

Use this checklist when porting any additional TMK converter to `qmk_firmware_my`.
Reference: the existing `keyboards/converter/ibmpc_usb/` is the model to follow.

## 1. Create the directory structure

```bash
mkdir -p keyboards/converter/<name>/keymaps/default/
```

Required files:
```
keyboards/converter/<name>/
├── keyboard.json       ← USB IDs, matrix size, layouts
├── config.h            ← pin assignments, matrix dimensions
├── rules.mk            ← CUSTOM_MATRIX=yes, SRC+=, feature flags
├── <protocol>.c        ← protocol driver (C, not C++)
├── <protocol>.h        ← platform-specific GPIO/interrupt macros
├── matrix.c            ← scan code state machine
├── ringbuf.h           ← copy from converter/ibmpc_usb/
└── keymaps/default/
    ├── keymap.c
    └── config.h
```

## 2. rules.mk minimum content

```makefile
CUSTOM_MATRIX = yes
MOUSEKEY_ENABLE = yes
EXTRAKEY_ENABLE = yes
NKRO_ENABLE = yes
CONSOLE_ENABLE = yes    # set to no for production
BOOTMAGIC_ENABLE = no

SRC += matrix.c <protocol>.c
DEFAULT_FOLDER = keyboards/converter/<name>/<default_mcu_variant>
```

## 3. C++ to C translation rules

When translating TMK C++ class to C:
- Class member variables → C globals with `<protocol>_` prefix
- `Class::method()` → `<protocol>_method()`
- Constructor initialization → `ibmpc_host_init()` function
- `ATOMIC_BLOCK` → keep for AVR; use bare reads on ARM (no per-byte ISR contention)
- C++ `bool` → `#include <stdbool.h>` + `bool` (already in QMK)

## 4. Platform ibmpc.h variants

For AVR variant:
- Use `gpio.h` wrappers for non-ISR paths
- Keep `INT_VECT` / `EIMSK` / `EICRA` macros for the interrupt
- ISR function named as `ISR(PROTOCOL_INT_VECT)`

For ARM/ChibiOS variant:
- Use PAL: `palSetLineMode`, `palWriteLine`, `palReadLine`
- Interrupt: `palEnableLineEvent(PIN, PAL_EVENT_MODE_FALLING_EDGE)` + `palSetLineCallback()`
- ISR callback: `void palCallback(void *arg) { protocol_interrupt_service_routine(); }`
- Add `chconf.h`, `halconf.h`, `mcuconf.h` for ChibiOS config

## 5. keyboard.json fields

```json
{
    "keyboard_name": "converter/<name>",
    "manufacturer": "TMK",
    "maintainer": "valmatv",
    "usb": { "vid": "0xFEED", "pid": "0x????", "device_version": "1.0.0" },
    "matrix_pins": { "custom": true },
    "matrix_size": { "rows": N, "cols": M },
    "features": { "nkro": true, "mousekey": true, "extrakey": true },
    "layouts": { "LAYOUT": { "layout": [...] } }
}
```

For MCU sub-targets add `"development_board": "blackpill_f401"` etc. in sub-target keyboard.json.

## 6. Debug logging — required in every port

In protocol.c:
- `dprintf("w%02X ", data)` on every byte sent
- `dprintf("r%02X ", ret)` on every byte received
- `ibmpc_isr_debug` captures last ISR state on any non-NEXT path

In matrix.c state machine — `xprintf` with timer on every state transition:
- `xprintf("I%u ", timer_read())` — INIT
- `xprintf("ID:%04X(%s) ", id, kind_str)` — after keyboard identification
- `xprintf("\n%u ERR:%02X ISR:%04X ", timer_read(), error, isr_debug)` — on error

## 7. Build and verify

```bash
qmk compile -kb converter/<name>/<variant> -km default
qmk console   # watch init sequence and scan codes
```

Expected console output on successful init:
```
I<time> A<time> W<time> ID:xxxx(PC_AT/PC_XT/PC_TERMINAL) S<time> L<time>
```
