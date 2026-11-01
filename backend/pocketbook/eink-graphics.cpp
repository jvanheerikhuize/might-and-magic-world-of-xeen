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

#define FORBIDDEN_SYMBOL_ALLOW_ALL

#include "eink-graphics.h"

#include "common/textconsole.h"

#include <string.h>

namespace PocketBook {

EinkGraphicsManager::EinkGraphicsManager(Platform *platform) : _platform(platform) {
	memset(_palette, 0, sizeof(_palette));
	_screen.create(kGameW, kGameH, Graphics::PixelFormat::createFormatCLUT8());
	_overlay.create(kFrameW, kFrameH, getOverlayFormat());
	_frame = new uint8_t[kFrameW * kFrameH];
	_shown = new uint8_t[kFrameW * kFrameH];
	memset(_frame, 0xff, kFrameW * kFrameH);
	memset(_shown, 0xff, kFrameW * kFrameH);
}

EinkGraphicsManager::~EinkGraphicsManager() {
	_screen.free();
	_overlay.free();
	delete[] _frame;
	delete[] _shown;
}

bool EinkGraphicsManager::hasFeature(OSystem::Feature f) const {
	return false;
}

#ifdef USE_RGB_COLOR
Common::List<Graphics::PixelFormat> EinkGraphicsManager::getSupportedFormats() const {
	Common::List<Graphics::PixelFormat> list;
	list.push_back(Graphics::PixelFormat::createFormatCLUT8());
	return list;
}
#endif

void EinkGraphicsManager::beginGFXTransaction() {
	_pendingW = _screen.w;
	_pendingH = _screen.h;
	_pendingFormatOk = true;
}

void EinkGraphicsManager::initSize(uint width, uint height, const Graphics::PixelFormat *format) {
	_pendingW = width;
	_pendingH = height;
	_pendingFormatOk = !format || format->bytesPerPixel == 1;
}

OSystem::TransactionError EinkGraphicsManager::endGFXTransaction() {
	int err = OSystem::kTransactionSuccess;
	if (!_pendingFormatOk)
		err |= OSystem::kTransactionFormatNotSupported;
	// Only 320x200 fits the 2x frame; that is what Xeen uses.
	if (_pendingW != kGameW || _pendingH != kGameH)
		err |= OSystem::kTransactionSizeChangeFailed;
	if (err == OSystem::kTransactionSuccess && (_screen.w != (int)_pendingW || _screen.h != (int)_pendingH)) {
		_screen.free();
		_screen.create(_pendingW, _pendingH, Graphics::PixelFormat::createFormatCLUT8());
		_screenChangeID++;
	}
	_forceFull = true;
	return (OSystem::TransactionError)err;
}

void EinkGraphicsManager::setPalette(const byte *colors, uint start, uint num) {
	assert(start + num <= 256);
	memcpy(_palette + 3 * start, colors, 3 * num);
	_lut.update(colors, start, num);
	// No dirty tracking needed: updateScreen() diffs the composed gray frame,
	// so a palette change shows up exactly where it changes gray levels.
}

void EinkGraphicsManager::grabPalette(byte *colors, uint start, uint num) const {
	assert(start + num <= 256);
	memcpy(colors, _palette + 3 * start, 3 * num);
}

void EinkGraphicsManager::copyRectToScreen(const void *buf, int pitch, int x, int y, int w, int h) {
	_screen.copyRectToSurface(buf, pitch, x, y, w, h);
}

Graphics::Surface *EinkGraphicsManager::lockScreen() {
	return &_screen;
}

void EinkGraphicsManager::unlockScreen() {
}

void EinkGraphicsManager::fillScreen(uint32 col) {
	_screen.fillRect(Common::Rect(_screen.w, _screen.h), col);
}

void EinkGraphicsManager::fillScreen(const Common::Rect &r, uint32 col) {
	Common::Rect c = r;
	c.clip(Common::Rect(_screen.w, _screen.h));
	_screen.fillRect(c, col);
}

void EinkGraphicsManager::setShakePos(int shakeXOffset, int shakeYOffset) {
	_shakeY = shakeYOffset;
}

void EinkGraphicsManager::showOverlay(bool inGUI) {
	_overlayVisible = true;
}

void EinkGraphicsManager::hideOverlay() {
	_overlayVisible = false;
}

Graphics::PixelFormat EinkGraphicsManager::getOverlayFormat() const {
	return Graphics::PixelFormat(2, 5, 6, 5, 0, 11, 5, 0, 0);
}

void EinkGraphicsManager::clearOverlay() {
	// The GUI expects the game screen as the overlay background.
	const Graphics::PixelFormat fmt = getOverlayFormat();
	for (int y = 0; y < kFrameH; y++) {
		uint16 *dst = (uint16 *)_overlay.getBasePtr(0, y);
		const byte *src = (const byte *)_screen.getBasePtr(0, y / kScale);
		for (int x = 0; x < kFrameW; x++) {
			const byte *c = _palette + 3 * src[x / kScale];
			dst[x] = fmt.RGBToColor(c[0], c[1], c[2]);
		}
	}
}

void EinkGraphicsManager::grabOverlay(Graphics::Surface &surface) const {
	assert(surface.w >= kFrameW && surface.h >= kFrameH);
	assert(surface.format.bytesPerPixel == 2);
	for (int y = 0; y < kFrameH; y++)
		memcpy(surface.getBasePtr(0, y), _overlay.getBasePtr(0, y), kFrameW * 2);
}

void EinkGraphicsManager::copyRectToOverlay(const void *buf, int pitch, int x, int y, int w, int h) {
	_overlay.copyRectToSurface(buf, pitch, x, y, w, h);
}

bool EinkGraphicsManager::showMouse(bool visible) {
	bool last = _mouseVisible;
	_mouseVisible = visible;
	return last;
}

void EinkGraphicsManager::warpMouse(int x, int y) {
}

bool EinkGraphicsManager::panelToScreen(int px, int py, int &x, int &y) const {
	int fx = px - kFrameX, fy = py - kFrameY;
	if (fx < 0 || fy < 0 || fx >= kFrameW || fy >= kFrameH)
		return false;
	if (_overlayVisible) {
		x = fx;
		y = fy;
	} else {
		x = fx / kScale;
		y = fy / kScale;
	}
	return true;
}

void EinkGraphicsManager::compose(uint8_t *out) const {
	if (_overlayVisible) {
		for (int y = 0; y < kFrameH; y++) {
			const uint16 *src = (const uint16 *)_overlay.getBasePtr(0, y);
			uint8_t *dst = out + y * kFrameW;
			for (int x = 0; x < kFrameW; x++)
				dst[x] = gray565(src[x]);
		}
		return;
	}
	// Game: dither at game resolution (as previewed in Phase 0), then 2x.
	for (int gy = 0; gy < kGameH; gy++) {
		int sy = gy - _shakeY;
		uint8_t *row = out + gy * kScale * kFrameW;
		if (sy < 0 || sy >= _screen.h) {
			memset(row, 0, kFrameW * kScale);
			continue;
		}
		const byte *src = (const byte *)_screen.getBasePtr(0, sy);
		for (int gx = 0; gx < kGameW; gx++) {
			uint8_t v = _lut.lookup(src[gx], bayerCell(gx, gy));
			row[gx * 2] = row[gx * 2 + 1] = v;
		}
		memcpy(row + kFrameW, row, kFrameW);
	}
}

void EinkGraphicsManager::updateScreen() {
	compose(_frame);
	Box dirty;
	if (_forceFull) {
		dirty.right = kFrameW;
		dirty.bottom = kFrameH;
		_forceFull = false;
	} else {
		dirty = diffBox(_frame, _shown, kFrameW, kFrameH);
	}
	if (dirty.empty())
		return;
	memcpy(_shown, _frame, kFrameW * kFrameH);
	_platform->present(_frame, dirty);
}

} // namespace PocketBook
