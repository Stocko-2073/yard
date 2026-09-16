#include "terrain.h"
#include <math.h>
#include <stdlib.h>

/* Seeded value noise: non-tiling over the yard, with smooth transitions. */
static float lattice(int x, int z) {
    uint32_t h = (uint32_t)x*0x8da6b343u ^ (uint32_t)z*0xd8163841u ^ 0x71c3a52du;
    h ^= h >> 16; h *= 0x7feb352du; h ^= h >> 15; h *= 0x846ca68bu; h ^= h >> 16;
    return (h & 0xffffffu)*(2.0f/16777215.0f)-1.0f;
}
static float noise(float x, float z) {
    int ix=(int)floorf(x), iz=(int)floorf(z);
    float u=x-ix, v=z-iz;
    u=u*u*u*(u*(u*6-15)+10);
    v=v*v*v*(v*(v*6-15)+10);
    float a=lattice(ix,iz), b=lattice(ix+1,iz), c=lattice(ix,iz+1), d=lattice(ix+1,iz+1);
    return (a+(b-a)*u)*(1-v)+(c+(d-c)*u)*v;
}

bool yard_terrain_create(yard_terrain *t, int size) {
    *t = yard_terrain{.size = size, .voxels = nullptr, .heights = nullptr, .mesh = {}};
    if (size < 2 || size > YARD_TERRAIN_SIZE) return false;
    size_t columns = (size_t)size*size;
    t->voxels = static_cast<uint8_t*>(malloc(columns*YARD_TERRAIN_DEPTH));
    t->heights = static_cast<float*>(malloc(columns*sizeof(*t->heights)));
    if (!t->voxels || !t->heights) goto fail;
    for (int z=0; z<size; ++z) for (int x=0; x<size; ++x) {
        float wx=(x+0.5f-size*0.5f)*0.01f, wz=(z+0.5f-size*0.5f)*0.01f;
        /* Bound is 4–60 cm inside the 64 cm slab. Independent offsets avoid
         * aligning the three scales on the same lattice. */
        float h = 32 + 20*noise(wx/4.0f+3.7f,wz/4.0f-1.3f)
                     + 6*noise(wx/1.5f-9.2f,wz/1.5f+7.4f)
                     + 2*noise(wx/0.55f+21.6f,wz/0.55f-13.8f);
        size_t column = (size_t)z*size+x;
        uint8_t *density = t->voxels+column*YARD_TERRAIN_DEPTH;
        for (int y=0; y<YARD_TERRAIN_DEPTH; ++y) {
            /* 16 density units/cm, saturated away from the surface. This is
             * a vertical signed-distance approximation, not Euclidean SDF. */
            float value = 127.5f+16*(h-(y+0.5f));
            density[y] = (uint8_t)lroundf(fmaxf(0,fminf(255,value)));
        }
        /* Interpolate the same byte field for sub-centimeter navigation. */
        for (int y=0; y<YARD_TERRAIN_DEPTH-1; ++y) {
            if (density[y] > 127 && density[y+1] <= 127) {
                t->heights[column] = y+0.5f+(127.5f-density[y])/(density[y+1]-density[y]);
                break;
            }
        }
    }
    if (!yard_marching_cubes(&t->mesh,t->voxels,size,YARD_TERRAIN_DEPTH,size,YARD_TERRAIN_MESH_STEP)) goto fail;
    for (size_t i=0; i<t->mesh.vertex_count; ++i) {
        float *p=t->mesh.vertices[i].position;
        p[0]=(p[0]+0.5f-size*0.5f)*0.01f;
        p[1]=(p[1]+0.5f)*0.01f;
        p[2]=(p[2]+0.5f-size*0.5f)*0.01f;
    }
    return true;
fail:
    yard_terrain_destroy(t); return false;
}

float yard_terrain_height(const yard_terrain *t, float x, float z) {
    float gx=x*100+t->size*0.5f-0.5f, gz=z*100+t->size*0.5f-0.5f;
    if (!isfinite(gx) || !isfinite(gz) || gx<0 || gz<0 || gx>t->size-1 || gz>t->size-1) return 0;
    int ix=(int)floorf(gx), iz=(int)floorf(gz);
    if (ix == t->size-1) --ix;
    if (iz == t->size-1) --iz;
    float fx=gx-ix, fz=gz-iz;
    const float *h=t->heights+(size_t)iz*t->size+ix;
    return ((h[0]*(1-fx)+h[1]*fx)*(1-fz)+(h[t->size]*(1-fx)+h[t->size+1]*fx)*fz)*0.01f;
}
void yard_terrain_destroy(yard_terrain *t) {
    yard_mesh_destroy(&t->mesh);
    free(t->voxels); free(t->heights); *t=yard_terrain{};
}
