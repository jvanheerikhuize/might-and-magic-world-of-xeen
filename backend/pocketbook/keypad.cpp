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

#include "keypad.h"
#include "pb-platform.h"

namespace PocketBook {

namespace {

// Common::KeyCode / Common::KBD_* values (common/keyboard.h), repeated here so
// this file stays free of ScummVM headers; tests/unit checks them.
enum {
	kTab = 9, kReturn = 13, kEscape = 27, kSpace = 32,
	kUp = 273, kDown = 274, kRight = 275, kLeft = 276,
	kF1 = 282,
	kCtrl = 1 << 0,
	kAsciiF1 = 315
};

// Xeen's keys (engines/mm/xeen/interface.cpp): arrows move/turn, Ctrl+Left/
// Right strafe, Tab control panel, Space wait, F1-F6 characters; exploring:
// b c d i m q r s v; combat: a b c f i o q r u and 1-3.
const int kGap = 8;
const int kRowH = 88;
const int kMargin = 16;
const int kTop = kFrameY + kFrameH + 16;  // 440

int rowY(int row) { return kTop + row * (kRowH + kGap); }

KeypadButton g_buttons[64];
int g_count = 0;

void add(int x, int y, int w, int h, const char *label, KeypadButton::Action a, int key, uint16_t ascii,
         uint8_t flags = 0) {
	g_buttons[g_count++] = KeypadButton{x, y, w, h, label, a, key, ascii, flags};
}

// One row of n equal buttons across [x0, x1).
void row(int r, int x0, int x1, int n, int i, const char *label, KeypadButton::Action a, int key,
         uint16_t ascii, uint8_t flags = 0) {
	int w = (x1 - x0 - (n - 1) * kGap) / n;
	add(x0 + i * (w + kGap), rowY(r), w, kRowH, label, a, key, ascii, flags);
}

void build() {
	if (g_count)
		return;
	const KeypadButton::Action K = KeypadButton::kKey;
	const int L = kMargin, R = kPanelW - kMargin;
	const int split = 368;  // movement pad | commands

	// Rows 0-1: movement pad (left), exploring commands (right).
	row(0, L, split - kGap, 3, 0, "Turn L", K, kLeft, 0);
	row(0, L, split - kGap, 3, 1, "Fwd", K, kUp, 0);
	row(0, L, split - kGap, 3, 2, "Turn R", K, kRight, 0);
	row(1, L, split - kGap, 3, 0, "Side L", K, kLeft, 0, kCtrl);
	row(1, L, split - kGap, 3, 1, "Back", K, kDown, 0);
	row(1, L, split - kGap, 3, 2, "Side R", K, kRight, 0, kCtrl);
	row(0, split, R, 4, 0, "Bash/Blk", K, 'b', 'b');
	row(0, split, R, 4, 1, "Cast", K, 'c', 'c');
	row(0, split, R, 4, 2, "Rest/Run", K, 'r', 'r');
	row(0, split, R, 4, 3, "Search", K, 's', 's');
	row(1, split, R, 4, 0, "Map", K, 'm', 'm');
	row(1, split, R, 4, 1, "Quick", K, 'q', 'q');
	row(1, split, R, 4, 2, "Info", K, 'i', 'i');
	row(1, split, R, 4, 3, "Quests", K, 'v', 'v');

	// Row 2: party members.
	static const char *const party[6] = {"1", "2", "3", "4", "5", "6"};
	for (int i = 0; i < 6; i++)
		row(2, L, R, 6, i, party[i], K, kF1 + i, (uint16_t)(kAsciiF1 + i));

	// Row 3: combat and waiting.
	row(3, L, R, 5, 0, "Attack", K, 'a', 'a');
	row(3, L, R, 5, 1, "Fight", K, 'f', 'f');
	row(3, L, R, 5, 2, "Use", K, 'u', 'u');
	row(3, L, R, 5, 3, "Dismiss", K, 'd', 'd');
	row(3, L, R, 5, 4, "Wait", K, kSpace, ' ');

	// Row 4: answers and choices.
	row(4, L, R, 6, 0, "Y", K, 'y', 'y');
	row(4, L, R, 6, 1, "N", K, 'n', 'n');
	row(4, L, R, 6, 2, "1", K, '1', '1');
	row(4, L, R, 6, 3, "2", K, '2', '2');
	row(4, L, R, 6, 4, "3", K, '3', '3');
	row(4, L, R, 6, 5, "4", K, '4', '4');

	// Row 5: system.
	row(5, L, R, 5, 0, "Esc", K, kEscape, 27);
	row(5, L, R, 5, 1, "Enter", K, kReturn, 13);
	row(5, L, R, 5, 2, "Panel", K, kTab, 9);
	row(5, L, R, 5, 3, "Abc", KeypadButton::kTextEntry, 0, 0);
	row(5, L, R, 5, 4, "ScummVM", KeypadButton::kMainMenu, 0, 0);
}

} // namespace

const KeypadButton *keypadButtons(int &count) {
	build();
	count = g_count;
	return g_buttons;
}

const KeypadButton *keypadHit(int px, int py) {
	build();
	for (int i = 0; i < g_count; i++) {
		const KeypadButton &b = g_buttons[i];
		if (px >= b.x && px < b.x + b.w && py >= b.y && py < b.y + b.h)
			return &b;
	}
	return nullptr;
}

int keypadTop() {
	return kTop;
}

} // namespace PocketBook
