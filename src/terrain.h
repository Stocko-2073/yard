#ifndef YARD_TERRAIN_H
#define YARD_TERRAIN_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define YARD_TERRAIN_SIZE 6361 /* 63.61 m square, just under one acre. */
#define YARD_TERRAIN_DEPTH 32

typedef struct { float position[3], normal[3]; } yard_terrain_vertex;
typedef struct {
    int size;
    uint8_t *voxels; /* [z][x][y], 0 = air, 1 = soil; centimetre cells. */
    uint8_t *heights; /* Surface cache for meshing and viewer navigation. */
    yard_terrain_vertex *vertices;
    uint32_t *indices;
    size_t quads, capacity;
} yard_terrain;

bool yard_terrain_create(yard_terrain *terrain, int size);
void yard_terrain_free_mesh(yard_terrain *terrain);
void yard_terrain_destroy(yard_terrain *terrain);
float yard_terrain_height(const yard_terrain *terrain, float x, float z);
#endif
