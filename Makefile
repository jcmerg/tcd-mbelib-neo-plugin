# SPDX-License-Identifier: GPL-3.0-or-later
#
# mbelib-neo, a codec plugin for tcd (urfd-ng)
#
#   make                      (downloads and builds mbelib-neo in ./mbelib-neo-<version> the first time)
#   make check                (loads the plugin and runs each codec, see tests/smoke.cpp)
#   sudo make install         (to /usr/local/lib/tcd/)
# tcd.ini then loads it:
#   CodecPlugin = /usr/local/lib/tcd/mbelib_neo_codec.so
#
# mbelib-neo is linked into the plugin statically, MBELIB= is where
# build-mbelib-neo.sh installed it, MBELIB_SRC= its source (transcode,
# mbelib_neo_transcode.c, uses its private headers); make clean leaves both
# there. Its version is VER= in build-mbelib-neo.sh, the one transcode is
# written for MBE_TRANSCODE_MBELIB in mbelib_neo_transcode.h. CFLAGS,
# CXXFLAGS, CPPFLAGS and LDFLAGS from the environment or the command line are
# taken (the default is -O2), WERROR=1 makes warnings errors, DESTDIR=
# installs into a staging directory.

# CodecPlugin.h: the copy here, or TCD=/path/to/urfd-ng/tcd for the one of a checkout
TCD ?= .
MBELIB_VER := $(shell sed -n 's/^VER=//p' build-mbelib-neo.sh)
MBELIB ?= mbelib-neo-$(MBELIB_VER)/install
MBELIB_SRC ?= $(MBELIB)/../src
PREFIX ?= /usr/local
PLUGINDIR ?= $(PREFIX)/lib/tcd

CFLAGS ?= -O2
CXXFLAGS ?= -O2
CPPFLAGS ?=
LDFLAGS ?=
override CFLAGS += -Wall -Wextra -std=gnu11 -fPIC -MMD -MP
override CXXFLAGS += -Wall -Wextra -std=c++17 -fPIC -MMD -MP
override CPPFLAGS += -I$(TCD) -I$(MBELIB)/include
ifeq ($(WERROR),1)
override CFLAGS += -Werror
override CXXFLAGS += -Werror
endif
OBJS = mbelib_neo_plugin.o mbelib_neo_transcode.o
PLUGIN = mbelib_neo_codec.so

all : $(PLUGIN)

# only tcd_codec_plugin leaves the plugin, mbelib-neo's own symbols stay
# inside; an undefined symbol shows when linking rather than in tcd's dlopen
ifeq ($(shell uname -s),Darwin)
LDEXPORT = -Wl,-exported_symbol,_tcd_codec_plugin
else
LDEXPORT = -Wl,--version-script=plugin.map -Wl,--no-undefined
endif

$(PLUGIN) : $(OBJS) $(MBELIB)/lib/libmbe-neo.a plugin.map
	$(CXX) $(LDFLAGS) -shared $(OBJS) $(MBELIB)/lib/libmbe-neo.a -o $@ $(LDEXPORT) -lm

# mbelib-neo first: its headers come with its install (also with make -j)
mbelib_neo_plugin.o : mbelib_neo_plugin.cpp mbelib_neo_transcode.h $(TCD)/CodecPlugin.h Makefile | $(MBELIB)/lib/libmbe-neo.a
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -fvisibility=hidden -c $< -o $@

# against mbelib-neo's private headers, of the version transcode is written for
# (mbelib_neo_plugin.cpp checks the installed library's)
MBELIB_PIN := $(shell sed -n 's/^\#define MBE_TRANSCODE_MBELIB "\(.*\)".*/\1/p' mbelib_neo_transcode.h)
mbelib_neo_transcode.o : mbelib_neo_transcode.c mbelib_neo_transcode.h Makefile | $(MBELIB)/lib/libmbe-neo.a $(MBELIB_SRC)/src/internal/ambe_encoder.h
	@grep -q '^project(mbelib-neo VERSION $(MBELIB_PIN) ' "$(MBELIB_SRC)/CMakeLists.txt" || { echo "the mbelib-neo source in $(MBELIB_SRC) is not $(MBELIB_PIN), which mbelib_neo_transcode.c is written for" >&2; exit 1; }
	$(CC) $(CPPFLAGS) -I$(MBELIB_SRC)/src/internal $(CFLAGS) -fvisibility=hidden -c $< -o $@

# without mbelib-neo, or without its source: fetch and build it here (the
# default location only). The source after the library, so that make -j runs
# the script once.
MBELIB_HERE = [ "$(MBELIB)" = "mbelib-neo-$(MBELIB_VER)/install" ] && [ "$(MBELIB_SRC)" = "$(MBELIB)/../src" ]
$(MBELIB)/lib/libmbe-neo.a :
	@$(MBELIB_HERE) || { echo "mbelib-neo is missing in $(MBELIB) (MBELIB=)" >&2; exit 1; }
	./build-mbelib-neo.sh
$(MBELIB_SRC)/src/internal/ambe_encoder.h : | $(MBELIB)/lib/libmbe-neo.a
	@[ -f "$@" ] || $(MBELIB_HERE) || { echo "mbelib-neo's source is missing in $(MBELIB_SRC) (MBELIB_SRC=)" >&2; exit 1; }
	@[ -f "$@" ] || ./build-mbelib-neo.sh

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
