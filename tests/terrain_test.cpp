#include "terrain.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static uint8_t encode(float distance) {
    return (uint8_t)lroundf(fmaxf(0,fminf(255,127.5f+16*distance)));
}
static double check_mesh(const yard_mesh *m) {
    assert(m->vertex_count > 0 && m->index_count > 0 && m->index_count%3 == 0);
    double projected_area=0;
    for (size_t i=0; i<m->vertex_count; ++i) {
        const yard_mesh_vertex *v=&m->vertices[i];
        float length=0;
        for (int k=0; k<3; ++k) {
            assert(isfinite(v->position[k]) && isfinite(v->normal[k]));
            length += v->normal[k]*v->normal[k];
        }
        assert(fabsf(length-1) < 1e-5f);
    }
    for (size_t i=0; i<m->index_count; i+=3) {
        for (int j=0; j<3; ++j) assert(m->indices[i+j] < m->vertex_count);
        const yard_mesh_vertex *a=&m->vertices[m->indices[i]], *b=&m->vertices[m->indices[i+1]], *c=&m->vertices[m->indices[i+2]];
        double u[3],v[3],n[3];
        for (int k=0; k<3; ++k) { u[k]=b->position[k]-a->position[k]; v[k]=c->position[k]-a->position[k]; }
        n[0]=u[1]*v[2]-u[2]*v[1]; n[1]=u[2]*v[0]-u[0]*v[2]; n[2]=u[0]*v[1]-u[1]*v[0];
        double outward=0;
        for (int k=0; k<3; ++k) outward += n[k]*(a->normal[k]+b->normal[k]+c->normal[k]);
        assert(outward > 0); /* Nondegenerate outward triangles, not just normals. */
        projected_area += n[1]*0.5;
    }
    return projected_area;
}
static int compare_edges(const void *a,const void *b) {
    uint64_t x=*(const uint64_t *)a, y=*(const uint64_t *)b;
    return (x>y)-(x<y);
}
static void check_closed(const yard_mesh *m) {
    uint64_t *edges=static_cast<uint64_t*>(malloc(m->index_count*sizeof(*edges)));
    assert(edges);
    for (size_t i=0; i<m->index_count; i+=3) for (int j=0; j<3; ++j) {
        uint32_t a=m->indices[i+j], b=m->indices[i+(j+1)%3];
        edges[i+j] = a<b ? ((uint64_t)a<<32)|b : ((uint64_t)b<<32)|a;
    }
    qsort(edges,m->index_count,sizeof(*edges),compare_edges);
    for (size_t i=0; i<m->index_count; i+=2) {
        assert(i+1 < m->index_count && edges[i] == edges[i+1]);
        assert(i+2 == m->index_count || edges[i] != edges[i+2]);
    }
    free(edges);
}
static void plane(int step) {
    const int nx=17,ny=32,nz=22;
    uint8_t *d=static_cast<uint8_t*>(malloc((size_t)nx*ny*nz));
    assert(d);
    for (int z=0; z<nz; ++z) for (int x=0; x<nx; ++x) for (int y=0; y<ny; ++y)
        d[((size_t)z*nx+x)*ny+y]=encode(12.3f+0.04f*x+0.02f*z-y);
    yard_mesh m;
    assert(yard_marching_cubes(&m,d,nx,ny,nz,step));
    assert(fabs(check_mesh(&m)-(nx-1)*(nz-1)) < 1e-3);
    for (size_t i=0; i<m.vertex_count; ++i) {
        float *p=m.vertices[i].position;
        assert(fabsf(p[1]-(12.3f+0.04f*p[0]+0.02f*p[2])) < 0.04f);
        assert(m.vertices[i].normal[1] > 0.99f);
    }
    yard_mesh_destroy(&m); free(d);
}
static void sphere(int step) {
    const int n=25;
    uint8_t *d=static_cast<uint8_t*>(malloc((size_t)n*n*n));
    assert(d);
    for (int z=0; z<n; ++z) for (int x=0; x<n; ++x) for (int y=0; y<n; ++y) {
        float a=x-12.2f,b=y-12.2f,c=z-12.2f;
        d[((size_t)z*n+x)*n+y]=encode(7.3f-sqrtf(a*a+b*b+c*c));
    }
    yard_mesh m;
    assert(yard_marching_cubes(&m,d,n,n,n,step));
    check_mesh(&m); check_closed(&m);
    for (size_t i=0; i<m.vertex_count; ++i) {
        float *p=m.vertices[i].position, a=p[0]-12.2f,b=p[1]-12.2f,c=p[2]-12.2f;
        assert(fabsf(sqrtf(a*a+b*b+c*c)-7.3f) < 0.6f);
    }
    yard_mesh_destroy(&m); free(d);
}
static void terrain(int size) {
    yard_terrain t;
    assert(yard_terrain_create(&t,size));
    assert(fabs(check_mesh(&t.mesh)-(size-1)*(size-1)*0.0001) < 1e-4);
    for (int z=0; z<size; ++z) for (int x=0; x<size; ++x) {
        const uint8_t *d=t.voxels+((size_t)z*size+x)*YARD_TERRAIN_DEPTH;
        assert(d[0] > 127 && d[YARD_TERRAIN_DEPTH-1] < 128);
        for (int y=1; y<YARD_TERRAIN_DEPTH; ++y) assert(d[y] <= d[y-1]);
        float h=t.heights[z*size+x];
        assert(h>3 && h<61);
        float wx=(x+0.5f-size*0.5f)*0.01f,wz=(z+0.5f-size*0.5f)*0.01f;
        assert(fabsf(yard_terrain_height(&t,wx,wz)-h*0.01f) < 1e-5f);
    }
    for (size_t i=0; i<t.mesh.vertex_count; ++i) {
        float *p=t.mesh.vertices[i].position;
        /* Navigation uses the dense field, mesh uses 4 cm samples. */
        assert(fabsf(yard_terrain_height(&t,p[0],p[2])-p[1]) < 0.001f);
    }
    assert(yard_terrain_height(&t,100,100) == 0);
    yard_terrain_destroy(&t);
}
int main(void) {
    plane(1); plane(4); sphere(1); sphere(4);
    terrain(2); terrain(37); terrain(513);
    puts("Marching cubes: plane coverage, closed sphere topology, interpolation, normals, winding, density and navigation passed.");
}
