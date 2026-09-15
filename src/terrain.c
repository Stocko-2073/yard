#include "terrain.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static int height(const yard_terrain *t, int x, int z) {
    if (x < 0 || z < 0 || x >= t->size || z >= t->size) return 0;
    return t->heights[(size_t)z*t->size+x];
}

/* Rectangle defined by origin and two edges. Their cross product is outward. */
static bool quad(yard_terrain *t, const int p[3], const int u[3], const int v[3]) {
    if (t->quads == t->capacity) {
        size_t capacity = t->capacity ? t->capacity*2 : 4096;
        /* sg_draw uses signed int element counts. */
        if (capacity > 2147483647u/6u) return false;
        void *vertices = realloc(t->vertices, capacity*4*sizeof(*t->vertices));
        if (!vertices) return false;
        t->vertices = vertices;
        void *indices = realloc(t->indices, capacity*6*sizeof(*t->indices));
        if (!indices) return false;
        t->indices = indices;
        t->capacity = capacity;
    }
    float n[3] = {(float)u[1]*v[2]-(float)u[2]*v[1],
                  (float)u[2]*v[0]-(float)u[0]*v[2],
                  (float)u[0]*v[1]-(float)u[1]*v[0]};
    float length = sqrtf(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
    const int a[4] = {0,1,1,0}, b[4] = {0,0,1,1};
    for (int i=0; i<4; ++i) {
        yard_terrain_vertex *vertex = &t->vertices[t->quads*4+i];
        for (int k=0; k<3; ++k) {
            float offset = k == 1 ? 0 : t->size*0.5f;
            vertex->position[k] = (p[k]+a[i]*u[k]+b[i]*v[k]-offset)*0.01f;
            vertex->normal[k] = n[k]/length;
        }
    }
    const uint32_t order[6] = {0,1,2,0,2,3};
    for (int i=0; i<6; ++i) t->indices[t->quads*6+i] = (uint32_t)(t->quads*4)+order[i];
    ++t->quads;
    return true;
}

bool yard_terrain_create(yard_terrain *t, int size) {
    *t = (yard_terrain){.size = size};
    if (size < 1 || size > YARD_TERRAIN_SIZE) return false;
    size_t columns = (size_t)size*size;
    t->voxels = malloc(columns*YARD_TERRAIN_DEPTH);
    t->heights = malloc(columns);
    uint8_t *used = calloc(columns, 1);
    float (*waves)[4] = malloc((size_t)size*sizeof(*waves));
    if (!t->voxels || !t->heights || !used || !waves) goto fail;
    for (int i=0; i<size; ++i) {
        float p = (i-size*0.5f)*0.01f;
        waves[i][0] = sinf(p*0.43f);
        waves[i][1] = sinf(p*1.17f+0.8f);
        waves[i][2] = sinf(p*2.71f+1.7f);
        waves[i][3] = sinf(p*5.13f+0.4f);
    }
    for (int z=0; z<size; ++z) for (int x=0; x<size; ++x) {
        /* Broad humps plus smaller irregularities, all within the 32 cm slab. */
        int h = (int)lroundf(19 + 6*waves[x][0]*waves[z][1]
                              + 3*waves[x][1]*waves[z][0]
                              + 1.5f*waves[x][2]*waves[z][2]
                              + 0.5f*waves[x][3]*waves[z][3]);
        size_t column = (size_t)z*size+x;
        t->heights[column] = (uint8_t)h;
        memset(t->voxels+column*YARD_TERRAIN_DEPTH, 1, (size_t)h);
        memset(t->voxels+column*YARD_TERRAIN_DEPTH+h, 0, (size_t)(YARD_TERRAIN_DEPTH-h));
    }
    free(waves);
    waves = NULL;
    /* Greedy rectangles on equal-height tops; no smoothing or spatial chunks. */
    for (int z=0; z<size; ++z) for (int x=0; x<size; ++x) {
        if (used[(size_t)z*size+x]) continue;
        int h = height(t,x,z), w = 1, d = 1;
        while (x+w < size && !used[(size_t)z*size+x+w] && height(t,x+w,z) == h) ++w;
        while (z+d < size) {
            int i=0;
            for (; i<w; ++i) if (used[(size_t)(z+d)*size+x+i] || height(t,x+i,z+d) != h) break;
            if (i != w) break;
            ++d;
        }
        for (int j=0; j<d; ++j) memset(used+(size_t)(z+j)*size+x, 1, (size_t)w);
        if (!quad(t, (int[3]){x,h,z}, (int[3]){0,0,d}, (int[3]){w,0,0})) goto fail;
    }
    /* Merge exposed risers along each grid boundary, including yard edges. */
    for (int axis=0; axis<2; ++axis) for (int boundary=0; boundary<=size; ++boundary) {
        int along=0;
        while (along<size) {
            int lo = axis == 0 ? height(t,boundary-1,along) : height(t,along,boundary-1);
            int hi = axis == 0 ? height(t,boundary,along) : height(t,along,boundary);
            if (lo == hi) { ++along; continue; }
            int run=1;
            while (along+run<size) {
                int a = axis == 0 ? height(t,boundary-1,along+run) : height(t,along+run,boundary-1);
                int b = axis == 0 ? height(t,boundary,along+run) : height(t,along+run,boundary);
                if (a != lo || b != hi) break;
                ++run;
            }
            int bottom = lo < hi ? lo : hi, top = lo > hi ? lo : hi;
            int p[3] = {axis == 0 ? boundary : along, bottom, axis == 0 ? along : boundary};
            int u[3] = {0,top-bottom,0}, v[3] = {axis == 0 ? 0 : run,0,axis == 0 ? run : 0};
            bool positive = lo > hi;
            bool reverse = axis == 0 ? !positive : positive;
            if (!quad(t,p,reverse ? v : u,reverse ? u : v)) goto fail;
            along += run;
        }
    }
    free(used);
    return true;
fail:
    free(waves);
    free(used);
    yard_terrain_destroy(t);
    return false;
}

float yard_terrain_height(const yard_terrain *t, float x, float z) {
    int ix = (int)floorf(x*100+t->size*0.5f);
    int iz = (int)floorf(z*100+t->size*0.5f);
    return height(t,ix,iz)*0.01f;
}

void yard_terrain_free_mesh(yard_terrain *t) {
    free(t->vertices); free(t->indices);
    t->vertices = NULL; t->indices = NULL; t->capacity = 0;
}
void yard_terrain_destroy(yard_terrain *t) {
    yard_terrain_free_mesh(t);
    free(t->voxels); free(t->heights);
    *t = (yard_terrain){0};
}
