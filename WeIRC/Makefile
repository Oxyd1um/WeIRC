# WeIRC build file.
#
#   make          builds weirc-cli, and also weirc (GUI) if FLTK is installed
#   make cli      terminal version only   (needs only g++)
#   make gui      FLTK window version     (needs libfltk1.3-dev)
#   make clean

CXX      ?= g++
CXXFLAGS ?= -O2 -Wall -Wextra
CXXFLAGS += -std=c++17 -pthread

# Files used by BOTH versions
CORE = src/app.cpp src/irc_client.cpp src/irc_server.cpp src/servers.cpp
HDRS = $(wildcard src/*.h)

all: cli
ifneq ($(shell command -v fltk-config 2>/dev/null),)
all: gui
endif

cli: weirc-cli
gui: weirc

weirc-cli: $(CORE) src/main_cli.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) $(CORE) src/main_cli.cpp -o $@

weirc: $(CORE) src/gui.cpp src/main_gui.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) $(shell fltk-config --cxxflags) $(CORE) src/gui.cpp src/main_gui.cpp \
	    $(shell fltk-config --ldflags) -o $@

clean:
	rm -f weirc weirc-cli

.PHONY: all cli gui clean
