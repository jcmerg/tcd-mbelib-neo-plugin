# mbelib-neo, a codec plugin for tcd (urfd-ng)
#
#   make                      (downloads and builds mbelib-neo 2.6.4 in ./mbelib-neo-2.6.4 the first time)
#   sudo make install         (to /usr/local/lib/tcd/)
# tcd.ini then loads it:
#   CodecPlugin = /usr/local/lib/tcd/mbelib_neo_codec.so
#
# mbelib-neo is linked into the plugin statically, MBELIB= is where
# build-mbelib-neo.sh installed it.

# CodecPlugin.h: the copy here, or TCD=/path/to/urfd-ng/tcd for the one of a checkout
TCD ?= .
MBELIB ?= mbelib-neo-2.6.4/install
PLUGINDIR ?= /usr/local/lib/tcd

CXXFLAGS = -O2 -W -Werror -std=c++17 -fPIC -I$(TCD) -I$(MBELIB)/include
PLUGIN = mbelib_neo_codec.so

all : $(PLUGIN)

$(PLUGIN) : mbelib_neo_plugin.o $(MBELIB)/lib/libmbe-neo.a
	$(CXX) -shared mbelib_neo_plugin.o $(MBELIB)/lib/libmbe-neo.a -o $@ -lm

# mbelib-neo first: its headers come with its install (also with make -j)
mbelib_neo_plugin.o : mbelib_neo_plugin.cpp $(TCD)/CodecPlugin.h Makefile | $(MBELIB)/lib/libmbe-neo.a
	$(CXX) $(CXXFLAGS) -c $< -o $@

# without mbelib-neo: fetch and build it here (the default location only)
$(MBELIB)/lib/libmbe-neo.a :
	@[ "$(MBELIB)" = "mbelib-neo-2.6.4/install" ] || { echo "mbelib-neo is missing in $(MBELIB)" >&2; exit 1; }
	./build-mbelib-neo.sh

clean :
	$(RM) *.o $(PLUGIN)

install : $(PLUGIN)
	mkdir -p $(PLUGINDIR)
	cp -f $(PLUGIN) $(PLUGINDIR)/

uninstall :
	$(RM) $(PLUGINDIR)/$(PLUGIN)
