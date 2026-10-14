# Rebuild GAME.EXE (v1.0.3) from asm/ + src/ and check it against the original.
BIN      := tools/mipsel-elf-binutils/bin
AS       := $(BIN)/mipsel-elf-as
LD       := $(BIN)/mipsel-elf-ld
OBJCOPY  := $(BIN)/mipsel-elf-objcopy
ASFLAGS  := -EL -mips4 -mabi=32 -G0 -no-pad-sections
ORIG     := original/partition/GAME.EXE
LOAD     := 0x800c4000
SDK      := original/sdk
CPPFLAGS := -E -P -Wno-nonportable-include-path -nostdinc -I$(SDK)/include -Iinclude -D__DJGPP__ -DSEATTLE -DRELEASE
# the game's sources are grouped in subfolders of src/game and include each other's headers
GAME_DIRS := $(patsubst %/,%,$(sort $(dir $(wildcard src/game/*/*.c))))
GAME_INC  := $(addprefix -I,$(GAME_DIRS))
PORT_DIRS := port/runtime port/video port/audio
PORT_INC  := $(addprefix -I,$(PORT_DIRS))
CC1_goose := -O3 -mips4 -mcpu=r5000 -mhard-float -fomit-frame-pointer -G 0 -fno-builtin
CC1_game  := -O3 -mips3 -fsigned-char -mhard-float -fomit-frame-pointer -G 0 -fno-builtin   # game objects: signed char, pre-R5000 ISA (docs/toolchain.md)
CC1_game2 := -O2 -mips3 -fsigned-char -mhard-float -fomit-frame-pointer -G 0 -fno-builtin   # the game's own files (credits, stream, ...)
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
prereqs-sdk:
	@test -d $(SDK)/include || { echo "*** $(SDK) is missing: ./setup.sh --sdk /path/to/midwayvideo (git clone https://github.com/historicalsource/midwayvideo)"; exit 1; }
prereqs: prereqs-sdk
	@test -f $(ORIG) || { echo "*** $(ORIG) is missing: ./setup.sh --raw IMAGE or --partition DIR (your own dump of CarnEvil v1.0.3)"; exit 1; }
	@test -x $(AS) || { echo "*** $(AS) is missing: run tools/build/get-binutils.sh once"; exit 1; }
	@command -v dosbox-x >/dev/null 2>&1 || command -v dosbox >/dev/null 2>&1 || echo "*** warning: DOSBox-X not on the PATH; C modules cannot be compiled (the SDK's compiler is a DOS program)"

asm/files.txt build/link_order.ld build/modules.mk: | prereqs
	$(DISASM)

build/asm/%.o: asm/%.s
	@mkdir -p $(dir $@)
	@$(AS) $(ASFLAGS) -o $@ $<

# the GOOSE and sound library sources are generated from the SDK checkout plus our patches
# (config/sdk_sources.txt, patches/, tools/build/sdksrc.py); they are not committed
SDK_GEN := $(shell awk '!/^\#/ && NF {print $$1}' config/sdk_sources.txt)
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
HOSTCFLAGS := -O2 -g -Wall -Wno-unused-result -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast -DPORT_HOST -DSEATTLE -DRELEASE -Iinclude -include port/runtime/compat.h -idirafter $(SDK)/include -Wno-nonportable-include-path
C_SRCS     := $(filter %.c,$(shell grep -v '^\#' config/modules.txt | awk 'NF {print $$2}'))

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
# run under Rosetta (as Wine does); elsewhere it is native with -no-pie.
PORT_SRCS := $(wildcard port/runtime/*.c port/video/*.c port/audio/*.c port/stubs/*.c)
# game sources that are not byte-exact modules yet still run in the port
PORT_GAME_SRCS := $(wildcard src/game/*/*.c)
PORT_OBJS := $(patsubst %.c,build/port/%.o,$(sort $(filter-out $(HOST_SKIP),$(C_SRCS)) $(PORT_GAME_SRCS)))
comma := ,
PORT_FORTIFY ?= 0     # libc fortification trips on ILP32 idioms; set 2 to debug with it
ifeq ($(shell uname),Darwin)
PORT_ARCH    := -arch x86_64
PORT_LDFLAGS := -Wl$(comma)-pagezero_size$(comma)0x4000 -framework Cocoa -framework OpenGL
# camera.c defines the camera position as three scalars, objects.c reads it as an array (docs/toolchain.md)
PORT_LDFLAGS += -Wl$(comma)-alias$(comma)_camera_x$(comma)_camera_pos
PORT_VIDEO   := port/video/video_macos.m
PORT_AUDIO   := port/audio/audio_macos.m
PORT_LDFLAGS += -framework AudioToolbox
else
PORT_ARCH    :=
PORT_LDFLAGS := -no-pie -Wl$(comma)--defsym=camera_pos=camera_x
PORT_VIDEO   := port/video/video_null.c
PORT_AUDIO   := port/audio/audio_null.c
endif
PORT_SRCS := $(filter-out port/video/video_null.c port/audio/audio_null.c,$(PORT_SRCS)) $(PORT_VIDEO) $(PORT_AUDIO)
# the virtual DCS sound board is C++ (it drives the vendored DCS decoder)
PORT_CXX_SRCS := port/audio/dcs_board.cpp port/third_party/dcsdecoder/DCSDecoder.cpp port/third_party/dcsdecoder/DCSDecoderNative.cpp
PORT_CXX_OBJS := $(patsubst %.cpp,build/port/%.o,$(PORT_CXX_SRCS))
HOSTCXX ?= clang++
build/port/%.o: %.cpp port/audio/dcs_board.h
	@mkdir -p $(dir $@)
	$(HOSTCXX) -std=c++17 -O2 $(PORT_ARCH) $(PORT_INC) -Wno-deprecated-declarations -c -o $@ $<
build/port/%.o: %.c include/types.h port/runtime/compat.h
	@mkdir -p $(dir $@)
	$(HOSTCC) $(HOSTCFLAGS) $(PORT_ARCH) -D_FORTIFY_SOURCE=$(PORT_FORTIFY) -c -o $@ $<
build/port/src/game/%.o: HOSTCFLAGS += $(GAME_INC)
build/host/carnevil: $(PORT_OBJS) $(PORT_CXX_OBJS) $(PORT_SRCS) port/runtime/hal.h port/video/video.h port/runtime/texture.h port/audio/audio.h
	@mkdir -p $(dir $@)
	$(HOSTCC) $(HOSTCFLAGS) $(PORT_INC) $(GAME_INC) $(PORT_ARCH) -D_FORTIFY_SOURCE=$(PORT_FORTIFY) -Wno-deprecated-declarations $(PORT_LDFLAGS) -lc++ -o $@ $(PORT_SRCS) $(PORT_OBJS) $(PORT_CXX_OBJS)
port: prereqs-sdk build/host/carnevil

check: build/GAME.EXE
	@cmp $(ORIG) build/GAME.EXE && echo "OK: build/GAME.EXE is byte-identical to $(ORIG)"

verify: build/game.bin
	python3 tools/build/verify.py $(ORIG) build/game.bin config/word_fallback.txt

regen:
	rm -rf asm && $(DISASM)

clean:
	rm -rf build/asm build/game.* build/GAME.EXE build/src
.PHONY: all check verify regen clean host port prereqs prereqs-sdk sdksrc sdksrc
.SECONDARY:
