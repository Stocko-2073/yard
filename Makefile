CC := xcrun clang
CPPFLAGS := -DSOKOL_METAL -Ivendor/sokol -Ibuild/generated
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -g -O0
FRAMEWORKS := -framework Cocoa -framework QuartzCore -framework Metal -framework MetalKit
HEADERS := $(wildcard vendor/sokol/*.h)
SHDC := tools/bin/sokol-shdc
SHADER_LANGS := metal_macos:metal_ios:metal_sim:hlsl5:glsl430:glsl300es:wgsl
SHADER_HEADER := build/generated/cube.glsl.h

.DELETE_ON_ERROR:
.PHONY: all run smoke-test clean setup-tools shaders
all: build/yard

build:
	mkdir -p build

build/main.o: src/main.c $(HEADERS) $(SHADER_HEADER) | build
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

# Dependency downloads are explicit; ordinary builds never use the network.
setup-tools:
	sh tools/setup-shdc.sh

$(SHDC):
	@echo "Missing sokol-shdc. Run 'make setup-tools' once (requires network)." >&2
	@exit 1

shaders: $(SHADER_HEADER)

$(SHADER_HEADER): shaders/cube.glsl $(SHDC) Makefile
	mkdir -p build/generated
	$(SHDC) --input $< --output $@ --slang $(SHADER_LANGS) --ifdef
