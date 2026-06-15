# IBM PC USB Keyboard Converter

Converts IBM PC keyboards (XT, AT, PS/2, Terminal) to USB HID.

Supports IBM Scan Code Sets 1, 2, and 3 — automatically detected at startup.

## Supported keyboards

| Protocol | Scan Code Set | Example keyboards |
|----------|---------------|-------------------|
| XT       | Set 1         | IBM PC 83-key (Model F), IBM XT 83-key |
| AT       | Set 2         | IBM AT 84-key (Model F), IBM PS/2 101/102-key (Model M) |
| PS/2     | Set 2         | IBM PS/2 101/102-key (Model M) and clones |
| Terminal | Set 3         | IBM 122-key terminal, Cherry G80-2551, IBM RT keyboard |

## Hardware wiring (Pro Micro)

| PS/2 DIN connector | Pro Micro pin | Signal |
|-------------------|---------------|--------|
| Pin 1 (Clock)     | D1 (INT1)     | Clock  |
| Pin 5 (Data)      | D0            | Data   |
| Pin 4             | GND           | Ground |
| Pin 3 or 6        | VCC           | 5V     |

XT reset lines (optional, for hard-reset support):

| Signal | Pro Micro pin |
|--------|---------------|
| RST0   | B6            |
| RST1   | B7            |

## Build

```bash
qmk compile -kb converter/ibmpc_usb/promicro -km default
```

## Flash

```bash
qmk flash -kb converter/ibmpc_usb/promicro -km default
```

## Debug console

Enable with `CONSOLE_ENABLE = yes` in `rules.mk` (already on by default), then:

```bash
qmk console
```

State machine output format:
- `I<time>` — INIT
- `A<time>` — AT_RESET (sending 0xFF)
- `X<time>` — XT_RESET complete
- `W<time>` — byte received during BAT wait
- `R<time>` — READ_ID start
- `ID:xxxx(KIND)` — keyboard identified
- `S<time>` — SETUP
- `L<time>` — entering LOOP

## Protocol notes

- XT keyboards: no 0xFF response → XT_RESET path with hard-reset via RST pins
- AT 84-key: responds to 0xFF with FA, then 0x00 to 0xF2 → keyboard ID 0x0000
- IBM 122-key terminal: sends AA BF BF after BAT → switched to Scan Code Set 3
- IBM 5576 (Japanese): special remapping applied for code sets 2 and 3
