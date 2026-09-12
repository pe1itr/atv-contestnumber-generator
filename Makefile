CC = x86_64-w64-mingw32-gcc
WINDRES = x86_64-w64-mingw32-windres
CFLAGS = -std=c11 -O2 -Wall -Wextra -Werror
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Werror
WINCXX = x86_64-w64-mingw32-g++
WINAR = x86_64-w64-mingw32-ar
PYTHON ?= python3
LDLIBS = -lwindowscodecs -lole32 -loleaut32 -luuid -lgdi32 -lcomctl32 -lcomdlg32 -lbcrypt -lws2_32 -lwinmm

.PHONY: all windows linux
.PRECIOUS: build/openh264-%/Makefile
all: linux windows
windows: dist/atv-contestnummer.exe
linux: dist/atv-contestnummer

dist/atv-contestnummer: src/linux.c src/core.c src/core.h src/app_info.c src/app_info.h third_party/openh264/license_text.inc src/pm5544.h src/datv.h build/pm5544-linux.o build/datv-linux.o build/openh264-linux/libopenh264.a
	mkdir -p dist
	$(HOSTCC) $(CFLAGS) src/linux.c src/core.c src/app_info.c build/pm5544-linux.o build/datv-linux.o build/openh264-linux/libopenh264.a -o $@ $(shell pkg-config --cflags --libs gtk+-3.0 pangocairo) -lstdc++ -lpthread -lm

build/pm_assets.h: tools/embed_pm5544.py assets/pm5544.jpg assets/pm5544w.jpg assets/FuBK-Testbild.png assets/FuBK_wide.jpg
	$(PYTHON) tools/embed_pm5544.py

build/pm5544-linux.o: src/pm5544.c src/pm5544.h build/pm_assets.h
	$(HOSTCC) $(CFLAGS) -Ibuild -c src/pm5544.c -o $@

build/pm5544-windows.o: src/pm5544.c src/pm5544.h build/pm_assets.h
	$(CC) $(CFLAGS) -Ibuild -c src/pm5544.c -o $@

HOSTCC ?= cc

build/app.o: src/app.rc src/resource.h src/app.manifest
	mkdir -p build
	$(WINDRES) -Isrc src/app.rc $@

dist/atv-contestnummer.exe: src/main.c src/core.c src/core.h src/resource.h src/app_info.c src/app_info.h third_party/openh264/license_text.inc src/pm5544.h src/datv.h build/app.o build/pm5544-windows.o build/datv-windows.o build/openh264-windows/libopenh264.a
	mkdir -p dist
	$(CC) $(CFLAGS) -municode -mwindows -static -s src/main.c src/core.c src/app_info.c build/app.o build/pm5544-windows.o build/datv-windows.o build/openh264-windows/libopenh264.a -o $@ $(LDLIBS) -lstdc++ -lwinpthread -lssp -lshell32

build/openh264-%/Makefile: tools/prepare_openh264.py
	$(PYTHON) tools/prepare_openh264.py $*

build/openh264-linux/libopenh264.a: build/openh264-linux/Makefile
	$(MAKE) -C build/openh264-linux CC=$(HOSTCC) CXX=$(CXX) libopenh264.a
	touch $@

build/openh264-windows/libopenh264.a: build/openh264-windows/Makefile
	$(MAKE) -C build/openh264-windows OS=mingw_nt ARCH=x86_64 CC=$(CC) CXX=$(WINCXX) AR=$(WINAR) libopenh264.a
	touch $@

build/datv-linux.o: src/datv.cpp src/datv.h src/core.h build/openh264-linux/Makefile
	$(CXX) $(CXXFLAGS) -Ibuild/openh264-linux/codec/api -c $< -o $@

build/datv-windows.o: src/datv.cpp src/datv.h src/core.h build/openh264-windows/Makefile
	$(WINCXX) $(CXXFLAGS) -Ibuild/openh264-windows/codec/api -c $< -o $@

.PHONY: test-linux test-ts
build/test-core: checks/test_core.c src/core.c src/core.h src/datv.h
	mkdir -p build
	$(HOSTCC) $(CFLAGS) -Isrc checks/test_core.c src/core.c -o $@

test-linux: linux build/test-core build/test-config
	build/test-core
	build/test-config
	$(PYTHON) tools/test_linux_images.py

test-ts: linux
	$(PYTHON) checks/check_ts.py

