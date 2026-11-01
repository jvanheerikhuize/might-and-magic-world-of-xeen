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

#ifndef BACKENDS_PLATFORM_POCKETBOOK_EINK_GRAPHICS_H
#define BACKENDS_PLATFORM_POCKETBOOK_EINK_GRAPHICS_H

#include "backends/graphics/graphics.h"
#include "graphics/surface.h"

#include "gray-map.h"
#include "pb-platform.h"

namespace PocketBook {

// Keeps the game's 8-bit screen and ScummVM's RGB565 overlay in memory,
// turns them into the gray frame (layout.h: 960x600 or 640x400) on updateScreen(), and hands only the
// pixels that really changed to the platform (PLAN.md section 3).
// The mouse cursor is not drawn: input is touch, so it would only cost refreshes.
class EinkGraphicsManager : public GraphicsManager {
public:
	explicit EinkGraphicsManager(Platform *platform);
	~EinkGraphicsManager() override;

	bool hasFeature(OSystem::Feature f) const override;
	void setFeatureState(OSystem::Feature f, bool enable) override {}
	bool getFeatureState(OSystem::Feature f) const override { return false; }

#ifdef USE_RGB_COLOR
	Graphics::PixelFormat getScreenFormat() const override { return Graphics::PixelFormat::createFormatCLUT8(); }
	Common::List<Graphics::PixelFormat> getSupportedFormats() const override;
#endif
	void initSize(uint width, uint height, const Graphics::PixelFormat *format = nullptr) override;
	int getScreenChangeID() const override { return _screenChangeID; }
	void beginGFXTransaction() override;
	OSystem::TransactionError endGFXTransaction() override;
	int16 getHeight() const override { return _screen.h; }
	int16 getWidth() const override { return _screen.w; }

	void setPalette(const byte *colors, uint start, uint num) override;
	void grabPalette(byte *colors, uint start, uint num) const override;

	void copyRectToScreen(const void *buf, int pitch, int x, int y, int w, int h) override;
	Graphics::Surface *lockScreen() override;
	void unlockScreen() override;
	void fillScreen(uint32 col) override;
	void fillScreen(const Common::Rect &r, uint32 col) override;
	void updateScreen() override;
	void setShakePos(int shakeXOffset, int shakeYOffset) override;
	void setFocusRectangle(const Common::Rect &rect) override {}
	void clearFocusRectangle() override {}

	void showOverlay(bool inGUI) override;
	void hideOverlay() override;
	bool isOverlayVisible() const override { return _overlayVisible; }
	Graphics::PixelFormat getOverlayFormat() const override;
	void clearOverlay() override;
	void grabOverlay(Graphics::Surface &surface) const override;
	void copyRectToOverlay(const void *buf, int pitch, int x, int y, int w, int h) override;
	// The ScummVM GUI is laid out for 640x400 and scaled up to the frame, so
	// its text is 1.5x larger in landscape instead of the same size on a wider page.
	float getHiDPIScreenFactor() const override { return _scale == 3 ? 3.0f : 1.0f; }
	int16 getOverlayHeight() const override { return _fh; }
	int16 getOverlayWidth() const override { return _fw; }

	bool showMouse(bool visible) override;
	void warpMouse(int x, int y) override;
	void setMouseCursor(const void *buf, uint w, uint h, int hotspotX, int hotspotY, uint32 keycolor,
	                    const Graphics::PixelFormat *format, const byte *mask, frac_t scaleX, frac_t scaleY) override {}
	void setCursorPalette(const byte *colors, uint start, uint num) override {}

	// Panel point -> game (or overlay) point; false if outside the frame.
	bool panelToScreen(int px, int py, int &x, int &y) const;

	const PaletteLut &lut() const { return _lut; }
	// Force the next updateScreen() to resend the whole frame (resume, etc.).
	void invalidate() { _forceFull = true; }

private:
	void compose(uint8_t *out) const;

	Platform *_platform;
	Graphics::Surface _screen;   // game, CLUT8
	const int _fw, _fh, _scale;  // frame size and game scale (layout())
	Graphics::Surface _overlay;  // ScummVM GUI, RGB565, _fw x _fh
	byte _palette[256 * 3];
	PaletteLut _lut;
	uint8_t *_frame;             // composed gray frame, _fw x _fh
	uint8_t *_shown;             // what the platform was last given
	bool _overlayVisible = false;
	bool _forceFull = true;
	bool _mouseVisible = false;
	int _shakeY = 0;
	int _screenChangeID = 0;
	uint _pendingW = kGameW, _pendingH = kGameH;
	bool _pendingFormatOk = true;
};

} // namespace PocketBook

#endif
