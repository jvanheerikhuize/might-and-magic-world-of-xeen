/* mmIV - ScummVM PocketBook (InkView) backend
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

// The touch keypad under the game (PLAN.md section 4). Pure data + hit
// testing, no ScummVM or InkView headers, so it is unit-tested on the host and
// shared by the OSystem (key events) and the InkView platform (drawing).
// Key codes are ScummVM Common::KeyCode values; they equal ASCII for
// printable keys, and the few special ones are spelled out in keypad.cpp.

#ifndef BACKENDS_PLATFORM_POCKETBOOK_KEYPAD_H
#define BACKENDS_PLATFORM_POCKETBOOK_KEYPAD_H

#include <stdint.h>

namespace PocketBook {

struct KeypadButton {
	enum Action { kKey, kTextEntry, kMainMenu };
	int x, y, w, h;      // panel coordinates
	const char *label;
	Action action;
	int keycode;         // Common::KeyCode
	uint16_t ascii;
	uint8_t flags;       // Common::KBD_* modifiers
};

// All buttons, in drawing order.
const KeypadButton *keypadButtons(int &count);

// The button under a panel point, or nullptr.
const KeypadButton *keypadHit(int px, int py);

// Top of the keypad area on the panel.
int keypadTop();

} // namespace PocketBook

#endif
