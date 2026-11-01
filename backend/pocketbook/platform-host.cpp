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

// Host stub platform: runs the PocketBook backend on the PC without a display.
// Input comes from a script, output frames go to PGM files and a present log,
// so behaviour can be checked headless and compared against golden files.
//
// Environment:
//   MMIV_SCRIPT   script file (required for input; without it the game idles)
//   MMIV_OUT      output directory for shots and present.log (default ".")
//   MMIV_APPDIR   directory with mm.dat (default: the binary's directory)
//   MMIV_TIMEOUT  hard stop in ms of wall time (default 120000)
//   MMIV_LAYOUT   landscape (default), landscape-flip or portrait (layout.h)
//
// Script lines (# comments allowed):
//   wait MS            let the engine run for MS milliseconds
//   tap X Y            touch down+up at panel coordinates
//   tapg X Y           touch at game coordinates (320x200; centre of the scaled pixel)
//   key NAME           key down+up: up down left right esc enter space tab
//                      backspace f1..f12 or a single character
//   shot NAME          write OUT/NAME.pgm (the gray frame as last presented: 960x600
//                      in landscape, 640x400 in portrait)
//   suspend / resume   app sent to / back from the background (suspend saves to slot 99)
//   quit               send EVENT_QUIT
// Taps below the game frame hit the touch buttons (keypad.cpp), as on the device.

#define FORBIDDEN_SYMBOL_ALLOW_ALL

#include "pocketbook.h"
#include "refresh-policy.h"

#include "base/main.h"
#include "common/keyboard.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <deque>
#include <string>
#include <vector>

namespace PocketBook {

namespace {

long wallMs() {
	timeval t;
	gettimeofday(&t, nullptr);
	return t.tv_sec * 1000L + t.tv_usec / 1000;
}

bool keyByName(const std::string &n, int &code, uint16_t &ascii) {
	struct Named { const char *name; Common::KeyCode code; uint16_t ascii; };
	static const Named names[] = {
		{"up", Common::KEYCODE_UP, 0}, {"down", Common::KEYCODE_DOWN, 0},
		{"left", Common::KEYCODE_LEFT, 0}, {"right", Common::KEYCODE_RIGHT, 0},
		{"esc", Common::KEYCODE_ESCAPE, 27}, {"enter", Common::KEYCODE_RETURN, 13},
		{"space", Common::KEYCODE_SPACE, ' '}, {"tab", Common::KEYCODE_TAB, 9},
		{"backspace", Common::KEYCODE_BACKSPACE, 8},
	};
	for (const Named &k : names)
		if (n == k.name) {
			code = k.code;
			ascii = k.ascii;
			return true;
		}
	if (n.size() >= 2 && n[0] == 'f' && atoi(n.c_str() + 1) >= 1 && atoi(n.c_str() + 1) <= 12) {
		int f = atoi(n.c_str() + 1);
		code = Common::KEYCODE_F1 + f - 1;
		ascii = Common::ASCII_F1 + f - 1;
		return true;
	}
	if (n.size() == 1) {
		char c = n[0];
		if (c >= 'A' && c <= 'Z')
			c = c - 'A' + 'a';
		code = c;  // KeyCode values equal ASCII for printable characters
		ascii = (uint16_t)n[0];
		return true;
	}
	return false;
}

class HostPlatform : public Platform {
public:
	HostPlatform(const char *argv0) {
		const char *a = getenv("MMIV_APPDIR");
		if (a) {
			_appDir = a;
		} else {
			_appDir = argv0;
			size_t s = _appDir.rfind('/');
			_appDir = s == std::string::npos ? "." : _appDir.substr(0, s);
		}
		const char *o = getenv("MMIV_OUT");
		_out = o ? o : ".";
		const char *t = getenv("MMIV_TIMEOUT");
		_deadline = wallMs() + (t ? atol(t) : 120000L);
		_presentLog = fopen((_out + "/present.log").c_str(), "w");
		const char *s = getenv("MMIV_SCRIPT");
		if (s)
			loadScript(s);
	}

	~HostPlatform() override {
		if (_presentLog) {
			fprintf(_presentLog, "summary presents %d panel_partial %d panel_full %d\n", _presents, _partials, _fulls);
			fclose(_presentLog);
		}
	}

	void present(const uint8_t *frame, const Box &dirty) override {
		memcpy(_frame.data(), frame, _frame.size());
		_presents++;
		long now = wallMs();
		_policy.onFrame(dirty, now);
		if (_presentLog) {
			fprintf(_presentLog, "%6ld present %d dirty %d,%d-%d,%d area %d%%\n", now - _start, _presents,
			        dirty.left, dirty.top, dirty.right, dirty.bottom, _policy.areaPct(dirty));
			fflush(_presentLog);
		}
	}

	// What the device's poll timer would do now (same policy, same clock).
	void panelTick() {
		long now = wallMs();
		RefreshOp op = _policy.poll(now);
		if (op.kind == RefreshOp::kNone || !_presentLog)
			return;
		if (op.kind == RefreshOp::kFull) {
			_fulls++;
			fprintf(_presentLog, "%6ld panel FULL\n", now - _start);
		} else {
			_partials++;
			fprintf(_presentLog, "%6ld panel partial %d,%d-%d,%d area %d%% ghost %d%%\n", now - _start, op.box.left,
			        op.box.top, op.box.right, op.box.bottom, _policy.areaPct(op.box), _policy.ghostPct());
		}
		fflush(_presentLog);
	}

