# Snap split keyboard QMK configuration

_Exported on 2/7/2026 at 14:08:58 EST from Cursor (2.4.27)_

---

**User**

Explain how the snap split keyboard is configured, putting it in the context of the qmk documentation on split keyboards: https://docs.qmk.fm/features/split_keyboard

@keyboards/nullbitsco/

---

**Cursor**

Now let me read the key files directly to show specific details.

Now let me read the remaining relevant files.

Here's a detailed explanation of how the nullbitsco SNAP split keyboard is configured, mapped to the concepts in the [QMK split keyboard documentation](https://docs.qmk.fm/features/split_keyboard).

---

## 1. Enabling Split Mode

The QMK docs say to add `SPLIT_KEYBOARD = yes` to `rules.mk`. The SNAP does this in `keyboard.json` instead (the modern data-driven equivalent):

```45:52:keyboards/nullbitsco/snap/keyboard.json
    "split": {
        "enabled": true,
        "transport": {
            "sync": {
                "oled": true
            }
        }
    },
```

The `"oled": true` sync option corresponds to `#define SPLIT_OLED_ENABLE` in the docs -- it syncs the OLED on/off state from master to slave.

---

## 2. Split Communication (Transport)

The docs describe two transport options: **soft serial** and **I2C**. The SNAP uses **soft serial** on pin `E6`:

```35:35:keyboards/nullbitsco/snap/config.h
#define SOFT_SERIAL_PIN E6
```

This is the single-wire half-duplex serial protocol described in the docs. The two halves communicate over a TRRS cable, with this pin carrying the data signal (plus GND and VCC on the other wires).

Notably, `SPLIT_USB_DETECT` is commented out:

```34:34:keyboards/nullbitsco/snap/config.h
// #define SPLIT_USB_DETECT // Enable if you have issues with USB
```

Since the SNAP uses ATmega32u4 (Pro Micro compatible), it can detect the USB side via VBUS voltage natively, so `SPLIT_USB_DETECT` is not needed by default (but available as a fallback).

---

## 3. Handedness Detection

The docs list several handedness methods. The SNAP uses **Handedness by Pin** -- it reads a GPIO pin to decide which half is left vs. right:

```36:38:keyboards/nullbitsco/snap/config.h
#define SPLIT_HAND_PIN B6
#define DISABLE_SYNC_TIMER
#define SPLIT_HAND_PIN_LOW_IS_LEFT
```

Pin `B6` is read at startup. When low, the controller assumes it's the **left** half; when high, the **right** half. This is a hardware-level determination -- each PCB half has `B6` wired differently so the firmware can auto-detect which side it's on.

This is the most reliable method because it works regardless of which side USB is plugged into and doesn't require EEPROM programming or separate firmware builds.

---

## 4. Matrix Configuration (The Interesting Part)

This is where the SNAP significantly diverges from a typical split keyboard. The docs describe a straightforward doubled-rows approach. The SNAP uses a **custom demultiplexed matrix** to save GPIO pins.

### 4a. Matrix Dimensions

```41:43:keyboards/nullbitsco/snap/config.h
#define MATRIX_ROWS 12
#define MATRIX_COLS 9
#define MATRIX_MUX_COLS 3
```

12 rows total = 6 rows per hand (standard split doubling). But only 3 GPIO pins are used for columns via a **demultiplexer** (3 pins address 2^3 = 8 physical columns), plus 1 "virtual" column for an extra pin.

### 4b. Asymmetric Pin Assignments

The docs mention `MATRIX_ROW_PINS_RIGHT` for keyboards with differently-shaped halves. The SNAP uses this extensively because each half has completely different pin assignments:

```52:58:keyboards/nullbitsco/snap/config.h
// Left side
#define MATRIX_ROW_PINS { D4, C6, D7, F4, B4, B5 }
#define MATRIX_COL_MUX_PINS { F7, F6, F5 }

//Right side
#define MATRIX_ROW_PINS_RIGHT { F4, F5, F6, F7, B1, B3 }
#define MATRIX_COL_MUX_PINS_RIGHT { D7, C6, D4 }
#define MATRIX_EXT_PIN_RIGHT B6
```

