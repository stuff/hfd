# High Fidelity Dreams, Amiga port: build/hfd.adf.
# The tools come from tools/setup.sh (vasm, ptplayer, capture in tools/.build).
# make AUTHENTIC=1: the ear screen lasts as long as in the demo (for verification).

B := build
TOOLS := tools/.build
VASM := $(TOOLS)/src/vasm/vasmm68k_mot -quiet -m68000 -Fbin -Isrc -I$(B)
PYTHON := $(TOOLS)/venv/bin/python
PTPLAYER := $(TOOLS)/src/ptplayer/ptplayer.asm
RIP := ../../rip
MODULE := $(RIP)/music/04 - Enigma aka Hyperbased (Firefox & Tip).mod
DEFINES := $(if $(AUTHENTIC),-DAUTHENTIC=1)

all: $(B)/hfd.adf

$(B)/intro.i: tools/mkintro.py $(wildcard $(RIP)/graphics/pictures/*.json $(RIP)/graphics/pictures/fullscreen/*.json)
	$(PYTHON) tools/mkintro.py $(RIP) $(B)

# ptplayer.asm ends with END, which would stop the assembly of main.s.
$(B)/ptplayer.asm: $(PTPLAYER)
	@mkdir -p $(B)
	sed '/^[[:space:]]*end[[:space:]]*$$/d' $< > $@

# (Make can't depend on a file name with spaces.)
$(B)/module.mod:
	@mkdir -p $(B)
	cp "$(MODULE)" $@

# The defines change the program: rebuild it when they change.
$(B)/defines: FORCE
	@mkdir -p $(B)
	@echo '$(DEFINES)' | cmp -s - $@ || echo '$(DEFINES)' > $@

$(B)/textpage.bin: $(RIP)/extracted/textpage/ram_012000_025b00.bin
	@mkdir -p $(B)
	cp $< $@

$(B)/main.bin: src/main.s src/intro.s src/textpage.s src/loader.s src/hardware.i src/memory.i $(B)/ptplayer.asm $(B)/intro.i $(B)/module.mod $(B)/textpage.bin $(B)/defines
	$(VASM) $(DEFINES) -o $@ $<
	$(PYTHON) -c "import sys; p=sys.argv[1]; d=open(p,'rb').read(); open(p,'wb').write(d.ljust(-(-len(d)//512)*512, b'\0'))" $@

$(B)/boot.bin: src/boot.s src/memory.i $(B)/main.bin
	$(VASM) -DMAIN_SIZE=$$(( $$(wc -c < $(B)/main.bin) )) \
	  -DPART1_SIZE=$$($(PYTHON) -c "import sys; print(int.from_bytes(open(sys.argv[1], 'rb').read()[4:8], 'big'))" $(B)/main.bin) -o $@ $<

$(B)/hfd.adf: tools/mkadf.py $(B)/boot.bin $(B)/main.bin
	$(PYTHON) tools/mkadf.py $(B)/boot.bin $(B)/main.bin $@

clean:
	rm -rf $(B)

.PHONY: all clean FORCE
