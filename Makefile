# Makefile -- Out-of-tree build for the Valgrind Bridge SDK + plugins.
#
# Usage:
#   make                          # build the tool with the default plugin
#   make PLUGIN_DIR=plugins/foo   # build with a different plugin
#   sudo make install             # copy the tool binary into Valgrind's libexec
#   make test                     # build and run the test programs
#   make clean                    # remove build artifacts
#
# Requirements: gcc, valgrind-dev headers installed system-wide.
#
# This file is part of the Valgrind Bridge SDK.
# License: GPL-2.0-or-later

# ---- SDK configuration (framework — do not modify per plugin) ----

TOOL_NAME    ?= bridge

# ---- Plugin configuration (set PLUGIN_DIR to point at your plugin) ----
# A plugin is any directory containing .c files that implement bridge_plugin_t
# (see include/bridge_plugin.h) and call BRIDGE_PLUGIN_REGISTER().

PLUGIN_DIR   ?= plugins/sharedstate

# Auto-detect platform
ARCH         := $(shell uname -m)
ifeq ($(ARCH),x86_64)
  VG_ARCH     := amd64
  VG_PLATFORM := amd64-linux
  TOOL_TBASE  := 0x58000000
  ARCH_CFLAGS := -m64
else ifeq ($(ARCH),i686)
  VG_ARCH     := x86
  VG_PLATFORM := x86-linux
  TOOL_TBASE  := 0x38000000
  ARCH_CFLAGS := -m32
else
  $(error Unsupported architecture: $(ARCH))
endif

# Paths
VG_INCLUDE   := /usr/include/valgrind
VG_LIBDIR    := /usr/lib/$(ARCH)-linux-gnu/valgrind
VG_LIBEXEC   := /usr/libexec/valgrind
BUILDDIR     := build

# Static libraries provided by the valgrind package
LIB_COREGRIND := $(VG_LIBDIR)/libcoregrind-$(VG_PLATFORM).a
LIB_VEX       := $(VG_LIBDIR)/libvex-$(VG_PLATFORM).a
LIB_VEXMULTI  := $(VG_LIBDIR)/libvexmultiarch-$(VG_PLATFORM).a
LIB_GCC_SUP   := $(VG_LIBDIR)/libgcc-sup-$(VG_PLATFORM).a

# The tool binary that goes into VG_LIBEXEC
TOOL_BIN     := $(BUILDDIR)/$(TOOL_NAME)-$(VG_PLATFORM)

# ---- Compiler flags ----
# Tools must: not link libc, use -fno-stack-protector, and be position-
# dependent (loaded at a fixed address by the Valgrind launcher).

CC       := gcc
CFLAGS   := -O2 -g \
            -Wall -Wextra -Wno-unused-parameter \
            -fno-stack-protector \
            -fno-strict-aliasing \
            -fno-builtin \
            $(ARCH_CFLAGS) \
            -I$(VG_INCLUDE) \
            -Iinclude \
            -DVGA_$(VG_ARCH)=1 \
            -DVGO_linux=1 \
            -DVGP_$(subst -,_,$(VG_PLATFORM))=1 \
            -DVGPV_$(subst -,_,$(VG_PLATFORM))_vanilla=1

LDFLAGS  := -static -nodefaultlibs -nostartfiles \
            $(ARCH_CFLAGS) \
            -Wl,-Ttext=$(TOOL_TBASE) \
            -Wl,--build-id=none

# ---- SDK source files (bridge core — no plugin-specific code) ----

BRIDGE_SRCS := bridge/bg_main.c \
               bridge/bg_helpers.c \
               bridge/bg_threads.c \
               bridge/bg_clientreqs.c

# ---- Plugin source files (all .c files in PLUGIN_DIR) ----

PLUGIN_SRCS := $(wildcard $(PLUGIN_DIR)/*.c)

ALL_SRCS    := $(BRIDGE_SRCS) $(PLUGIN_SRCS)
ALL_OBJS    := $(patsubst %.c,$(BUILDDIR)/%.o,$(ALL_SRCS))

# ---- Targets ----

.PHONY: all clean install test

all: $(TOOL_BIN)

$(BUILDDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(TOOL_BIN): $(ALL_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ $^ \
	    $(LIB_COREGRIND) $(LIB_VEX) $(LIB_VEXMULTI) $(LIB_GCC_SUP) \
	    -lgcc

clean:
	rm -rf $(BUILDDIR)

install: $(TOOL_BIN)
	install -m 755 $(TOOL_BIN) $(VG_LIBEXEC)/$(TOOL_NAME)-$(VG_PLATFORM)
	@echo "Installed $(TOOL_NAME)-$(VG_PLATFORM) to $(VG_LIBEXEC)/"
	@echo "Run with: valgrind --tool=$(TOOL_NAME) ./your_program"

# ---- Test targets ----

TEST_SRCS := $(wildcard tests/*.c) $(wildcard tests/*.cpp)
TEST_BINS := $(patsubst tests/%.c,$(BUILDDIR)/tests/%,$(filter %.c,$(TEST_SRCS))) \
             $(patsubst tests/%.cpp,$(BUILDDIR)/tests/%,$(filter %.cpp,$(TEST_SRCS)))

$(BUILDDIR)/tests/%: tests/%.c
	@mkdir -p $(dir $@)
	$(CC) -O0 -g -pthread -I/usr/include -Iinclude -o $@ $<

$(BUILDDIR)/tests/%: tests/%.cpp
	@mkdir -p $(dir $@)
	g++ -O0 -g -std=c++17 -pthread -I/usr/include -Iinclude -o $@ $<

test-build: $(TEST_BINS)

test: all test-build install
	@echo "=== Running tests under Valgrind Bridge ==="
	@for t in $(TEST_BINS); do \
	    echo "--- $$t ---"; \
	    valgrind --tool=$(TOOL_NAME) $$t 2>&1 | tail -30; \
	    echo; \
	done
