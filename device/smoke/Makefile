# Phase 1 C++ smoke test for the PB628.
#   make                 build ../../build/smoke/mmiv-smoke (needs the SDK)
#   make install         copy to the mounted reader (PB628 internal storage)
# SDK: the SDK-B288 tree (branch 6.5 of pocketbook/SDK_6.3.0), as in inkquest.
# Its compiler needs the SDK's own host libs on LD_LIBRARY_PATH (compiler only).
SDK ?= $(HOME)/opt/pocketbook-sdk/SDK_6.3.0-6.5/SDK-B288
CROSS ?= $(SDK)/usr/bin/arm-obreey-linux-gnueabi-
CCENV := LD_LIBRARY_PATH=$(SDK)/usr/lib
CXX := $(CCENV) $(CROSS)g++
READER ?= /run/media/$(USER)/PB628

OUT := ../../build/smoke
BIN := $(OUT)/mmiv-smoke

# Vendor flags (soft-float ABI: the firmware has only the soft-float loader).
CXXFLAGS := -std=c++11 -O2 -Wall -Wextra -fsigned-char -march=armv7-a -mfpu=neon -mfloat-abi=softfp
# libstdc++/libgcc static: the firmware's C++ runtime version is not ours to rely on.
# glibc and InkView stay dynamic (the device's glibc 2.23 and libinkview.so).
LDFLAGS := -static-libstdc++ -static-libgcc -pthread
LDLIBS := -linkview

all: $(BIN)

$(BIN): smoke.cpp
	@mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS) $(LDLIBS)
	$(CROSS)strip $@ 2>/dev/null || $(CCENV) $(CROSS)strip $@

install: $(BIN)
	@test -d $(READER)/applications || { echo "reader not mounted at $(READER)"; exit 1; }
	mkdir -p $(READER)/applications/mmiv-smoke
	cp $(BIN) $(READER)/applications/mmiv-smoke/
	cp mmiv-smoke.app $(READER)/applications/
	sync

clean:
	rm -f $(BIN)

.PHONY: all install clean
