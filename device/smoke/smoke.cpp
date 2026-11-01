// Phase 1 smoke test for the PocketBook Touch Lux 5 (PB628).
//
// Proves, on the real device, what the ScummVM InkView backend depends on:
//   1. C++11 runtime (statically linked libstdc++): threads, mutex/condvar,
//      atomics, exceptions, RTTI, containers, static constructors, chrono.
//   2. Engine-thread handoff: a worker thread renders 320x200 8-bit frames;
//      the InkView main thread picks them up on a timer and draws them.
//      InkView itself is only ever called from the main thread.
//   3. Drawing an 8-bit gray frame at 2x where the game will sit (portrait).
//   4. Refresh timings: Partial / Dynamic / Full on the game rect and full screen.
//   5. Input: touches and keys, logged.
//
// Everything is written to smoke.log next to the binary.
// Controls: tap the game area = next frame (timed tap-to-panel latency),
//           NEXT/PREV = change refresh mode, BACK or tap "Exit" = quit.

#include <inkview.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

const int kGameW = 320, kGameH = 200, kScale = 2;
const int kTop = 24;  // game drawn at (centred x, kTop), as in PLAN.md section 4

using Clock = std::chrono::steady_clock;

FILE *g_log = nullptr;
std::string g_dir = ".";
ifont *g_font = nullptr;
int g_sw = 0, g_sh = 0, g_gx = 0;

void logf(const char *fmt, ...) {
	static const Clock::time_point t0 = Clock::now();
	long ms = (long)std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count();
	va_list ap;
	va_start(ap, fmt);
	char buf[512];
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	if (g_log) {
		fprintf(g_log, "%7ld %s\n", ms, buf);
		fflush(g_log);
	}
}

// ---- 1. C++ runtime checks --------------------------------------------------

struct StaticInit {
	int value;
	StaticInit() : value(42) {}
};
StaticInit g_static;  // must be constructed before main

struct Base {
	virtual ~Base() {}
	virtual int id() const { return 1; }
};
struct Derived : Base {
	int id() const override { return 2; }
};

std::vector<std::string> g_results;

void check(const char *name, bool ok) {
	char line[128];
	snprintf(line, sizeof line, "%s %s", ok ? "PASS" : "FAIL", name);
	g_results.push_back(line);
	logf("runtime %s", line);
}

void runtime_checks() {
	check("static constructor", g_static.value == 42);

	bool caught = false;
	try {
		throw std::runtime_error("boom");
	} catch (const std::exception &e) {
		caught = std::strcmp(e.what(), "boom") == 0;
	}
	check("exceptions", caught);

	std::unique_ptr<Base> b(new Derived);
	check("RTTI dynamic_cast", dynamic_cast<Derived *>(b.get()) != nullptr && b->id() == 2);

	std::map<std::string, std::vector<int>> m;
	for (int i = 0; i < 1000; i++) m[std::to_string(i % 10)].push_back(i);
	check("containers + to_string", m.size() == 10 && m["3"].size() == 100);

	std::atomic<int> counter(0);
	std::mutex mu;
	int guarded = 0;
	std::vector<std::thread> ts;
	for (int t = 0; t < 4; t++)
		ts.emplace_back([&] {
			for (int i = 0; i < 10000; i++) {
				counter++;
				std::lock_guard<std::mutex> lk(mu);
				guarded++;
			}
		});
	for (auto &t : ts) t.join();
	check("threads + atomics + mutex", counter == 40000 && guarded == 40000);

	auto a = Clock::now();
	std::this_thread::sleep_for(std::chrono::milliseconds(20));
	long d = (long)std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - a).count();
	check("chrono + sleep_for", d >= 19 && d < 200);
}

// ---- 2. Worker thread rendering frames ---------------------------------------

struct FrameQueue {
	std::mutex mu;
	std::condition_variable cv;
	bool want = false, quit = false, ready = false;
	int requested = 0;
	Clock::time_point requested_at;
	std::vector<unsigned char> frame = std::vector<unsigned char>(kGameW * kGameH);
	int frame_no = 0;
};
FrameQueue g_q;
std::thread g_worker;

