# for debugging, don't call into this directly, use the makefile in bin/ instead.
# also handles some global settings for compilers and debug flags.

.PHONY:all src clean
include bin/config.mk

# dr dobb's idea about makefile debugging:
OLD_SHELL := $(SHELL)
# SHELL = $(warning [$@ ($^) ($?)])$(OLD_SHELL)
SHELL = $(warning [$@ ($?)])$(OLD_SHELL)
export OPT_CFLAGS OPT_LDFLAGS CC CXX GLSLC AR OLD_SHELL SHELL

all: src

# overwrites the above optimised build flags:
debug:OPT_CFLAGS=-g -gdwarf-2 -ggdb3 -O0
debug:OPT_LDFLAGS=
debug:all

src: ext Makefile
	$(MAKE) -C src

clean:
	$(MAKE) -C src/ clean
