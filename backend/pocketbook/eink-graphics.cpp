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

EinkGraphicsManager::EinkGraphicsManager(Platform *platform)
	: _platform(platform), _fw(layout().frameW), _fh(layout().frameH), _scale(layout().scale) {
	memset(_palette, 0, sizeof(_palette));
	_screen.create(kGameW, kGameH, Graphics::PixelFormat::createFormatCLUT8());
	_overlay.create(_fw, _fh, getOverlayFormat());
	_frame = new uint8_t[_fw * _fh];
	_shown = new uint8_t[_fw * _fh];
	memset(_frame, 0xff, _fw * _fh);
	memset(_shown, 0xff, _fw * _fh);
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
	// Only 320x200 fits the frame at an integer scale; that is what Xeen uses.
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
	for (int y = 0; y < _fh; y++) {
		uint16 *dst = (uint16 *)_overlay.getBasePtr(0, y);
		const byte *src = (const byte *)_screen.getBasePtr(0, y / _scale);
		for (int x = 0; x < _fw; x++) {
			const byte *c = _palette + 3 * src[x / _scale];
			dst[x] = fmt.RGBToColor(c[0], c[1], c[2]);
		}
	}
}

void EinkGraphicsManager::grabOverlay(Graphics::Surface &surface) const {
	assert(surface.w >= _fw && surface.h >= _fh);
	assert(surface.format.bytesPerPixel == 2);
	for (int y = 0; y < _fh; y++)
		memcpy(surface.getBasePtr(0, y), _overlay.getBasePtr(0, y), _fw * 2);
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
	int fx = px - layout().frameX, fy = py - layout().frameY;
	if (fx < 0 || fy < 0 || fx >= _fw || fy >= _fh)
		return false;
	if (_overlayVisible) {
		x = fx;
		y = fy;
	} else {
		x = fx / _scale;
		y = fy / _scale;
	}
	return true;
}

void EinkGraphicsManager::compose(uint8_t *out) const {
	if (_overlayVisible) {
		for (int y = 0; y < _fh; y++) {
			const uint16 *src = (const uint16 *)_overlay.getBasePtr(0, y);
			uint8_t *dst = out + y * _fw;
			for (int x = 0; x < _fw; x++)
				dst[x] = gray565(src[x]);
		}
		return;
	}
	// Game: dither at game resolution (as previewed in Phase 0), then scale.
	for (int gy = 0; gy < kGameH; gy++) {
		int sy = gy - _shakeY;
		uint8_t *row = out + gy * _scale * _fw;
		if (sy < 0 || sy >= _screen.h) {
			memset(row, 0, _fw * _scale);
			continue;
		}
		const byte *src = (const byte *)_screen.getPixels();
		uint8_t *dst = row;
		for (int gx = 0; gx < kGameW; gx++) {
			uint8_t v = mapPixel(_lut, src, _screen.pitch, _screen.w, _screen.h, gx, sy, bayerCell(gx, gy));
			for (int i = 0; i < _scale; i++)
				*dst++ = v;
		}
		for (int i = 1; i < _scale; i++)
			memcpy(row + i * _fw, row, _fw);
	}
}

void EinkGraphicsManager::updateScreen() {
	compose(_frame);
	Box dirty;
	if (_forceFull) {
		dirty.right = _fw;
		dirty.bottom = _fh;
		_forceFull = false;
	} else {
		dirty = diffBox(_frame, _shown, _fw, _fh);
	}
	if (dirty.empty())
		return;
	memcpy(_shown, _frame, _fw * _fh);
	_platform->present(_frame, dirty);
}

} // namespace PocketBook
