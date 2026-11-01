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

// The seam between the ScummVM side (OSystem, graphics manager; runs on the
// engine thread) and the device side (InkView on the main thread, or the
// host stub). Implemented by platform-inkview.cpp and platform-host.cpp.

#ifndef BACKENDS_PLATFORM_POCKETBOOK_PB_PLATFORM_H
#define BACKENDS_PLATFORM_POCKETBOOK_PB_PLATFORM_H

#include "gray-map.h"

#include <stdint.h>

namespace PocketBook {

// Panel geometry (PB628 portrait, measured in Phase 1).
enum {
	kPanelW = 758, kPanelH = 1024,
	kGameW = 320, kGameH = 200, kScale = 2,
	kFrameW = kGameW * kScale, kFrameH = kGameH * kScale,  // 640x400 gray frame
	kFrameX = (kPanelW - kFrameW) / 2, kFrameY = 24         // where it sits on the panel
};

struct Input {
	enum Type { kNone, kTouchDown, kTouchUp, kKeyDown, kKeyUp, kQuit, kSuspend, kResume };
	Type type = kNone;
	int x = 0, y = 0;        // touch: panel coordinates
	int keycode = 0;         // key: Common::KeyCode value
	uint16_t ascii = 0;
	uint8_t flags = 0;       // key: Common::KBD_* modifiers
};

// Save slot written when the app goes to the background (the firmware ends
// the app ~10 s after Home, Phase 1). Slot 0 is Xeen's autosave slot and may
// hold the player's own save, so a separate slot is used. The launcher
// resumes the most recently written save.
enum { kSuspendSaveSlot = 99 };

class Platform {
public:
	virtual ~Platform() {}
	// Called on the engine thread after each updateScreen() that changed pixels.
	// frame: kFrameW x kFrameH gray bytes; dirty: changed area in frame coordinates.
	virtual void present(const uint8_t *frame, const Box &dirty) = 0;
	// Called on the engine thread from pollEvent(); returns false when no input.
	virtual bool pollInput(Input &in) = 0;
	// Directory holding the binary, mm.dat and the log (no trailing slash).
	virtual const char *appDir() const = 0;
	virtual void log(const char *message) = 0;
	// The engine asked to quit (OSystem::quit) or scummvm_main returned.
	virtual void engineFinished() = 0;
};

Platform *platform();

} // namespace PocketBook

#endif
