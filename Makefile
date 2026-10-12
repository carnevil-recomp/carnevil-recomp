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
CC1_goose := -O3 -mips4 -mcpu=r5000 -mhard-float -fomit-frame-pointer -G 0 -fno-builtin
CC1_game  := -O3 -mips4 -mhard-float -fomit-frame-pointer -G 0 -fno-builtin
CC1_noopt := -mips4 -mhard-float -fomit-frame-pointer -G 0 -fno-builtin
CC1_o2r5k := -O2 -mips4 -mcpu=r5000 -mhard-float -fomit-frame-pointer -G 0 -fno-builtin   # sound library (sound.mak: CC2)
DISASM   := python3 tools/disasm.py $(ORIG) asm --functions config/functions_v103.txt --symbols config/binja_symbols.txt \
            --names config/names.txt --modules config/modules.txt --word-fallback config/word_fallback.txt --nosym config/nosym.txt

ASM_SRCS := $(shell cat asm/files.txt 2>/dev/null | sed 's|^|asm/|')
ASM_OBJS := $(patsubst asm/%.s,build/asm/%.o,$(ASM_SRCS))
-include build/modules.mk

all: build/GAME.EXE check

asm/files.txt build/link_order.ld build/modules.mk:
	$(DISASM)

build/asm/%.o: asm/%.s
	@mkdir -p $(dir $@)
	@$(AS) $(ASFLAGS) -o $@ $<

# C modules: preprocess with clang, compile with the SN cc1 under DOSBox-X, fix up for gas, assemble
build/%.i: %.c
	@mkdir -p $(dir $@)
	clang $(CPPFLAGS) -x c $< -o $@
build/%.sn.s: build/%.i tools/sncc.sh
	CC1FLAGS="$(CC1_$(PROFILE))" tools/sncc.sh $< $@
build/%.s: build/%.sn.s tools/sn2gas.py
	python3 tools/sn2gas.py $< $@
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
	python3 tools/exe.py pack $< $(LOAD) $@

# ---- native host build (the port) ----
HOSTCC     := clang
HOSTCFLAGS := -O2 -g -Wall -Wno-unused-result -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast -DPORT_HOST -DSEATTLE -DRELEASE -Iinclude -include port/host/compat.h -idirafter $(SDK)/include -Wno-nonportable-include-path
C_SRCS     := $(filter %.c,$(shell grep -v '^\#' config/modules.txt | awk 'NF {print $$2}'))

HOST_LINK_SRCS := src/goose/crc.c src/goose/sincos.c     # modules the self-test links; the rest are compile-checked
# mthread.c is the MIPS context switcher (register save/restore in inline asm); the port
# replaces it with its own scheduler under port/, so it is never compiled natively.
HOST_SKIP  := src/goose/mthread.c
HOST_OBJS := $(patsubst %.c,build/host/%.o,$(filter-out $(HOST_SKIP),$(C_SRCS)))

build/host/%.o: %.c include/types.h port/host/compat.h
	@mkdir -p $(dir $@)
	$(HOSTCC) $(HOSTCFLAGS) -c -o $@ $<

build/host/selftest: port/host/selftest.c $(HOST_LINK_SRCS) include/types.h port/host/compat.h
	@mkdir -p build/host
	$(HOSTCC) $(HOSTCFLAGS) -o $@ port/host/selftest.c $(HOST_LINK_SRCS)

host: $(HOST_OBJS) build/host/selftest
	./build/host/selftest $(ORIG)

# ---- the playable port: GOOSE + game modules with the host HAL (port/host/*.c) ----
# The game sees pointers as int, so port/host/mem.c keeps all its memory below 4 GB. The arm64
# macOS kernel refuses executables that map that low, so on macOS the port is an x86_64 binary
# run under Rosetta (as Wine does); elsewhere it is native with -no-pie.
PORT_SRCS := $(filter-out port/host/selftest.c,$(wildcard port/host/*.c))
PORT_OBJS := $(patsubst %.c,build/port/%.o,$(filter-out $(HOST_SKIP),$(C_SRCS)))
comma := ,
PORT_FORTIFY ?= 0     # libc fortification trips on ILP32 idioms; set 2 to debug with it
ifeq ($(shell uname),Darwin)
PORT_ARCH    := -arch x86_64
PORT_LDFLAGS := -Wl$(comma)-pagezero_size$(comma)0x4000 -framework Cocoa -framework OpenGL
PORT_VIDEO   := port/host/video_macos.m
else
PORT_ARCH    :=
PORT_LDFLAGS := -no-pie
PORT_VIDEO   := port/host/video_null.c
endif
PORT_SRCS := $(filter-out port/host/video_null.c,$(PORT_SRCS)) $(PORT_VIDEO)
build/port/%.o: %.c include/types.h port/host/compat.h
	@mkdir -p $(dir $@)
	$(HOSTCC) $(HOSTCFLAGS) $(PORT_ARCH) -D_FORTIFY_SOURCE=$(PORT_FORTIFY) -c -o $@ $<
build/host/carnevil: $(PORT_OBJS) $(PORT_SRCS) port/host/hal.h port/host/video.h port/host/texture.h
	$(HOSTCC) $(HOSTCFLAGS) $(PORT_ARCH) -D_FORTIFY_SOURCE=$(PORT_FORTIFY) -Wno-deprecated-declarations $(PORT_LDFLAGS) -o $@ $(PORT_SRCS) $(PORT_OBJS)
port: build/host/carnevil

check: build/GAME.EXE
	@cmp $(ORIG) build/GAME.EXE && echo "OK: build/GAME.EXE is byte-identical to $(ORIG)"

verify: build/game.bin
	python3 tools/verify.py $(ORIG) build/game.bin config/word_fallback.txt

regen:
	rm -rf asm && $(DISASM)

clean:
	rm -rf build/asm build/game.* build/GAME.EXE build/src
.PHONY: all check verify regen clean host port
.SECONDARY:
