# Bonanza Bros. (Mega Drive) disassembly

CROSS   ?= m68k-linux-gnu-
AS      := $(CROSS)as
LD      := $(CROSS)ld
OBJCOPY := $(CROSS)objcopy
PYTHON  ?= python3

ASFLAGS := -m68000 --register-prefix-optional -I .

BUILD   := build
TARGET  := $(BUILD)/bonanza.md
SRCS    := $(wildcard src/*.s) $(wildcard include/*.inc)

.PHONY: all compare clean extract disasm

all: compare

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

clean:
	rm -rf $(BUILD) assets
