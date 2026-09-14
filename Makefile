CC := xcrun clang
CPPFLAGS := -DSOKOL_METAL -Ivendor/sokol
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -g -O0
FRAMEWORKS := -framework Cocoa -framework QuartzCore -framework Metal -framework MetalKit
HEADERS := $(wildcard vendor/sokol/*.h) src/cube_shader.h

.PHONY: all run smoke-test clean
all: build/yard

build:
	mkdir -p build

build/main.o: src/main.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build/sokol.o: src/sokol.m $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -fobjc-arc -c $< -o $@

build/yard: build/main.o build/sokol.o
	$(CC) $^ $(FRAMEWORKS) -o $@

run: build/yard
	./build/yard

smoke-test: build/yard
	./build/yard --smoke-test

clean:
	rm -rf build
