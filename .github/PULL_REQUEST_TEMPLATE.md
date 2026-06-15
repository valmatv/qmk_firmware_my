<!--- Provide a general summary of your changes in the title above. -->

<!--- Anything on lines wrapped in comments like these will not show up in the final text. -->

## Description

<!--- Describe your changes in detail here. -->

## Types of Changes

<!--- What types of changes does your code introduce? Put an `x` in all the boxes that apply. -->
- [ ] Core
- [ ] Bugfix
- [ ] New feature
- [ ] Enhancement/optimization
- [ ] Keyboard (addition or update)
- [ ] Keymap/layout (addition or update)
- [ ] Documentation

## Issues Fixed or Closed by This PR

* 

## Checklist

<!--- Go over all the following points, and put an `x` in all the boxes that apply. -->
<!--- If you're unsure about any of these, don't hesitate to ask. We're here to help! -->
- [ ] My code follows the code style of this project: [**C**](https://docs.qmk.fm/#/coding_conventions_c), [**Python**](https://docs.qmk.fm/#/coding_conventions_python)
- [ ] I have read the [**PR Checklist** document](https://docs.qmk.fm/#/pr_checklist) and have made the appropriate changes.
- [ ] My change requires a change to the documentation.
- [ ] I have updated the documentation accordingly.
- [ ] I have read the [**CONTRIBUTING** document](https://docs.qmk.fm/#/contributing).
- [ ] I have added tests to cover my changes.
- [ ] I have tested the changes and verified that they work and don't break anything (as well as I can manage).

---

<!--- Remove the section below if this PR does not touch keyboards/converter/ibmpc_usb/ -->

## IBM PC USB Converter checklist

**Build verification**
- [ ] `qmk compile -kb converter/ibmpc_usb/promicro -km default` passes
- [ ] `qmk compile -kb converter/ibmpc_usb/blackpill_f401 -km default` passes
- [ ] `qmk compile -kb converter/ibmpc_usb/blackpill_f411 -km default` passes

**Hardware test**
- Controller used: <!--- Pro Micro / BlackPill F401 / BlackPill F411 -->
- Keyboard tested: <!--- IBM Model M 101-key / XT 83-key / Terminal 122-key / etc. -->
- Scan code set detected (from `qmk console`): <!--- PC_XT / PC_AT / PC_TERMINAL -->

**Protocol correctness** (for changes to `ibmpc.c`)
- [ ] ISR timeout is `> 10` ms
- [ ] AT parity check present
- [ ] `host_send()` returns `-1` immediately if ISR is busy (no spin-wait)
- [ ] `clock_lo()` called in ISR ERROR path

**Console output** — paste `qmk console` init sequence:
```
```
