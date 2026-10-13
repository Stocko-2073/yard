CC := xcrun clang
CXX := xcrun clang++
CPPFLAGS := -DSOKOL_METAL -Ivendor/sokol -Ibuild/generated
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -g -O0
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -g -O0
FRAMEWORKS := -framework Cocoa -framework QuartzCore -framework Metal -framework MetalKit
HEADERS := $(wildcard vendor/sokol/*.h)
SHDC := tools/bin/sokol-shdc
SHADER_LANGS := metal_macos:metal_ios:metal_sim:hlsl5:glsl430:glsl300es:wgsl
SHADER_HEADER := build/generated/yard.glsl.h

.DELETE_ON_ERROR:
.PHONY: all run run-yard clean setup-tools shaders
all: build/yard

build:
	mkdir -p build

build/main.o: src/main.cpp src/tree.h src/tree_params.inc src/geometry.h src/astronomy.h src/skyglow.h src/camera.h $(HEADERS) $(SHADER_HEADER) | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

build/sokol.o: src/sokol.m $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -fobjc-arc -c $< -o $@

build/astronomy.o: src/astronomy.cpp src/astronomy.h | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/skyglow.o: src/skyglow.cpp src/skyglow.h src/astronomy.h | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/camera.o: src/camera.cpp src/camera.h | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/yard: build/terrain.o build/marching_cubes.o build/visibility.o build/tree.o build/geometry.o build/main.o build/sokol.o build/astronomy.o build/skyglow.o build/camera.o
	$(CXX) $^ $(FRAMEWORKS) -o $@

run: build/yard
	./build/yard

run-yard: build/yard
	./build/yard --yard-demo --time 12

clean:
	rm -rf build

# Dependency downloads are explicit; ordinary builds never use the network.
setup-tools:
	sh tools/setup-shdc.sh

$(SHDC):
	@echo "Missing sokol-shdc. Run 'make setup-tools' once (requires network)." >&2
	@exit 1

shaders: $(SHADER_HEADER)

$(SHADER_HEADER): shaders/yard.glsl $(SHDC) Makefile
	mkdir -p build/generated
	$(SHDC) --input $< --output $@ --slang $(SHADER_LANGS) --ifdef

build/geometry.o: src/geometry.cpp src/geometry.h vendor/earcut/earcut.hpp | build
	$(CXX) $(CXXFLAGS) -isystem vendor/earcut -c $< -o $@

TREE_DATA := src/tree_params.inc src/tree_presets.inc src/tree_shapes.inc src/tree_validate.inc
build/tree.o: src/tree.cpp src/tree.h $(TREE_DATA) | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/terrain.o: src/terrain.cpp src/terrain.h src/marching_cubes.h | build
	$(CXX) $(CXXFLAGS) -O2 -c $< -o $@
build/marching_cubes.o: src/marching_cubes.cpp src/marching_cubes.h vendor/marching_cubes/tables.h | build
	$(CXX) $(CXXFLAGS) -O2 -c $< -o $@
build/visibility.o: src/visibility.cpp src/visibility.h src/terrain.h | build
	$(CXX) $(CXXFLAGS) -O2 -c $< -o $@
