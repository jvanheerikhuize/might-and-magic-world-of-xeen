// Unit tests for the backend's pure parts (no ScummVM needed):
// gray-map (palette -> gray, dither, diff box) and refresh-policy.
// Run: make -C tests/unit

#include "gray-map.h"
#include "keypad.h"
#include "layout.h"
#include "pb-platform.h"
#include "refresh-policy.h"

#include <stdio.h>
#include <string.h>

using namespace PocketBook;

static int g_fail = 0, g_pass = 0;

#define CHECK(cond)                                                          \
	do {                                                                     \
		if (cond)                                                            \
			g_pass++;                                                        \
		else {                                                               \
			g_fail++;                                                        \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
		}                                                                    \
	} while (0)

static int level(uint8_t r, uint8_t g, uint8_t b) {
	GrayMapParams p;
	return quantize(mapColor(p, r, g, b), 0, false) / 17;
}

static void testPaperMapping() {
	// Expected levels (0 = black, 15 = white) come from the Phase 0 reference,
	// tools/eink-preview.py --mode paper, on colours sampled from Xeen.
	CHECK(level(214, 154, 123) == 14);  // parchment -> near white
	CHECK(level(107, 239, 82) == 0);    // green value text -> black
	CHECK(level(57, 60, 57) == 3);      // dark stone
	CHECK(level(148, 93, 49) == 8);     // brown border
	CHECK(level(99, 158, 222) == 13);   // sky blue keeps its lightness
	CHECK(level(82, 82, 255) == 0);     // blue text -> black
	CHECK(level(255, 255, 82) == 7);    // yellow -> mid gray
	CHECK(level(214, 0, 0) == 0);       // red -> black
	CHECK(level(255, 255, 255) == 15);  // white
	// The point of the mapping: green text vs parchment, far apart.
	CHECK(level(214, 154, 123) - level(107, 239, 82) >= 12);
}

static void testQuantizeAndDither() {
	CHECK(quantize(0.0f, 0, false) == 0);
	CHECK(quantize(1.0f, 0, false) == 255);
	for (int c = 0; c < kBayerCells; c++) {
		uint8_t v = quantize(0.5f, c, true);
		CHECK(v % 17 == 0);  // always one of the 16 panel levels
	}
	// A mid value dithers between two adjacent levels across the 4x4 cell.
	int lo = 255, hi = 0;
	for (int c = 0; c < kBayerCells; c++) {
		int v = quantize(7.5f / 15, c, true);
		lo = v < lo ? v : lo;
		hi = v > hi ? v : hi;
	}
	CHECK(lo == 7 * 17 && hi == 8 * 17);
	CHECK(bayerCell(5, 6) == ((6 & 3) << 2 | (5 & 3)));
}

static void testLut() {
	PaletteLut lut;
	GrayMapParams p;
	p.dither = false;
	lut.setParams(p);
	uint8_t pal[6] = {214, 154, 123, 107, 239, 82};
	lut.update(pal, 10, 2);
	CHECK(lut.lookup(10, 0) == 14 * 17);
	CHECK(lut.lookup(11, 5) == 0);
	CHECK(lut.lookup(9, 0) == 0);  // untouched entries stay black
}

static void testGray565() {
	CHECK(gray565(0x0000) == 0);
	CHECK(gray565(0xffff) == 255);
	CHECK(gray565(0x7bef) % 17 == 0);  // mid gray lands on a panel level
	CHECK(gray565(0x7bef) <= 85);       // and is darkened: white labels on it stand out
}

