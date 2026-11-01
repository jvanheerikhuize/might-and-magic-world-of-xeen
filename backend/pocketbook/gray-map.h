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

// Colour -> e-ink gray. Pure C++ (no ScummVM headers) so it can be unit-tested
// on its own. Reference implementation: tools/eink-preview.py ("paper" mode),
// chosen in Phase 0 because plain luma loses Xeen's coloured value text on the
// tan parchment (PLAN.md section 3).

#ifndef BACKENDS_PLATFORM_POCKETBOOK_GRAY_MAP_H
#define BACKENDS_PLATFORM_POCKETBOOK_GRAY_MAP_H

#include <stdint.h>

namespace PocketBook {

enum { kGrayLevels = 16, kBayerCells = 16 };

struct GrayMapParams {
	float lBlack = 12.0f;     // CIE L* that maps to black
	float lWhite = 72.0f;     // CIE L* that maps to white (Xeen parchment is ~69)
	float gamma = 1.0f;
	float k = 0.9f;           // darkening per unit of a*b* distance from the paper colour
	float chromaLo = 45.0f;   // chroma below this is never darkened
	float chromaHi = 75.0f;   // chroma above this is fully darkened
	uint8_t paper[3] = {214, 154, 123};  // dominant UI panel colour (Xeen parchment)
	bool dither = true;       // 4x4 Bayer at game resolution
};

// One colour -> gray in 0..1, "paper" mapping.
float mapColor(const GrayMapParams &p, uint8_t r, uint8_t g, uint8_t b);

// Quantise 0..1 to one of 16 panel levels (0, 17, ..., 255) for Bayer cell 0..15.
uint8_t quantize(float v, int bayerCell, bool dither);

// Bayer cell index for a game pixel: (y & 3) * 4 + (x & 3).
inline int bayerCell(int x, int y) { return ((y & 3) << 2) | (x & 3); }

// 256-colour palette -> per-colour, per-Bayer-cell panel byte.
class PaletteLut {
public:
	PaletteLut();
	void setParams(const GrayMapParams &p) { _params = p; }
	const GrayMapParams &params() const { return _params; }
	// colors: num RGB triplets for palette entries start..start+num-1
	void update(const uint8_t *colors, unsigned start, unsigned num);
	uint8_t lookup(uint8_t index, int cell) const { return _lut[index][cell]; }
	// The same colour by lightness alone (no darkening of saturated colours).
	uint8_t lookupPlain(uint8_t index, int cell) const { return _plain[index][cell]; }
	// A colour the paper mapping darkens a lot (saturated text such as Xeen's
	// green "on" or orange headings): readable on parchment, lost on dark ground.
	bool conflict(uint8_t index) const { return _conflict[index]; }
	// Undithered gray 0..255 of a colour: paper mapping and plain lightness.
	uint8_t value(uint8_t index) const { return _value[index]; }
	uint8_t plainValue(uint8_t index) const { return _plainValue[index]; }

private:
	GrayMapParams _params;
	uint8_t _lut[256][kBayerCells];
	uint8_t _plain[256][kBayerCells];
	uint8_t _value[256], _plainValue[256];
	bool _conflict[256];
};

// One game pixel -> panel byte, looking at its surroundings: a conflict
// colour gets whichever of its two grays (paper or plain) stands out more
// from the average gray of the other pixels in the 5x5 around it. Green text
// on parchment stays dark; green on Xeen's blue toggle boxes and orange
// headings on the dark marble of the credits and character creation become
// light. src: w x h palette indices, pitch bytes.
uint8_t mapPixel(const PaletteLut &lut, const uint8_t *src, int pitch, int w, int h, int x, int y, int cell);

// RGB565 (ScummVM overlay) -> panel byte: luma with gamma 1.6, 16 levels,
// no dither. The gamma darkens the mid-gray buttons of the light theme
// (scummmodern) so their white labels stand out; black and white stay put.
uint8_t gray565(uint16_t c);

// Integer rectangle, half-open [left, right) x [top, bottom).
struct Box {
	int left = 0, top = 0, right = 0, bottom = 0;
	bool empty() const { return right <= left || bottom <= top; }
	void extend(const Box &o);
	void clip(int w, int h);
};

// Smallest box covering every byte that differs between a and b (both w*h,
// pitch w). Returns an empty box if they are identical.
Box diffBox(const uint8_t *a, const uint8_t *b, int w, int h);

} // namespace PocketBook

#endif
