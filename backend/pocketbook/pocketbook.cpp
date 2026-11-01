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

#include "pocketbook.h"
#include "eink-graphics.h"
#include "keypad.h"

#include "backends/events/default/default-events.h"
#include "backends/fs/posix/posix-fs-factory.h"
#include "backends/mixer/null/null-mixer.h"
#include "backends/mutex/null/null-mutex.h"
#include "backends/saves/default/default-saves.h"
#include "backends/timer/default/default-timer.h"
#include "common/fs.h"
#include "common/keyboard.h"
#include "engines/engine.h"

#include <time.h>
#include <unistd.h>

namespace PocketBook {

OSystem_PocketBook::OSystem_PocketBook(Platform *platform) : _platform(platform) {
	_fsFactory = new POSIXFilesystemFactory();
	gettimeofday(&_startTime, nullptr);
}

OSystem_PocketBook::~OSystem_PocketBook() {
}

void OSystem_PocketBook::initBackend() {
	_timerManager = new DefaultTimerManager();
	_eventManager = new DefaultEventManager(this);
	_savefileManager = new DefaultSaveFileManager();
	_graphicsManager = new EinkGraphicsManager(_platform);
	_mixerManager = new NullMixerManager();  // no audio on the reader
	_mixerManager->init();
	BaseBackend::initBackend();
}

EinkGraphicsManager *OSystem_PocketBook::eink() {
	return static_cast<EinkGraphicsManager *>(_graphicsManager);
}

void OSystem_PocketBook::translate(const Input &in) {
	Common::Event ev;
	switch (in.type) {
	case Input::kTouchDown:
	case Input::kTouchUp: {
		int x, y;
		if (!eink()->panelToScreen(in.x, in.y, x, y)) {
			if (in.type == Input::kTouchDown)
				pressKeypad(in.x, in.y);
			return;
		}
		_mouse = Common::Point(x, y);
		ev.mouse = _mouse;
		if (in.type == Input::kTouchDown) {
			// Move first so hover-driven UI sees the position, then press.
			ev.type = Common::EVENT_MOUSEMOVE;
			_pending.push(ev);
			ev.type = Common::EVENT_LBUTTONDOWN;
		} else {
			ev.type = Common::EVENT_LBUTTONUP;
		}
		_pending.push(ev);
		return;
	}
	case Input::kKeyDown:
	case Input::kKeyUp:
		ev.type = in.type == Input::kKeyDown ? Common::EVENT_KEYDOWN : Common::EVENT_KEYUP;
		ev.kbd = Common::KeyState((Common::KeyCode)in.keycode, in.ascii, in.flags);
		ev.mouse = _mouse;
		_pending.push(ev);
		return;
	case Input::kQuit:
		ev.type = Common::EVENT_QUIT;
		_pending.push(ev);
		return;
	case Input::kResume:
		eink()->invalidate();
		return;
	case Input::kSuspend:
		saveOnSuspend();
		return;
	case Input::kNone:
		return;
	}
}

void OSystem_PocketBook::pressKeypad(int px, int py) {
	const KeypadButton *b = keypadHit(px, py);
	if (!b)
		return;
	Common::Event ev;
	ev.mouse = _mouse;
	switch (b->action) {
	case KeypadButton::kKey:
		ev.kbd = Common::KeyState((Common::KeyCode)b->keycode, b->ascii, b->flags);
		ev.type = Common::EVENT_KEYDOWN;
		_pending.push(ev);
		ev.type = Common::EVENT_KEYUP;
		_pending.push(ev);
		break;
	case KeypadButton::kMainMenu:
		ev.type = Common::EVENT_MAINMENU;
		_pending.push(ev);
		break;
	case KeypadButton::kTextEntry:
		break;  // the platform opens its on-screen keyboard and sends the text as keys
	}
}

void OSystem_PocketBook::saveOnSuspend() {
	if (!g_engine) {
		logMessage(LogMessageType::kInfo, "suspend: no engine running, nothing to save\n");
		return;
	}
	Common::U32String why;
	if (!g_engine->canSaveGameStateCurrently(&why)) {
		logMessage(LogMessageType::kInfo, "suspend: game cannot be saved right now (combat, event or no-save area)\n");
		return;
	}
	Common::Error err = g_engine->saveGameState(kSuspendSaveSlot, "Suspended", false);
	logMessage(LogMessageType::kInfo, err.getCode() == Common::kNoError
	                                      ? "suspend: saved to slot 99\n"
	                                      : "suspend: saving to slot 99 FAILED\n");
}

bool OSystem_PocketBook::pollEvent(Common::Event &event) {
	((DefaultTimerManager *)getTimerManager())->checkTimers();
	((NullMixerManager *)_mixerManager)->update(1);

	Input in;
	while (_pending.empty() && _platform->pollInput(in))
		translate(in);
	if (_pending.empty())
		return false;
	event = _pending.pop();
	return true;
}

Common::MutexInternal *OSystem_PocketBook::createMutex() {
	// ScummVM runs single-threaded on the engine thread (timers are driven
	// from pollEvent); cross-thread hand-off is inside the platform layer.
	return new NullMutexInternal();
}

uint32 OSystem_PocketBook::getMillis(bool skipRecord) {
	timeval now;
	gettimeofday(&now, nullptr);
	return (uint32)((now.tv_sec - _startTime.tv_sec) * 1000 + (now.tv_usec - _startTime.tv_usec) / 1000);
}

void OSystem_PocketBook::delayMillis(uint msecs) {
	usleep(msecs * 1000);
}

void OSystem_PocketBook::getTimeAndDate(TimeDate &td, bool skipRecord) const {
	time_t now = time(nullptr);
	struct tm t;
	localtime_r(&now, &t);
	td.tm_sec = t.tm_sec;
	td.tm_min = t.tm_min;
	td.tm_hour = t.tm_hour;
	td.tm_mday = t.tm_mday;
	td.tm_mon = t.tm_mon;
	td.tm_year = t.tm_year;
	td.tm_wday = t.tm_wday;
}

void OSystem_PocketBook::quit() {
	_platform->engineFinished();
}

void OSystem_PocketBook::logMessage(LogMessageType::Type type, const char *message) {
	_platform->log(message);
}

void OSystem_PocketBook::addSysArchivesToSearchSet(Common::SearchSet &s, int priority) {
	// mm.dat ships next to the binary.
	Common::FSNode dir{Common::Path(_platform->appDir(), '/')};
	if (dir.exists())
		s.add("appdir", new Common::FSDirectory(dir, 1), priority);
}

Common::Path OSystem_PocketBook::getDefaultConfigFileName() {
	return Common::Path(_platform->appDir(), '/').appendComponent("scummvm.ini");
}

} // namespace PocketBook
