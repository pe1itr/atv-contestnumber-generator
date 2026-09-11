CC = x86_64-w64-mingw32-gcc
WINDRES = x86_64-w64-mingw32-windres
CFLAGS = -std=c11 -O2 -Wall -Wextra -Werror
LDLIBS = -lwindowscodecs -lole32 -loleaut32 -luuid -lgdi32 -lcomctl32 -lbcrypt

.PHONY: all windows linux test test-linux test-ui-linux
all: linux windows
windows: dist/atv-contestnummer.exe
linux: dist/atv-contestnummer

dist/atv-contestnummer: src/linux.c src/core.c src/core.h src/app_info.c src/app_info.h src/pm5544.h build/pm5544-linux.o
	mkdir -p dist
	$(HOSTCC) $(CFLAGS) src/linux.c src/core.c src/app_info.c build/pm5544-linux.o -o $@ $(shell pkg-config --cflags --libs gtk+-3.0 pangocairo)

build/pm_assets.h: tools/embed_pm5544.py assets/pm5544.jpg assets/pm5544w.jpg
	python3 tools/embed_pm5544.py

build/pm5544-linux.o: src/pm5544.c src/pm5544.h build/pm_assets.h
	$(HOSTCC) $(CFLAGS) -Ibuild -c src/pm5544.c -o $@

build/pm5544-windows.o: src/pm5544.c src/pm5544.h build/pm_assets.h
	$(CC) $(CFLAGS) -Ibuild -c src/pm5544.c -o $@

HOSTCC ?= cc

test-linux: linux test
	./dist/atv-contestnummer --smoke-test build/smoke-linux
	python3 tests/check_images.py build/smoke-linux

build/test_linux_ui: tests/test_linux_ui.c src/linux.c src/core.c src/core.h src/app_info.c src/app_info.h src/pm5544.h build/pm5544-linux.o
	$(HOSTCC) $(CFLAGS) tests/test_linux_ui.c src/core.c src/app_info.c build/pm5544-linux.o -o $@ $(shell pkg-config --cflags --libs gtk+-3.0 pangocairo)

build/test_windows_ui.exe: tests/test_windows_ui.c src/main.c src/core.c src/core.h src/app_info.c src/app_info.h src/pm5544.h build/app.o build/pm5544-windows.o
	$(CC) $(CFLAGS) -municode -static tests/test_windows_ui.c src/core.c src/app_info.c build/app.o build/pm5544-windows.o -o $@ $(LDLIBS)

test-ui-linux: build/test_linux_ui
	timeout 30s ./build/test_linux_ui

build/app.o: src/app.rc src/resource.h src/app.manifest
	mkdir -p build
	$(WINDRES) -Isrc src/app.rc $@

dist/atv-contestnummer.exe: src/main.c src/core.c src/core.h src/resource.h src/app_info.c src/app_info.h src/pm5544.h build/app.o build/pm5544-windows.o
	mkdir -p dist
	$(CC) $(CFLAGS) -municode -mwindows -static -s src/main.c src/core.c src/app_info.c build/app.o build/pm5544-windows.o -o $@ $(LDLIBS)

test: build/pm5544-linux.o
	mkdir -p build
	cc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_core.c src/core.c -o build/test_core
	./build/test_core
	$(HOSTCC) $(CFLAGS) -Isrc tests/test_pm5544.c src/core.c build/pm5544-linux.o -o build/test_pm5544
	./build/test_pm5544