The right side also has an **extended pin** (`B6`) that maps to a single extra matrix position `[row 2, col 8]`. This is how the 9th column exists -- it's not a full column, just one additional key on the right side.

### 4c. Custom Matrix Implementation

Because of the demultiplexer, the SNAP can't use QMK's built-in matrix scanning. It declares a **custom lite matrix** in `rules.mk`:

```1:1:keyboards/nullbitsco/snap/rules.mk
CUSTOM_MATRIX = lite
```

The `lite` mode means the SNAP provides `matrix_init_custom()` and `matrix_scan_custom()` while QMK handles the split transport. The actual scanning logic in `matrix.c` works like this:

1. **Initialization** -- selects the correct pin arrays based on `isLeftHand` (from QMK's `split_util.h`):

```93:100:keyboards/nullbitsco/snap/matrix.c
void matrix_init_custom(void) {
    if (!isLeftHand) {
        row_pins = row_pins_right;
        col_pins = col_pins_right;
    }

    init_pins();
}
```

2. **Column selection via demux** -- instead of driving columns directly, it writes a 3-bit address to the demux pins:

```60:66:keyboards/nullbitsco/snap/matrix.c
static void select_col(uint8_t col) {
    // Drive demux with correct column address
    for (uint8_t bit = 0; bit < MATRIX_MUX_COLS; bit++) {
        uint8_t state = (col & (0b1 << bit)) >> bit;
        gpio_write_pin(col_pins[bit], !state);
    }
}
```

3. **Row reading** -- after selecting a column, it reads all 6 row pins with a 5us settling delay:

```68:80:keyboards/nullbitsco/snap/matrix.c
static void read_rows_on_col(matrix_row_t current_matrix[], uint8_t current_col) {
    select_col(current_col % PHYS_COLS_PER_HAND);
    wait_us(5);

    // Read each row sequentially
    for (uint8_t row_index = 0; row_index < ROWS_PER_HAND; row_index++) {
        if (gpio_read_pin(row_pins[row_index]) == 0) {
            current_matrix[row_index] |= (COL_SHIFTER << current_col);
        } else {
            current_matrix[row_index] &= ~(COL_SHIFTER << current_col);
        }
    }
}
```

4. **Extended pin** -- the right-side-only extra key is read separately and injected into the matrix at position `[2][8]`:

```82:91:keyboards/nullbitsco/snap/matrix.c
static void read_ext_pin(matrix_row_t current_matrix[]) {
    // Read the state of the extended matrix pin
    if (!isLeftHand) {
        if (gpio_read_pin(MATRIX_EXT_PIN_RIGHT) == 0) {
            current_matrix[EXT_PIN_ROW] |= (COL_SHIFTER << EXT_PIN_COL);
        } else {
            current_matrix[EXT_PIN_ROW] &= ~(COL_SHIFTER << EXT_PIN_COL);
        }
    }
}
```

---

## 5. Layout Macro and Matrix Mapping

The docs explain that in a split keyboard, QMK doubles the rows: left half gets rows 0-5, right half gets rows 6-11. You can see this clearly in the layout definitions in `keyboard.json`. For example, the top function row:

-   **Left keys** use matrix rows `0-5` (e.g., `[0,1]`, `[0,2]`, ..., `[0,7]`)
-   **Right keys** use matrix rows `6-11` (e.g., `[6,0]`, `[6,1]`, ..., `[6,7]`)

The extended pin on the right side shows up as `[8, 8]` -- row 8 (= right half row 2), column 8 (the virtual 9th column). In the ANSI layout, this is the key at position `(16, 1)` -- the Delete/Insert key to the right of Backspace.

---

## 6. RGB Split Configuration

The docs describe `RGBLED_SPLIT` for directly-wired LEDs. The SNAP configures this in `keyboard.json`:

```31:31:keyboards/nullbitsco/snap/keyboard.json
        "split_count": [5, 5],
```

10 WS2812 LEDs total, 5 on each side, driven from pin `B2`. This implicitly enables `RGBLIGHT_SPLIT` as the docs note.

---

## 7. Additional Features (Beyond Standard Split)

### Remote Keyboard over UART

The SNAP has a unique **remote keyboard** feature (`common/remote_kb.c`) that uses the UART driver. This allows an external device to send keystrokes over the TRRS cable -- essentially making the SNAP act as a USB bridge for other devices. This is separate from the split transport and is a nullbitsco-specific feature.

```6:8:keyboards/nullbitsco/snap/rules.mk
SRC +=  common/bitc_led.c \
        common/remote_kb.c \
        matrix.c
```

### Bit-C LED

The Bit-C LED on pin `F0` shows Caps Lock status (dim when active, off otherwise), controlled in `snap.c`.

### AVR Deep Sleep Workaround

```26:31:keyboards/nullbitsco/snap/config.h
// NOTE: There is a bug in AVR deep sleep, which
// causes the MCU to stop responding in some cases.
// Disabling the watchdog prevents the MCU from entering
// power down, while still turning off LEDs, audio, etc.
// See qmk_firmware/issues/20087 for background
#undef WDT_vect
```

---

## Summary: How the SNAP Maps to the QMK Split Docs

| QMK Split Feature | SNAP Implementation                                                                |
| ----------------- | ---------------------------------------------------------------------------------- |
| Enable split      | `"split": { "enabled": true }` in `keyboard.json`                                  |
| Transport         | Soft serial on pin `E6` (TRRS cable)                                               |
| Handedness        | Pin-based: `B6` low = left, high = right                                           |
| Master detection  | VBUS voltage detection (native ATmega32u4)                                         |
| Matrix            | Custom lite: demuxed 3-pin columns (8 cols) + 1 extended pin = 9 cols, 6 rows/hand |
| Pin asymmetry     | `MATRIX_ROW_PINS_RIGHT`, `MATRIX_COL_MUX_PINS_RIGHT`                               |
| RGB split         | 5+5 WS2812 LEDs on pin `B2`                                                        |
| Data sync         | OLED sync enabled                                                                  |
| MCU               | ATmega32u4 (Pro Micro compatible)                                                  |

The SNAP is a more complex split keyboard than the "standard" Let's Split model the docs use as a reference. Its key distinguishing features are the **demultiplexed column scanning** (saving GPIOs at the cost of custom matrix code), the **asymmetric right-side extended pin**, and the **remote keyboard UART forwarding**.

---

**User**

i have been having issues with the right half stopping responding seemingly at random. The usb is connected to the left. disconnecting adn reconnecting usb normally fixes it. Could any of this configuration lead to that behavior?

---

**Cursor**

This is a well-known class of issue with split keyboards, and there are several things in this configuration that are relevant. Let me walk through them in order of likelihood.

## 1. No Split Watchdog (Most Likely Culprit)

The QMK docs describe `SPLIT_WATCHDOG_ENABLE` as a feature that "will reboot the keyboard if no successful communication occurs within `SPLIT_WATCHDOG_TIMEOUT`." This is **exactly** the recovery mechanism for your symptom -- the slave (right) half hangs, and without a watchdog, it has no way to self-recover. The only fix is a power cycle (unplugging USB, which cuts VCC to both halves via the TRRS cable).

The SNAP doesn't enable this. But there's a complication...

## 2. `#undef WDT_vect` Blocks the Watchdog

```26:31:keyboards/nullbitsco/snap/config.h
// NOTE: There is a bug in AVR deep sleep, which
// causes the MCU to stop responding in some cases.
// Disabling the watchdog prevents the MCU from entering
// power down, while still turning off LEDs, audio, etc.
// See qmk_firmware/issues/20087 for background
#undef WDT_vect
```

This is a double-edged sword. It was added to work around an AVR bug where the MCU **stops responding** after entering deep sleep -- which, ironically, sounds a lot like the symptom you're experiencing. The workaround removes the watchdog interrupt vector so the MCU never fully enters power-down mode.

However, this also means that `SPLIT_WATCHDOG_ENABLE` (which uses the AVR watchdog timer) may not work correctly with this workaround in place. The watchdog's reset mode (used by `SPLIT_WATCHDOG_ENABLE`) is distinct from the interrupt mode (used by `WDT_vect` for sleep wake), so there's a _chance_ they don't conflict -- but it would need testing.

**The core tension**: the `#undef WDT_vect` was added to prevent _exactly this kind of hang_, but it may not be sufficient, and it blocks the most natural recovery mechanism.

## 3. `DISABLE_SYNC_TIMER`

```37:37:keyboards/nullbitsco/snap/config.h
#define DISABLE_SYNC_TIMER
```

This disables timer synchronization between the halves. If the slave's internal timer drifts relative to the master, it could cause the soft serial protocol to become unreliable over time, eventually leading to communication breakdown. This is a less likely cause but could contribute to the problem becoming more frequent the longer the keyboard runs.

## 4. Soft Serial Sensitivity

The SNAP uses single-wire soft serial on pin `E6`. This protocol is inherently sensitive to:

-   **Electrical noise** on the TRRS cable (especially longer cables)
-   **Loose TRRS connections** -- even slight contact issues can corrupt the serial signal
-   **Hot-plugging** the TRRS cable (the docs explicitly warn against this since VCC runs through the cable)

A single corrupted byte can desync the protocol, and without a watchdog, the slave may never recover.

## What to Try

Here's what I'd suggest, in order:

**First, the easy hardware check**: try a different (shorter, higher-quality) TRRS cable. This is the most common cause of intermittent split keyboard failures.

**Second, try enabling the split watchdog** in your keymap's `config.h`. This is the safest firmware change since it only affects the slave side's recovery behavior:

```c
#define SPLIT_WATCHDOG_ENABLE
#define SPLIT_WATCHDOG_TIMEOUT 3000
```

The question is whether this conflicts with the `#undef WDT_vect` in the keyboard-level config. If you enable the watchdog and the right side starts resetting in a loop (instead of just hanging), that would confirm a conflict and you'd want to revert.

**Third, if the watchdog doesn't help**, you could try removing `DISABLE_SYNC_TIMER` (override it in your keymap `config.h`):

```c
#undef DISABLE_SYNC_TIMER
```

**Fourth, try `SPLIT_USB_DETECT`** if the above don't help. Uncomment it in the keyboard config or add it to your keymap config:

```c
#define SPLIT_USB_DETECT
```

This changes master/slave detection from VBUS voltage sensing to active USB communication detection, which can be more reliable on some setups.

Would you like me to add any of these changes to your keymap's `config.h` so you can test them?

---

**User**

This is all very helpful. I have noticed that if I power the keyboard using a powered hub, then into my host, it resets less. does that make any of these more likely?

Also, I do not mind if the keyboard remains on while the machine is asleep. I use this keyboard with my laptop that is connected to a dongle so it is regularly disconnected

---

**Cursor**

That's a very telling clue. It makes the **USB suspend/power-down path** the most likely trigger, and significantly changes the priority ordering.

## Why the Powered Hub Matters

When your laptop sleeps or you disconnect the dongle, the USB host signals a **suspend** to the keyboard. QMK's AVR suspend handler tries to enter a low-power state -- and this is exactly the code path that the `#undef WDT_vect` workaround was meant to protect against (the AVR deep sleep bug from issue #20087).

With a **powered hub**, the hub maintains stable 5V to the keyboard and may absorb or buffer the suspend signaling, so the keyboard experiences fewer suspend/resume cycles. Without the hub, every laptop sleep, dongle disconnect, or USB power fluctuation triggers the problematic suspend path -- and with your dongle setup, that's happening frequently.

The sequence is likely:

1. Laptop sleeps or dongle disconnects -> USB suspend signal
2. Left (master) MCU enters suspend -> stops talking to right (slave)
3. On resume, the master wakes up, but the slave is stuck in a bad state
4. No watchdog exists to recover the slave -> right half is dead until full power cycle

## The Fix

Since you **don't care about the keyboard sleeping**, the cleanest solution is to just **prevent the power-down entirely** and add the **watchdog as a safety net**. Good news: `SPLIT_WATCHDOG_ENABLE` uses the watchdog in hardware _reset_ mode, not _interrupt_ mode, so it should work fine alongside the `#undef WDT_vect` workaround in the keyboard config.

Here's what I'd add to your keymap `config.h`:

Here's what these two changes do:

**`NO_SUSPEND_POWER_DOWN`** -- This is the direct fix. It tells QMK to skip the power-down code path entirely during USB suspend. Since you don't want the keyboard to sleep anyway, this eliminates the trigger. The LEDs and OLED will still turn off (that's handled separately), but the MCU stays fully awake and responsive. This addresses the root cause more cleanly than `#undef WDT_vect` -- it prevents the problematic code from running rather than breaking the interrupt vector it depends on.

