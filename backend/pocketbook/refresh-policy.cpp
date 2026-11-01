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

#include "refresh-policy.h"

namespace PocketBook {

int RefreshPolicy::areaPct(const Box &b) const {
	if (b.empty())
		return 0;
	long a = (long)(b.right - b.left) * (b.bottom - b.top) * 100;
	long total = (long)_w * _h;
	return (int)((a + total - 1) / total);  // round up: any change is >= 1%
}

void RefreshPolicy::onFrame(const Box &dirty, long now) {
	if (dirty.empty())
		return;
	if (_pending.empty() && !_full)
		_firstAt = now;
	_pending.extend(dirty);
	_lastAt = now;
}

void RefreshPolicy::forceFull(long now) {
	if (_pending.empty() && !_full)
		_firstAt = now;
	_full = true;
	_lastAt = now - _p.settleMs;  // no need to wait for anything to settle
}

RefreshOp RefreshPolicy::poll(long now) {
	RefreshOp op;
	if (_pending.empty() && !_full)
		return op;
	// Responsive: the change began shortly after the player's last input.
	bool responsive = _full || _firstAt - _inputAt <= _p.inputWindowMs;
	bool quiet = now - _lastAt >= _p.settleMs;
	bool overdue = now - _firstAt >= _p.maxWaitMs;
	if (!quiet && !overdue)
		return op;
	if (!responsive && now - _lastPanel < _p.ambientIntervalMs)
		return op;  // animation: keep accumulating
	int area = areaPct(_pending);
	if (!_full && area < _p.smallAreaPct && now - _lastPanel < _p.smallIntervalMs)
		return op;  // keep accumulating small animation changes

	bool flash = _full || _ghost + area >= _p.ghostHardPct ||
	             (responsive && (area >= _p.fullAreaPct || _ghost + area >= _p.ghostBudgetPct));
	if (flash) {
		op.kind = RefreshOp::kFull;
		op.box.right = _w;
		op.box.bottom = _h;
		_ghost = 0;
	} else {
		op.kind = RefreshOp::kPartial;
		op.box = _pending;
		_ghost += area;
	}
	_pending = Box();
	_full = false;
	_lastPanel = now;
	return op;
}

} // namespace PocketBook
