#pragma once
#include "sokol_gfx.h"
#include "cube.glsl.h"

// Optional static terrain/grass preview, adapted from prototype/voxel-yard.
// CPU density and navigation heights outlive the disposable render meshes.
void yard_scene_create();
void yard_scene_destroy();
float yard_scene_height(float x, float z);
void yard_scene_draw(const vs_params_t& view, const light_params_t& light, bool grass);
