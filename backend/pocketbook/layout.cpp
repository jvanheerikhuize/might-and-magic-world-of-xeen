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

#include "layout.h"

#include <string.h>

namespace PocketBook {

namespace {

// PB628: 1024x758 panel (Phase 1). Landscape: 3x leaves 32 px either side
// and 158 px below; the frame sits 4 px from the top, the button strip below.
// Portrait: 2x centred, 24 px from the top as in Phase 2/3.
const Layout kLayouts[] = {
	{Layout::kLandscape, "landscape", 1, 1024, 758, 3, kGameW * 3, kGameH * 3, 32, 4},
	{Layout::kLandscapeFlip, "landscape-flip", 2, 1024, 758, 3, kGameW * 3, kGameH * 3, 32, 4},
	{Layout::kPortrait, "portrait", 0, 758, 1024, 2, kGameW * 2, kGameH * 2, (758 - kGameW * 2) / 2, 24},
};

Layout::Mode g_mode = Layout::kLandscape;

} // namespace

Layout::Mode layoutModeFromName(const char *name) {
	if (name)
		for (const Layout &l : kLayouts)
			if (!strcmp(name, l.name))
				return l.mode;
	return Layout::kLandscape;
}

void setLayout(Layout::Mode mode) {
	g_mode = mode;
}

const Layout &layout() {
	return kLayouts[g_mode];
}

} // namespace PocketBook
