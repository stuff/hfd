# High Fidelity Dreams, Amiga port: build/hfd.adf.
# The tools come from tools/setup.sh (vasm, vlink and capture in tools/.build).

TOOLS := tools/.build
VASM := $(TOOLS)/src/vasm/vasmm68k_mot
PTPLAYER := $(TOOLS)/src/ptplayer/ptplayer.asm
RIP := ../../rip
MODULE := $(RIP)/music/01 - Act of Impulse (Mantronix & Tip).mod
B := build

all: $(B)/hfd.adf

$(B)/data.i $(B)/ear.raw: tools/mkdata.py $(RIP)/graphics/pictures/loading_screen_indexed.png
	python3 tools/mkdata.py $(RIP) $(B)

# ptplayer.asm ends with END, which would stop the assembly of main.s.
$(B)/ptplayer.asm: $(PTPLAYER)
	@mkdir -p $(B)
	sed '/^[[:space:]]*end[[:space:]]*$$/d' $< > $@

# (Make can't depend on a file name with spaces.)
$(B)/module.mod:
	@mkdir -p $(B)
	cp "$(MODULE)" $@

$(B)/main.bin: src/main.s $(B)/ptplayer.asm $(B)/data.i $(B)/ear.raw $(B)/module.mod
	$(VASM) -quiet -m68000 -Fbin -I$(B) -o $@ $<
	python3 -c "import sys; p=sys.argv[1]; d=open(p,'rb').read(); open(p,'wb').write(d.ljust(-(-len(d)//512)*512, b'\0'))" $@

$(B)/boot.bin: src/boot.s $(B)/main.bin
	$(VASM) -quiet -m68000 -Fbin -DMAIN_SIZE=$$(stat -f%z $(B)/main.bin 2>/dev/null || stat -c%s $(B)/main.bin) -o $@ $<

$(B)/hfd.adf: tools/mkadf.py $(B)/boot.bin $(B)/main.bin
	python3 tools/mkadf.py $(B)/boot.bin $(B)/main.bin $@

clean:
	rm -rf $(B)

.PHONY: all clean
