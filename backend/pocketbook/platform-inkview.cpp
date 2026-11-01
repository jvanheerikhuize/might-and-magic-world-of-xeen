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

// Device platform: InkView owns the main thread (InkViewMain); ScummVM runs on
// an engine thread. The two meet in a mutex-protected frame buffer and input
// queue; InkView is only ever called from the main thread (proven in Phase 1).
//
// When to refresh and whether partial or full is decided by RefreshPolicy
// (refresh-policy.h), shared with the host stub so it can be tested there.

#include <inkview.h>

#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#define FORBIDDEN_SYMBOL_ALLOW_ALL

#include "pocketbook.h"
#include "refresh-policy.h"

#include "base/main.h"
#include "common/keyboard.h"

namespace PocketBook {

namespace {

const int kPollMs = 20;

long nowMs() {
	timeval t;
	gettimeofday(&t, nullptr);
	return t.tv_sec * 1000L + t.tv_usec / 1000;
}

class InkViewPlatform : public Platform {
public:
	void init(int argc, char **argv) {
		_appDir = argc > 0 ? argv[0] : ".";
		size_t s = _appDir.rfind('/');
		_appDir = s == std::string::npos ? "." : _appDir.substr(0, s);
		_log = fopen((_appDir + "/scummvm.log").c_str(), "w");
		for (int i = 0; i < argc; i++)
			_args.push_back(argv[i]);
		memset(_frame, 0xff, sizeof(_frame));
	}

	// ---- engine thread ----

	void present(const uint8_t *frame, const Box &dirty) override {
		std::lock_guard<std::mutex> lk(_mu);
		for (int y = dirty.top; y < dirty.bottom; y++)
			memcpy(_frame + y * kFrameW + dirty.left, frame + y * kFrameW + dirty.left, dirty.right - dirty.left);
		_policy.onFrame(dirty, nowMs());
	}

	bool pollInput(Input &in) override {
		std::lock_guard<std::mutex> lk(_mu);
		if (_input.empty())
			return false;
		in = _input.front();
		_input.pop_front();
		return true;
	}

	const char *appDir() const override { return _appDir.c_str(); }

	void log(const char *message) override {
		std::lock_guard<std::mutex> lk(_logMu);
		if (_log) {
			fputs(message, _log);
			fflush(_log);
		}
	}

	void engineFinished() override {
		std::lock_guard<std::mutex> lk(_mu);
		_engineDone = true;
	}

	// ---- main (InkView) thread ----

	void startEngine() {
		_engine = std::thread([this] {
			std::vector<const char *> argv;
			for (auto &a : _args)
				argv.push_back(a.c_str());
			argv.push_back(nullptr);
			logf("engine thread: scummvm_main with %d args\n", (int)_args.size());
			int rc = scummvm_main((int)_args.size(), argv.data());
			logf("engine thread: scummvm_main returned %d\n", rc);
			engineFinished();
		});
	}

	void push(const Input &in) {
		std::lock_guard<std::mutex> lk(_mu);
		_input.push_back(in);
	}

	void poll() {
		RefreshOp op;
		bool done;
		{
			std::lock_guard<std::mutex> lk(_mu);
			op = _policy.poll(nowMs());
			done = _engineDone;
			if (op.kind != RefreshOp::kNone)
				Stretch(_frame, IMAGE_GRAY8, kFrameW, kFrameH, kFrameW, kFrameX, kFrameY, kFrameW, kFrameH, ROTATE0);
		}
		// The panel update itself (hundreds of ms) runs outside the lock.
		if (op.kind == RefreshOp::kFull)
			FullUpdate();
		else if (op.kind == RefreshOp::kPartial)
			PartialUpdate(kFrameX + op.box.left, kFrameY + op.box.top, op.box.right - op.box.left,
			              op.box.bottom - op.box.top);
		if (done) {
			shutdown();
			CloseApp();
			return;
		}
		SetHardTimer("mmiv_poll", pollThunk, kPollMs);
	}

	void resumed() {
		std::lock_guard<std::mutex> lk(_mu);
		_policy.forceFull(nowMs());
	}

	void shutdown() {
		if (_engine.joinable()) {
			{
				std::lock_guard<std::mutex> lk(_mu);
				if (!_engineDone) {
					Input q;
					q.type = Input::kQuit;
					_input.push_back(q);
				}
			}
			_engine.join();
		}
	}

	void logf(const char *fmt, ...) {
		char buf[512];
		va_list ap;
		va_start(ap, fmt);
		vsnprintf(buf, sizeof buf, fmt, ap);
		va_end(ap);
		log(buf);
	}

	static void pollThunk();

private:
	std::string _appDir;
	std::vector<std::string> _args;
	FILE *_log = nullptr;
	std::mutex _mu, _logMu;
	std::deque<Input> _input;
	uint8_t _frame[kFrameW * kFrameH];
	RefreshPolicy _policy{kFrameW, kFrameH};
	bool _engineDone = false;
	std::thread _engine;
};

InkViewPlatform *g_platform = nullptr;

void InkViewPlatform::pollThunk() {
	g_platform->poll();
}

// Hardware buttons until the Phase 3 keypad exists: PREV/NEXT step
// forward/back, MENU and BACK are Escape.
bool mapKey(int ivKey, int &code, uint16_t &ascii) {
	switch (ivKey) {
	case IV_KEY_PREV: code = Common::KEYCODE_UP; ascii = 0; return true;
	case IV_KEY_NEXT: code = Common::KEYCODE_DOWN; ascii = 0; return true;
	case IV_KEY_MENU:
	case IV_KEY_BACK: code = Common::KEYCODE_ESCAPE; ascii = 27; return true;
	default: return false;
	}
}

int handler(int type, int p1, int p2) {
	InkViewPlatform *pf = g_platform;
	Input in;
	switch (type) {
	case EVT_INIT:
		pf->logf("EVT_INIT screen %dx%d\n", ScreenWidth(), ScreenHeight());
		ClearScreen();
		FullUpdate();
		pf->startEngine();
		SetHardTimer("mmiv_poll", InkViewPlatform::pollThunk, kPollMs);
		break;
	case EVT_POINTERDOWN:
	case EVT_POINTERUP:
		in.type = type == EVT_POINTERDOWN ? Input::kTouchDown : Input::kTouchUp;
		in.x = p1;
		in.y = p2;
		pf->push(in);
		break;
	case EVT_KEYPRESS:
	case EVT_KEYRELEASE: {
		uint16_t ascii = 0;
		if (mapKey(p1, in.keycode, ascii)) {
			in.type = type == EVT_KEYPRESS ? Input::kKeyDown : Input::kKeyUp;
			in.ascii = ascii;
			pf->push(in);
		}
		break;
	}
	case EVT_BACKGROUND:
		pf->logf("EVT_BACKGROUND\n");
		in.type = Input::kSuspend;
		pf->push(in);
		break;
	case EVT_FOREGROUND:
		pf->logf("EVT_FOREGROUND\n");
		in.type = Input::kResume;
		pf->push(in);
		pf->resumed();
		break;
	case EVT_EXIT:
		pf->logf("EVT_EXIT\n");
		pf->shutdown();
		break;
	}
	return 0;
}

} // namespace

Platform *platform() {
	return g_platform;
}

} // namespace PocketBook

int main(int argc, char *argv[]) {
	PocketBook::g_platform = new PocketBook::InkViewPlatform();
	PocketBook::g_platform->init(argc, argv);
	g_system = new PocketBook::OSystem_PocketBook(PocketBook::g_platform);
	InkViewMain(PocketBook::handler);
	return 0;
}
