#include "terrain.h"
#include <math.h>
#include <stdlib.h>

bool yard_terrain_create(yard_terrain *t, int size) {
    *t = (yard_terrain){.size = size};
    if (size < 2 || size > YARD_TERRAIN_SIZE) return false;
    size_t columns = (size_t)size*size;
    t->voxels = malloc(columns*YARD_TERRAIN_DEPTH);
    t->heights = malloc(columns*sizeof(*t->heights));
    float (*waves)[4] = malloc((size_t)size*sizeof(*waves));
    if (!t->voxels || !t->heights || !waves) goto fail;
    for (int i=0; i<size; ++i) {
        float p = (i-size*0.5f)*0.01f;
        waves[i][0] = sinf(p*0.43f);
        waves[i][1] = sinf(p*1.17f+0.8f);
        waves[i][2] = sinf(p*2.71f+1.7f);
        waves[i][3] = sinf(p*5.13f+0.4f);
    }
    for (int z=0; z<size; ++z) for (int x=0; x<size; ++x) {
        /* Same broad humps as the block prototype, without rounding height. */
        float h = 19 + 6*waves[x][0]*waves[z][1] + 3*waves[x][1]*waves[z][0]
                     + 1.5f*waves[x][2]*waves[z][2] + 0.5f*waves[x][3]*waves[z][3];
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
    free(waves); waves = NULL;
    if (!yard_marching_cubes(&t->mesh,t->voxels,size,YARD_TERRAIN_DEPTH,size,YARD_TERRAIN_MESH_STEP)) goto fail;
    for (size_t i=0; i<t->mesh.vertex_count; ++i) {
        float *p=t->mesh.vertices[i].position;
        p[0]=(p[0]+0.5f-size*0.5f)*0.01f;
        p[1]=(p[1]+0.5f)*0.01f;
        p[2]=(p[2]+0.5f-size*0.5f)*0.01f;
    }
    return true;
fail:
    free(waves); yard_terrain_destroy(t); return false;
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
    free(t->voxels); free(t->heights); *t=(yard_terrain){0};
}
