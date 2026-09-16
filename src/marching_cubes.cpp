#include "marching_cubes.h"
#include "../vendor/marching_cubes/tables.h"
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static const int corner[8][3] = {
    {0,0,0}, {1,0,0}, {1,1,0}, {0,1,0},
    {0,0,1}, {1,0,1}, {1,1,1}, {0,1,1}
};
static const int edge[12][2] = {
    {0,1}, {1,2}, {2,3}, {3,0}, {4,5}, {5,6},
    {6,7}, {7,4}, {0,4}, {1,5}, {2,6}, {3,7}
};
static int minimum(int a, int b) { return a < b ? a : b; }
static float sample(const uint8_t *d, const int n[3], const float p[3]) {
    int lo[3], hi[3];
    float f[3];
    for (int k=0; k<3; ++k) {
        float v = fmaxf(0, fminf(n[k]-1, p[k]));
        lo[k] = (int)floorf(v); hi[k] = minimum(lo[k]+1,n[k]-1); f[k] = v-lo[k];
    }
    float result = 0;
    for (int i=0; i<8; ++i) {
        int x=corner[i][0] ? hi[0] : lo[0], y=corner[i][1] ? hi[1] : lo[1];
        int z=corner[i][2] ? hi[2] : lo[2];
        float weight = 1;
        for (int k=0; k<3; ++k) weight *= corner[i][k] ? f[k] : 1-f[k];
        result += d[((size_t)z*n[0]+x)*n[1]+y]*weight;
    }
    return result;
}
static bool vertex(yard_mesh *m, const uint8_t *d, const int n[3],
                   const float p[3], int step, uint32_t *index) {
    if (m->vertex_count == m->vertex_capacity) {
        size_t cap = m->vertex_capacity ? m->vertex_capacity*2 : 4096;
        if (cap > UINT32_MAX || cap > SIZE_MAX/sizeof(*m->vertices)) return false;
        void *ptr = realloc(m->vertices, cap*sizeof(*m->vertices));
        if (!ptr) return false;
        m->vertices = static_cast<yard_mesh_vertex*>(ptr); m->vertex_capacity = cap;
    }
    *index = (uint32_t)m->vertex_count++;
    yard_mesh_vertex *v = &m->vertices[*index];
    memcpy(v->position,p,sizeof(v->position));
    float length = 0;
    for (int k=0; k<3; ++k) {
        float a[3] = {p[0],p[1],p[2]}, b[3] = {p[0],p[1],p[2]};
        a[k] = fmaxf(0,p[k]-step); b[k] = fminf(n[k]-1,p[k]+step);
        v->normal[k] = (sample(d,n,a)-sample(d,n,b))/(b[k]-a[k]);
        length += v->normal[k]*v->normal[k];
    }
    length = sqrtf(length);
    if (length < 1e-6f) return false;
    for (int k=0; k<3; ++k) v->normal[k] /= length;
    return true;
}
static bool triangle(yard_mesh *m, uint32_t a, uint32_t b, uint32_t c) {
    if (m->index_count+3 > m->index_capacity) {
        size_t cap = m->index_capacity ? m->index_capacity*2 : 8192;
        if (cap > INT_MAX || cap > SIZE_MAX/sizeof(*m->indices)) return false;
        void *ptr = realloc(m->indices,cap*sizeof(*m->indices));
        if (!ptr) return false;
        m->indices = static_cast<uint32_t*>(ptr); m->index_capacity = cap;
    }
    m->indices[m->index_count++] = a;
    m->indices[m->index_count++] = b;
    m->indices[m->index_count++] = c;
    return true;
}
bool yard_marching_cubes(yard_mesh *m, const uint8_t *d,
                        int nx, int ny, int nz, int step) {
    *m = yard_mesh{};
    if (!d || nx<2 || ny<2 || nz<2 || step<1 || step>16) return false;
    const int n[3] = {nx,ny,nz};
    int cells[3];
    for (int k=0; k<3; ++k) cells[k] = (n[k]-2)/step+1;
    /* Two rolling edge-cache planes share vertices across all cells. */
    size_t plane = (size_t)(cells[0]+1)*(cells[1]+1)*3;
    if (plane > SIZE_MAX/(2*sizeof(uint32_t))) return false;
    uint32_t *cache = static_cast<uint32_t*>(malloc(2*plane*sizeof(*cache)));
    if (!cache) return false;
    memset(cache,255,2*plane*sizeof(*cache));
    for (int z=0; z<cells[2]; ++z) {
        memset(cache+((z+1)&1)*plane,255,plane*sizeof(*cache));
        for (int x=0; x<cells[0]; ++x) for (int y=0; y<cells[1]; ++y) {
            int base[3] = {x,y,z}, p[8][3], values[8], code=0;
            for (int i=0; i<8; ++i) {
                for (int k=0; k<3; ++k) p[i][k] = minimum((base[k]+corner[i][k])*step,n[k]-1);
                values[i] = d[((size_t)p[i][2]*nx+p[i][0])*ny+p[i][1]];
                /* Set bits for the solid side; reverse table winding below. */
                if (values[i] > 127.5f) code |= 1<<i;
            }
            if (!edge_table[code]) continue;
            uint32_t indices[12];
            for (int e=0; e<12; ++e) if (edge_table[code] & (1<<e)) {
                int a=edge[e][0], b=edge[e][1], axis=0, low[3];
                for (int k=0; k<3; ++k) {
                    low[k] = base[k]+minimum(corner[a][k],corner[b][k]);
                    if (corner[a][k] != corner[b][k]) axis=k;
                }
                size_t slot = (low[2]&1)*plane+((size_t)low[0]*(cells[1]+1)+low[1])*3+axis;
                if (cache[slot] == UINT32_MAX) {
                    float t=(127.5f-values[a])/(values[b]-values[a]), position[3];
                    for (int k=0; k<3; ++k) position[k] = p[a][k]+t*(p[b][k]-p[a][k]);
                    if (!vertex(m,d,n,position,step,&cache[slot])) goto fail;
                }
                indices[e] = cache[slot];
            }
            for (int i=0; triangle_table[code][i] >= 0; i+=3) {
                if (!triangle(m,indices[(int)triangle_table[code][i]],
                              indices[(int)triangle_table[code][i+2]],
                              indices[(int)triangle_table[code][i+1]])) goto fail;
            }
        }
    }
    free(cache);
    return true;
fail:
    free(cache); yard_mesh_destroy(m); return false;
}
void yard_mesh_destroy(yard_mesh *m) {
    free(m->vertices); free(m->indices); *m = yard_mesh{};
}
