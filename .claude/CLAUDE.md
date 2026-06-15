# qmk_firmware_my — IBM PC USB Converter Project

## Repo layout

```
/home/val/Documents/qmk.raspberry/
├── qmk_firmware_my/                   ← ONLY THIS REPO IS EVER MODIFIED
├── tmk_keyboard/                      ← reference only (authoritative protocol source)
└── vial-qmk-with-ibmpc-usb-converter/ ← reference only (partial C port, has known bugs)
```

Target keyboard directory: `keyboards/converter/ibmpc_usb/`

## Active plan

Full implementation plan: `.claude/plans/ibmpc_usb_port.md`
(Copy of `/home/val/.claude/plans/we-do-have-a-dazzling-shannon.md`)

## Build commands

```bash
qmk compile -kb converter/ibmpc_usb/promicro -km default
qmk compile -kb converter/ibmpc_usb/blackpill_f401 -km default
qmk compile -kb converter/ibmpc_usb/blackpill_f411 -km default
```

## Flash commands

```bash
# Pro Micro (ATmega32U4, Caterina bootloader)
qmk flash -kb converter/ibmpc_usb/promicro -km default

# WeAct BlackPill (STM32, DFU bootloader) — hold BOOT0 then press RESET
dfu-util -d 0483:df11 -a 0 -s 0x08000000:leave -D <firmware>.bin
```

## Debug console

```bash
CONSOLE_ENABLE=yes   # set in rules.mk during development
qmk console          # view output while keyboard is connected
```

Use `xprintf()` for state machine events (always emitted).
Use `dprintf()` for per-byte protocol traffic (gated by `debug_enable = true`).

## Coding rules

- **C only** — no C++, no classes, no `new`, no references
- **Platform abstraction for GPIO:**
  - AVR: `writePinHigh/Low()`, `setPinInput/Output()`, `readPin()` from `quantum/gpio.h`
  - ARM/ChibiOS: `palSetLineMode()`, `palWriteLine()`, `palReadLine()` from ChibiOS HAL
  - Never use raw AVR registers (`DDRD`, `PORTD`) in shared code paths
- **Protocol state globals** use `ibmpc_` prefix: `ibmpc_error`, `ibmpc_protocol`, `ibmpc_isr_debug`
- **Authoritative source**: `../../tmk_keyboard/tmk_core/protocol/ibmpc.cpp` — when in doubt, defer to this
- **No Vial-specific code** in protocol or matrix files

## Known bugs in vial-qmk (do not replicate)

1. ISR timeout `>= 3` ms — must be `> 10` ms (TMK commit `3c7ca5b6`)
2. AT parity check missing — must validate odd parity; handle `IBMPC_ERR_PARITY_AA`
3. `case 0b11000000` in ISR adds clock-wait inside the ISR — must go directly to XT_Clone-done
4. No `clock_lo()` in ISR ERROR path — required to inhibit device on receive error
5. `host_send()` spin-waits on busy ISR — must return `-1` immediately (non-blocking)
6. Missing error codes `IBMPC_ERR_PARITY (0x01)` and `IBMPC_ERR_PARITY_AA (0x02)` in `ibmpc.h`