// A stand-in for an engine frame: 16-level ramp, Bayer-dithered gradient,
// a moving "corridor" block so successive frames differ like stepping forward.
void render(std::vector<unsigned char> &f, int n) {
	static const int bayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
	for (int y = 0; y < kGameH; y++)
		for (int x = 0; x < kGameW; x++) {
			int v;
			if (y < 40)
				v = (x * 16 / kGameW) * 17;  // 16 flat gray steps
			else if (y < 80) {
				int lvl = x * 15 * 16 / kGameW + bayer[y & 3][x & 3];  // dithered gradient
				v = (lvl / 16) * 17;
			} else {
				int cx = kGameW / 2, inset = (n * 12) % 100;
				bool wall = x > 20 + inset && x < kGameW - 20 - inset && y > 90 + inset / 3 && y < kGameH - 10 - inset / 3;
				v = wall ? 0x22 : (((x / 8) ^ (y / 8)) & 1 ? 0xcc : 0xee);
				if (std::abs(x - cx) < 2) v = 0;
			}
			f[y * kGameW + x] = (unsigned char)v;
		}
}

void worker_main() {
	logf("worker thread started");
	std::vector<unsigned char> local(kGameW * kGameH);
	for (;;) {
		int n;
		{
			std::unique_lock<std::mutex> lk(g_q.mu);
			g_q.cv.wait(lk, [] { return g_q.want || g_q.quit; });
			if (g_q.quit) break;
			g_q.want = false;
			n = g_q.requested;
		}
		render(local, n);
		std::lock_guard<std::mutex> lk(g_q.mu);
		g_q.frame.swap(local);
		g_q.frame_no = n;
		g_q.ready = true;
	}
	logf("worker thread exiting");
}

void request_frame() {
	std::lock_guard<std::mutex> lk(g_q.mu);
	g_q.requested++;
	g_q.requested_at = Clock::now();
	g_q.want = true;
	g_q.cv.notify_one();
}

// ---- 3/4. Drawing and refresh ------------------------------------------------

enum Mode { kPartial, kDynamic, kFull, kModes };
const char *kModeName[kModes] = {"Partial", "Dynamic", "Full"};
int g_mode = kPartial;

long update(int mode, int x, int y, int w, int h) {
	auto a = Clock::now();
	if (mode == kPartial)
		PartialUpdate(x, y, w, h);
	else if (mode == kDynamic)
		DynamicUpdate(x, y, w, h);
	else
		FullUpdate();
	WaitForUpdateComplete();
	return (long)std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - a).count();
}

void blit(const std::vector<unsigned char> &f) {
	// Nearest-neighbour 2x ourselves so pixels stay sharp, then a 1:1 Stretch.
	static std::vector<unsigned char> big(kGameW * kScale * kGameH * kScale);
	const int bw = kGameW * kScale;
	for (int y = 0; y < kGameH; y++) {
		unsigned char *row = &big[(y * kScale) * bw];
		for (int x = 0; x < kGameW; x++) row[x * 2] = row[x * 2 + 1] = f[y * kGameW + x];
		std::memcpy(row + bw, row, bw);
	}
	Stretch(big.data(), IMAGE_GRAY8, bw, kGameH * kScale, bw, g_gx, kTop, bw, kGameH * kScale, ROTATE0);
}

void draw_text(int x, int y, const std::string &s) {
	SetFont(g_font, BLACK);
	DrawString(x, y, s.c_str());
}

const int kInfoY = kTop + kGameH * kScale + 16;
const int kExitW = 160, kExitH = 70;

void draw_chrome() {
	FillArea(0, kInfoY, g_sw, g_sh - kInfoY, WHITE);
	int y = kInfoY;
	char line[160];
	snprintf(line, sizeof line, "mmIV smoke test  screen %dx%d  mode: %s", g_sw, g_sh, kModeName[g_mode]);
	draw_text(16, y, line);
	y += 30;
	for (auto &r : g_results) {
		draw_text(16, y, r);
		y += 26;
	}
	draw_text(16, y + 10, "Tap game = next frame   NEXT/PREV = mode   BACK = quit");
	DrawRect(g_sw - kExitW - 16, g_sh - kExitH - 16, kExitW, kExitH, BLACK);
	draw_text(g_sw - kExitW + 30, g_sh - kExitH + 6, "Exit");
}

