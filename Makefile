# Bonanza Bros. (Mega Drive) disassembly

CROSS   ?= m68k-linux-gnu-
AS      := $(CROSS)as
LD      := $(CROSS)ld
OBJCOPY := $(CROSS)objcopy
PYTHON  ?= python3

ASFLAGS := -m68000 --register-prefix-optional -I .

BUILD   := build
TARGET  := $(BUILD)/bonanza.md
SRCS    := src/bonanza.s $(wildcard include/*.inc)

.PHONY: all compare clean extract disasm recomp port

all: compare

# the disassembly is made from the ROM (it is not in the repository)
src/bonanza.s assets.txt: baserom.md tools/disasm.py tools/config.py
	$(PYTHON) tools/disasm.py

$(BUILD)/.assets: assets.txt baserom.md tools/extract_assets.py
	@mkdir -p $(BUILD)
	$(PYTHON) tools/extract_assets.py
	@touch $@

extract: $(BUILD)/.assets

$(BUILD)/bonanza.o: $(SRCS) $(BUILD)/.assets
	@mkdir -p $(BUILD)
	$(AS) $(ASFLAGS) -o $@ src/bonanza.s

$(BUILD)/bonanza.elf: $(BUILD)/bonanza.o bonanza.ld
	$(LD) -T bonanza.ld -Map $(BUILD)/bonanza.map -o $@ $<

$(TARGET): $(BUILD)/bonanza.elf
	$(OBJCOPY) -O binary $< $@

compare: $(TARGET)
	@sha1sum -c bonanza.sha1

# Regenerate src/ from the ROM (overwrites src/ and assets.txt).
disasm:
	$(PYTHON) tools/disasm.py

# Regenerate the recompiled C code of the native port (port/gen/).
recomp:
	$(PYTHON) tools/recomp.py
	$(PYTHON) tools/z80recomp.py

# Build the native port for this machine (needs CMake and SDL2).
port:
	cmake -S port -B port/build -DCMAKE_BUILD_TYPE=Release
	cmake --build port/build -j

clean:
	rm -rf $(BUILD) assets