	bool pollInput(Input &in) override {
		panelTick();
		if (!_queued.empty()) {
			in = _queued.front();
			_queued.pop_front();
			return true;
		}
		long now = wallMs();
		if (now > _deadline) {
			fprintf(stderr, "mmiv-host: timeout, quitting\n");
			in.type = Input::kQuit;
			_deadline = now + 1000000;
			return true;
		}
		while (now >= _due && _line < _script.size())
			run(_script[_line++], now);
		if (!_queued.empty())
			return pollInput(in);
		return false;
	}

	const char *appDir() const override { return _appDir.c_str(); }

	void log(const char *message) override {
		fputs(message, stderr);
	}

	void engineFinished() override {
		delete this;  // writes the summary line
		exit(0);
	}

private:
	void loadScript(const char *path) {
		FILE *f = fopen(path, "r");
		if (!f) {
			fprintf(stderr, "mmiv-host: cannot open script %s\n", path);
			exit(2);
		}
		char buf[256];
		while (fgets(buf, sizeof buf, f)) {
			char *hash = strchr(buf, '#');
			if (hash)
				*hash = 0;
			std::string s(buf);
			while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '\t'))
				s.pop_back();
			size_t b = s.find_first_not_of(" \t");
			if (b != std::string::npos)
				_script.push_back(s.substr(b));
		}
		fclose(f);
	}

	void touch(int x, int y) {
		Input d;
		d.type = Input::kTouchDown;
		d.x = x;
		d.y = y;
		Input u = d;
		u.type = Input::kTouchUp;
		_queued.push_back(d);
		_queued.push_back(u);
	}

	void run(const std::string &cmd, long now) {
		char name[64] = {0};
		int a = 0, b = 0;
		if (sscanf(cmd.c_str(), "wait %d", &a) == 1) {
			_due = now + a;
		} else if (sscanf(cmd.c_str(), "tapg %d %d", &a, &b) == 2) {
			const Layout &l = layout();
			touch(l.frameX + a * l.scale + l.scale / 2, l.frameY + b * l.scale + l.scale / 2);
		} else if (sscanf(cmd.c_str(), "tap %d %d", &a, &b) == 2) {
			touch(a, b);
		} else if (sscanf(cmd.c_str(), "key %63s", name) == 1) {
			Input k;
			uint16_t ascii = 0;
			if (!keyByName(name, k.keycode, ascii)) {
				fprintf(stderr, "mmiv-host: unknown key '%s'\n", name);
				return;
			}
			k.ascii = ascii;
			k.type = Input::kKeyDown;
			_queued.push_back(k);
			k.type = Input::kKeyUp;
			_queued.push_back(k);
		} else if (sscanf(cmd.c_str(), "shot %63s", name) == 1) {
			writePgm(name);
		} else if (cmd == "quit" || cmd == "suspend" || cmd == "resume") {
			Input q;
			q.type = cmd == "quit" ? Input::kQuit : cmd == "suspend" ? Input::kSuspend : Input::kResume;
			_queued.push_back(q);
			if (q.type == Input::kResume)
				_policy.forceFull(now);
		} else {
			fprintf(stderr, "mmiv-host: bad script line '%s'\n", cmd.c_str());
		}
	}

	void writePgm(const char *name) {
		std::string path = _out + "/" + name + ".pgm";
		FILE *f = fopen(path.c_str(), "wb");
		if (!f)
			return;
		fprintf(f, "P5\n%d %d\n255\n", layout().frameW, layout().frameH);
		fwrite(_frame.data(), 1, _frame.size(), f);
		fclose(f);
		if (_presentLog) {
			fprintf(_presentLog, "shot %s after %d presents\n", name, _presents);
			fflush(_presentLog);
		}
	}

	std::string _appDir, _out;
	std::vector<std::string> _script;
	size_t _line = 0;
	long _due = 0, _deadline = 0;
	std::deque<Input> _queued;
	std::vector<uint8_t> _frame = std::vector<uint8_t>(layout().frameW * layout().frameH, 0xff);
	int _presents = 0, _partials = 0, _fulls = 0;
	long _start = wallMs();
	RefreshPolicy _policy{layout().frameW, layout().frameH};
	FILE *_presentLog = nullptr;
};

HostPlatform *g_platform = nullptr;

} // namespace

Platform *platform() {
	return g_platform;
}

} // namespace PocketBook

int main(int argc, char *argv[]) {
	PocketBook::setLayout(PocketBook::layoutModeFromName(getenv("MMIV_LAYOUT")));
	PocketBook::g_platform = new PocketBook::HostPlatform(argv[0]);
	g_system = new PocketBook::OSystem_PocketBook(PocketBook::g_platform);
	int res = scummvm_main(argc, argv);
	g_system->destroy();
	delete PocketBook::g_platform;
	return res;
}
