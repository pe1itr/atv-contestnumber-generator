CXX ?= c++
CC ?= cc
PKGS = gtk+-3.0 gstreamer-app-1.0 gstreamer-rtsp-server-1.0 gstreamer-sdp-1.0 gstreamer-rtsp-1.0
PKG_CONFIG ?= pkg-config
# Optional local development SDK; ordinary installations use system pkg-config.
SDK_ROOT ?= build/sdk
PKG_ENV = PKG_CONFIG_PATH="$(abspath $(SDK_ROOT)/usr/lib64/pkgconfig):$(PKG_CONFIG_PATH)"
CPPFLAGS += -I../src $(shell $(PKG_ENV) $(PKG_CONFIG) --cflags $(PKGS))
CXXFLAGS += -std=c++17 -O2 -g -Wall -Wextra -Werror
LDLIBS += $(shell $(PKG_ENV) $(PKG_CONFIG) --libs $(PKGS)) -pthread -Wl,-rpath,'$$ORIGIN/lib:$$ORIGIN/../bin/lib'
.PHONY: all test clean
all: bin/datv-streambridge
build/core.o: ../src/core.c ../src/core.h ../src/datv.h
	mkdir -p build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -I../src -c $< -o $@
bin/datv-streambridge: src/main.cpp src/engine.cpp src/engine.hpp src/mux.hpp src/network.hpp src/avc.hpp src/input.hpp src/rtsp.hpp build/core.o
	mkdir -p bin
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) src/main.cpp src/engine.cpp build/core.o $(LDLIBS) -o $@
test: all test-unit
	python3 checks/integration.py
clean:
	rm -rf build bin

build/test-transport: checks/test_transport.cpp src/avc.hpp src/mux.hpp src/network.hpp
	mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Isrc $< $(LDLIBS) -o $@
build/test-ui: checks/test_ui.cpp src/main.cpp src/engine.cpp src/engine.hpp src/avc.hpp src/mux.hpp src/network.hpp src/input.hpp src/rtsp.hpp build/core.o
	mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Isrc $< src/engine.cpp build/core.o $(LDLIBS) -o $@
.PHONY: test-unit test-ui
test-unit: build/test-transport
	build/test-transport
test-ui: build/test-ui
	build/test-ui

.PHONY: test-rtsp
test-rtsp: all
	python3 checks/rtsp_smoke.py

build/larix-publisher: checks/larix_publisher.cpp
	mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< $(LDLIBS) -o $@
