// Copyright 2021 Jay Greco
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// Reduce VIA layers to fit encoder map in EEPROM
// Default is 4 layers, but we only use 2
#define DYNAMIC_KEYMAP_LAYER_COUNT 2

// Prevent the MCU from entering power-down during USB suspend.
// This avoids the AVR deep sleep bug (qmk_firmware#20087) more
// directly than the #undef WDT_vect workaround in the keyboard
// config, and means the keyboard stays responsive through
// laptop sleep / dongle disconnect cycles.
#define NO_SUSPEND_POWER_DOWN

// If the slave (right) half loses communication with the master,
// the watchdog will reboot it after 3 seconds instead of hanging
// permanently. Requires flashing both halves.
#define SPLIT_WATCHDOG_ENABLE
#define SPLIT_WATCHDOG_TIMEOUT 3000

