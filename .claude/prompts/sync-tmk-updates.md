# Sync TMK protocol updates to qmk_firmware_my

Use this checklist whenever `tmk_keyboard` receives new commits that may affect the ibmpc_usb converter.

## 1. Check TMK git log

```bash
cd /home/val/Documents/qmk.raspberry/tmk_keyboard
git log --oneline -30
git log --oneline -- tmk_core/protocol/ibmpc.cpp tmk_core/protocol/ibmpc.hpp
git log --oneline -- converter/ibmpc_usb/
```

## 2. For each relevant commit, diff against our ibmpc.c

```bash
# Show full diff of a TMK commit
git show <commit-hash>

# Compare specific function in our port
grep -n "timer_start\|IBMPC_ERR\|0b11000000\|0b10100000" \
    /home/val/Documents/qmk.raspberry/qmk_firmware_my/keyboards/converter/ibmpc_usb/ibmpc.c
```

## 3. ISR checklist — verify these values in our ibmpc.c

- [ ] Timeout: `(uint8_t)(t - timer_start) > 10`  (not `>= 3`, not `>= 5`)
- [ ] Parity check present for AT-done (`case 0b??010000`) — `IBMPC_ERR_PARITY_AA` + `IBMPC_ERR_PARITY`
- [ ] `case 0b11000000` → XT_Clone-done (no clock-wait inside ISR)
- [ ] `case 0b11100000` → XT_IBM-error-done (separate case)
- [ ] `case 0b10100000` → has `if (!protocol)` guard before clock-wait
- [ ] ERROR path calls `clock_lo()` before resetting `isr_state`
- [ ] DONE path uses `goto END` (does not fall through to ERROR)

## 4. Error code checklist — verify ibmpc.h defines

```c
#define IBMPC_ERR_NONE        0
#define IBMPC_ERR_PARITY      0x01
#define IBMPC_ERR_PARITY_AA   0x02
#define IBMPC_ERR_SEND        0x10
#define IBMPC_ERR_TIMEOUT     0x20
#define IBMPC_ERR_FULL        0x40
#define IBMPC_ERR_ILLEGAL     0x80
```

## 5. Scan code table checklist — compare against TMK unimap_trans.h

```bash
diff \
    /home/val/Documents/qmk.raspberry/tmk_keyboard/converter/ibmpc_usb/unimap_trans.h \
    /home/val/Documents/qmk.raspberry/qmk_firmware_my/keyboards/converter/ibmpc_usb/ibmpc_usb.c
```

Verify:
- [ ] E0 remap table in `cs1_e0code()` matches TMK
- [ ] E0 remap table in `cs2_e0code()` matches TMK
- [ ] Pause/Break sequence (CS1: `E1 1D 45`; CS2: `E1 14 77 E1 F0 14 F0 77`)
- [ ] Unicomp New Model M Pause/Break mapping