static void testDiffBox() {
	const int w = 16, h = 8;
	uint8_t a[w * h], b[w * h];
	memset(a, 1, sizeof a);
	memcpy(b, a, sizeof a);
	CHECK(diffBox(a, b, w, h).empty());
	b[2 * w + 3] = 9;
	b[5 * w + 12] = 9;
	Box d = diffBox(a, b, w, h);
	CHECK(d.left == 3 && d.top == 2 && d.right == 13 && d.bottom == 6);
	Box e;
	e.extend(d);
	CHECK(e.left == 3 && e.right == 13);
	Box c;
	c.left = -4;
	c.top = -1;
	c.right = 40;
	c.bottom = 3;
	c.clip(w, h);
	CHECK(c.left == 0 && c.top == 0 && c.right == w && c.bottom == 3);
}

static Box box(int l, int t, int r, int b) {
	Box x;
	x.left = l;
	x.top = t;
	x.right = r;
	x.bottom = b;
	return x;
}

static void testRefreshSettle() {
	RefreshPolicy p(640, 400);
	CHECK(p.poll(0).kind == RefreshOp::kNone);
	p.onInput(990);
	p.onFrame(box(0, 0, 320, 200), 1000);           // 25%
	CHECK(p.poll(1050).kind == RefreshOp::kNone);   // not settled yet
	RefreshOp op = p.poll(1080);                     // 80 ms quiet
	CHECK(op.kind == RefreshOp::kPartial);
	CHECK(op.box.right == 320 && op.box.bottom == 200);
	CHECK(p.poll(1100).kind == RefreshOp::kNone);   // nothing pending
}

static void testRefreshMaxWait() {
	// A fade after a tap: new frames every 16 ms never settle; maxWait forces an update.
	RefreshPolicy p(640, 400);
	long t = 0;
	p.onInput(0);
	RefreshOp op;
	for (; t < 1000 && op.kind == RefreshOp::kNone; t += 16) {
		p.onFrame(box(0, 0, 640, 400), t);
		op = p.poll(t);
	}
	CHECK(op.kind == RefreshOp::kFull);  // 100% area -> full
	CHECK(t >= 400 && t < 450);
}

static void testRefreshSmallThrottle() {
	// An idle sprite: a few pixels every frame -> at most one update per second.
	RefreshPolicy p(640, 400);
	int ops = 0;
	for (long t = 0; t < 5000; t += 20) {
		p.onFrame(box(282, 120, 288, 124), t);
		if (p.poll(t).kind != RefreshOp::kNone)
			ops++;
	}
	CHECK(ops >= 4 && ops <= 6);
	CHECK(p.ghostPct() <= 6);  // and it never triggers a full refresh by itself
}

static void testRefreshFullRules() {
	RefreshParams rp;
	RefreshPolicy p(640, 400, rp);
	long t = 0;
	// Turning: ~55% of the frame -> partial, no flash.
	p.onInput(t);
	p.onFrame(box(18, 22, 516, 332), t);
	CHECK(p.poll(t += 100).kind == RefreshOp::kPartial);
	// Screen switch: 91% -> full.
	p.onInput(t += 1000);
	p.onFrame(box(0, 0, 640, 366), t);
	CHECK(p.poll(t += 100).kind == RefreshOp::kFull);
	CHECK(p.ghostPct() == 0);
	// Ghost budget: partials add up until a full refresh clears them.
	int partials = 0;
	RefreshOp op;
	do {
		p.onInput(t += 1000);
		p.onFrame(box(0, 0, 640, 200), t);  // 50%
		op = p.poll(t += 100);
		if (op.kind == RefreshOp::kPartial)
			partials++;
	} while (op.kind != RefreshOp::kFull && partials < 100);
	CHECK(op.kind == RefreshOp::kFull);
	CHECK(partials == rp.ghostBudgetPct / 50 - 1);
	// Resume from background: full refresh without waiting.
	p.forceFull(t += 5000);
	CHECK(p.poll(t).kind == RefreshOp::kFull);
}

static const KeypadButton *byLabel(const char *label) {
	int n;
	const KeypadButton *bs = keypadButtons(n);
	for (int i = 0; i < n; i++)
		if (!strcmp(bs[i].label, label))
			return &bs[i];
	return nullptr;
}

