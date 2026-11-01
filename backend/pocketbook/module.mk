MODULE := backends/platform/pocketbook

MODULE_OBJS := \
	gray-map.o \
	layout.o \
	refresh-policy.o \
	keypad.o \
	eink-graphics.o \
	pocketbook.o

ifdef POCKETBOOK_HOST_STUB
MODULE_OBJS += platform-host.o
else
MODULE_OBJS += platform-inkview.o pbagent/pbagent.o
endif

# We don't use rules.mk but rather manually update OBJS and MODULE_DIRS.
MODULE_OBJS := $(addprefix $(MODULE)/, $(MODULE_OBJS))

# libpbagent (pbtk's remote-control agent, vendored in pbagent/) is C. The
# configure step sets no CC, so use the C compiler next to CXX and the same
# ABI flags as tools/build-device.sh.
$(MODULE)/pbagent/pbagent.o: CC := $(subst g++,gcc,$(CXX))
$(MODULE)/pbagent/pbagent.o: CFLAGS := -O2 -std=gnu11 -fsigned-char -march=armv7-a -mfpu=neon -mfloat-abi=softfp
OBJS := $(MODULE_OBJS) $(OBJS)
MODULE_DIRS += $(sort $(dir $(MODULE_OBJS)))

# No audio on the reader: we use the null mixer, which backends/module.mk only
# builds for the SDL and null backends or with the event recorder enabled.
ifndef ENABLE_EVENTRECORDER
OBJS += backends/mixer/null/null-mixer.o
MODULE_DIRS += backends/mixer/null/
endif
