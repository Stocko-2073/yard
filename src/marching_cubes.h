#ifndef YARD_MARCHING_CUBES_H
#define YARD_MARCHING_CUBES_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct { float position[3], normal[3]; } yard_mesh_vertex;
typedef struct {
    yard_mesh_vertex *vertices;
    uint32_t *indices;
    size_t vertex_count, index_count;
    size_t vertex_capacity, index_capacity;
} yard_mesh;

/* Density [z][x][y], inside > 127.5. Positions are in sample-grid units.
 * Uniform step, with the last interval shortened to reach each volume edge.
 * Surfaces at the volume boundary are open; no implicit exterior padding.
 * Classical MC tables, not a topology-guaranteed variant for arbitrary fields. */
bool yard_marching_cubes(yard_mesh *mesh, const uint8_t *density,
                        int nx, int ny, int nz, int step);
void yard_mesh_destroy(yard_mesh *mesh);
#endif