void benchmark() {
	std::vector<unsigned char> f(kGameW * kGameH);
	const int gw = kGameW * kScale, gh = kGameH * kScale;
	for (int mode = 0; mode < kModes; mode++) {
		long total = 0, worst = 0;
		const int runs = mode == kFull ? 3 : 5;
		for (int i = 0; i < runs; i++) {
			render(f, i + 1);
			auto a = Clock::now();
			blit(f);
			long draw = (long)std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - a).count();
			long ms = update(mode, g_gx, kTop, gw, gh);
			total += ms;
			if (ms > worst) worst = ms;
			if (i == 0) logf("bench blit 640x400: %ld us", draw);
		}
		logf("bench %-7s game rect: avg %ld ms, worst %ld ms (%d runs)", kModeName[mode], total / runs, worst, runs);
		char line[128];
		snprintf(line, sizeof line, "%s update: avg %ld ms", kModeName[mode], total / runs);
		g_results.push_back(line);
	}
	long full_screen = update(kPartial, 0, 0, g_sw, g_sh);
	logf("bench Partial full screen: %ld ms", full_screen);
}

// ---- 5. Event handling -------------------------------------------------------

void poll_frames() {
	bool have = false;
	int n = 0;
	Clock::time_point at;
	std::vector<unsigned char> f;
	{
		std::lock_guard<std::mutex> lk(g_q.mu);
		if (g_q.ready) {
			g_q.ready = false;
			f = g_q.frame;
			n = g_q.frame_no;
			at = g_q.requested_at;
			have = true;
		}
	}
	if (have) {
		blit(f);
		update(g_mode, g_gx, kTop, kGameW * kScale, kGameH * kScale);
		long ms = (long)std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - at).count();
		logf("frame %d via worker, %s: tap-to-panel %ld ms", n, kModeName[g_mode], ms);
	}
	SetHardTimer("mmiv_poll", poll_frames, 20);
}

void quit() {
	{
		std::lock_guard<std::mutex> lk(g_q.mu);
		g_q.quit = true;
		g_q.cv.notify_one();
	}
	if (g_worker.joinable()) g_worker.join();
	logf("exit");
	CloseApp();
}

int handler(int type, int p1, int p2) {
	switch (type) {
	case EVT_INIT: {
		g_sw = ScreenWidth();
		g_sh = ScreenHeight();
		g_gx = (g_sw - kGameW * kScale) / 2;
		logf("EVT_INIT screen %dx%d orientation %d", g_sw, g_sh, GetOrientation());
		g_font = OpenFont(DEFAULTFONT, 20, 1);
		ClearScreen();
		runtime_checks();
		g_worker = std::thread(worker_main);
		FullUpdate();
		benchmark();
		draw_chrome();
		std::vector<unsigned char> f(kGameW * kGameH);
		render(f, 0);
		blit(f);
		FullUpdate();
		WaitForUpdateComplete();
		SetHardTimer("mmiv_poll", poll_frames, 20);
		logf("ready");
		break;
	}
	case EVT_SHOW:
		logf("EVT_SHOW");
		break;
	case EVT_FOREGROUND:
		logf("EVT_FOREGROUND");
		FullUpdate();
		break;
	case EVT_BACKGROUND:
		logf("EVT_BACKGROUND");
		break;
	case EVT_POINTERDOWN:
		logf("touch down %d,%d", p1, p2);
		if (p1 >= g_sw - kExitW - 16 && p2 >= g_sh - kExitH - 16)
			quit();
		else if (p2 >= kTop && p2 < kTop + kGameH * kScale)
			request_frame();
		break;
	case EVT_POINTERUP:
		logf("touch up   %d,%d", p1, p2);
		break;
	case EVT_KEYPRESS:
		logf("key press %d", p1);
		if (p1 == IV_KEY_BACK) {
			quit();
		} else if (p1 == IV_KEY_NEXT || p1 == IV_KEY_PREV) {
			g_mode = (g_mode + (p1 == IV_KEY_NEXT ? 1 : kModes - 1)) % kModes;
			draw_chrome();
			PartialUpdate(0, kInfoY, g_sw, g_sh - kInfoY);
			logf("mode -> %s", kModeName[g_mode]);
		}
		break;
	case EVT_KEYRELEASE:
		logf("key release %d", p1);
		break;
	case EVT_EXIT:
		logf("EVT_EXIT");
		break;
	}
	return 0;
}

}  // namespace

int main(int argc, char **argv) {
	if (argc > 0) {
		g_dir = argv[0];
		size_t slash = g_dir.rfind('/');
		g_dir = slash == std::string::npos ? "." : g_dir.substr(0, slash);
	}
	g_log = fopen((g_dir + "/smoke.log").c_str(), "a");
	logf("---- mmIV smoke test start, built " __DATE__ " " __TIME__ ", gcc " __VERSION__);
	InkViewMain(handler);
	return 0;
}
