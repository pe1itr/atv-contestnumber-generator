CXX ?= c++
CC ?= cc
PKGS = gtk+-3.0 gstreamer-app-1.0 gstreamer-video-1.0 pangocairo
SDK_ROOT ?= ../streambridge/build/sdk
PKG_ENV = PKG_CONFIG_PATH="$(abspath $(SDK_ROOT)/usr/lib64/pkgconfig):$(PKG_CONFIG_PATH)"
CPPFLAGS += -I../src -I../streambridge/src $(shell $(PKG_ENV) pkg-config --cflags $(PKGS))
CXXFLAGS += -std=c++17 -O2 -g -Wall -Wextra -Werror
LDLIBS += $(shell $(PKG_ENV) pkg-config --libs $(PKGS)) -pthread
.PHONY: all test
all: bin/datv-camera ../dist/datv-camera
build/core.o: ../src/core.c ../src/core.h ../src/datv.h
	mkdir -p build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -I../src -c $< -o $@
bin/datv-camera: src/main.cpp src/engine.cpp src/settings.cpp src/engine.hpp ../streambridge/src/mux.hpp ../streambridge/src/avc.hpp build/core.o
	mkdir -p bin
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) src/main.cpp src/engine.cpp src/settings.cpp build/core.o $(LDLIBS) -o $@
../dist/datv-camera: bin/datv-camera
	mkdir -p ../dist
	cp $< $@
test: all
	python3 checks/integration.py

build/test-ui: checks/test_ui.cpp src/main.cpp src/engine.cpp src/settings.cpp src/engine.hpp ../streambridge/src/mux.hpp ../streambridge/src/avc.hpp build/core.o
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) checks/test_ui.cpp src/engine.cpp src/settings.cpp build/core.o $(LDLIBS) -o $@
.PHONY: test-ui
test-ui: build/test-ui
	build/test-ui

build/test-mux: checks/test_mux.cpp ../streambridge/src/mux.hpp build/core.o
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< build/core.o $(LDLIBS) -o $@
.PHONY: test-unit
test-unit: build/test-mux
	build/test-mux
test: test-unit
