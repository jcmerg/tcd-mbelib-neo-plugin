# SPDX-License-Identifier: GPL-3.0-or-later
#
# mbelib-neo, a codec plugin for tcd (urfd-ng)
#
#   make                      (downloads and builds mbelib-neo 2.6.5 in ./mbelib-neo-2.6.5 the first time)
#   make check                (loads the plugin and runs each codec, see tests/smoke.cpp)
#   sudo make install         (to /usr/local/lib/tcd/)
# tcd.ini then loads it:
#   CodecPlugin = /usr/local/lib/tcd/mbelib_neo_codec.so
#
# mbelib-neo is linked into the plugin statically, MBELIB= is where
# build-mbelib-neo.sh installed it; make clean leaves it there. CXXFLAGS,
# CPPFLAGS and LDFLAGS from the environment or the command line are taken (the
# default is -O2), WERROR=1 makes warnings errors, DESTDIR= installs into a
# staging directory.

# CodecPlugin.h: the copy here, or TCD=/path/to/urfd-ng/tcd for the one of a checkout
TCD ?= .
MBELIB ?= mbelib-neo-2.6.5/install
PREFIX ?= /usr/local
PLUGINDIR ?= $(PREFIX)/lib/tcd

CXXFLAGS ?= -O2
CPPFLAGS ?=
LDFLAGS ?=
override CXXFLAGS += -Wall -Wextra -std=c++17 -fPIC -MMD -MP
override CPPFLAGS += -I$(TCD) -I$(MBELIB)/include
ifeq ($(WERROR),1)
override CXXFLAGS += -Werror
endif
PLUGIN = mbelib_neo_codec.so

all : $(PLUGIN)

# only tcd_codec_plugin leaves the plugin, mbelib-neo's own symbols stay
# inside; an undefined symbol shows when linking rather than in tcd's dlopen
ifeq ($(shell uname -s),Darwin)
LDEXPORT = -Wl,-exported_symbol,_tcd_codec_plugin
else
LDEXPORT = -Wl,--version-script=plugin.map -Wl,--no-undefined
endif

$(PLUGIN) : mbelib_neo_plugin.o $(MBELIB)/lib/libmbe-neo.a plugin.map
	$(CXX) $(LDFLAGS) -shared mbelib_neo_plugin.o $(MBELIB)/lib/libmbe-neo.a -o $@ $(LDEXPORT) -lm

# mbelib-neo first: its headers come with its install (also with make -j)
mbelib_neo_plugin.o : mbelib_neo_plugin.cpp $(TCD)/CodecPlugin.h Makefile | $(MBELIB)/lib/libmbe-neo.a
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -fvisibility=hidden -c $< -o $@

# without mbelib-neo: fetch and build it here (the default location only)
$(MBELIB)/lib/libmbe-neo.a :
	@[ "$(MBELIB)" = "mbelib-neo-2.6.5/install" ] || { echo "mbelib-neo is missing in $(MBELIB)" >&2; exit 1; }
	./build-mbelib-neo.sh

# dlopen is in libSystem on macOS and libdl on Linux
tests/smoke : tests/smoke.cpp $(TCD)/CodecPlugin.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@ $(LDFLAGS) $(if $(filter Darwin,$(shell uname -s)),,-ldl)

# the plugin as tcd loads it; in tcd/plugins/<name>/ of an urfd-ng checkout the
# copy of CodecPlugin.h must also be tcd's, and the version in the READMEs the plugin's
check : $(PLUGIN) tests/smoke
	./tests/smoke ./$(PLUGIN)
	@[ ! -f ../../CodecPlugin.h ] || cmp CodecPlugin.h ../../CodecPlugin.h || { echo "CodecPlugin.h differs from ../../CodecPlugin.h (tcd's)" >&2; exit 1; }
	@v=$$(sed -n 's/^#define PLUGIN_VERSION "\(.*\)"/\1/p' mbelib_neo_plugin.cpp); \
	for r in README.md README.de.md; do \
		grep -m 1 '^- \*\*[0-9]' $$r | grep -q "^- \*\*$$v\*\*" || { echo "$$r: the newest version is not $$v" >&2; exit 1; }; \
	done

clean :
	$(RM) -r *.o *.d $(PLUGIN) tests/smoke tests/*.d *.dSYM tests/*.dSYM

install : $(PLUGIN)
	install -d "$(DESTDIR)$(PLUGINDIR)"
	install -m 0755 $(PLUGIN) "$(DESTDIR)$(PLUGINDIR)/"

uninstall :
	$(RM) "$(DESTDIR)$(PLUGINDIR)/$(PLUGIN)"

.PHONY : all check clean install uninstall

-include *.d tests/*.d
