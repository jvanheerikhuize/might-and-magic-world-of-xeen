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
	p.onFrame(box(0, 0, 320, 200), 1000);           // 25%
	CHECK(p.poll(1050).kind == RefreshOp::kNone);   // not settled yet
	RefreshOp op = p.poll(1080);                     // 80 ms quiet
	CHECK(op.kind == RefreshOp::kPartial);
	CHECK(op.box.right == 320 && op.box.bottom == 200);
	CHECK(p.poll(1100).kind == RefreshOp::kNone);   // nothing pending
}

static void testRefreshMaxWait() {
	// A fade: new frames every 16 ms never settle; maxWait forces an update.
	RefreshPolicy p(640, 400);
	long t = 0;
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
	p.onFrame(box(18, 22, 516, 332), t);
	CHECK(p.poll(t += 100).kind == RefreshOp::kPartial);
	// Screen switch: 91% -> full.
	p.onFrame(box(0, 0, 640, 366), t += 1000);
	CHECK(p.poll(t += 100).kind == RefreshOp::kFull);
	CHECK(p.ghostPct() == 0);
	// Ghost budget: partials add up until a full refresh clears them.
	int partials = 0;
	RefreshOp op;
	do {
		p.onFrame(box(0, 0, 640, 200), t += 1000);  // 50%
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
	setLayout(Layout::kLandscape);
	CHECK(layout().scale == 3 && layout().orientation == 1);
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
	checkButtons(7);
	CHECK(byLabel("Fwd") == nullptr);  // movement is on Xeen's own arrows
	CHECK(byLabel("Esc")->keycode == 27 && byLabel("Enter")->keycode == 13);
	CHECK(byLabel("Wait")->keycode == 32 && byLabel("Yes")->ascii == 'y' && byLabel("No")->ascii == 'n');
	CHECK(byLabel("Keyboard")->action == KeypadButton::kTextEntry);
	CHECK(byLabel("ScummVM")->action == KeypadButton::kMainMenu);
	CHECK(byLabel("Esc")->h >= 120);  // the whole strip height
}

int main() {
	testLayouts();
	testKeypad();
	testPaperMapping();
	testQuantizeAndDither();
	testLut();
	testGray565();
	testDiffBox();
	testRefreshSettle();
	testRefreshMaxWait();
	testRefreshSmallThrottle();
	testRefreshFullRules();
	printf("%d passed, %d failed\n", g_pass, g_fail);
	return g_fail ? 1 : 0;
}