// Every button inside the panel, below the frame, finger-sized, hit at its
// centre, and no two overlapping.
static void checkButtons(int expected) {
	const Layout &l = layout();
	int n;
	const KeypadButton *bs = keypadButtons(n);
	CHECK(n == expected);
	CHECK(keypadTop() >= l.frameY + l.frameH);
	for (int i = 0; i < n; i++) {
		const KeypadButton &b = bs[i];
		CHECK(b.x >= 0 && b.y >= keypadTop() && b.x + b.w <= l.panelW && b.y + b.h <= l.panelH);
		CHECK(b.w >= 60 && b.h >= 60);  // >= ~7 mm at 212 ppi: finger-sized
		CHECK(keypadHit(b.x + b.w / 2, b.y + b.h / 2) == &b);
		for (int j = i + 1; j < n; j++) {
			const KeypadButton &o = bs[j];
			bool overlap = b.x < o.x + o.w && o.x < b.x + b.w && b.y < o.y + o.h && o.y < b.y + b.h;
			CHECK(!overlap);
		}
	}
	CHECK(keypadHit(l.frameX + 10, l.frameY + 10) == nullptr);  // game area is not keypad
}

static void testLayouts() {
	CHECK(layoutModeFromName(nullptr) == Layout::kLandscape);
	CHECK(layoutModeFromName("") == Layout::kLandscape);
	CHECK(layoutModeFromName("bogus") == Layout::kLandscape);
	CHECK(layoutModeFromName("portrait") == Layout::kPortrait);
	CHECK(layoutModeFromName("landscape-flip") == Layout::kLandscapeFlip);
	const Layout::Mode modes[] = {Layout::kLandscape, Layout::kLandscapeFlip, Layout::kPortrait};
	for (Layout::Mode m : modes) {
		setLayout(m);
		const Layout &l = layout();
		CHECK(l.mode == m);
		CHECK(l.frameW == kGameW * l.scale && l.frameH == kGameH * l.scale);
		CHECK(l.frameX >= 0 && l.frameY >= 0);
		CHECK(l.frameX + l.frameW <= l.panelW && l.frameY + l.frameH <= l.panelH);
		CHECK(l.frameW <= kMaxFrameW && l.frameH <= kMaxFrameH);
		CHECK(l.landscape() == (l.panelW > l.panelH));
	}
	// Rotation for the portrait panel: every logical pixel of a rectangle
	// lands inside its panel rectangle, touches map back, and the frame is
	// centred on the physical panel too.
	for (Layout::Mode m : modes) {
		setLayout(m);
		const Layout &l = layout();
		CHECK(l.physW() == 758 && l.physH() == 1024);
		const PanelRect r = {l.frameX + 5, l.frameY + 7, 40, 30};
		PanelRect p = toPanel(l, r);
		CHECK(p.w * p.h == r.w * r.h && p.x >= 0 && p.y >= 0 && p.x + p.w <= l.physW() && p.y + p.h <= l.physH());
		bool inside = true, back = true;
		for (int y = r.y; y < r.y + r.h; y++)
			for (int x = r.x; x < r.x + r.w; x++) {
				int off = panelOffset(l, x, y, l.physW());
				int px = off % l.physW(), py = off / l.physW();
				inside &= px >= p.x && px < p.x + p.w && py >= p.y && py < p.y + p.h;
				int lx, ly;
				fromPanel(l, px, py, lx, ly);
				back &= lx == x && ly == y;
			}
		CHECK(inside && back);
		PanelRect f = toPanel(l, {l.frameX, l.frameY, l.frameW, l.frameH});
		CHECK(f.x == l.physW() - f.x - f.w || f.x + 1 == l.physW() - f.x - f.w);
		CHECK(f.y == l.physH() - f.y - f.h || f.y + 1 == l.physH() - f.y - f.h || l.mode == Layout::kPortrait);
	}
	// Turned counter-clockwise: the game's top-left is the panel's top-right.
	setLayout(Layout::kLandscape);
	int ptx, pty;
	fromPanel(layout(), 757, 0, ptx, pty);
	CHECK(ptx == 0 && pty == 0);
	CHECK(layout().scale == 3 && layout().orientation == 0 && layout().rotate == 1 && layout().keyboardOrientation == 1);
	CHECK(layout().scale * 25 >= 75);  // Xeen's 25x21 icons: >= 75 px (~9 mm) wide
}

