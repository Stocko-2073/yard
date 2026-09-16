#ifndef YARD_VISIBILITY_H
#define YARD_VISIBILITY_H
#include "terrain.h"
#define YARD_DRAW_CELLS 256

typedef struct { float min[3], max[3]; } yard_bounds;
typedef struct { float planes[6][4]; } yard_frustum;
typedef struct {
    int x, z, width, depth, root_start;
    int index_start, index_count;
    yard_bounds grass, ground;
} yard_draw_region;
typedef struct {
    yard_draw_region *regions;
    int count;
    float *roots;
} yard_draw_layout;

void yard_frustum_make(yard_frustum *f, const float position[3], float yaw,
                       float pitch, float aspect, float focal);
bool yard_frustum_visible(const yard_frustum *f, const yard_bounds *bounds);
/* Reorders indices and packs heights into static draw regions. No state/LOD changes. */
bool yard_draw_layout_create(yard_draw_layout *layout, yard_terrain *terrain);
void yard_draw_layout_destroy(yard_draw_layout *layout);
#endif
