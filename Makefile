## gb-rogue-org Makefile (GBDK-2020)
## GBDK_HOME can be overridden on the command line or via scripts/env.sh.

GBDK_HOME ?= $(CURDIR)/vendor/gbdk
LCC       := $(GBDK_HOME)/bin/lcc

ROM_NAME  := gbrogue
BUILD_DIR := build
SRC_DIRS  := src assets
INC_DIRS  := src

ROM       := $(BUILD_DIR)/$(ROM_NAME).gb

# Flat object layout (basenames must be unique across SRC_DIRS).
SRCS      := $(foreach d,$(SRC_DIRS),$(wildcard $(d)/*.c))
OBJS      := $(patsubst %.c,$(BUILD_DIR)/%.o,$(notdir $(SRCS)))
VPATH     := $(SRC_DIRS)
# Header change -> full rebuild (see docs/status.md 既知の教訓).
HDRS      := $(foreach d,$(INC_DIRS),$(wildcard $(d)/*.h))

# -Wl-m / -Wl-j: map + no$gmb symbol files; tests/gbtest.py reads the .sym
#   to peek WRAM state from PyBoy.
# -Wm-yt0x03: cart = MBC1 + RAM + BATTERY (suspend save lives in SRAM)
# -Wm-ya1:    one 8 KB SRAM bank at 0xA000
# -Wm-yc: GBC-compatible cart (color on GBC, monochrome on DMG)
# -Wm-yo8: 8 ROM banks (128KB). MBC1 scales to 2MB ROM (or 512KB with
#          32KB RAM in mode 1); big const data lives in bank 2+ and is
#          fetched through NONBANKED helpers. Bumped from 64KB to leave
#          headroom for art/glyph banks (button icons etc.); HOME code
#          still lives in the fixed first 32KB regardless of this.
LCCFLAGS  := -Wl-m -Wl-j -Wf--opt-code-size -Wf--max-allocs-per-node55000 \
             -Wm-yt0x03 -Wm-ya1 -Wm-yo8 -Wm-yc -Wm-yn"GBROGUE" \
             $(CFLAGS_EXTRA) \
             $(addprefix -I,$(INC_DIRS))

.PHONY: all clean run verify font debugrom

all: $(ROM)

## Debug ROM with the deterministic M7 test kit; used by verify_m7.
debugrom:
	$(MAKE) BUILD_DIR=build/dbg CFLAGS_EXTRA=-DGBR_DEBUG_KIT all

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

## Any header change rebuilds everything — cheap at this size and it
## prevents stale-object mismatches across modules.
$(BUILD_DIR)/%.o: %.c $(HDRS) | $(BUILD_DIR)
	$(LCC) $(LCCFLAGS) -c -o $@ $<

$(ROM): $(OBJS)
	$(LCC) $(LCCFLAGS) -o $@ $(OBJS)
	python3 scripts/fix_boot.py $(ROM)
	@ls -l $(ROM)

clean:
	rm -rf $(BUILD_DIR)

## Open in SameBoy if installed; otherwise explain.
run: $(ROM)
	@open -a SameBoy $(ROM) 2>/dev/null || \
	 open -a Emulicious $(ROM) 2>/dev/null || \
	 echo "No GUI emulator found. Headless checks: make verify"

## Headless verification (PyBoy). Builds both ROMs first.
verify: $(ROM) debugrom
	.venv/bin/python tests/run_all.py

## Regenerate the ASCII tile atlas C file.
font:
	python3 scripts/gen_font.py
