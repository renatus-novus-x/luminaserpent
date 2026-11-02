# LUMINA SERPENT for X68000 (elf2x68k, C++)
#
#   make            -> luminaserpent.sys (bootable, no Human68k) + .x
#   make boot       -> luminaserpent.xdf (disk whose only file is HUMAN.SYS)
#   make test       -> luminaserpent-test.xdf (starts with LASER + OPTION,
#                      near the boss, for WebX68k verification only)
#
# Toolchain: PATH must include the elf2x68k m68k-xelf/bin directory.

HUMAN68K_XDF ?= $(HOME)/workspace/x68k/webx68k/human302.xdf

CROSS = m68k-xelf-
CXX = $(CROSS)g++
CXXFLAGS = -m68000 -O2 -g -Wall

# make PARALLAX=1 -> enable the pseudo-parallax scrolling (heavier;
# off by default, see the PARALLAX_SCROLL section in the source)
ifeq ($(PARALLAX),1)
CXXFLAGS += -DPARALLAX_SCROLL
endif

TARGETS = luminaserpent.sys luminaserpent.x

all: $(TARGETS)

luminaserpent.sys: luminaserpent_sys.o
	$(CXX) -Ttext=0x6800 -s -specs=x68knodos.specs -o $@ $^

luminaserpent_sys.o: luminaserpent.cpp
	$(CXX) $(CXXFLAGS) -DSTANDALONE -c $< -o $@

luminaserpent.x: luminaserpent_x.o
	$(CXX) -o $@ $^

luminaserpent_x.o: luminaserpent.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

luminaserpent_test.o: luminaserpent.cpp
	$(CXX) $(CXXFLAGS) -DSTANDALONE -DSTG_TEST -c $< -o $@

luminaserpent-test.sys: luminaserpent_test.o
	$(CXX) -Ttext=0x6800 -s -specs=x68knodos.specs -o $@ $^

luminaserpent.xdf: luminaserpent.sys tools/make_boot_xdf.py
	python3 tools/make_boot_xdf.py luminaserpent.sys luminaserpent.xdf "$(HUMAN68K_XDF)"

luminaserpent-test.xdf: luminaserpent-test.sys tools/make_boot_xdf.py
	python3 tools/make_boot_xdf.py luminaserpent-test.sys luminaserpent-test.xdf "$(HUMAN68K_XDF)"

boot: luminaserpent.xdf

test: luminaserpent-test.xdf

clean:
	-rm -f *.o *.elf* $(TARGETS) luminaserpent-test.sys luminaserpent.xdf luminaserpent-test.xdf

.PHONY: all boot test clean
