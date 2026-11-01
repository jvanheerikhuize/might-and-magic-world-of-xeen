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

#ifndef BACKENDS_PLATFORM_POCKETBOOK_POCKETBOOK_H
#define BACKENDS_PLATFORM_POCKETBOOK_POCKETBOOK_H

#include "backends/modular-backend.h"
#include "common/events.h"
#include "common/queue.h"

#include "pb-platform.h"

#include <sys/time.h>

namespace PocketBook {

class EinkGraphicsManager;

class OSystem_PocketBook : public ModularMixerBackend, public ModularGraphicsBackend, Common::EventSource {
public:
	explicit OSystem_PocketBook(Platform *platform);
	~OSystem_PocketBook() override;

	void initBackend() override;
	bool pollEvent(Common::Event &event) override;

	Common::MutexInternal *createMutex() override;
	uint32 getMillis(bool skipRecord = false) override;
	void delayMillis(uint msecs) override;
	void getTimeAndDate(TimeDate &td, bool skipRecord = false) const override;
	void quit() override;
	void logMessage(LogMessageType::Type type, const char *message) override;
	void addSysArchivesToSearchSet(Common::SearchSet &s, int priority) override;

protected:
	Common::Path getDefaultConfigFileName() override;

private:
	void translate(const Input &in);
	void pressKeypad(int px, int py);
	bool trySave(const char *reason, bool logRefusal);
	void handleSaves();
	EinkGraphicsManager *eink();

	// Safety save every 2 minutes: a save takes ~35 ms on the reader, and the
	// first landscape session lost ~3 minutes to a refused suspend save.
	enum { kRetryMs = 500, kSafetySaveMs = 2 * 60 * 1000 };

	Platform *_platform;
	timeval _startTime;
	Common::Queue<Common::Event> _pending;
	Common::Point _mouse;
	bool _suspended = false, _suspendSaved = false, _saving = false;
	uint32 _lastSaveTry = 0, _lastSafetySave = 0, _safetySaveMs = kSafetySaveMs;
};

} // namespace PocketBook

#endif
