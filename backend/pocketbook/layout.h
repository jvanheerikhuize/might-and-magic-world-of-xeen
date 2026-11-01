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

// Where the game sits on the panel. Chosen once at start-up (MMIV_LAYOUT,
// set by the launcher) before anything else is created. Pure C++, no
// ScummVM or InkView headers, so it is unit-tested on the host.
//
//   landscape (default)  panel 1024x758, game 3x = 960x600 centred on black,
//                        no buttons: Xeen's own icons are ~9 mm at 3x, so play
//                        is by tapping the game itself. Esc and the ScummVM
//                        menu are on the MENU key (short / long press), and
//                        text prompts open the reader's keyboard.
//   landscape-flip       the same, rotated the other way round.
//   portrait             panel 758x1024, game 2x = 640x400, the 36-button
//                        touch keypad below (keypad.cpp).

#ifndef BACKENDS_PLATFORM_POCKETBOOK_LAYOUT_H
#define BACKENDS_PLATFORM_POCKETBOOK_LAYOUT_H

namespace PocketBook {

enum { kGameW = 320, kGameH = 200 };

struct Layout {
	enum Mode { kLandscape, kLandscapeFlip, kPortrait };
	Mode mode;
	const char *name;
	int orientation;       // InkView SetOrientation(): 0 portrait, 1 landscape 90, 2 landscape 270
	int panelW, panelH;    // ScreenWidth()/ScreenHeight() in that orientation
	int scale;             // game pixel -> panel pixels
	int frameW, frameH;    // the gray frame: kGameW*scale x kGameH*scale
	int frameX, frameY;    // where the frame sits on the panel

	bool landscape() const { return mode != kPortrait; }
};

// Largest frame of any layout (for fixed buffers).
enum { kMaxFrameW = 960, kMaxFrameH = 600 };

// Mode for a name ("landscape", "landscape-flip", "portrait"); null, empty
// or unknown names give the default, landscape.
Layout::Mode layoutModeFromName(const char *name);

// Select the layout. Call before creating the OSystem and platform.
void setLayout(Layout::Mode mode);
const Layout &layout();

} // namespace PocketBook

#endif
