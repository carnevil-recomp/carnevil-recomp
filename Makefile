# Rebuild GAME.EXE (v1.0.3) from asm/ + src/ and check it against the original.
# One checkout can be built from macOS and from Linux (e.g. a VM sharing the folder): what is
# host-specific lives in per-OS places. macOS keeps the original paths (build/host/carnevil,
# build/port, tools/mipsel-elf-binutils); Linux uses build/linux/... and tools/mipsel-elf-binutils-linux.
UNAME    := $(shell uname -s)
# WINDOWS=1 cross-compiles the port for 32-bit Windows from Linux with MinGW-w64 (see below)
WINDOWS  ?=
ifeq ($(UNAME),Darwin)
BINUTILS ?= tools/mipsel-elf-binutils
PORT_OUT := build/port
PORT_BIN := build/host/carnevil
else
BINUTILS ?= tools/mipsel-elf-binutils-linux
ifeq ($(WINDOWS),1)
PORT_OUT := build/win/port
PORT_BIN := build/win/carnevil.exe
else
PORT_OUT := build/linux/port
PORT_BIN := build/linux/carnevil
endif
# lower-case links to the SDK's capitalised headers (tools/build/sdkcase.sh)
SDKCASE  := build/linux/sdkcase
SDKCASE_HOST := build/linux/sdkcase-host
endif
BIN      := $(BINUTILS)/bin
AS       := $(BIN)/mipsel-elf-as
LD       := $(BIN)/mipsel-elf-ld
OBJCOPY  := $(BIN)/mipsel-elf-objcopy
ASFLAGS  := -EL -mips4 -mabi=32 -G0 -no-pad-sections
ORIG     := original/partition/GAME.EXE
LOAD     := 0x800c4000
SDK      := original/sdk
CPPFLAGS := -E -P -Wno-nonportable-include-path -nostdinc -I$(SDK)/include -Iinclude $(if $(SDKCASE),-idirafter $(SDKCASE)) -D__DJGPP__ -DSEATTLE -DRELEASE
# the game's sources are grouped in subfolders of src/game and include each other's headers
GAME_DIRS := $(patsubst %/,%,$(sort $(dir $(wildcard src/game/*/*.c))))
GAME_INC  := $(addprefix -I,$(GAME_DIRS))
PORT_DIRS := port/runtime port/video port/audio
PORT_INC  := $(addprefix -I,$(PORT_DIRS))
CC1_goose := -O3 -mips4 -mcpu=r5000 -mhard-float -fomit-frame-pointer -G 0 -fno-builtin
CC1_game  := -O3 -mips3 -fsigned-char -mhard-float -fomit-frame-pointer -G 0 -fno-builtin   # game objects: signed char, pre-R5000 ISA (docs/toolchain.md)
CC1_game2 := -O2 -mips3 -fsigned-char -mhard-float -fomit-frame-pointer -G 0 -fno-builtin   # the game's own files (credits, stream, ...)
CC1_game2nsr := -O2 -mips3 -fsigned-char -mhard-float -fomit-frame-pointer -G 0 -fno-builtin -fno-strength-reduce   # the end credits (no loop strength reduction)
CC1_game1 := -O -mips3 -fsigned-char -mhard-float -fomit-frame-pointer -G 0 -fno-builtin    # the audit helpers
CC1_game0 := -mips3 -fsigned-char -mhard-float -fomit-frame-pointer -G 0 -fno-builtin       # the high-score file
CC1_noopt := -mips4 -mhard-float -fomit-frame-pointer -G 0 -fno-builtin
CC1_o2r5k := -O2 -mips4 -mcpu=r5000 -mhard-float -fomit-frame-pointer -G 0 -fno-builtin   # sound library (sound.mak: CC2)
DISASM   := python3 tools/build/disasm.py $(ORIG) asm --functions config/functions_v103.txt --symbols config/binja_symbols.txt \
            --names config/names.txt --modules config/modules.txt --word-fallback config/word_fallback.txt --nosym config/nosym.txt

ASM_SRCS := $(shell cat asm/files.txt 2>/dev/null | sed 's|^|asm/|')
ASM_OBJS := $(patsubst asm/%.s,build/asm/%.o,$(ASM_SRCS))
-include build/modules.mk

all: prereqs build/GAME.EXE check

# what each build needs (see README "Getting started"): the port needs the SDK checkout for its
# headers; the byte-exact rebuild also needs your GAME.EXE, the binutils and DOSBox-X
prereqs-sdk: $(if $(SDKCASE),$(SDKCASE)/.stamp $(SDKCASE_HOST)/.stamp)
	@test -d $(SDK)/include || { echo "*** $(SDK) is missing: ./setup.sh --sdk /path/to/midwayvideo (git clone https://github.com/historicalsource/midwayvideo)"; exit 1; }
ifneq ($(SDKCASE),)
$(SDKCASE)/.stamp: tools/build/sdkcase.sh
	@test -d $(SDK)/include || { echo "*** $(SDK) is missing: ./setup.sh --sdk /path/to/midwayvideo"; exit 1; }
	tools/build/sdkcase.sh $(SDK)/include $(SDKCASE)
$(SDKCASE_HOST)/.stamp: tools/build/sdkcase.sh
	@test -d $(SDK)/include || { echo "*** $(SDK) is missing: ./setup.sh --sdk /path/to/midwayvideo"; exit 1; }
	tools/build/sdkcase.sh --wrap $(SDK)/include $(SDKCASE_HOST)
endif
prereqs: prereqs-sdk
	@test -f $(ORIG) || { echo "*** $(ORIG) is missing: ./setup.sh --raw IMAGE or --partition DIR (your own dump of CarnEvil v1.0.3)"; exit 1; }
	@test -x $(AS) || { echo "*** $(AS) is missing: run tools/build/get-binutils.sh $(BINUTILS) once"; exit 1; }
	@command -v dosbox-x >/dev/null 2>&1 || command -v dosbox >/dev/null 2>&1 || echo "*** warning: DOSBox-X not on the PATH; C modules cannot be compiled (the SDK's compiler is a DOS program)"

asm/files.txt build/link_order.ld build/modules.mk: | prereqs
	$(DISASM)

build/asm/%.o: asm/%.s
	@mkdir -p $(dir $@)
	@$(AS) $(ASFLAGS) -o $@ $<

# the GOOSE and sound library sources are generated from the SDK checkout plus our patches
# (config/sdk_sources.txt, patches/, tools/build/sdksrc.py); they are not committed
# a literal '#' for $(shell ...): GNU make 3.81 (macOS) reads a bare # there as a comment, while
# make 4.x passes a backslash-escaped one through to the command; a variable works with both
HASH := \#
SDK_GEN := $(shell awk '!/^$(HASH)/ && NF {print $$1}' config/sdk_sources.txt)
$(SDK_GEN): tools/build/sdksrc.py config/sdk_sources.txt $(wildcard patches/*/*.patch) | prereqs-sdk
	@python3 tools/build/sdksrc.py
sdksrc: $(SDK_GEN)

# C modules: preprocess with clang, compile with the SN cc1 under DOSBox-X, fix up for gas, assemble
build/%.i: %.c
	@mkdir -p $(dir $@)
	clang $(CPPFLAGS) -x c $< -o $@
build/src/game/%.i: CPPFLAGS += $(GAME_INC)
build/%.sn.s: build/%.i tools/build/sncc.sh
	CC1FLAGS="$(CC1_$(PROFILE))" tools/build/sncc.sh $< $@
SN2GAS_game := --fpu-hazard
SN2GAS_game2 := --fpu-hazard
SN2GAS_game1 := --fpu-hazard
SN2GAS_game2nsr := --fpu-hazard
SN2GAS_game0 := --fpu-hazard
build/%.s: build/%.sn.s tools/build/sn2gas.py
	python3 tools/build/sn2gas.py $< $@ $(SN2GAS_$(PROFILE))
build/%.o: build/%.s
	$(AS) $(ASFLAGS) -o $@ $<

# assembly-source modules (SDK .S files transcribed to GNU as syntax, e.g. src/goose/pshell.s)
build/%.o: %.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -o $@ $<

build/game.elf: $(ASM_OBJS) $(C_OBJS) config/game.ld config/symbols_abs.ld build/link_order.ld
	@echo "LD $@ ($(words $(ASM_OBJS)) asm objects, $(words $(C_OBJS)) C modules)"
	@$(LD) -EL -T config/game.ld -o $@ $(ASM_OBJS) $(C_OBJS)

build/game.bin: build/game.elf
	$(OBJCOPY) -O binary $< $@

build/GAME.EXE: build/game.bin
	python3 tools/build/exe.py pack $< $(LOAD) $@

# ---- native host build (the port) ----
HOSTCC     := clang
HOSTCFLAGS := -O2 -g -Wall -Wno-unused-result -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast -DPORT_HOST -DSEATTLE -DRELEASE -Iinclude -include port/runtime/compat.h $(if $(SDKCASE_HOST),-idirafter $(SDKCASE_HOST)) -idirafter $(SDK)/include -Wno-nonportable-include-path
C_SRCS     := $(filter %.c,$(shell grep -v '^$(HASH)' config/modules.txt | awk 'NF {print $$2}'))

HOST_LINK_SRCS := src/goose/crc.c src/goose/sincos.c     # modules the self-test links; the rest are compile-checked
# mthread.c is the MIPS context switcher (register save/restore in inline asm); the port
# replaces it with its own scheduler under port/, so it is never compiled natively.
HOST_SKIP  := src/goose/mthread.c
HOST_OBJS := $(patsubst %.c,build/host/%.o,$(filter-out $(HOST_SKIP),$(C_SRCS)))

build/host/%.o: %.c include/types.h port/runtime/compat.h
	@mkdir -p $(dir $@)
	$(HOSTCC) $(HOSTCFLAGS) -c -o $@ $<
build/host/src/game/%.o: HOSTCFLAGS += $(GAME_INC)

build/host/selftest: port/tests/selftest.c $(HOST_LINK_SRCS) include/types.h port/runtime/compat.h
	@mkdir -p build/host
	$(HOSTCC) $(HOSTCFLAGS) -o $@ port/tests/selftest.c $(HOST_LINK_SRCS)

host: prereqs-sdk $(HOST_OBJS) build/host/selftest
	./build/host/selftest $(ORIG)

# ---- the playable port: GOOSE + game modules with the host layer (port/runtime, video, audio, stubs) ----
# The game sees pointers as int, so port/runtime/mem.c keeps all its memory below 4 GB. The arm64
# macOS kernel refuses executables that map that low, so on macOS the port is an x86_64 binary
# run under Rosetta (as Wine does); on Linux it is a native x86_64 build with -no-pie, a window
# and sound through SDL2 (packages: libsdl2-dev, libgl-dev).
PORT_SRCS := $(wildcard port/runtime/*.c port/video/*.c port/audio/*.c port/stubs/*.c)
# game sources that are not byte-exact modules yet still run in the port
PORT_GAME_SRCS := $(wildcard src/game/*/*.c)
PORT_OBJS := $(patsubst %.c,$(PORT_OUT)/%.o,$(sort $(filter-out $(HOST_SKIP),$(C_SRCS)) $(PORT_GAME_SRCS)))
comma := ,
PORT_FORTIFY ?= 0     # libc fortification trips on ILP32 idioms; set 2 to debug with it
ifeq ($(WINDOWS),1)
# 32-bit Windows (i686), cross-compiled with MinGW-w64: a 32-bit process matches the game's ILP32
# layout outright, and --large-address-aware lets the gun ports at 0xb6800000/0xb7f00000 map.
# SDL2's MinGW development package (SDL2-devel-*-mingw) gives the window, input and sound.
HOSTCC       := i686-w64-mingw32-gcc
HOSTCXX      := i686-w64-mingw32-g++
SDL2_MINGW   ?= $(HOME)/win-deps/SDL2-2.30.9/i686-w64-mingw32
PORT_ARCH    :=
HOSTCFLAGS   := $(filter-out -Wno-nonportable-include-path,$(HOSTCFLAGS)) -fcommon -D_FILE_OFFSET_BITS=64 -I$(SDL2_MINGW)/include/SDL2
PORT_LDFLAGS := -Wl$(comma)--large-address-aware -static-libgcc -static-libstdc++
# the HAL's file layer (hal.c defines __wrap_NAME): see port/runtime/compat.h
PORT_LDFLAGS += $(foreach f,open close read write lseek lseek64 fopen fopen64 remove rename,-Wl$(comma)--wrap=$(f))
PORT_VIDEO   := port/video/video_sdl.c
PORT_AUDIO   := port/audio/audio_sdl.c
PORT_LIBS    := -L$(SDL2_MINGW)/lib -lSDL2 -lopengl32 -Wl$(comma)-Bstatic -lstdc++ -lpthread -Wl$(comma)-Bdynamic -lm
# MinGW ships a dir.h of its own; the SDK's (struct ffblk, findfirst) must win, so it gets a
# one-file include directory searched before the system's
HOSTCFLAGS   += -Ibuild/win/sdkdir
build/win/sdkdir/dir.h: $(SDKCASE_HOST)/.stamp
	@mkdir -p $(dir $@)
	cp $(SDKCASE_HOST)/dir.h $@
PORT_EXTRA   := build/win/SDL2.dll
build/win/SDL2.dll: $(SDL2_MINGW)/bin/SDL2.dll
	@mkdir -p $(dir $@)
	cp $< $@
else ifeq ($(shell uname),Darwin)
PORT_ARCH    := -arch x86_64
PORT_LDFLAGS := -Wl$(comma)-pagezero_size$(comma)0x4000 -framework Cocoa -framework OpenGL
PORT_VIDEO   := port/video/video_macos.m
PORT_AUDIO   := port/audio/audio_macos.m
PORT_LDFLAGS += -framework AudioToolbox -framework GameController -framework IOKit
PORT_LIBS    := -lc++
else
PORT_ARCH    :=
SDL_CFLAGS   := $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL_LIBS     := $(shell pkg-config --libs sdl2 2>/dev/null)
PORT_LDFLAGS := -no-pie
# globals defined in more than one file (stream_volume, ...) were merged as common symbols by the
# game's GCC 2.7; modern compilers default to -fno-common
HOSTCFLAGS   += -fcommon
ifneq ($(SDL_LIBS),)
PORT_VIDEO   := port/video/video_sdl.c
PORT_AUDIO   := port/audio/audio_sdl.c
PORT_LIBS    := $(SDL_LIBS) -lGL -lstdc++ -ldl -lm -lpthread
HOSTCFLAGS   += $(SDL_CFLAGS)
else
$(warning SDL2 not found (pkg-config sdl2): building a headless port; install libsdl2-dev for a window and sound)
PORT_VIDEO   := port/video/video_null.c
PORT_AUDIO   := port/audio/audio_null.c
PORT_LIBS    := -lstdc++ -ldl -lm -lpthread
endif
endif
PORT_SRCS := $(filter-out port/video/video_null.c port/audio/audio_null.c port/video/video_sdl.c port/video/mice_sdl.c port/audio/audio_sdl.c,$(PORT_SRCS)) $(PORT_VIDEO) $(PORT_AUDIO) $(if $(filter port/video/video_sdl.c,$(PORT_VIDEO)),port/video/mice_sdl.c)
# the virtual DCS sound board is C++ (it drives the vendored DCS decoder)
PORT_CXX_SRCS := port/audio/dcs_board.cpp port/third_party/dcsdecoder/DCSDecoder.cpp port/third_party/dcsdecoder/DCSDecoderNative.cpp
PORT_CXX_OBJS := $(patsubst %.cpp,$(PORT_OUT)/%.o,$(PORT_CXX_SRCS))
HOSTCXX ?= clang++
$(PORT_OUT)/%.o: %.cpp port/audio/dcs_board.h
	@mkdir -p $(dir $@)
	$(HOSTCXX) -std=c++17 -O2 $(PORT_ARCH) $(PORT_INC) -Wno-deprecated-declarations -c -o $@ $<
$(PORT_OUT)/%.o: %.c include/types.h port/runtime/compat.h
	@mkdir -p $(dir $@)
	$(HOSTCC) $(HOSTCFLAGS) $(PORT_ARCH) -D_FORTIFY_SOURCE=$(PORT_FORTIFY) -c -o $@ $<
$(PORT_OUT)/src/game/%.o: HOSTCFLAGS += $(GAME_INC)
$(PORT_BIN): $(PORT_OBJS) $(PORT_CXX_OBJS) $(PORT_SRCS) port/runtime/hal.h port/video/video.h port/runtime/texture.h port/audio/audio.h
	@mkdir -p $(dir $@)
	$(HOSTCC) $(HOSTCFLAGS) $(PORT_INC) $(GAME_INC) $(PORT_ARCH) -D_FORTIFY_SOURCE=$(PORT_FORTIFY) -Wno-deprecated-declarations $(PORT_LDFLAGS) -o $@ $(PORT_LINK) $(PORT_LIBS)
ifeq ($(WINDOWS),1)
$(PORT_OBJS) $(PORT_CXX_OBJS) $(PORT_BIN): | build/win/sdkdir/dir.h
# the stubs last, so a real definition anywhere else comes first and wins (no weak symbols on PE)
PORT_LINK := $(filter-out port/stubs/%,$(PORT_SRCS)) $(PORT_OBJS) $(PORT_CXX_OBJS) $(filter port/stubs/%,$(PORT_SRCS)) -Wl$(comma)--allow-multiple-definition
else
PORT_LINK := $(PORT_SRCS) $(PORT_OBJS) $(PORT_CXX_OBJS)
endif
port: prereqs-sdk $(PORT_BIN) $(PORT_EXTRA)

check: build/GAME.EXE
	@cmp $(ORIG) build/GAME.EXE && echo "OK: build/GAME.EXE is byte-identical to $(ORIG)"

verify: build/game.bin
	python3 tools/build/verify.py $(ORIG) build/game.bin config/word_fallback.txt

regen:
	rm -rf asm && $(DISASM)

# ---- DIAG.EXE (the operator diagnostics the test switch execs), rebuilt the same way ----
DIAG_ORIG := original/partition/DIAG.EXE
DIAG_LOAD := 0x80100000
DIAG_DISASM := python3 tools/build/disasm.py $(DIAG_ORIG) asm_diag --functions config/diag/functions.txt \
            --names config/diag/names.txt --modules config/diag/modules.txt --word-fallback config/diag/word_fallback.txt \
            --nosym config/diag/nosym.txt --load $(DIAG_LOAD) --code-end 0x80152170 --build-dir build/diag --abs-out config/diag/symbols_abs.ld
DIAG_ASM_OBJS := $(patsubst %.s,build/asm_diag/%.o,$(shell cat asm_diag/files.txt 2>/dev/null))
asm_diag/files.txt build/diag/link_order.ld: | prereqs
	@test -f $(DIAG_ORIG) || { echo "*** $(DIAG_ORIG) is missing (it is on the same partition as GAME.EXE)"; exit 1; }
	$(DIAG_DISASM)
build/asm_diag/%.o: asm_diag/%.s
	@mkdir -p $(dir $@)
	@$(AS) $(ASFLAGS) -o $@ $<
build/diag/diag.elf: $(DIAG_ASM_OBJS) config/diag/diag.ld build/diag/link_order.ld
	@echo "LD $@ ($(words $(DIAG_ASM_OBJS)) asm objects)"
	@$(LD) -EL -T config/diag/diag.ld -o $@ $(DIAG_ASM_OBJS)
build/diag/diag.bin: build/diag/diag.elf
	$(OBJCOPY) -O binary $< $@
build/diag/DIAG.EXE: build/diag/diag.bin
	python3 tools/build/exe.py pack $< $(DIAG_LOAD) $@
# the asm list is only known after the split: a first `make diag` splits, then rebuilds
diag: asm_diag/files.txt
	@$(MAKE) --no-print-directory build/diag/DIAG.EXE
	@cmp $(DIAG_ORIG) build/diag/DIAG.EXE && echo "OK: build/diag/DIAG.EXE is byte-identical to $(DIAG_ORIG)"
diag-regen:
	rm -rf asm_diag && $(DIAG_DISASM)

clean:
	rm -rf build/asm build/game.* build/GAME.EXE build/src build/asm_diag build/diag
.PHONY: all check verify regen clean diag diag-regen host port prereqs prereqs-sdk sdksrc sdksrc
.SECONDARY:
