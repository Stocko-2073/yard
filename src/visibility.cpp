#include "visibility.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

void yard_frustum_make(yard_frustum *f, const float p[3], float yaw,
                       float pitch, float aspect, float focal) {
    float cy = cosf(yaw), sy = sinf(yaw), cp = cosf(pitch), sp = sinf(pitch);
    const float right[3] = {cy,0,sy}, up[3] = {-sy*sp,cp,cy*sp};
    const float forward[3] = {sy*cp,sp,-cy*cp};
    for (int j=0; j<3; ++j) {
        f->planes[0][j] = forward[j]+right[j]*focal/aspect;
        f->planes[1][j] = forward[j]-right[j]*focal/aspect;
        f->planes[2][j] = forward[j]+up[j]*focal;
        f->planes[3][j] = forward[j]-up[j]*focal;
        f->planes[4][j] = forward[j];
        f->planes[5][j] = -forward[j];
    }
    for (int i=0; i<6; ++i) {
        f->planes[i][3] = 0;
        for (int j=0; j<3; ++j) f->planes[i][3] -= f->planes[i][j]*p[j];
    }
    f->planes[4][3] -= 0.1f;
    f->planes[5][3] += 1000.0f;
}

bool yard_frustum_visible(const yard_frustum *f, const yard_bounds *b) {
    for (int i=0; i<6; ++i) {
        float maximum = f->planes[i][3];
        for (int j=0; j<3; ++j)
            maximum += f->planes[i][j]*(f->planes[i][j] >= 0 ? b->max[j] : b->min[j]);
        if (maximum < -0.0001f) return false;
    }
    return true;
}

static void empty_bounds(yard_bounds *b) {
    for (int j=0; j<3; ++j) { b->min[j] = FLT_MAX; b->max[j] = -FLT_MAX; }
}
static void extend(yard_bounds *b, const float p[3]) {
    for (int j=0; j<3; ++j) { b->min[j] = fminf(b->min[j],p[j]); b->max[j] = fmaxf(b->max[j],p[j]); }
}
static int triangle_region(const yard_terrain *t, size_t i, int side) {
    float x=0, z=0;
    for (int j=0; j<3; ++j) {
        const float *p = t->mesh.vertices[t->mesh.indices[i+j]].position;
        x += p[0]; z += p[2];
    }
    int tx = (int)((x/3*100+t->size*0.5f)/YARD_DRAW_CELLS);
    int tz = (int)((z/3*100+t->size*0.5f)/YARD_DRAW_CELLS);
    if (tx < 0) tx = 0;
    if (tz < 0) tz = 0;
    if (tx >= side) tx = side-1;
    if (tz >= side) tz = side-1;
    return tz*side+tx;
}

bool yard_draw_layout_create(yard_draw_layout *l, yard_terrain *t) {
    memset(l,0,sizeof(*l));
    int side = (t->size+YARD_DRAW_CELLS-1)/YARD_DRAW_CELLS;
    l->count = side*side;
    l->regions = static_cast<yard_draw_region*>(calloc((size_t)l->count,sizeof(*l->regions)));
    l->roots = static_cast<float*>(malloc((size_t)t->size*t->size*sizeof(float)));
    uint32_t *indices = static_cast<uint32_t*>(malloc(t->mesh.index_count*sizeof(uint32_t)));
    int *cursor = static_cast<int*>(calloc((size_t)l->count,sizeof(int)));
    if (!l->regions || !l->roots || !indices || !cursor) {
        free(indices); free(cursor); yard_draw_layout_destroy(l); return false;
    }
    int root = 0;
    for (int z=0; z<side; ++z) for (int x=0; x<side; ++x) {
        yard_draw_region *r = &l->regions[z*side+x];
        r->x=x*YARD_DRAW_CELLS; r->z=z*YARD_DRAW_CELLS;
        r->width=t->size-r->x; r->depth=t->size-r->z;
        if (r->width>YARD_DRAW_CELLS) r->width=YARD_DRAW_CELLS;
        if (r->depth>YARD_DRAW_CELLS) r->depth=YARD_DRAW_CELLS;
        r->root_start=root;
        empty_bounds(&r->grass); empty_bounds(&r->ground);
        for (int dz=0; dz<r->depth; ++dz) for (int dx=0; dx<r->width; ++dx) {
            float h=t->heights[(r->z+dz)*t->size+r->x+dx];
            l->roots[root++]=h;
            r->grass.min[1]=fminf(r->grass.min[1],h*0.01f);
            r->grass.max[1]=fmaxf(r->grass.max[1],h*0.01f+0.05f);
        }
        // Full random root footprint plus the triangle's half-width in any azimuth.
        r->grass.min[0]=(r->x-t->size*0.5f)*0.01f-0.0025f;
        r->grass.max[0]=(r->x+r->width-t->size*0.5f)*0.01f+0.0025f;
        r->grass.min[2]=(r->z-t->size*0.5f)*0.01f-0.0025f;
        r->grass.max[2]=(r->z+r->depth-t->size*0.5f)*0.01f+0.0025f;
    }
    for (size_t i=0; i<t->mesh.index_count; i+=3)
        l->regions[triangle_region(t,i,side)].index_count += 3;
    int start=0;
    for (int i=0; i<l->count; ++i) {
        l->regions[i].index_start=start; cursor[i]=start;
        start+=l->regions[i].index_count;
    }
    for (size_t i=0; i<t->mesh.index_count; i+=3) {
        int region=triangle_region(t,i,side);
        for (int j=0; j<3; ++j) {
            uint32_t index=t->mesh.indices[i+j];
            indices[cursor[region]++]=index;
            // Include all vertices, even when a triangle crosses a region boundary.
            extend(&l->regions[region].ground,t->mesh.vertices[index].position);
        }
    }
    free(cursor); free(t->mesh.indices); t->mesh.indices=indices;
    t->mesh.index_capacity=t->mesh.index_count;
    return true;
}
void yard_draw_layout_destroy(yard_draw_layout *l) {
    free(l->regions); free(l->roots); memset(l,0,sizeof(*l));
}
