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

#include <stdlib.h>
#include <time.h>
#include <unistd.h>

namespace PocketBook {

OSystem_PocketBook::OSystem_PocketBook(Platform *platform) : _platform(platform) {
	_fsFactory = new POSIXFilesystemFactory();
	gettimeofday(&_startTime, nullptr);
	// Tests shorten the safety-save interval; the reader uses the default.
	const char *s = getenv("MMIV_SAFETY_SAVE_MS");
	_safetySaveMs = s && atoi(s) > 0 ? (uint32)atoi(s) : (uint32)kSafetySaveMs;
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
		_suspended = false;
		eink()->invalidate();
		return;
	case Input::kSuspend:
		_suspended = true;
		_suspendSaved = trySave("suspend", true);
		_lastSaveTry = getMillis();
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

// Save to slot 99 if the game allows it right now. `reason` goes to the log;
// `logRefusal` is off for the periodic safety save, which retries quietly.
bool OSystem_PocketBook::trySave(const char *reason, bool logRefusal) {
	if (_saving || !g_engine)
		return false;
	if (!g_engine->canSaveGameStateCurrently()) {
		if (logRefusal)
			logMessage(LogMessageType::kInfo,
			           Common::String::format("%s: game cannot be saved right now (combat, event or no-save area)\n",
			                                  reason).c_str());
		return false;
	}
	_saving = true;
	uint32 t0 = getMillis();
	Common::Error err = g_engine->saveGameState(kSuspendSaveSlot, "Suspended", false);
	_saving = false;
	bool ok = err.getCode() == Common::kNoError;
	logMessage(LogMessageType::kInfo, Common::String::format("%s: %s slot 99 (%u ms)\n", reason,
	                                                         ok ? "saved to" : "FAILED saving to",
	                                                         getMillis() - t0).c_str());
	return ok;
}

// Called from pollEvent. While in the background the firmware ends the app
// ~10 s after Home (Phase 1), so a refused suspend save is retried until the
// game can be saved (e.g. a dialogue ends). While playing, a rolling safety
// save every kSafetySaveMs bounds what a refused suspend save can lose.
void OSystem_PocketBook::handleSaves() {
	uint32 now = getMillis();
	if (_suspended) {
		if (!_suspendSaved && now - _lastSaveTry >= kRetryMs) {
			_lastSaveTry = now;
			_suspendSaved = trySave("suspend retry", false);
		}
	} else if (now - _lastSafetySave >= _safetySaveMs) {
		if (trySave("safety save", false))
			_lastSafetySave = now;
	}
}

bool OSystem_PocketBook::pollEvent(Common::Event &event) {
	((DefaultTimerManager *)getTimerManager())->checkTimers();
	((NullMixerManager *)_mixerManager)->update(1);
	handleSaves();

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
