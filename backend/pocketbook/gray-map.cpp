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

#include "gray-map.h"

#include <math.h>
#include <string.h>

namespace PocketBook {

namespace {

const int kBayer4[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};

float srgbToLinear(uint8_t c) {
	float v = c / 255.0f;
	return v <= 0.04045f ? v / 12.92f : powf((v + 0.055f) / 1.055f, 2.4f);
}

float labF(float t) {
	return t > 0.008856f ? cbrtf(t) : 7.787f * t + 16.0f / 116.0f;
}

void toLab(uint8_t r8, uint8_t g8, uint8_t b8, float &L, float &A, float &B) {
	float r = srgbToLinear(r8), g = srgbToLinear(g8), b = srgbToLinear(b8);
	float x = (0.4124f * r + 0.3576f * g + 0.1805f * b) / 0.95047f;
	float y = 0.2126f * r + 0.7152f * g + 0.0722f * b;
	float z = (0.0193f * r + 0.1192f * g + 0.9505f * b) / 1.08883f;
	float fx = labF(x), fy = labF(y), fz = labF(z);
	L = 116.0f * fy - 16.0f;
	A = 500.0f * (fx - fy);
	B = 200.0f * (fy - fz);
}

float smoothstep(float lo, float hi, float v) {
	float t = (v - lo) / (hi - lo);
	t = t < 0 ? 0 : (t > 1 ? 1 : t);
	return t * t * (3 - 2 * t);
}

} // namespace

float mapColor(const GrayMapParams &p, uint8_t r, uint8_t g, uint8_t b) {
	float L, A, B, pL, pA, pB;
	toLab(r, g, b, L, A, B);
	toLab(p.paper[0], p.paper[1], p.paper[2], pL, pA, pB);
	float chroma = hypotf(A, B);
	float hueDist = hypotf(A - pA, B - pB);
	L -= p.k * smoothstep(p.chromaLo, p.chromaHi, chroma) * hueDist;
	float x = (L - p.lBlack) / (p.lWhite - p.lBlack);
	x = x < 0 ? 0 : (x > 1 ? 1 : x);
	return p.gamma == 1.0f ? x : powf(x, p.gamma);
}

uint8_t quantize(float v, int bayerCell, bool dither) {
	float q = v * (kGrayLevels - 1);
	if (dither)
		q += (kBayer4[bayerCell & 15] + 0.5f) / 16.0f - 0.5f;
	int level = (int)lrintf(q);  // round half to even, as Python's round()
	level = level < 0 ? 0 : (level > kGrayLevels - 1 ? kGrayLevels - 1 : level);
	return (uint8_t)(level * 255 / (kGrayLevels - 1));
}

PaletteLut::PaletteLut() {
	memset(_lut, 0, sizeof(_lut));
	memset(_plain, 0, sizeof(_plain));
	memset(_conflict, 0, sizeof(_conflict));
	memset(_value, 0, sizeof(_value));
	memset(_plainValue, 0, sizeof(_plainValue));
}

void PaletteLut::update(const uint8_t *colors, unsigned start, unsigned num) {
	GrayMapParams plainParams = _params;
	plainParams.k = 0;
	for (unsigned i = 0; i < num && start + i < 256; i++) {
		const uint8_t *c = colors + 3 * i;
		float v = mapColor(_params, c[0], c[1], c[2]);
		float plain = mapColor(plainParams, c[0], c[1], c[2]);
		for (int cell = 0; cell < kBayerCells; cell++) {
			_lut[start + i][cell] = quantize(v, cell, _params.dither);
			_plain[start + i][cell] = quantize(plain, cell, _params.dither);
		}
		_conflict[start + i] = plain - v >= 0.35f;
		_value[start + i] = (uint8_t)lrintf(v * 255);
		_plainValue[start + i] = (uint8_t)lrintf(plain * 255);
	}
}

uint8_t mapPixel(const PaletteLut &lut, const uint8_t *src, int pitch, int w, int h, int x, int y, int cell) {
	uint8_t c = src[y * pitch + x];
	if (!lut.conflict(c))
		return lut.lookup(c, cell);
	int sum = 0, other = 0;
	for (int yy = y - 2; yy <= y + 2; yy++) {
		if (yy < 0 || yy >= h)
			continue;
		const uint8_t *row = src + yy * pitch;
		for (int xx = x - 2; xx <= x + 2; xx++) {
			if (xx < 0 || xx >= w || lut.conflict(row[xx]))
				continue;
			other++;
			sum += lut.value(row[xx]);
		}
	}
	if (!other)
		return lut.lookup(c, cell);
	int ground = sum / other;
	int paper = lut.value(c), plain = lut.plainValue(c);
	int dPaper = paper > ground ? paper - ground : ground - paper;
	int dPlain = plain > ground ? plain - ground : ground - plain;
	return dPlain > dPaper ? lut.lookupPlain(c, cell) : lut.lookup(c, cell);
}

uint8_t gray565(uint16_t c) {
	int r = (c >> 11) & 0x1f, g = (c >> 5) & 0x3f, b = c & 0x1f;
	r = (r << 3) | (r >> 2);
	g = (g << 2) | (g >> 4);
	b = (b << 3) | (b >> 2);
	int luma = (299 * r + 587 * g + 114 * b) / 1000;
	return quantize(powf(luma / 255.0f, 1.6f), 0, false);
}

void Box::extend(const Box &o) {
	if (o.empty())
		return;
	if (empty()) {
		*this = o;
		return;
	}
	if (o.left < left) left = o.left;
	if (o.top < top) top = o.top;
	if (o.right > right) right = o.right;
	if (o.bottom > bottom) bottom = o.bottom;
}

void Box::clip(int w, int h) {
	if (left < 0) left = 0;
	if (top < 0) top = 0;
	if (right > w) right = w;
	if (bottom > h) bottom = h;
}

Box diffBox(const uint8_t *a, const uint8_t *b, int w, int h) {
	Box box;
	int top = -1, bottom = -1;
	for (int y = 0; y < h; y++) {
		if (memcmp(a + y * w, b + y * w, w)) {
			if (top < 0)
				top = y;
			bottom = y;
		}
	}
	if (top < 0)
		return box;
	int left = w, right = -1;
	for (int y = top; y <= bottom; y++) {
		const uint8_t *ra = a + y * w, *rb = b + y * w;
		for (int x = 0; x < left; x++)
			if (ra[x] != rb[x]) {
				left = x;
				break;
			}
		for (int x = w - 1; x > right; x--)
			if (ra[x] != rb[x]) {
				right = x;
				break;
			}
	}
	box.left = left;
	box.top = top;
	box.right = right + 1;
	box.bottom = bottom + 1;
	return box;
}

} // namespace PocketBook
