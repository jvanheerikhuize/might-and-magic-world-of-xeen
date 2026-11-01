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

// PB628: 1024x758 panel (Phase 1). Landscape: 3x centred, black around it
// (32 px left and right, 79 px above and below), no buttons.
// Portrait: 2x centred, 24 px from the top as in Phase 2/3.
const Layout kLayouts[] = {
	{Layout::kLandscape, "landscape", 0, 1024, 758, 1, 1, 3, kGameW * 3, kGameH * 3, 32, 79},
	{Layout::kLandscapeFlip, "landscape-flip", 0, 1024, 758, 2, 2, 3, kGameW * 3, kGameH * 3, 32, 79},
	{Layout::kPortrait, "portrait", 0, 758, 1024, 0, 0, 2, kGameW * 2, kGameH * 2, (758 - kGameW * 2) / 2, 24},
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

PanelRect toPanel(const Layout &l, PanelRect r) {
	if (l.rotate == 1)
		return {l.panelH - r.y - r.h, r.x, r.h, r.w};
	if (l.rotate == 2)
		return {r.y, l.panelW - r.x - r.w, r.h, r.w};
	return r;
}

void fromPanel(const Layout &l, int px, int py, int &x, int &y) {
	if (l.rotate == 1) {
		x = py;
		y = l.panelH - 1 - px;
	} else if (l.rotate == 2) {
		x = l.panelW - 1 - py;
		y = px;
	} else {
		x = px;
		y = py;
	}
}

} // namespace PocketBook
