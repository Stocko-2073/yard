CC := xcrun clang
CXX := xcrun clang++
CPPFLAGS := -DSOKOL_METAL -Ivendor/sokol -Ibuild/generated
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -g -O0
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -g -O0
FRAMEWORKS := -framework Cocoa -framework QuartzCore -framework Metal -framework MetalKit
HEADERS := $(wildcard vendor/sokol/*.h)
SHDC := tools/bin/sokol-shdc
SHADER_LANGS := metal_macos:metal_ios:metal_sim:hlsl5:glsl430:glsl300es:wgsl
SHADER_HEADER := build/generated/cube.glsl.h

.DELETE_ON_ERROR:
.PHONY: all run smoke-test test clean setup-tools shaders
all: build/yard

build:
	mkdir -p build

build/main.o: src/main.cpp src/astronomy.h src/skyglow.h src/camera.h $(HEADERS) $(SHADER_HEADER) | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

build/sokol.o: src/sokol.m $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -fobjc-arc -c $< -o $@

build/astronomy.o: src/astronomy.cpp src/astronomy.h | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/skyglow.o: src/skyglow.cpp src/skyglow.h src/astronomy.h | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/camera.o: src/camera.cpp src/camera.h | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/yard: build/main.o build/sokol.o build/astronomy.o build/skyglow.o build/camera.o
	$(CXX) $^ $(FRAMEWORKS) -o $@

run: build/yard
	./build/yard

smoke-test: build/yard
	./build/yard --smoke-test

build/astronomy-test: tests/astronomy_test.cpp src/astronomy.cpp src/astronomy.h | build
	$(CXX) $(CXXFLAGS) -Isrc tests/astronomy_test.cpp src/astronomy.cpp -o $@

build/skyglow-test: tests/skyglow_test.cpp src/skyglow.cpp src/skyglow.h src/astronomy.h | build
	$(CXX) $(CXXFLAGS) -Isrc tests/skyglow_test.cpp src/skyglow.cpp -o $@

build/camera-test: tests/camera_test.cpp src/camera.cpp src/camera.h | build
	$(CXX) $(CXXFLAGS) -Isrc tests/camera_test.cpp src/camera.cpp -o $@

test: build/astronomy-test build/skyglow-test build/camera-test
	./build/astronomy-test
	./build/skyglow-test
	./build/camera-test
	PYTHONDONTWRITEBYTECODE=1 python3 tests/skyglow_import_test.py

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