**`SPLIT_WATCHDOG_ENABLE`** -- This is the safety net. If the right half _does_ lose communication for any reason (flaky TRRS connection, electrical noise, etc.), it will automatically reboot after 3 seconds instead of hanging forever. This turns your current "unplug and replug USB" recovery into an automatic 3-second self-heal.

You'll need to flash both halves with the new firmware for the watchdog to take effect on the right side. Want me to check if there's anything else worth adjusting, or would you like to build and test this?

---

**User**

THe right half becomes unresponsive during active use, not just when fiddling with the dongle or sleeping the machine.

Also, should I remove the undef WDT_vect?

---

**Cursor**

Good point on both counts.

**On the unresponsiveness during active use**: That shifts the likely cause away from suspend/resume and toward **serial communication failure on the TRRS link**. The soft serial protocol can lose sync from electrical noise, a marginal TRRS connection, or timing drift -- and once the slave desynchronizes, it hangs with no recovery. The `SPLIT_WATCHDOG_ENABLE` is the most important change then, since it directly addresses this: the right half will auto-reboot in 3 seconds instead of staying dead.

The `NO_SUSPEND_POWER_DOWN` is still worth keeping -- it can't hurt, and it removes one additional way the MCU can get into a bad state. But the watchdog is doing the heavy lifting for your active-use scenario.

