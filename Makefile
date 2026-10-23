# LUMINA SERPENT for X68000 (elf2x68k, C++)
#
#   make            -> luminaserpent.sys (bootable, no Human68k) + luminaserpent.x
#   make boot       -> luminaserpent.xdf  (disk whose only file is HUMAN.SYS)
#   make xdisk      -> luminaserpent-x.xdf (Human68k disk, AUTOEXEC runs the .X)
#   make test       -> luminaserpent-test.xdf (verification build, see README.md)
#
# Toolchain: PATH must include the elf2x68k m68k-xelf/bin directory.
# The xdf targets need a Human68k 3.02 disk image as a template;
# override its location with: make boot HUMAN68K_XDF=/path/to/human302.xdf

CROSS = m68k-xelf-
CXX = $(CROSS)g++
CXXFLAGS = -m68000 -O2 -g -Wall
HUMAN68K_XDF ?= $(HOME)/workspace/x68k/webx68k/human302.xdf

TARGETS = luminaserpent.sys luminaserpent.x

all: $(TARGETS)

luminaserpent.sys: luminaserpent_sys.o
	$(CXX) -Ttext=0x6800 -s -specs=x68knodos.specs -o $@ $^

luminaserpent_sys.o: luminaserpent.cpp earth_bg.h
	$(CXX) $(CXXFLAGS) -DSTANDALONE -c $< -o $@

luminaserpent.x: luminaserpent_x.o
	$(CXX) -o $@ $^

luminaserpent_x.o: luminaserpent.cpp earth_bg.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

luminaserpent_test.o: luminaserpent.cpp earth_bg.h
	$(CXX) $(CXXFLAGS) -DSTANDALONE -DSTG_TEST -c $< -o $@

luminaserpent-test.sys: luminaserpent_test.o
	$(CXX) -Ttext=0x6800 -s -specs=x68knodos.specs -o $@ $^

luminaserpent.xdf: luminaserpent.sys tools/make_boot_xdf.py
	python3 tools/make_boot_xdf.py luminaserpent.sys luminaserpent.xdf "$(HUMAN68K_XDF)"

luminaserpent-test.xdf: luminaserpent-test.sys tools/make_boot_xdf.py
	python3 tools/make_boot_xdf.py luminaserpent-test.sys luminaserpent-test.xdf "$(HUMAN68K_XDF)"

luminaserpent-x.xdf: luminaserpent.x tools/make_x_xdf.py
	python3 tools/make_x_xdf.py luminaserpent.x luminaserpent-x.xdf "$(HUMAN68K_XDF)"

boot: luminaserpent.xdf

test: luminaserpent-test.xdf

xdisk: luminaserpent-x.xdf

clean:
	-rm -f *.o *.elf* $(TARGETS) luminaserpent-test.sys luminaserpent.xdf luminaserpent-test.xdf luminaserpent-x.xdf

.PHONY: all boot test xdisk clean