build/test-datv-linux-ui: checks/test_datv_linux_ui.c src/linux.c src/core.c src/app_info.c src/datv.h build/pm5544-linux.o build/datv-linux.o build/openh264-linux/libopenh264.a
	$(HOSTCC) $(CFLAGS) -Isrc checks/test_datv_linux_ui.c src/core.c src/app_info.c build/pm5544-linux.o build/datv-linux.o build/openh264-linux/libopenh264.a -o $@ $(shell pkg-config --cflags --libs gtk+-3.0 pangocairo) -lstdc++ -lpthread -lm

build/test-datv-windows-ui.exe: checks/test_datv_windows_ui.c src/main.c src/core.c src/app_info.c src/datv.h build/app.o build/pm5544-windows.o build/datv-windows.o build/openh264-windows/libopenh264.a
	$(CC) $(CFLAGS) -municode -static -Isrc checks/test_datv_windows_ui.c src/core.c src/app_info.c build/app.o build/pm5544-windows.o build/datv-windows.o build/openh264-windows/libopenh264.a -o $@ $(LDLIBS) -lstdc++ -lwinpthread -lssp -lshell32

build/core-linux.o: src/core.c src/core.h src/datv.h
	$(HOSTCC) $(CFLAGS) -c $< -o $@
build/core-windows.o: src/core.c src/core.h src/datv.h
	$(CC) $(CFLAGS) -c $< -o $@
build/udp-sender: checks/udp_sender.cpp src/datv.h build/core-linux.o build/datv-linux.o build/openh264-linux/libopenh264.a
	$(CXX) $(CXXFLAGS) -Isrc checks/udp_sender.cpp build/core-linux.o build/datv-linux.o build/openh264-linux/libopenh264.a -lpthread -o $@
build/udp-sender.exe: checks/udp_sender.cpp src/datv.h build/core-windows.o build/datv-windows.o build/openh264-windows/libopenh264.a
	$(WINCXX) $(CXXFLAGS) -static -Isrc checks/udp_sender.cpp build/core-windows.o build/datv-windows.o build/openh264-windows/libopenh264.a -lws2_32 -lwinmm -lwinpthread -lssp -o $@
.PHONY: test-udp
test-udp: build/udp-sender build/test-udp-errors
	build/test-udp-errors
	$(PYTHON) checks/check_udp.py

build/test-udp-errors: checks/test_udp_errors.cpp src/datv.cpp src/datv.h src/core.h build/core-linux.o build/openh264-linux/libopenh264.a
	$(CXX) $(CXXFLAGS) -Isrc -Ibuild/openh264-linux/codec/api $< build/core-linux.o build/openh264-linux/libopenh264.a -lpthread -o $@
build/test-udp-errors.exe: checks/test_udp_errors.cpp src/datv.cpp src/datv.h src/core.h build/core-windows.o build/openh264-windows/libopenh264.a
	$(WINCXX) $(CXXFLAGS) -static -Isrc -Ibuild/openh264-windows/codec/api $< build/core-windows.o build/openh264-windows/libopenh264.a -lws2_32 -lwinmm -lwinpthread -lssp -o $@

build/test-config: checks/test_config.c src/core.c src/core.h src/datv.h
	$(HOSTCC) $(CFLAGS) -Isrc checks/test_config.c src/core.c -o $@
build/test-config.exe: checks/test_config.c src/core.c src/core.h src/datv.h
	$(CC) $(CFLAGS) -static -Isrc checks/test_config.c src/core.c -o $@

build/test-config-linux-ui: checks/test_config_linux_ui.c src/linux.c src/core.c src/app_info.c src/datv.h build/pm5544-linux.o build/datv-linux.o build/openh264-linux/libopenh264.a
	$(HOSTCC) $(CFLAGS) -Isrc checks/test_config_linux_ui.c src/core.c src/app_info.c build/pm5544-linux.o build/datv-linux.o build/openh264-linux/libopenh264.a -o $@ $(shell pkg-config --cflags --libs gtk+-3.0 pangocairo) -lstdc++ -lpthread -lm
build/test-config-windows-ui.exe: checks/test_config_windows_ui.c src/main.c src/core.c src/app_info.c src/datv.h build/app.o build/pm5544-windows.o build/datv-windows.o build/openh264-windows/libopenh264.a
	$(CC) $(CFLAGS) -municode -static -Isrc checks/test_config_windows_ui.c src/core.c src/app_info.c build/app.o build/pm5544-windows.o build/datv-windows.o build/openh264-windows/libopenh264.a -o $@ $(LDLIBS) -lstdc++ -lwinpthread -lssp -lshell32
