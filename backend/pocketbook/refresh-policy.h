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

// When and how to refresh the e-ink panel. Pure C++ so it can be unit-tested
// and driven identically by the device platform and the host stub.
//
// Why (Phase 2 host-stub run, tests/host/smoke-from-save.txt):
//   - loading a save fades the palette in: ~30 near-full frames in a row;
//   - idle sprites change a few pixels every frame, forever.
// And from the first landscape session on the reader: the title screen,
// battles and conversations animate continuously, and refreshing them as
// fast as they change (with a full flash for every big change) flickers
// heavily. So:
//   - responsive: within inputWindowMs of a tap or key, wait until the frame
//     has been unchanged for settleMs, but never longer than maxWaitMs;
//   - ambient (animation nobody asked for): at most every ambientIntervalMs;
//   - small changes (< smallAreaPct) refresh at most every smallIntervalMs;
//   - full refreshes (the flash) only right after input: a screen switch
//     (fullAreaPct or more) or once the partials since the last full one add
//     up to ghostBudgetPct. Ambient animation never flashes, except as a
//     backstop once ghosting reaches ghostHardPct.
// Panel timings from Phase 1: Partial ~225 ms, Full ~430 ms on the game rect.

#ifndef BACKENDS_PLATFORM_POCKETBOOK_REFRESH_POLICY_H
#define BACKENDS_PLATFORM_POCKETBOOK_REFRESH_POLICY_H

#include "gray-map.h"

namespace PocketBook {

struct RefreshParams {
	long settleMs = 80;
	long maxWaitMs = 400;
	int smallAreaPct = 2;
	long smallIntervalMs = 1000;
	// Turning or stepping changes 50-60% of the frame (the 3D view); keep those
	// partial (~225 ms, no flash). Screen switches (character sheet, 91%) go full.
	int fullAreaPct = 85;
	int ghostBudgetPct = 400;
	long inputWindowMs = 1500;
	long ambientIntervalMs = 1500;
	int ghostHardPct = 1500;
};

struct RefreshOp {
	enum Kind { kNone, kPartial, kFull };
	Kind kind = kNone;
	Box box;  // frame coordinates; the whole frame for kFull
};

class RefreshPolicy {
public:
	RefreshPolicy(int frameW, int frameH, const RefreshParams &p = RefreshParams())
		: _w(frameW), _h(frameH), _p(p) {}

	// The frame changed in `dirty` at time now (ms, any monotonic clock).
	void onFrame(const Box &dirty, long now);
	// What to do on the panel now, if anything. Call periodically.
	RefreshOp poll(long now);
	// Next poll should be a full refresh (resume from background, etc.).
	void forceFull(long now);
	// The player tapped or pressed a key at time now.
	void onInput(long now) { _inputAt = now; }

	int areaPct(const Box &b) const;
	int ghostPct() const { return _ghost; }

private:
	int _w, _h;
	RefreshParams _p;
	Box _pending;
	bool _full = false;
	long _firstAt = 0, _lastAt = 0, _lastPanel = -1000000, _inputAt = -1000000;
	int _ghost = 0;
};

} // namespace PocketBook

#endif
