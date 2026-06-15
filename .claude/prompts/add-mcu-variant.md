# Add a new MCU variant to ibmpc_usb converter

Use when adding support for a new microcontroller board to `keyboards/converter/ibmpc_usb/`.

## 1. Create the sub-target directory

```bash
mkdir -p keyboards/converter/ibmpc_usb/<variant>/
```

## 2. Required files per variant

### For AVR (e.g. promicro, atmega32u4_*)

```
<variant>/
├── config.h    ← pin assignments
├── rules.mk    ← MCU=, BOOTLOADER=
└── ibmpc.h     ← AVR interrupt/GPIO macros (INT vector, EIMSK, EICRA, port/bit macros)
```

`config.h` template:
```c
#pragma once
#define IBMPC_CLOCK_PIN   D1    // must be INT-capable
#define IBMPC_DATA_PIN    D0
#define IBMPC_RST_PIN0    B6
#define IBMPC_RST_PIN1    B7
```

`rules.mk` template:
```makefile
MCU = atmega32u4
BOOTLOADER = caterina   # or atmel-dfu, qmk-dfu, halfkay, etc.
```

`ibmpc.h` — defines interrupt macros for the chosen INT pin:
```c
// Example for INT1 on D1
#define IBMPC_INT_INIT()  /* setPinInput handled by init */
#define IBMPC_INT_ON()    do { EIMSK |= (1<<INT1); EICRA = (EICRA & ~(3<<ISC10)) | (2<<ISC10); } while(0)
#define IBMPC_INT_OFF()   do { EIMSK &= ~(1<<INT1); } while(0)
#define IBMPC_INT_VECT    INT1_vect
// Direct port access for ISR timing (must read within 5us of clock edge)
#define IBMPC_CLOCK_PORT  PORTD
#define IBMPC_CLOCK_DDR   DDRD
#define IBMPC_CLOCK_PIN_REG PIND
#define IBMPC_CLOCK_BIT   1
```

### For ARM/ChibiOS (e.g. blackpill_f401, blackpill_f411)

```
<variant>/
├── config.h      ← pin assignments (QMK pin names: A0, B1, etc.)
├── rules.mk      ← MCU=STM32Fxxx, BOOTLOADER=stm32-dfu, KEYBOARD_SHARED_EP=yes
├── ibmpc.h       ← ChibiOS PAL macros + palCallback declaration
├── <variant>.c   ← board init (can be empty)
├── chconf.h      ← ChibiOS kernel config
├── halconf.h     ← HAL driver enables (PAL_USE_CALLBACKS=TRUE)
└── mcuconf.h     ← MCU clock config
```

`config.h` template:
```c
#pragma once
#define IBMPC_CLOCK_PIN   A0
#define IBMPC_DATA_PIN    A1
#define IBMPC_RST_PIN0    A2
#define IBMPC_RST_PIN1    A3
```

`rules.mk` template:
```makefile
MCU = STM32F401          # or STM32F411
BOOTLOADER = stm32-dfu
KEYBOARD_SHARED_EP = yes
```

`keyboard.json` (if using QMK board alias):
```json
{ "development_board": "blackpill_f401" }
```

## 3. ibmpc.h selection

The top-level `keyboards/converter/ibmpc_usb/ibmpc.h` selects the platform header:
```c
#if defined(__AVR__)
#  include_next "ibmpc.h"   // resolved from the sub-target directory
#elif defined(STM32F401xx) || defined(STM32F411xx)
#  include_next "ibmpc.h"
#endif
```

Each sub-target's `ibmpc.h` defines the same macro interface:
`clock_lo/hi/in`, `data_lo/hi/in`, `inhibit`, `idle`, `inhibit_xt`,
`IBMPC_RST_HIZ`, `IBMPC_RST_LO`,
`IBMPC_INT_INIT`, `IBMPC_INT_ON`, `IBMPC_INT_OFF`

## 4. Verify the new variant builds

```bash
qmk compile -kb converter/ibmpc_usb/<variant> -km default
```

Check that:
- Correct MCU in build output header
- No undefined macro errors for INT/GPIO macros
- Binary size fits the target MCU flash
