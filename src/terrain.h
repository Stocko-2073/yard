#ifndef YARD_TERRAIN_H
#define YARD_TERRAIN_H
#include "marching_cubes.h"

#define YARD_TERRAIN_SIZE 6361 /* 63.61 m square, just under one acre. */
#define YARD_TERRAIN_DEPTH 32
#define YARD_TERRAIN_MESH_STEP 4 /* Uniform preview extraction spacing, in cm. */

typedef struct {
    int size;
    uint8_t *voxels; /* Density [z][x][y] at 1 cm cell centers, inside > 127.5. */
    float *heights; /* Density-derived surface cache for viewer navigation, cm. */
    yard_mesh mesh;
} yard_terrain;

bool yard_terrain_create(yard_terrain *terrain, int size);
void yard_terrain_destroy(yard_terrain *terrain);
float yard_terrain_height(const yard_terrain *terrain, float x, float z);
#endif
