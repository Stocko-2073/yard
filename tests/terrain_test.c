#include "terrain.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static void check(int size) {
    yard_terrain t;
    assert(yard_terrain_create(&t,size));
    double expected = (double)size*size;
    for (int z=0; z<size; ++z) for (int x=0; x<size; ++x) {
        int h = t.heights[z*size+x];
        assert(h > 0 && h <= YARD_TERRAIN_DEPTH);
        for (int y=0; y<YARD_TERRAIN_DEPTH; ++y)
            assert(t.voxels[((size_t)z*size+x)*YARD_TERRAIN_DEPTH+y] == (y<h));
        if (x == 0) expected += h;
        if (z == 0) expected += h;
        expected += x+1 == size ? h : abs(h-t.heights[z*size+x+1]);
        expected += z+1 == size ? h : abs(h-t.heights[(z+1)*size+x]);
        float wx = (x+0.5f-size*0.5f)*0.01f, wz = (z+0.5f-size*0.5f)*0.01f;
        assert(fabsf(yard_terrain_height(&t,wx,wz)-h*0.01f) < 1e-6f);
    }
    double actual = 0;
    for (size_t q=0; q<t.quads; ++q) {
        const yard_terrain_vertex *a=&t.vertices[q*4], *b=a+1, *d=a+3;
        double u[3], v[3], cross[3];
        for (int k=0; k<3; ++k) { u[k]=b->position[k]-a->position[k]; v[k]=d->position[k]-a->position[k]; }
        cross[0]=u[1]*v[2]-u[2]*v[1]; cross[1]=u[2]*v[0]-u[0]*v[2]; cross[2]=u[0]*v[1]-u[1]*v[0];
        double area=0;
        for (int k=0; k<3; ++k) area += cross[k]*a->normal[k];
        assert(area > 0); /* Outward winding matches the supplied normal. */
        actual += area*10000;
        for (int i=0; i<6; ++i) assert(t.indices[q*6+i] >= q*4 && t.indices[q*6+i] < q*4+4);
    }
    assert(fabs(actual-expected) < expected*1e-5);
    assert(yard_terrain_height(&t,100,100) == 0);
    yard_terrain_destroy(&t);
}
int main(void) {
    check(1); check(37); check(513);
    puts("Terrain: voxel occupancy, surface area, winding, indices and navigation passed.");
}