**On removing `#undef WDT_vect`**: Yes, you should. Here's why:

-   `#undef WDT_vect` was a workaround for the AVR deep sleep hang. With `NO_SUSPEND_POWER_DOWN`, the MCU never enters deep sleep, so the workaround is redundant.
-   More importantly, while `SPLIT_WATCHDOG_ENABLE` uses the watchdog in _reset mode_ (which doesn't need `WDT_vect`), removing the `#undef` is cleaner -- it restores the full watchdog interrupt vector so nothing in QMK's internals is working with a broken assumption.
-   Since this is in the keyboard-level config (not your keymap), it would affect all keymaps for this board, but that's fine -- `NO_SUSPEND_POWER_DOWN` replaces its purpose.

Let me verify both files look right.

Both files look clean. To summarize the changes:

**Keyboard-level `config.h`**: Removed `#undef WDT_vect`, replaced with a comment explaining the history. The watchdog hardware is now fully available.

**Keymap-level `config.h`**: Added `NO_SUSPEND_POWER_DOWN` (replaces what `#undef WDT_vect` was doing, but without crippling the watchdog) and `SPLIT_WATCHDOG_ENABLE` with a 3-second timeout (auto-reboots the right half when serial communication drops during active use).

One note on the keymap comment -- it still references compatibility with the `#undef` since I wrote it before we removed it. Let me clean that up.

Remember to **flash both halves** -- the watchdog runs on the slave (right) side, so it needs the new firmware too.

---

Let me look at the keymap configuration and keyboard definition to understand where these keys are mapped.

[10 tools called]

This is a great diagnostic question. Let me trace each of your dead keys through the matrix to find the pattern.

## Matrix analysis of the non-working keys

Your keymap uses `LAYOUT_ansi`. Here are the matrix positions for each dead key:

| Key you reported | Keycode in keymap | Matrix position | Right-half local row | **Column**         |
| ---------------- | ----------------- | --------------- | -------------------- | ------------------ |
| `\`              | `KC_BSLS`         | `[9, 7]`        | row 3 (pin F7)       | **7 (demux)**      |
| End              | `KC_END`          | `[8, 7]`        | row 2 (pin F6)       | **7 (demux)**      |
| Page Up          | `KC_PGUP`         | `[10, 7]`       | row 4 (pin B1)       | **7 (demux)**      |
| Page Down        | `KC_PGDN`         | `[11, 7]`       | row 5 (pin B3)       | **7 (demux)**      |
| Delete           | `KC_HOME`\*       | `[8, 8]`        | row 2 (pin F6)       | **8 (ext pin B6)** |

\*Note: your keymap maps the physical "Delete" position to `KC_HOME`, not `KC_DEL`. If you remapped it to Delete through VIA, see below.

## Verdict: almost certainly hardware

**Four of the five dead keys share a single electrical column -- column 7 of the right-half demux.** They span four different row pins (F6, F7, B1, B3), so the row scanning is fine. The common element is the demux output.

Column 7 is demux address `111` (binary). The `select_col` function in `matrix.c` drives all three mux pins (`D7`, `C6`, `D4` on the right side) to select this column:

```60:66:keyboards/nullbitsco/snap/matrix.c
static void select_col(uint8_t col) {
    // Drive demux with correct column address
    for (uint8_t bit = 0; bit < MATRIX_MUX_COLS; bit++) {
        uint8_t state = (col & (0b1 << bit)) >> bit;
        gpio_write_pin(col_pins[bit], !state);
    }
}
```

If the demux output line for address 7 is broken -- a cracked trace, bad solder joint on the demux IC, or a damaged demux channel -- every key on column 7 goes dead while all other columns work perfectly. This matches your symptoms exactly.

The fifth key ("Delete" at `[8, 8]`) uses a completely different electrical path: pin `B6` (the extended pin), not the demux. The fact that it's also dead could mean:

-   **Physical PCB damage in the right edge area** -- all five keys live in the same narrow vertical strip on the far right of the right PCB. A hairline crack, corrosion, or flex damage could take out multiple traces regardless of which column circuit they belong to.
-   **Or a VIA remap issue** -- your keymap actually maps that position to `KC_HOME`, not `KC_DEL`. If you remapped it to Delete via VIA and the EEPROM got corrupted/cleared, the key would revert to Home, which you'd perceive as "Delete stopped working" even though the switch is fine.

## Quick test to confirm

**Test the Pause key** (`KC_PAUS`, matrix `[6, 7]`). It's in the top-right area of the right half and also on demux column 7. If Pause is also dead, that's a definitive confirmation that the demux column 7 output has failed -- a hardware problem.

## What you can try (software, quick)

Before assuming hardware failure, try **clearing the VIA EEPROM** to rule out corrupted key mappings. Hold the key at the top-left of the right half (Escape position, `[0,1]`) while plugging in USB -- or if your QMK build supports it, use the `EE_CLR` keycode to reset EEPROM. This will revert all VIA remaps to the compiled keymap defaults. If the column 7 keys come back, it was VIA corruption; if not, it's hardware.

## If it is hardware

The most likely failure points, in order:

1. **Demux IC solder joints** on the right PCB -- reflow the demux chip
2. **Cracked/corroded trace** from the demux output 7 to the column line -- inspect under magnification
3. **TRRS connector/cable** -- less likely since it would affect all right-side keys, not just one column, but worth swapping the cable to rule it out
