#include "yard_scene.h"
#include "terrain.h"
#include "visibility.h"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace {
yard_terrain terrain{};
yard_draw_layout layout{};
sg_pipeline ground_pipeline{}, grass_pipeline{};
sg_bindings ground_bindings{}, grass_bindings{};
sg_buffer upload(const void* data, std::size_t size, const char* label, bool index=false) {
    sg_buffer_desc desc{};
    desc.usage.index_buffer=index;
    desc.data={data,size};desc.label=label;
    auto buffer=sg_make_buffer(desc);
    if(sg_query_buffer_state(buffer)!=SG_RESOURCESTATE_VALID)throw std::runtime_error("Cannot upload yard geometry");
    return buffer;
}
}
void yard_scene_create() {
    if(!yard_terrain_create(&terrain,YARD_TERRAIN_SIZE)||!yard_draw_layout_create(&layout,&terrain))
        throw std::runtime_error("Cannot create voxel yard");
    ground_bindings.vertex_buffers[0]=upload(terrain.mesh.vertices,terrain.mesh.vertex_count*sizeof(yard_mesh_vertex),"yard terrain vertices");
    ground_bindings.index_buffer=upload(terrain.mesh.indices,terrain.mesh.index_count*sizeof(uint32_t),"yard terrain indices",true);
    static const float blade[][2]={{-.0025f,0},{.0025f,0},{0,.05f}};
    grass_bindings.vertex_buffers[0]=upload(blade,sizeof(blade),"5 cm grass triangle");
    grass_bindings.vertex_buffers[1]=upload(layout.roots,std::size_t(terrain.size)*terrain.size*sizeof(float),"grass root heights");
    sg_pipeline_desc ground{};
    ground.shader=sg_make_shader(terrain_shader_desc(sg_query_backend()));
    ground.layout.attrs[ATTR_terrain_position].format=SG_VERTEXFORMAT_FLOAT3;
    ground.layout.attrs[ATTR_terrain_normal].format=SG_VERTEXFORMAT_FLOAT3;
    ground.index_type=SG_INDEXTYPE_UINT32;
    ground.cull_mode=SG_CULLMODE_BACK;ground.face_winding=SG_FACEWINDING_CCW;
    ground.depth.pixel_format=SG_PIXELFORMAT_DEPTH;
    ground.depth.write_enabled=true;ground.depth.compare=SG_COMPAREFUNC_LESS_EQUAL;
    ground.colors[0].pixel_format=SG_PIXELFORMAT_RGBA8;ground.sample_count=1;
    ground.label="yard terrain";
    ground_pipeline=sg_make_pipeline(ground);
    sg_pipeline_desc grass{};
    grass.shader=sg_make_shader(grass_shader_desc(sg_query_backend()));
    grass.layout.buffers[1].stride=sizeof(float);
    grass.layout.buffers[1].step_func=SG_VERTEXSTEP_PER_INSTANCE;
    grass.layout.buffers[1].step_rate=1;
    grass.layout.attrs[ATTR_grass_blade].format=SG_VERTEXFORMAT_FLOAT2;
    grass.layout.attrs[ATTR_grass_root_height].buffer_index=1;
    grass.layout.attrs[ATTR_grass_root_height].format=SG_VERTEXFORMAT_FLOAT;
    grass.depth=ground.depth;grass.colors[0]=ground.colors[0];grass.sample_count=1;
    grass.cull_mode=SG_CULLMODE_NONE;grass.label="yard grass";
    grass_pipeline=sg_make_pipeline(grass);
    if(sg_query_pipeline_state(ground_pipeline)!=SG_RESOURCESTATE_VALID ||
       sg_query_pipeline_state(grass_pipeline)!=SG_RESOURCESTATE_VALID)throw std::runtime_error("Cannot create yard pipelines");
    std::printf("Yard: %.2f m voxel terrain, %zu triangles, %d grass blades, %d draw regions\n",
        terrain.size*.01,terrain.mesh.index_count/3,terrain.size*terrain.size,layout.count);
    free(layout.roots);layout.roots=nullptr;
    yard_mesh_destroy(&terrain.mesh);
}
float yard_scene_height(float x,float z) {return yard_terrain_height(&terrain,x,z);}
void yard_scene_draw(const vs_params_t& view,const light_params_t& light,bool grass) {
    auto uniforms=view;
    uniforms.lens[1]=float(terrain.size);uniforms.lens[2]=1;
    yard_frustum frustum;
    yard_frustum_make(&frustum,uniforms.camera_position,uniforms.view[0],uniforms.view[1],uniforms.view[2],uniforms.lens[0]);
    sg_apply_pipeline(ground_pipeline);sg_apply_bindings(ground_bindings);
    sg_apply_uniforms(UB_vs_params,SG_RANGE(uniforms));sg_apply_uniforms(UB_light_params,SG_RANGE(light));
    for(int i=0;i<layout.count;++i) {
        const auto& region=layout.regions[i];
        if(region.index_count&&yard_frustum_visible(&frustum,&region.ground))sg_draw(region.index_start,region.index_count,1);
    }
    if(!grass)return;
    sg_apply_pipeline(grass_pipeline);
    sg_apply_uniforms(UB_vs_params,SG_RANGE(uniforms));sg_apply_uniforms(UB_light_params,SG_RANGE(light));
    for(int i=0;i<layout.count;++i) {
        const auto& region=layout.regions[i];
        if(!yard_frustum_visible(&frustum,&region.grass))continue;
        grass_bindings.vertex_buffer_offsets[1]=region.root_start*int(sizeof(float));
        sg_apply_bindings(grass_bindings);
        grass_region_params_t region_uniforms{{float(region.x),float(region.z),float(region.width),0}};
        sg_apply_uniforms(UB_grass_region_params,SG_RANGE(region_uniforms));
        sg_draw(0,3,region.width*region.depth);
    }
}
void yard_scene_destroy() {
    // GPU resources are released together by the application's sg_shutdown().
    yard_draw_layout_destroy(&layout);yard_terrain_destroy(&terrain);
}
