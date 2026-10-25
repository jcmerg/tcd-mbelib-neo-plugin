# mbelib-neo, a codec plugin for tcd (urfd-ng)
#
#   ./build-mbelib-neo.sh     (once: downloads and builds mbelib-neo 2.6.2 in ./mbelib-neo-2.6.2)
#   make
#   sudo make install         (to /usr/local/lib/tcd/)
# tcd.ini then loads it:
#   CodecPlugin = /usr/local/lib/tcd/mbelib_neo_codec.so
#
# mbelib-neo is linked into the plugin statically, MBELIB= is where
# build-mbelib-neo.sh installed it.

# CodecPlugin.h: the copy here, or TCD=/path/to/urfd-ng/tcd for the one of a checkout
TCD ?= .
MBELIB ?= mbelib-neo-2.6.2/install
PLUGINDIR ?= /usr/local/lib/tcd

CXXFLAGS = -O2 -W -Werror -std=c++17 -fPIC -I$(TCD) -I$(MBELIB)/include
PLUGIN = mbelib_neo_codec.so

all : $(PLUGIN)

$(PLUGIN) : mbelib_neo_plugin.o $(MBELIB)/lib/libmbe-neo.a
	$(CXX) -shared mbelib_neo_plugin.o $(MBELIB)/lib/libmbe-neo.a -o $@ -lm

mbelib_neo_plugin.o : mbelib_neo_plugin.cpp $(TCD)/CodecPlugin.h Makefile
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(MBELIB)/lib/libmbe-neo.a :
	@echo "mbelib-neo is missing in $(MBELIB): run ../../tools/build-mbelib-neo.sh first" >&2; exit 1

clean :
	$(RM) *.o $(PLUGIN)

install : $(PLUGIN)
	mkdir -p $(PLUGINDIR)
	cp -f $(PLUGIN) $(PLUGINDIR)/

uninstall :
	$(RM) $(PLUGINDIR)/$(PLUGIN)
