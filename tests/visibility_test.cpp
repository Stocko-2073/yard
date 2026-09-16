#include "visibility.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void frustum_test(void) {
    yard_frustum f;
    const float origin[3]={0,0,0};
    yard_frustum_make(&f,origin,0,0,1,1);
    const yard_bounds visible[]={
        {{-1,-1,-3},{1,1,-2}}, /* centered */
        {{1.9f,-0.1f,-2.1f},{2.1f,0.1f,-1.9f}}, /* crossing side */
        {{-100,-100,-100},{100,100,100}}, /* camera inside */
        {{0,0,-0.11f},{0,0,-0.09f}}, /* crossing near */
        {{0,0,-1001},{0,0,-999}}, /* crossing far */
    };
    const yard_bounds hidden[]={
        {{-1,-1,2},{1,1,3}}, {{3,0,-2},{4,1,-1}},
        {{-4,0,-2},{-3,1,-1}}, {{0,3,-2},{1,4,-1}},
        {{0,-4,-2},{1,-3,-1}}, {{0,0,-0.05f},{0,0,0}},
        {{0,0,-1002},{0,0,-1001}},
    };
    for (size_t i=0; i<sizeof(visible)/sizeof(*visible); ++i) assert(yard_frustum_visible(&f,&visible[i]));
    for (size_t i=0; i<sizeof(hidden)/sizeof(*hidden); ++i) assert(!yard_frustum_visible(&f,&hidden[i]));
    // Independently project world points using the shader's camera basis;
    // any point inside clip space must survive a surrounding AABB test.
    const float p[3]={7,0.6f,-11};
    for (int a=0; a<24; ++a) for (int b=-3; b<=3; ++b) {
        float yaw=a*0.2617994f, pitch=b*0.5f, cy=cosf(yaw), sy=sinf(yaw), cp=cosf(pitch), sp=sinf(pitch);
        float aspect=4.0f/3, focal=1.7320508f;
        yard_frustum_make(&f,p,yaw,pitch,aspect,focal);
        for (int x=-40; x<=40; x+=4) for (int z=-40; z<=40; z+=4) {
            float dx=x-p[0], dy=0.3f-p[1], dz=z-p[2];
            float vx=cy*dx+sy*dz, vy=-sy*sp*dx+cp*dy+cy*sp*dz;
            float vz=sy*cp*dx+sp*dy-cy*cp*dz;
            if (vz>=0.1f && vz<=1000 && fabsf(vx)*focal<=vz*aspect && fabsf(vy)*focal<=vz) {
                yard_bounds box={{x-0.02f,0.25f,z-0.02f},{x+0.02f,0.35f,z+0.02f}};
                assert(yard_frustum_visible(&f,&box));
            }
        }
    }
}
static void layout_test(void) {
    yard_terrain t;
    assert(yard_terrain_create(&t,257)); /* Includes 1-cell-wide edge regions. */
    size_t n=t.mesh.index_count;
    uint32_t *before=static_cast<uint32_t*>(malloc(n*sizeof(*before)));
    memcpy(before,t.mesh.indices,n*sizeof(*before));
    yard_draw_layout l;
    assert(yard_draw_layout_create(&l,&t));
    assert(l.count==4);
    int roots=0, indices=0;
    for (int i=0; i<l.count; ++i) {
        const yard_draw_region *r=&l.regions[i];
        assert(r->root_start==roots && r->index_start==indices);
        for (int z=0; z<r->depth; ++z) for (int x=0; x<r->width; ++x) {
            float height=t.heights[(r->z+z)*t.size+r->x+x];
            assert(l.roots[roots++]==height);
            assert(height*0.01f>=r->grass.min[1] && height*0.01f+0.05f<=r->grass.max[1]);
            // Every possible random root and blade endpoint fits horizontally.
            float lo[2]={(r->x+x-t.size*0.5f)*0.01f-0.0025f,(r->z+z-t.size*0.5f)*0.01f-0.0025f};
            float hi[2]={(r->x+x+1-t.size*0.5f)*0.01f+0.0025f,(r->z+z+1-t.size*0.5f)*0.01f+0.0025f};
            for (int k=0; k<2; ++k) { assert(lo[k]>=r->grass.min[k*2]); assert(hi[k]<=r->grass.max[k*2]); }
        }
        for (int j=0; j<r->index_count; ++j) {
            const float *p=t.mesh.vertices[t.mesh.indices[indices++]].position;
            for (int k=0; k<3; ++k) assert(p[k]>=r->ground.min[k] && p[k]<=r->ground.max[k]);
        }
    }
    assert(roots==257*257 && (size_t)indices==n);
    // Compare complete ordered triangles, not just the vertex multiset.
    for (size_t i=0; i<n; i+=3) {
        int matches=0;
        for (size_t j=0; j<n; j+=3) if (memcmp(before+i,t.mesh.indices+j,3*sizeof(uint32_t))==0) ++matches;
        assert(matches==1);
    }
    free(before); yard_draw_layout_destroy(&l); yard_terrain_destroy(&t);
}
int main(void) { frustum_test(); layout_test(); puts("Visibility tests passed."); }
