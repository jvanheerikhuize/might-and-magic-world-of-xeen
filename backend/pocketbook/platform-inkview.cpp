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
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define FORBIDDEN_SYMBOL_ALLOW_ALL

#include "keypad.h"
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
		// Append across launches (suspend/resume spans two); start over past 256 KB.
		std::string logPath = _appDir + "/scummvm.log";
		_log = fopen(logPath.c_str(), "a");
		if (_log && ftell(_log) > 256 * 1024) {
			fclose(_log);
			_log = fopen(logPath.c_str(), "w");
		}
		time_t now = time(nullptr);
		char stamp[32];
		strftime(stamp, sizeof stamp, "%Y-%m-%d %H:%M:%S", localtime(&now));
		logf("==== mmiv start %s ====\n", stamp);
		for (int i = 0; i < argc; i++) {
			_args.push_back(argv[i]);
			logf("arg %d: %s\n", i, argv[i]);
		}
		// The PB628 has no speaker, jack or Bluetooth; record what the system has.
		FILE *cards = fopen("/proc/asound/cards", "r");
		char card[128] = "";
		if (cards) {
			if (!fgets(card, sizeof card, cards))
				card[0] = 0;
			fclose(cards);
		}
		logf("audio: /dev/snd %s, /proc/asound/cards: %s\n", access("/dev/snd", F_OK) == 0 ? "present" : "absent",
		     card[0] ? card : "(none)\n");
		const Layout &l = layout();
		logf("layout %s: %dx%d frame at %d,%d (%dx), orientation %d, rotate %d\n", l.name, l.frameW, l.frameH,
		     l.frameX, l.frameY, l.scale, l.orientation, l.rotate);
	}

	// ---- engine thread ----

	void present(const uint8_t *frame, const Box &dirty) override {
		std::lock_guard<std::mutex> lk(_mu);
		const int w = layout().frameW;
		for (int y = dirty.top; y < dirty.bottom; y++)
			memcpy(&_frame[y * w + dirty.left], frame + y * w + dirty.left, dirty.right - dirty.left);
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

	// Identical consecutive messages are collapsed into one "repeated" line
	// (Xeen's sprite-override check warns 18 times per area load).
	void log(const char *message) override {
		std::lock_guard<std::mutex> lk(_logMu);
		if (!_log)
			return;
		if (_lastLog == message) {
			_repeats++;
			return;
		}
		if (_repeats)
			fprintf(_log, "  (last message repeated %d more times)\n", _repeats);
		_repeats = 0;
		_lastLog = message;
		fputs(message, _log);
		fflush(_log);
	}

	void engineFinished() override {
		std::lock_guard<std::mutex> lk(_mu);
		_engineDone = true;
	}

	// Engine thread; the keyboard is opened by poll() on the main thread.
	void textInput(bool on) override {
		std::lock_guard<std::mutex> lk(_mu);
		_wantKeyboard = on;
		_wantKeyboardAt = nowMs();
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
		if (in.type == Input::kTouchDown || in.type == Input::kKeyDown || in.type == Input::kMainMenu)
			_policy.onInput(nowMs());
	}

	void pushKey(int code, uint16_t ascii) {
		Input in;
		in.keycode = code;
		in.ascii = ascii;
		in.type = Input::kKeyDown;
		push(in);
		in.type = Input::kKeyUp;
		push(in);
	}

	// MENU: a short press is Escape, holding it opens the ScummVM menu.
	void menuKey(int type) {
		if (type == EVT_KEYPRESS) {
			_menuDownAt = nowMs();
			_menuDone = false;
		} else if (!_menuDone && (type == EVT_KEYREPEAT || nowMs() - _menuDownAt >= kLongPressMs)) {
			_menuDone = true;
			Input in;
			in.type = Input::kMainMenu;
			push(in);
			logf("MENU held: ScummVM menu\n");
		} else if (type == EVT_KEYRELEASE && !_menuDone) {
			_menuDone = true;
			pushKey(Common::KEYCODE_ESCAPE, 27);
		}
	}

	void poll() {
		const Layout &l = layout();
		RefreshOp op;
		bool done, keyboard;
		{
			std::lock_guard<std::mutex> lk(_mu);
			keyboard = _wantKeyboard && !_keyboardOpen && nowMs() - _wantKeyboardAt >= kKeyboardDelayMs;
			op = _policy.poll(nowMs());
			done = _engineDone;
			if (op.kind != RefreshOp::kNone)
				blitFrame();
		}
		// The panel update itself (hundreds of ms) runs outside the lock.
		long t0 = nowMs();
		if (op.kind == RefreshOp::kFull) {
			FullUpdate();
			_stats.fulls++;
			_stats.fullMs += nowMs() - t0;
		} else if (op.kind == RefreshOp::kPartial) {
			PanelRect r = toPanel(l, {l.frameX + op.box.left, l.frameY + op.box.top, op.box.right - op.box.left,
			                          op.box.bottom - op.box.top});
			PartialUpdate(r.x, r.y, r.w, r.h);
			_stats.partials++;
			_stats.partialMs += nowMs() - t0;
			_stats.partialArea += _policy.areaPct(op.box);
		}
		if (nowMs() - _stats.since >= 60000)
			logStats();
		if (keyboard && !done) {
			logf("keyboard: the game asks for text\n");
			openKeyboard(true);
		}
		if (done) {
			shutdown();
			CloseApp();
			return;
		}
		SetHardTimer("mmiv_poll", pollThunk, kPollMs);
	}

	// Panel activity per minute, for tuning RefreshParams on the real panel.
	void logStats() {
		long secs = (nowMs() - _stats.since) / 1000;
		logf("panel %lds: %d partial (avg %d%% area, %ld ms/call), %d full (%ld ms/call)\n", secs, _stats.partials,
		     _stats.partials ? _stats.partialArea / _stats.partials : 0,
		     _stats.partials ? _stats.partialMs / _stats.partials : 0L, _stats.fulls,
		     _stats.fulls ? _stats.fullMs / _stats.fulls : 0L);
		_stats = PanelStats();
	}

	// Landscape is set per app; the firmware may restore its own orientation
	// when the app returns from the background or the keyboard.
	void applyOrientation() {
		const Layout &l = layout();
		// The firmware's status panel is on unless an app turns it off, and it
		// moves the picture by its height (~79 px along the canvas rows, with
		// wrap-around: both landscape photos) and keeps our black off the panel.
		SetPanelType(PANEL_DISABLED);
		if (GetOrientation() != l.orientation)
			SetOrientation(l.orientation);
		if (ScreenWidth() != l.physW() || ScreenHeight() != l.physH())
			logf("WARNING: screen %dx%d after SetOrientation(%d), layout expects %dx%d\n", ScreenWidth(),
			     ScreenHeight(), l.orientation, l.physW(), l.physH());
	}

	// Copy the gray frame into the canvas at (frameX, frameY), turned for the
	// portrait panel in the landscape layouts (layout.h). Direct when the canvas
	// is the 8-bit panel; Stretch() otherwise (no rotation then).
	void blitFrame() {
		const Layout &l = layout();
		icanvas *cv = GetCanvas();
		if (_directBlit) {
			if (!l.rotate) {
				for (int y = 0; y < l.frameH; y++)
					memcpy(cv->addr + (l.frameY + y) * cv->scanline + l.frameX, &_frame[y * l.frameW], l.frameW);
				return;
			}
			// One logical column per pass: a panel row, written in order.
			const int step = l.rotate == 1 ? -1 : 1;
			for (int x = 0; x < l.frameW; x++) {
				uint8_t *dst = cv->addr + panelOffset(l, l.frameX + x, l.frameY, cv->scanline);
				const uint8_t *src = &_frame[x];
				for (int y = 0; y < l.frameH; y++, src += l.frameW, dst += step)
					*dst = *src;
			}
			return;
		}
		Stretch(_frame.data(), IMAGE_GRAY8, l.frameW, l.frameH, l.frameW, l.frameX, l.frameY, l.frameW, l.frameH, ROTATE0);
	}

	// Main thread, at start-up: log the canvas and choose direct copying when
	// it is the 8-bit panel. (Fourth landscape session: Stretch drew where asked;
	// the firmware's landscape mode moved the picture on the panel instead.)
	void checkCanvas() {
		const Layout &l = layout();
		icanvas *cv = GetCanvas();
		if (!cv || !cv->addr) {
			logf("canvas: none, using Stretch\n");
			return;
		}
		logf("canvas: %dx%d scanline %d depth %d clip %d,%d-%d,%d; panel height %d, fb offset %d\n", cv->width,
		     cv->height, cv->scanline, cv->depth, cv->clipx1, cv->clipy1, cv->clipx2, cv->clipy2, PanelHeight(),
		     PanelHeightFBOffset());
		_directBlit = cv->depth == 8 && cv->width == l.physW() && cv->height == l.physH();
		logf("canvas: %s\n", _directBlit ? "copying the frame directly" : "size differs from the layout, using Stretch");
	}

	// The first touches, to check on the reader that touch coordinates follow
	// the rotation (the game and the buttons depend on it).
	void logTouch(int x, int y) {
		if (_touchesLogged < 5) {
			_touchesLogged++;
			logf("touch %d,%d\n", x, y);
		}
	}

	void resumed() {
		applyOrientation();
		ClearScreen();
		drawKeypad();
		std::lock_guard<std::mutex> lk(_mu);
		_policy.forceFull(nowMs());
	}

	// ---- keypad (main thread) ----

	void drawKeypad() {
		if (layout().landscape()) {
			// Nothing but the game: black around it (the frame is drawn over this).
			// Straight into the canvas: FillArea keeps to the canvas clip.
			icanvas *cv = GetCanvas();
			if (_directBlit)
				memset(cv->addr, 0, (size_t)cv->scanline * cv->height);
			else
				FillArea(0, 0, ScreenWidth(), ScreenHeight(), BLACK);
			return;
		}
		if (!_font)
			_font = OpenFont(DEFAULTFONTB, 24, 1);
		int n;
		const KeypadButton *bs = keypadButtons(n);
		FillArea(0, keypadTop() - 8, ScreenWidth(), ScreenHeight() - keypadTop() + 8, WHITE);
		SetFont(_font, BLACK);
		for (int i = 0; i < n; i++) {
			const KeypadButton &b = bs[i];
			DrawRect(b.x, b.y, b.w, b.h, BLACK);
			DrawRect(b.x + 1, b.y + 1, b.w - 2, b.h - 2, BLACK);
			DrawTextRect(b.x + 2, b.y + 2, b.w - 4, b.h - 4, b.label, ALIGN_CENTER | VALIGN_MIDDLE);
		}
	}

	// Invert the button while it is held so the tap visibly registers.
	void keypadDown(int x, int y) {
		const KeypadButton *b = keypadHit(x, y);
		if (!b)
			return;
		_held = b;
		InvertArea(b->x + 2, b->y + 2, b->w - 4, b->h - 4);
		PartialUpdate(b->x, b->y, b->w, b->h);
		if (b->action == KeypadButton::kTextEntry)
			openKeyboard(false);
	}

	void keypadUp() {
		if (!_held)
			return;
		const KeypadButton *b = _held;
		_held = nullptr;
		InvertArea(b->x + 2, b->y + 2, b->w - 4, b->h - 4);
		PartialUpdate(b->x, b->y, b->w, b->h);
	}

	// automatic: the game asked for text (mirror, riddle, name, amount), so the
	// answer is completed with Enter, or cancelled with Escape. Otherwise (the
	// keypad's Abc button) the text is just typed.
	void openKeyboard(bool automatic) {
		_held = nullptr;  // the keyboard covers the screen; we redraw on return
		_keyboardOpen = true;
		_keyboardAuto = automatic;
		memset(_text, 0, sizeof(_text));
		// Upright for a reader held sideways; resumed() turns the panel back.
		if (layout().keyboardOrientation != layout().orientation)
			SetOrientation(layout().keyboardOrientation);
		OpenKeyboard(automatic ? "Answer" : "Text for the game", _text, sizeof(_text) - 1, 0, keyboardThunk);
	}

	// Typed text goes to the game as key presses (names, riddles, passwords).
	void keyboardDone(char *s) {
		if (s) {
			// Prompts may come with text already in the field (ScummVM's save
			// name is pre-filled): typed text replaces it, an empty answer keeps it.
			if (_keyboardAuto && *s) {
				Input begin;
				begin.type = Input::kTextBegin;  // clears a pre-filled ScummVM field
				push(begin);
			}
			for (const char *p = s; *p; p++) {
				char c = *p;
				pushKey((c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : (unsigned char)c, (unsigned char)c);
			}
			if (_keyboardAuto)
				pushKey(Common::KEYCODE_RETURN, 13);
			logf("keyboard: sent %d characters%s\n", (int)strlen(s), _keyboardAuto ? " + Enter" : "");
		} else if (_keyboardAuto) {
			pushKey(Common::KEYCODE_ESCAPE, 27);
			logf("keyboard: cancelled, sent Escape\n");
		}
		{
			std::lock_guard<std::mutex> lk(_mu);
			_keyboardOpen = false;
			_wantKeyboard = false;  // the engine asks again for the next prompt
		}
		resumed();
	}

	static void keyboardThunk(char *s);

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
	std::string _lastLog;
	int _repeats = 0;
	std::mutex _mu, _logMu;
	std::deque<Input> _input;
	std::vector<uint8_t> _frame = std::vector<uint8_t>(layout().frameW * layout().frameH, 0xff);
	RefreshPolicy _policy{layout().frameW, layout().frameH};
	int _touchesLogged = 0;
	bool _directBlit = false;
	bool _wantKeyboard = false, _keyboardOpen = false, _keyboardAuto = false;
	long _menuDownAt = 0;
	bool _menuDone = true;
	long _wantKeyboardAt = 0;
	// The keyboard covers the screen: open it a moment after the prompt so
	// the question (and the field's current text) can be read first.
	enum { kLongPressMs = 700, kKeyboardDelayMs = 1200 };
	bool _engineDone = false;
	std::thread _engine;
	ifont *_font = nullptr;
	const KeypadButton *_held = nullptr;
	char _text[64];
	struct PanelStats {
		long since = nowMs();
		int partials = 0, fulls = 0, partialArea = 0;
		long partialMs = 0, fullMs = 0;
	} _stats;
};

InkViewPlatform *g_platform = nullptr;

void InkViewPlatform::pollThunk() {
	g_platform->poll();
}

void InkViewPlatform::keyboardThunk(char *s) {
	g_platform->keyboardDone(s);
}

// Hardware buttons: PREV/NEXT step forward/back (page-turn buttons are the
// quickest way to walk); in landscape (orientation 1) the buttons sit on the
// right with PREV pointing down, so there PREV steps back and NEXT forward
// (first landscape session). BACK is Escape, MENU is Escape on a short press and
// the ScummVM menu when held (InkViewPlatform::menuKey). Everything else is on the
// touch buttons (keypad.cpp) or, in landscape, on the game's own icons.
bool mapKey(int ivKey, int &code, uint16_t &ascii) {
	switch (ivKey) {
	case IV_KEY_PREV:
	case IV_KEY_NEXT: {
		bool forward = (ivKey == IV_KEY_PREV) != (layout().mode == Layout::kLandscape);
		code = forward ? Common::KEYCODE_UP : Common::KEYCODE_DOWN;
		ascii = 0;
		return true;
	}
	case IV_KEY_BACK: code = Common::KEYCODE_ESCAPE; ascii = 27; return true;
	default: return false;
	}
}

int handler(int type, int p1, int p2) {
	InkViewPlatform *pf = g_platform;
	Input in;
	switch (type) {
	case EVT_INIT:
		pf->applyOrientation();
		pf->logf("EVT_INIT screen %dx%d orientation %d\n", ScreenWidth(), ScreenHeight(), GetOrientation());
		pf->checkCanvas();
		ClearScreen();
		pf->drawKeypad();
		FullUpdate();
		pf->startEngine();
		SetHardTimer("mmiv_poll", InkViewPlatform::pollThunk, kPollMs);
		break;
	case EVT_POINTERDOWN:
	case EVT_POINTERUP:
		in.type = type == EVT_POINTERDOWN ? Input::kTouchDown : Input::kTouchUp;
		fromPanel(layout(), p1, p2, in.x, in.y);
		if (type == EVT_POINTERDOWN) {
			pf->logTouch(in.x, in.y);
			if (keypadHit(in.x, in.y))
				pf->keypadDown(in.x, in.y);
		} else {
			pf->keypadUp();
		}
		pf->push(in);  // the OSystem turns button taps into keys (keypad.cpp)
		break;
	case EVT_KEYPRESS:
	case EVT_KEYREPEAT:
	case EVT_KEYRELEASE: {
		if (p1 == IV_KEY_MENU) {
			pf->menuKey(type);
			break;
		}
		if (type == EVT_KEYREPEAT)
			break;
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
	case EVT_ORIENTATION:
		return 1;  // keep our orientation; ignore the g-sensor
	case EVT_EXIT:
		pf->logf("EVT_EXIT\n");
		pf->logStats();
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
	PocketBook::setLayout(PocketBook::layoutModeFromName(getenv("MMIV_LAYOUT")));
	PocketBook::g_platform = new PocketBook::InkViewPlatform();
	PocketBook::g_platform->init(argc, argv);
	g_system = new PocketBook::OSystem_PocketBook(PocketBook::g_platform);
	InkViewMain(PocketBook::handler);
	return 0;
}