static void testKeypad() {
	setLayout(Layout::kPortrait);
	checkButtons(36);
	// Key values must equal ScummVM's (common/keyboard.h, checked against v2026.4.0).
	CHECK(byLabel("Fwd")->keycode == 273);      // KEYCODE_UP
	CHECK(byLabel("Turn L")->keycode == 276);   // KEYCODE_LEFT
	CHECK(byLabel("Side L")->keycode == 276 && byLabel("Side L")->flags == 1);  // Ctrl+Left (KBD_CTRL)
	CHECK(byLabel("Side R")->keycode == 275 && byLabel("Side R")->flags == 1);
	CHECK(byLabel("1")->keycode == 282 && byLabel("1")->ascii == 315);  // F1 / ASCII_F1
	CHECK(byLabel("Panel")->keycode == 9);      // Tab: Xeen control panel
	CHECK(byLabel("Esc")->keycode == 27);
	CHECK(byLabel("Abc")->action == KeypadButton::kTextEntry);
	CHECK(byLabel("ScummVM")->action == KeypadButton::kMainMenu);
	CHECK(keypadTop() == 440);  // unchanged from Phase 3 (tests/host/keypad-and-suspend.txt)

	setLayout(Layout::kLandscape);
	checkButtons(0);  // nothing but the game
}

static void testRefreshAmbient() {
	// Title screen / battle animation with nobody touching the reader: big
	// changes every 50 ms. At most one refresh per ambientIntervalMs, all
	// partial until the hard ghost backstop.
	RefreshParams rp;
	RefreshPolicy p(960, 600, rp);
	int partials = 0, fulls = 0;
	long firstFull = -1;
	for (long t = 0; t < 60000; t += 50) {
		p.onFrame(box(0, 0, 960, 600), t);  // 100%, would flash if responsive
		RefreshOp op = p.poll(t);
		if (op.kind == RefreshOp::kPartial)
			partials++;
		if (op.kind == RefreshOp::kFull) {
			fulls++;
			if (firstFull < 0)
				firstFull = t;
		}
	}
	CHECK(partials + fulls <= 60000 / rp.ambientIntervalMs + 1);
	CHECK(fulls <= 60000 / rp.ambientIntervalMs / (rp.ghostHardPct / 100) + 1);  // backstop only
	CHECK(firstFull >= rp.ambientIntervalMs * (rp.ghostHardPct / 100 - 1));
	// A tap during the animation: the next change is shown quickly, and may flash.
	RefreshPolicy q(960, 600, rp);
	q.onFrame(box(0, 0, 960, 600), 0);
	CHECK(q.poll(500).kind == RefreshOp::kPartial);   // ambient, first one is allowed
	q.onInput(600);
	q.onFrame(box(0, 0, 960, 600), 650);
	RefreshOp op = q.poll(750);                        // settled, < ambientInterval later
	CHECK(op.kind == RefreshOp::kFull);
	// Small ambient change long after input: partial, throttled.
	q.onFrame(box(10, 10, 20, 20), 10000);
	CHECK(q.poll(10100).kind == RefreshOp::kPartial);
	q.onFrame(box(10, 10, 20, 20), 10200);
	CHECK(q.poll(10300).kind == RefreshOp::kNone);
	CHECK(q.poll(10100 + rp.ambientIntervalMs).kind == RefreshOp::kPartial);
}

