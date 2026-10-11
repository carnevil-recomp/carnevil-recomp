# Rebuild GAME.EXE (v1.0.3) from asm/ and check it against the original.
BIN      := tools/mipsel-elf-binutils/bin
AS       := $(BIN)/mipsel-elf-as
LD       := $(BIN)/mipsel-elf-ld
OBJCOPY  := $(BIN)/mipsel-elf-objcopy
ASFLAGS  := -EL -mips4 -mabi=32 -G0 -no-pad-sections
ORIG     := original/partition/GAME.EXE
LOAD     := 0x800c4000
ASM_SRCS := $(shell cat asm/files.txt 2>/dev/null | sed 's|^|asm/|')
OBJS     := $(patsubst asm/%.s,build/asm/%.o,$(ASM_SRCS))

all: build/GAME.EXE check

asm/files.txt:
	python3 tools/disasm.py $(ORIG) asm --functions config/functions_v103.txt --word-fallback config/word_fallback.txt

build/asm/%.o: asm/%.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -o $@ $<

build/game.elf: $(OBJS) config/game.ld
	$(LD) -EL -T config/game.ld -o $@ $(OBJS)

build/game.bin: build/game.elf
	$(OBJCOPY) -O binary $< $@

build/GAME.EXE: build/game.bin
	python3 tools/exe.py pack $< $(LOAD) $@

check: build/GAME.EXE
	@cmp $(ORIG) build/GAME.EXE && echo "OK: build/GAME.EXE is byte-identical to $(ORIG)"

verify: build/game.bin
	python3 tools/verify.py $(ORIG) build/game.bin config/word_fallback.txt

regen:
	rm -rf asm && python3 tools/disasm.py $(ORIG) asm --functions config/functions_v103.txt --word-fallback config/word_fallback.txt

clean:
	rm -rf build
.PHONY: all check verify regen clean
