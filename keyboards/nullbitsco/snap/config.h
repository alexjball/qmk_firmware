/* Copyright 2021 Jay Greco
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
// clang-format off
#pragma once

/* Used to set host for remote KB if VUSB detect doesn't work. */
// #define KEYBOARD_HOST // Force host mode
// #define KEYBOARD_REMOTE // Force remote mode

// Workarounds for sleep/wake issues
#define USB_SUSPEND_WAKEUP_DELAY 250

// NOTE: There was a bug in AVR deep sleep (qmk_firmware#20087)
// which caused the MCU to stop responding. The original workaround
// was #undef WDT_vect, but NO_SUSPEND_POWER_DOWN (set in keymap
// config) is a cleaner fix that also allows SPLIT_WATCHDOG_ENABLE
// to function with full watchdog support.

/* split config */
// #define SPLIT_USB_DETECT // Enable if you have issues with USB
#define SOFT_SERIAL_PIN E6
#define SPLIT_HAND_PIN B6
#define DISABLE_SYNC_TIMER
#define SPLIT_HAND_PIN_LOW_IS_LEFT

/* key matrix size */
#define MATRIX_ROWS 12
#define MATRIX_COLS 9
#define MATRIX_MUX_COLS 3

/*
 * Keyboard Matrix Assignments
 * The snap uses a demultiplexer for the cols.
 * to free up more IOs for awesomeness!
 * See matrix.c for more details.
*/
// Left side
#define MATRIX_ROW_PINS { D4, C6, D7, F4, B4, B5 }
#define MATRIX_COL_MUX_PINS { F7, F6, F5 }

//Right side
#define MATRIX_ROW_PINS_RIGHT { F4, F5, F6, F7, B1, B3 }
#define MATRIX_COL_MUX_PINS_RIGHT { D7, C6, D4 }
#define MATRIX_EXT_PIN_RIGHT B6

/* Optional speaker pin */
#define AUDIO_PIN B6