static void testRefreshAmbientAnyClock() {
	// The reader's clock is wall-clock ms in a 32-bit long: it can start at any
	// value, negative included. Idle animation before the first input must be
	// throttled the same way wherever the clock starts (2026-11-01 device test:
	// ~240 partials/min of a 2% monster animation right after each start).
	RefreshParams rp;
	const long bases[] = {0, 1000, -2000000000L, -5000, 2000000000L - 120000};
	for (long base : bases) {
		RefreshPolicy p(960, 600, rp);
		int refreshes = 0;
		for (long t = 0; t < 60000; t += 250) {
			p.onFrame(box(400, 200, 520, 300), base + t);  // a ~2% sprite, every 250 ms
			if (p.poll(base + t + 100).kind != RefreshOp::kNone)
				refreshes++;
		}
		CHECK(refreshes <= 60000 / rp.ambientIntervalMs + 1);
		CHECK(refreshes >= 60000 / rp.ambientIntervalMs / 2);  // still shown, just slower
	}
}

static void testContextMapping() {
	// Palette: 0 parchment, 1 Xeen green text, 2 Xeen toggle blue, 3 black,
	// 4 credits orange, 5 credits marble teal.
	const uint8_t pal[] = {214, 154, 123, 104, 236, 80, 20, 36, 208, 0, 0, 0, 252, 152, 44, 0, 76, 68};
	PaletteLut lut;
	lut.update(pal, 0, 6);
	CHECK(!lut.conflict(0) && lut.conflict(1) && !lut.conflict(2) && !lut.conflict(3));
	CHECK(lut.conflict(4) && !lut.conflict(5));
	// 12x12: left half parchment, right half blue; a green 2x2 "letter" in each.
	uint8_t img[12 * 12];
	for (int y = 0; y < 12; y++)
		for (int x = 0; x < 12; x++)
			img[y * 12 + x] = x < 6 ? 0 : 2;
	for (int y = 5; y < 7; y++)
		for (int x = 2; x < 10; x += (x == 3 ? 5 : 1))
			img[y * 12 + x] = 1;
	int onParchment = 0, onBlue = 0, blue = 0, parchment = 0;
	for (int cell = 0; cell < 16; cell++) {
		onParchment += mapPixel(lut, img, 12, 12, 12, 2, 5, cell);
		onBlue += mapPixel(lut, img, 12, 12, 12, 9, 6, cell);
		blue += mapPixel(lut, img, 12, 12, 12, 10, 1, cell);
		parchment += mapPixel(lut, img, 12, 12, 12, 0, 0, cell);
	}
	CHECK(onParchment / 16 < 100 && parchment / 16 > 200);  // dark text on light paper (Phase 0)
	CHECK(onBlue / 16 > 200 && blue / 16 < 60);             // light text on the dark box
	// Orange heading on mid-dark teal marble (credits, character creation).
	uint8_t marble[12 * 12];
	for (int i = 0; i < 12 * 12; i++)
		marble[i] = (i % 3 == 0) ? 3 : 5;  // teal with black flecks
	for (int x = 4; x < 8; x++)
		marble[6 * 12 + x] = 4;
	int orange = 0;
	for (int cell = 0; cell < 16; cell++)
		orange += mapPixel(lut, marble, 12, 12, 12, 5, 6, cell);
	CHECK(orange / 16 > 200);  // light on the dark marble
}

int main() {
	testLayouts();
	testKeypad();
	testPaperMapping();
	testQuantizeAndDither();
	testLut();
	testGray565();
	testContextMapping();
	testDiffBox();
	testRefreshSettle();
	testRefreshMaxWait();
	testRefreshSmallThrottle();
	testRefreshFullRules();
	testRefreshAmbient();
	testRefreshAmbientAnyClock();
	printf("%d passed, %d failed\n", g_pass, g_fail);
	return g_fail ? 1 : 0;
}
