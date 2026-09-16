#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"
#include "cube.glsl.h"
#include "astronomy.h"
#include "skyglow.h"
#include "camera.h"
#include "terrain.h"
#include "visibility.h"
#include "tree.h"
#include "geometry.h"
#include <cstddef>
#include <exception>
#include <stdexcept>
#include <utility>


static struct {
    sg_pipeline preview_pipeline;
    sg_bindings preview_bindings;
    sg_attachments camera_attachments;
    sg_pipeline downsample_pipeline;
    sg_attachments downsample_attachments;
    sg_bindings downsample_bindings;
    int ssaa, render_width, render_height;
    bool grass_volume;
    float lod_start, lod_end;
    sg_pipeline volume_pipeline;
    sg_bindings volume_bindings;
    sg_attachments volume_attachments;
    sg_view scene_texture, volume_texture;
    volume_params_t volume_params;
    float vertical_fov;
    yard_camera camera;
    double capture_elapsed;
    unsigned captures;
    sg_pipeline pipeline;
    const char* tree_species;
    bool geometry_demo;
    int object_index_count;
    sg_pipeline object_pipeline;
    sg_bindings object_bindings;
    sg_pipeline grass_pipeline;
    sg_bindings grass_bindings;
    int grass_count;
    int grass_stride, msaa;
    bool no_grass, no_culling;
    yard_draw_layout layout;
    uint64_t submitted_grass, submitted_triangles;
    sg_pipeline sky_pipeline;
    sg_bindings sky_bindings;
    double utc;
    bool track_moon, zoom;
    yard_ephemeris ephemeris;
    yard_site site;
    float yaw, pitch;
    float position[4];
    bool keys[SAPP_MAX_KEYCODES];
    int64_t title_minute;
    sg_bindings bindings;
    yard_terrain terrain;
    int terrain_index_count;
    float eye_height;
    bool paused;
    bool smoke_test, terrain_smoke_test;
    double smoke_elapsed;
} state;

static void sunlight(const double direction[3], float color[4], double intensity) {
    // Same spherical atmosphere and quadrature as the sky shader, evaluated
    // once per frame for the directional light at ground level.
    double radius = 6360.0025, b = radius * direction[1];
    double step = (-b + sqrt(b*b - radius*radius + 6460.0*6460.0)) / 8.0;
    double depth_r = 0, depth_m = 0;
    for (int i = 0; i < 8; ++i) {
        double t = (i + 0.5) * step;
        double altitude = fmax(0, sqrt(radius*radius + t*t + 2*b*t) - 6360.0);
        depth_r += exp(-altitude/8.0) * step;
        depth_m += exp(-altitude/1.2) * step;
    }
    const double beta_r[3] = {0.0058, 0.0135, 0.0331};
    for (int i = 0; i < 3; ++i) {
        color[i] = direction[1] > 0 ? (float)(intensity*exp(-beta_r[i]*depth_r - 0.0044*depth_m)) : 0;
    }
    color[3] = 0;
}

static void make_objects() {
    using namespace yard::geometry;
    struct RenderVertex { Vertex geometry; Vec3 color; };
    std::vector<RenderVertex> vertices;
    std::vector<uint32_t> indices;
    auto append = [&](Mesh mesh, Vec3 color, Vec3 offset, bool checker=false) {
        auto base = static_cast<uint32_t>(vertices.size());
        for (auto vertex : mesh.vertices) {
            vertex.position.x += offset.x; vertex.position.y += offset.y; vertex.position.z += offset.z;
            if (!checker) vertex.uv = {0,0};
            vertices.push_back({vertex,color});
        }
        for (auto i : mesh.indices) indices.push_back(base+i);
    };
    // Keep PR #7's half-metre occlusion reference cube, embedded in the soil.
    const float bottom=yard_terrain_height(&state.terrain,0,0)-.08f;
    const std::array<Ring,1> square{Ring{{0,0},{.5f,0},{.5f,.5f},{0,.5f}}};
    const Vec3 origins[]={{-.25f,bottom,-.25f},{.25f,bottom,-.25f},
        {-.25f,bottom,-.25f},{-.25f,bottom+.5f,.25f},
        {.25f,bottom,-.25f},{-.25f,bottom,.25f}};
    const Vec3 axes_u[]={{0,0,1},{0,1,0},{1,0,0},{1,0,0},{-1,0,0},{1,0,0}};
    const Vec3 axes_v[]={{0,1,0},{0,0,1},{0,0,1},{0,0,-1},{0,1,0},{0,1,0}};
    for(int i=0;i<6;++i) append(polygon(square,origins[i],axes_u[i],axes_v[i]),{.77f,.77f,.77f},{0,0,0});
    if(state.geometry_demo) {
        const std::array<Bezier,1> curve{Bezier{{Vec3{2,0,0},Vec3{3,1,0},Vec3{1.5f,3,0},Vec3{2.5f,4,0}},.35f,.08f}};
        append(sweep(sample_curve(curve),circle_profile(16)),{.65f,.45f,.25f},{0,0,0},true);
        const std::array<Ring,2> panel{Ring{{0,0},{2,0},{2,3},{0,3}},Ring{{.5f,1},{1.5f,1},{1.5f,2},{.5f,2}}};
        append(polygon(panel,{-4,0,0}),{.35f,.65f,.4f},{0,0,0},true);
    }
    if(state.tree_species || state.smoke_test) {
        auto skeleton=yard::tree::generate(yard::tree::preset(state.tree_species ? state.tree_species : "fan_palm"),123);
        auto tree=yard::tree::mesh(skeleton);
        Vec3 root{0,yard_terrain_height(&state.terrain,0,-6)-.02f,-6};
        append(std::move(tree.wood),{.40f,.27f,.16f},root);
        append(std::move(tree.leaves),{.23f,.46f,.13f},root);
        append(std::move(tree.blossoms),{.95f,.65f,.75f},root);
    }
    state.object_index_count=static_cast<int>(indices.size());
    sg_buffer_desc buffer{};
    buffer.data={vertices.data(),vertices.size()*sizeof(RenderVertex)};buffer.label="scene object vertices";
    state.object_bindings.vertex_buffers[0]=sg_make_buffer(buffer);
    buffer={};buffer.usage.index_buffer=true;
    buffer.data={indices.data(),indices.size()*sizeof(uint32_t)};buffer.label="scene object indices";
    state.object_bindings.index_buffer=sg_make_buffer(buffer);
    sg_pipeline_desc pipeline{};
    pipeline.shader=sg_make_shader(object_shader_desc(sg_query_backend()));
    pipeline.layout.buffers[0].stride=sizeof(RenderVertex);
    pipeline.layout.attrs[ATTR_object_position].format=SG_VERTEXFORMAT_FLOAT3;
    pipeline.layout.attrs[ATTR_object_position].offset=offsetof(RenderVertex,geometry)+offsetof(Vertex,position);
    pipeline.layout.attrs[ATTR_object_normal].format=SG_VERTEXFORMAT_FLOAT3;
    pipeline.layout.attrs[ATTR_object_normal].offset=offsetof(RenderVertex,geometry)+offsetof(Vertex,normal);
    pipeline.layout.attrs[ATTR_object_color].format=SG_VERTEXFORMAT_FLOAT3;
    pipeline.layout.attrs[ATTR_object_color].offset=offsetof(RenderVertex,color);
    pipeline.layout.attrs[ATTR_object_uv].format=SG_VERTEXFORMAT_FLOAT2;
    pipeline.layout.attrs[ATTR_object_uv].offset=offsetof(RenderVertex,geometry)+offsetof(Vertex,uv);
    pipeline.index_type=SG_INDEXTYPE_UINT32;pipeline.cull_mode=SG_CULLMODE_NONE;
    pipeline.face_winding=SG_FACEWINDING_CCW;
    pipeline.depth.pixel_format=SG_PIXELFORMAT_DEPTH;pipeline.depth.write_enabled=true;
    pipeline.depth.compare=SG_COMPAREFUNC_LESS_EQUAL;
    pipeline.colors[0].pixel_format=SG_PIXELFORMAT_RGBA16F;pipeline.sample_count=state.msaa;
    pipeline.label="opaque scene objects";
    state.object_pipeline=sg_make_pipeline(pipeline);
    if(sg_query_pipeline_state(state.object_pipeline)!=SG_RESOURCESTATE_VALID ||
       sg_query_buffer_state(state.object_bindings.vertex_buffers[0])!=SG_RESOURCESTATE_VALID ||
       sg_query_buffer_state(state.object_bindings.index_buffer)!=SG_RESOURCESTATE_VALID)
        throw std::runtime_error("Cannot upload scene objects");
}

static void make_grass_volume(void) {
    const int step=4, side=(state.terrain.size-1)/step+1;
    size_t count=(size_t)side*side;
    float *heights=static_cast<float*>(malloc(count*sizeof(float)));
    if (!heights) { fprintf(stderr,"Cannot allocate volume height field.\n"); exit(EXIT_FAILURE); }
    float low=1e6f,high=-1e6f,dx=0,dz=0;
    for (int z=0; z<side; ++z) for (int x=0; x<side; ++x) {
        float h=state.terrain.heights[z*step*state.terrain.size+x*step]*0.01f;
        heights[z*side+x]=h;
        low=fminf(low,h); high=fmaxf(high,h);
        if (x) dx=fmaxf(dx,fabsf(h-heights[z*side+x-1])/0.04f);
        if (z) dz=fmaxf(dz,fabsf(h-heights[(z-1)*side+x])/0.04f);
    }
    volume_params_t volume_uniforms{};
    volume_uniforms.volume_field[0] = (0.5f-state.terrain.size*0.5f)*0.01f;
    volume_uniforms.volume_field[1] = 0.04f;
    volume_uniforms.volume_field[2] = (float)side;
    volume_uniforms.volume_field[3] = hypotf(dx,dz)+0.0001f;
    volume_uniforms.volume_bounds[0] = low;
    volume_uniforms.volume_bounds[1] = high+0.05f;
    volume_uniforms.volume_bounds[2] = state.terrain.size*0.005f;
    volume_uniforms.volume_bounds[3] = 0;
    state.volume_params=volume_uniforms;
    sg_image_desc field_desc{};
    field_desc.width = side;
    field_desc.height = side;
    field_desc.pixel_format = SG_PIXELFORMAT_R32F;
    field_desc.data.mip_levels[0] = {heights,count*sizeof(float)};
    field_desc.label = "4 cm volume root height field";
    sg_image field=sg_make_image(field_desc);
    free(heights);
    sg_image_desc volume_output_desc{};
    volume_output_desc.width = state.render_width;
    volume_output_desc.height = state.render_height;
    volume_output_desc.usage.color_attachment = true;
    volume_output_desc.pixel_format = SG_PIXELFORMAT_RGBA16F;
    volume_output_desc.sample_count = 1;
    volume_output_desc.label = "grass volume composite";
    sg_image output=sg_make_image(volume_output_desc);
    sg_view_desc volume_attachment_view{};
    volume_attachment_view.color_attachment.image = output;
    state.volume_attachments.colors[0]=sg_make_view(volume_attachment_view);
    sg_view_desc volume_texture_view{};
    volume_texture_view.texture.image = output;
    state.volume_texture=sg_make_view(volume_texture_view);
    sg_sampler sampler=state.downsample_bindings.samplers[SMP_source_sampler];
    sg_bindings volume_bindings{};
    volume_bindings.vertex_buffers[0] = state.sky_bindings.vertex_buffers[0];
    volume_bindings.views[VIEW_scene_image] = state.scene_texture;
    sg_view_desc height_texture_view{};
    height_texture_view.texture.image = field;
    volume_bindings.views[VIEW_height_image] = sg_make_view(height_texture_view);
    volume_bindings.samplers[SMP_scene_sampler] = sampler;
    volume_bindings.samplers[SMP_height_sampler] = sampler;
    state.volume_bindings=volume_bindings;
    sg_pipeline_desc volume_pipeline{};
    volume_pipeline.shader = sg_make_shader(volume_shader_desc(sg_query_backend()));
    volume_pipeline.layout.attrs[ATTR_volume_position].format = SG_VERTEXFORMAT_FLOAT2;
    volume_pipeline.depth.pixel_format = SG_PIXELFORMAT_NONE;
    volume_pipeline.depth.compare = SG_COMPAREFUNC_ALWAYS;
    volume_pipeline.colors[0].pixel_format = SG_PIXELFORMAT_RGBA16F;
    volume_pipeline.sample_count = 1;
    volume_pipeline.label = "shallow grass density volume";
    state.volume_pipeline=sg_make_pipeline(volume_pipeline);
    if (sg_query_image_state(field)!=SG_RESOURCESTATE_VALID ||
        sg_query_image_state(output)!=SG_RESOURCESTATE_VALID ||
        sg_query_pipeline_state(state.volume_pipeline)!=SG_RESOURCESTATE_VALID) {
        fprintf(stderr,"Cannot create grass volume resources.\n"); exit(EXIT_FAILURE);
    }
    printf("Yard: grass volume %s, transition %.1f–%.1f m, %dx%d height field, slope bound %.3f\n",
           state.grass_volume ? "on" : "off",state.lod_start,state.lod_end,side,side,state.volume_params.volume_field[3]);
}

static void init(void) {
    sg_desc graphics{};
    graphics.environment = sglue_environment();
    graphics.logger.func = slog_func;
    sg_setup(graphics);

    double scale = sqrt((double)state.ssaa);
    state.render_width = (int)ceil(state.camera.profile.width*scale);
    state.render_height = (int)ceil(state.camera.profile.height*scale);
    int maximum = sg_query_limits().max_image_size_2d;
    if (state.render_width > maximum || state.render_height > maximum) {
        fprintf(stderr,"Supersampled camera exceeds device texture limit %d.\n",maximum);
        exit(EXIT_FAILURE);
    }
    printf("Yard: %dx SSAA, %dx%d internal -> %dx%d camera, %dx MSAA\n",
           state.ssaa,state.render_width,state.render_height,state.camera.profile.width,state.camera.profile.height,state.msaa);
    static const float sky_vertices[][2] = {{-1,-1}, {3,-1}, {-1,3}};
    sg_buffer_desc sky_buffer{};
    sky_buffer.data = SG_RANGE(sky_vertices);
    sky_buffer.label = "sky triangle";
    state.sky_bindings.vertex_buffers[0] = sg_make_buffer(sky_buffer);
    sg_image_desc color_desc{};
    color_desc.usage.color_attachment = true;
    color_desc.width = state.render_width;
    color_desc.height = state.render_height;
    color_desc.pixel_format = SG_PIXELFORMAT_RGBA16F;
    color_desc.sample_count = state.msaa;
    color_desc.label = "SVGA camera color";
    sg_image camera_color = sg_make_image(color_desc);
    sg_image_desc depth_desc{};
    depth_desc.usage.depth_stencil_attachment = true;
    depth_desc.width = state.render_width;
    depth_desc.height = state.render_height;
    depth_desc.pixel_format = SG_PIXELFORMAT_DEPTH;
    depth_desc.sample_count = state.msaa;
    depth_desc.label = "SVGA camera depth";
    sg_image camera_depth = sg_make_image(depth_desc);
    sg_attachments attachments{};
    sg_view_desc color_attachment_view{};
    color_attachment_view.color_attachment.image = camera_color;
    attachments.colors[0] = sg_make_view(color_attachment_view);
    sg_view_desc depth_attachment_view{};
    depth_attachment_view.depth_stencil_attachment.image = camera_depth;
    attachments.depth_stencil = sg_make_view(depth_attachment_view);
    state.camera_attachments = attachments;
    sg_image camera_output = camera_color;
    if (state.msaa > 1) {
        sg_image_desc resolve_desc{};
        resolve_desc.usage.resolve_attachment = true;
        resolve_desc.width = state.render_width;
        resolve_desc.height = state.render_height;
        resolve_desc.pixel_format = SG_PIXELFORMAT_RGBA16F;
        resolve_desc.sample_count = 1;
        resolve_desc.label = "resolved SVGA camera image";
        camera_output = sg_make_image(resolve_desc);
        sg_view_desc resolve_view{};
        resolve_view.resolve_attachment.image = camera_output;
        state.camera_attachments.resolves[0] = sg_make_view(resolve_view);
    }
    if (sg_query_image_state(camera_color) != SG_RESOURCESTATE_VALID ||
        sg_query_image_state(camera_depth) != SG_RESOURCESTATE_VALID ||
        sg_query_image_state(camera_output) != SG_RESOURCESTATE_VALID) {
        fprintf(stderr, "Cannot create %dx camera attachments.\n", state.msaa);
        exit(EXIT_FAILURE);
    }
    sg_image_desc final_desc{};
    final_desc.usage.color_attachment = true;
    final_desc.width = state.camera.profile.width;
    final_desc.height = state.camera.profile.height;
    final_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
    final_desc.sample_count = 1;
    final_desc.label = "final RGBA8 camera image";
    sg_image camera_final = sg_make_image(final_desc);
    sg_view_desc final_view{};
    final_view.color_attachment.image = camera_final;
    sg_view final_attachment = sg_make_view(final_view);
    sg_view_desc source_view{};
    source_view.texture.image = camera_output;
    sg_view current_texture = sg_make_view(source_view);
    state.scene_texture=current_texture;
    if (sg_query_image_state(camera_final) != SG_RESOURCESTATE_VALID) {
        fprintf(stderr,"Cannot create final camera image.\n"); exit(EXIT_FAILURE);
    }
    state.downsample_attachments.colors[0] = final_attachment;
    sg_bindings downsample_bindings{};
    downsample_bindings.vertex_buffers[0] = state.sky_bindings.vertex_buffers[0];
    downsample_bindings.views[VIEW_source_image] = current_texture;
    sg_sampler_desc source_sampler{};
    source_sampler.min_filter = SG_FILTER_NEAREST;
    source_sampler.mag_filter = SG_FILTER_NEAREST;
    source_sampler.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    source_sampler.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    downsample_bindings.samplers[SMP_source_sampler] = sg_make_sampler(source_sampler);
    state.downsample_bindings = downsample_bindings;
    sg_pipeline_desc downsample_pipeline{};
    downsample_pipeline.shader = sg_make_shader(downsample_shader_desc(sg_query_backend()));
    downsample_pipeline.layout.attrs[ATTR_downsample_position].format = SG_VERTEXFORMAT_FLOAT2;
    downsample_pipeline.depth.pixel_format = SG_PIXELFORMAT_NONE;
    downsample_pipeline.depth.compare = SG_COMPAREFUNC_ALWAYS;
    downsample_pipeline.colors[0].pixel_format = SG_PIXELFORMAT_RGBA8;
    downsample_pipeline.sample_count = 1;
    downsample_pipeline.label = "area-filtered supersampling resolve";
    state.downsample_pipeline = sg_make_pipeline(downsample_pipeline);
    sg_bindings preview_bindings{};
    preview_bindings.vertex_buffers[0] = state.sky_bindings.vertex_buffers[0];
    sg_view_desc preview_view{};
    preview_view.texture.image = camera_final;
    preview_bindings.views[VIEW_camera_image] = sg_make_view(preview_view);
    sg_sampler_desc preview_sampler{};
    preview_sampler.min_filter = SG_FILTER_NEAREST;
    preview_sampler.mag_filter = SG_FILTER_NEAREST;
    preview_sampler.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    preview_sampler.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    preview_bindings.samplers[SMP_camera_sampler] = sg_make_sampler(preview_sampler);
    state.preview_bindings = preview_bindings;
    sg_pipeline_desc preview_pipeline{};
    preview_pipeline.shader = sg_make_shader(preview_shader_desc(sg_query_backend()));
    preview_pipeline.layout.attrs[ATTR_preview_position].format = SG_VERTEXFORMAT_FLOAT2;
    preview_pipeline.depth.compare = SG_COMPAREFUNC_ALWAYS;
    preview_pipeline.label = "camera preview";
    state.preview_pipeline = sg_make_pipeline(preview_pipeline);
    sg_pipeline_desc sky_pipeline{};
    sky_pipeline.shader = sg_make_shader(sky_shader_desc(sg_query_backend()));
    sky_pipeline.layout.attrs[ATTR_sky_position].format = SG_VERTEXFORMAT_FLOAT2;
    sky_pipeline.depth.pixel_format = SG_PIXELFORMAT_DEPTH;
    sky_pipeline.depth.write_enabled = false;
    sky_pipeline.depth.compare = SG_COMPAREFUNC_ALWAYS;
    sky_pipeline.colors[0].pixel_format = SG_PIXELFORMAT_RGBA16F;
    sky_pipeline.sample_count = state.msaa;
    sky_pipeline.label = "atmosphere and ground";
    state.sky_pipeline = sg_make_pipeline(sky_pipeline);

    if (!yard_terrain_create(&state.terrain, YARD_TERRAIN_SIZE)) {
        fprintf(stderr, "Cannot allocate terrain or build its mesh.\n");
        exit(EXIT_FAILURE);
    }
    printf("Yard: marching cubes, %d cm mesh spacing, %.2f m square, %zu density bytes, %zu vertices, %zu triangles, %.1f MiB mesh\n",
           YARD_TERRAIN_MESH_STEP, state.terrain.size*0.01,
           (size_t)state.terrain.size*state.terrain.size*YARD_TERRAIN_DEPTH,
           state.terrain.mesh.vertex_count, state.terrain.mesh.index_count/3,
           (state.terrain.mesh.vertex_count*sizeof(yard_mesh_vertex)+state.terrain.mesh.index_count*sizeof(uint32_t))/1048576.0);
    if (!yard_draw_layout_create(&state.layout, &state.terrain)) {
        fprintf(stderr,"Cannot build visibility draw regions.\n");
        exit(EXIT_FAILURE);
    }
    printf("Yard: %d static draw regions, frustum culling %s\n",
           state.layout.count,state.no_culling ? "off" : "on");
    state.terrain_index_count = (int)state.terrain.mesh.index_count;
    sg_buffer_desc terrain_vertices{};
    terrain_vertices.data = {state.terrain.mesh.vertices, state.terrain.mesh.vertex_count*sizeof(yard_mesh_vertex)};
    terrain_vertices.label = "static voxel terrain vertices";
    state.bindings.vertex_buffers[0] = sg_make_buffer(terrain_vertices);
    sg_buffer_desc terrain_indices{};
    terrain_indices.usage.index_buffer = true;
    terrain_indices.data = {state.terrain.mesh.indices, state.terrain.mesh.index_count*sizeof(uint32_t)};
    terrain_indices.label = "static voxel terrain indices";
    state.bindings.index_buffer = sg_make_buffer(terrain_indices);
    if (sg_query_buffer_state(state.bindings.vertex_buffers[0]) != SG_RESOURCESTATE_VALID ||
        sg_query_buffer_state(state.bindings.index_buffer) != SG_RESOURCESTATE_VALID) {
        fprintf(stderr, "Cannot upload terrain mesh.\n");
        exit(EXIT_FAILURE);
    }
    static const float grass_triangle[][2] = {{-0.0025f,0},{0.0025f,0},{0,0.05f}};
    state.grass_count = state.terrain.size*state.terrain.size;
    sg_buffer_desc grass_vertices{};
    grass_vertices.data = SG_RANGE(grass_triangle);
    grass_vertices.label = "5 cm grass triangle";
    state.grass_bindings.vertex_buffers[0] = sg_make_buffer(grass_vertices);
    sg_buffer_desc grass_roots{};
    grass_roots.data = {state.layout.roots, (size_t)state.grass_count*sizeof(float)};
    grass_roots.label = "one grass root per surface voxel";
    state.grass_bindings.vertex_buffers[1] = sg_make_buffer(grass_roots);
    if (sg_query_buffer_state(state.grass_bindings.vertex_buffers[0]) != SG_RESOURCESTATE_VALID ||
        sg_query_buffer_state(state.grass_bindings.vertex_buffers[1]) != SG_RESOURCESTATE_VALID) {
        fprintf(stderr, "Cannot upload grass roots.\n");
        exit(EXIT_FAILURE);
    }
    free(state.layout.roots);
    state.layout.roots = nullptr;
    sg_pipeline_desc grass_pipeline{};
    grass_pipeline.shader = sg_make_shader(grass_shader_desc(sg_query_backend()));
    grass_pipeline.layout.buffers[1].stride = (int)sizeof(float)*state.grass_stride;
    grass_pipeline.layout.buffers[1].step_func = SG_VERTEXSTEP_PER_INSTANCE;
    grass_pipeline.layout.buffers[1].step_rate = 1;
    grass_pipeline.layout.attrs[ATTR_grass_blade].buffer_index = 0;
    grass_pipeline.layout.attrs[ATTR_grass_blade].format = SG_VERTEXFORMAT_FLOAT2;
    grass_pipeline.layout.attrs[ATTR_grass_root_height].buffer_index = 1;
    grass_pipeline.layout.attrs[ATTR_grass_root_height].format = SG_VERTEXFORMAT_FLOAT;
    grass_pipeline.cull_mode = SG_CULLMODE_NONE;
    grass_pipeline.depth.pixel_format = SG_PIXELFORMAT_DEPTH;
    grass_pipeline.depth.write_enabled = true;
    grass_pipeline.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
    grass_pipeline.colors[0].pixel_format = SG_PIXELFORMAT_RGBA16F;
    grass_pipeline.sample_count = state.msaa;
    grass_pipeline.label = "two-sided grass triangles";
    state.grass_pipeline = sg_make_pipeline(grass_pipeline);
    printf("Yard: %d grass blades, 5 cm tall, 5 mm wide, %.1f MiB root buffer\n",
           state.grass_count, state.grass_count*sizeof(float)/1048576.0);
    state.grass_count = 0;
    if (!state.no_grass) for (int i=0; i<state.layout.count; ++i) {
        const yard_draw_region *r=&state.layout.regions[i];
        state.grass_count += (r->width*r->depth+state.grass_stride-1)/state.grass_stride;
    }
    printf("Yard: %dx MSAA, %d blades before culling (stride %d)\n", state.msaa,state.grass_count,state.grass_stride);
    try { make_objects(); }
    catch(const std::exception& e) { fprintf(stderr,"Scene generation failed: %s\n",e.what());exit(EXIT_FAILURE); }
    make_grass_volume();
    yard_mesh_destroy(&state.terrain.mesh);
    sg_shader shader = sg_make_shader(cube_shader_desc(sg_query_backend()));
    sg_pipeline_desc terrain_pipeline{};
    terrain_pipeline.shader = shader;
    terrain_pipeline.layout.attrs[ATTR_cube_position].format = SG_VERTEXFORMAT_FLOAT3;
    terrain_pipeline.layout.attrs[ATTR_cube_normal].format = SG_VERTEXFORMAT_FLOAT3;
    terrain_pipeline.index_type = SG_INDEXTYPE_UINT32;
    terrain_pipeline.cull_mode = SG_CULLMODE_BACK;
    terrain_pipeline.face_winding = SG_FACEWINDING_CCW;
    terrain_pipeline.depth.pixel_format = SG_PIXELFORMAT_DEPTH;
    terrain_pipeline.depth.write_enabled = true;
    terrain_pipeline.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
    terrain_pipeline.colors[0].pixel_format = SG_PIXELFORMAT_RGBA16F;
    terrain_pipeline.sample_count = state.msaa;
    terrain_pipeline.label = "voxel terrain pipeline";
    state.pipeline = sg_make_pipeline(terrain_pipeline);
}

static void frame(void) {
    if (state.captures > 0) state.smoke_elapsed += sapp_frame_duration();
    float dt = (float)fmin(sapp_frame_duration(), 0.1);
    if (!state.paused) state.utc += dt * 360.0; // Four minutes per day.
    state.utc += dt * 7200.0 * ((int)state.keys[SAPP_KEYCODE_RIGHT] - (int)state.keys[SAPP_KEYCODE_LEFT]);
    // Keep the calendar within the documented range of this visual ephemeris.
    state.utc = fmax(-2208970800.0, fmin(4133998799.0, state.utc));
    // Viewer follows the voxel surface; no rigid-body controller or collision solver.
    float forward = (float)((int)state.keys[SAPP_KEYCODE_W] - (int)state.keys[SAPP_KEYCODE_S]);
    float right = (float)((int)state.keys[SAPP_KEYCODE_D] - (int)state.keys[SAPP_KEYCODE_A]);
    float length = hypotf(forward, right);
    if (length > 0) {
        float speed = (state.keys[SAPP_KEYCODE_LEFT_SHIFT] || state.keys[SAPP_KEYCODE_RIGHT_SHIFT]) ? 9.0f : 3.0f;
        float step = speed * dt / length;
        state.position[0] += (sinf(state.yaw)*forward + cosf(state.yaw)*right)*step;
        state.position[2] += (-cosf(state.yaw)*forward + sinf(state.yaw)*right)*step;
    }
    if (state.smoke_test) {
        const char *dates[] = {"2026-01-18", "2026-01-25", "2026-02-01", "2026-02-09"};
        const double hours[] = {12, 19, 21, 5};
        unsigned preset = (state.captures / 30) % 4;
        yard_local_datetime(dates[preset], hours[preset], &state.utc);
        state.track_moon = true;
        state.zoom = true;
    }
    if (state.terrain_smoke_test) {
        yard_local_datetime("2026-09-14", 9, &state.utc);
        float progress = state.captures / 120.0f;
        state.position[0] = 12.0f*sinf(progress*6.2831853f);
        state.position[2] = 12.0f*cosf(progress*6.2831853f);
        state.yaw = progress*6.2831853f;
        state.pitch = -0.35f;
        state.track_moon = false;
        state.zoom = false;
    }
    state.position[1] = yard_terrain_height(&state.terrain, state.position[0], state.position[2]) + state.eye_height;
    yard_astronomy(state.utc, state.site.latitude, state.site.longitude, &state.ephemeris);
    if (state.track_moon) {
        state.yaw = (float)atan2(state.ephemeris.moon[0], -state.ephemeris.moon[2]);
        state.pitch = (float)asin(state.ephemeris.moon[1]);
    }
    state.capture_elapsed += dt;
    int64_t minute = (int64_t)floor(state.utc / 60);
    if (minute != state.title_minute) {
        char title[384], date[64];
        struct tm local;
        yard_local_calendar(state.utc, &local);
        strftime(date, sizeof(date), "%Y-%m-%d %H:%M %Z", &local);
        snprintf(title, sizeof(title), "Yard | %dx%d %dx SSAA %dx MSAA | Culling %s | Grass %s | Manual %.2f ms %.1fx (AEC %d, gain %d) | Lat %.5f, Lon %.5f | %s | Moon %.0f%% %s%s",
                 state.camera.profile.width, state.camera.profile.height, state.ssaa, state.msaa, state.no_culling ? "off" : "on", state.grass_volume ? "volume LOD" : "blades", yard_camera_exposure_ms(&state.camera),
                 yard_camera_gain(&state.camera), state.camera.exposure_lines, state.camera.gain_index,
                 state.site.latitude, state.site.longitude, date, state.ephemeris.illuminated*100, state.ephemeris.waxing ? "waxing" : "waning",
                 state.ephemeris.moon[1] < 0 ? " (below horizon)" : "");
        sapp_set_window_title(title);
        state.title_minute = minute;
    }
    if (sapp_width() <= 0 || sapp_height() <= 0) return;
    if (state.captures == 0 || state.capture_elapsed >= (1.0 / state.camera.profile.fps)) {
        state.capture_elapsed = fmod(state.capture_elapsed, (1.0 / state.camera.profile.fps));
        const vs_params_t uniforms = {
            .view = {state.yaw, state.pitch, (float)state.camera.profile.width / state.camera.profile.height, 0},
            .lens = {1.0f / tanf(state.vertical_fov * 0.00872664626f), (float)state.terrain.size, (float)state.grass_stride, 0},
            .camera_position = {state.position[0], state.position[1], state.position[2], 0},
            .grass_lod = {state.grass_volume && !state.no_grass ? 1.0f : 0.0f,
                          state.lod_start,state.lod_end,1.0f/state.grass_stride},
        };
        light_params_t light = {};
        sunlight(state.ephemeris.sun, light.sun_color, 3.0);
        light.camera_exposure[0] = yard_camera_multiplier(&state.camera);
        yard_night_light(&state.site, state.ephemeris.sun[1], light.night_radiance);
        for (int i=0; i<3; ++i) light.sun_direction[i] = (float)state.ephemeris.sun[i];
        sky_params_t sky = {};
        sky.sky_camera_exposure[0] = light.camera_exposure[0];
        memcpy(sky.sky_night_radiance, light.night_radiance, sizeof(sky.sky_night_radiance));
        memcpy(sky.sky_camera_position, uniforms.camera_position, sizeof(sky.sky_camera_position));
        memcpy(sky.sky_lens, uniforms.lens, sizeof(sky.sky_lens));
        memcpy(sky.sky_view, uniforms.view, sizeof(sky.sky_view));
        memcpy(sky.sky_sun, light.sun_direction, sizeof(sky.sky_sun));
        memcpy(sky.sky_sun_color, light.sun_color, sizeof(sky.sky_sun_color));
        for (int i=0; i<3; ++i) {
            sky.sky_moon[i] = (float)state.ephemeris.moon[i];
            sky.moon_sun[i] = (float)state.ephemeris.moon_to_sun[i];
            sky.moon_north[i] = (float)state.ephemeris.celestial_north[i];
        }
        sky.sky_moon[3] = (float)state.ephemeris.moon_radius;
        sg_pass camera_pass{};
        camera_pass.action.colors[0].load_action = SG_LOADACTION_CLEAR;
        camera_pass.action.colors[0].clear_value.r = .055f;
        camera_pass.action.colors[0].clear_value.g = .075f;
        camera_pass.action.colors[0].clear_value.b = .09f;
        camera_pass.action.colors[0].clear_value.a = 1;
        camera_pass.attachments = state.camera_attachments;
        sg_begin_pass(camera_pass);
        sg_apply_pipeline(state.sky_pipeline);
        sg_apply_bindings(&state.sky_bindings);
        sg_apply_uniforms(UB_sky_params, SG_RANGE(sky));
        sg_draw(0, 3, 1);
        sg_apply_pipeline(state.pipeline);
        sg_apply_bindings(&state.bindings);
        sg_apply_uniforms(UB_vs_params, SG_RANGE(uniforms));
        sg_apply_uniforms(UB_light_params, SG_RANGE(light));
        yard_frustum frustum;
        yard_frustum_make(&frustum,state.position,state.yaw,state.pitch,
                          uniforms.view[2],uniforms.lens[0]);
        if (state.no_culling) {
            sg_draw(0,state.terrain_index_count,1);
            state.submitted_triangles += state.terrain_index_count/3;
        } else for (int i=0; i<state.layout.count; ++i) {
            const yard_draw_region *r=&state.layout.regions[i];
            if (r->index_count && yard_frustum_visible(&frustum,&r->ground)) {
                sg_draw(r->index_start,r->index_count,1);
                state.submitted_triangles += r->index_count/3;
            }
        }
        sg_apply_pipeline(state.object_pipeline);
        sg_apply_bindings(&state.object_bindings);
        sg_apply_uniforms(UB_vs_params,SG_RANGE(uniforms));
        sg_apply_uniforms(UB_light_params,SG_RANGE(light));
        sg_draw(0,state.object_index_count,1);
        if (state.grass_count > 0) {
            sg_apply_pipeline(state.grass_pipeline);
            sg_apply_uniforms(UB_vs_params, SG_RANGE(uniforms));
            sg_apply_uniforms(UB_light_params, SG_RANGE(light));
            for (int i=0; i<state.layout.count; ++i) {
                const yard_draw_region *r=&state.layout.regions[i];
                if (!state.no_culling && !yard_frustum_visible(&frustum,&r->grass)) continue;
                if (state.grass_volume) {
                    float dx=fmaxf(r->grass.min[0]-state.position[0],fmaxf(0,state.position[0]-r->grass.max[0]));
                    float dz=fmaxf(r->grass.min[2]-state.position[2],fmaxf(0,state.position[2]-r->grass.max[2]));
                    if (hypotf(dx,dz)>=state.lod_end) continue;
                }
                const grass_region_params_t region = {.grass_region = {
                    (float)r->x,(float)r->z,(float)r->width,0}};
                state.grass_bindings.vertex_buffer_offsets[1]=r->root_start*(int)sizeof(float);
                sg_apply_bindings(&state.grass_bindings);
                sg_apply_uniforms(UB_grass_region_params,SG_RANGE(region));
                int count=(r->width*r->depth+state.grass_stride-1)/state.grass_stride;
                sg_draw(0,3,count);
                state.submitted_grass += count;
            }
        }
        sg_end_pass();
        state.downsample_bindings.views[VIEW_source_image]=state.scene_texture;
        if (state.grass_volume && !state.no_grass) {
            const volume_camera_params_t camera={
                .volume_view={uniforms.view[0],uniforms.view[1],uniforms.view[2],0},
                .volume_lens={uniforms.lens[0],uniforms.lens[1],uniforms.lens[2],0},
                .volume_camera_position={state.position[0],state.position[1],state.position[2],0},
                .volume_grass_lod={1,state.lod_start,state.lod_end,1.0f/state.grass_stride}};
            sg_pass volume_pass{};
            volume_pass.attachments = state.volume_attachments;
            sg_begin_pass(volume_pass);
            sg_apply_pipeline(state.volume_pipeline);
            sg_apply_bindings(&state.volume_bindings);
            sg_apply_uniforms(UB_volume_camera_params,SG_RANGE(camera));
            sg_apply_uniforms(UB_volume_params,SG_RANGE(state.volume_params));
            sg_apply_uniforms(UB_light_params,SG_RANGE(light));
            sg_draw(0,3,1);
            sg_end_pass();
            state.downsample_bindings.views[VIEW_source_image]=state.volume_texture;
        }
        const downsample_params_t filter = {.output_size = {
            (float)state.camera.profile.width,(float)state.camera.profile.height,0,0},
        };
        sg_pass downsample_pass{};
        downsample_pass.attachments = state.downsample_attachments;
        sg_begin_pass(downsample_pass);
        sg_apply_pipeline(state.downsample_pipeline);
        sg_apply_bindings(&state.downsample_bindings);
        sg_apply_uniforms(UB_downsample_params,SG_RANGE(filter));
        sg_apply_uniforms(UB_light_params,SG_RANGE(light));
        sg_draw(0,3,1);
        sg_end_pass();
        ++state.captures;
    }
    // Letterbox the fixed sensor image. Window size never changes its intrinsics.
    int window_width = sapp_width(), window_height = sapp_height();
    float scale = fminf((float)window_width/state.camera.profile.width, (float)window_height/state.camera.profile.height);
    int width = (int)(state.camera.profile.width*scale), height = (int)(state.camera.profile.height*scale);
    if (width < 1) width = 1;
    if (height < 1) height = 1;
    sg_pass preview_pass{};
    preview_pass.action.colors[0].load_action = SG_LOADACTION_CLEAR;
    preview_pass.action.colors[0].clear_value = {0,0,0,1};
    preview_pass.swapchain = sglue_swapchain();
    sg_begin_pass(preview_pass);
    sg_apply_viewport((window_width-width)/2, (window_height-height)/2, width, height, true);
    sg_apply_pipeline(state.preview_pipeline);
    sg_apply_bindings(&state.preview_bindings);
    const preview_params_t preview = {.preview_settings = {state.zoom ? 8.0f : 1.0f,0,0,0}};
    sg_apply_uniforms(UB_preview_params, SG_RANGE(preview));
    sg_draw(0, 3, 1);
    sg_end_pass();
    sg_commit();
    if (state.captures == 120 && (state.smoke_test || state.terrain_smoke_test)) {
        printf("Yard: rendered 120 %dx%d camera frames on Metal, %.1f fps cap, %.1f captures/s observed (%s).\n",
               state.camera.profile.width, state.camera.profile.height, state.camera.profile.fps, 119.0/state.smoke_elapsed,
               state.terrain_smoke_test ? "terrain orbit" : "four lunar phases");
        printf("Yard: average submitted %.2f million grass blades (%.1f%%), %.2f million terrain triangles (%.1f%%)\n",
               state.submitted_grass/(120.0*1e6),
               state.grass_count ? 100.0*state.submitted_grass/(120.0*state.grass_count) : 0,
               state.submitted_triangles/(120.0*1e6),
               100.0*state.submitted_triangles/(120.0*(state.terrain_index_count/3)));
        sapp_request_quit();
    }
}

static void event(const sapp_event *ev) {
    if (ev->type == SAPP_EVENTTYPE_UNFOCUSED) {
        memset(state.keys, 0, sizeof(state.keys));
        sapp_lock_mouse(false);
    }
    if (ev->type == SAPP_EVENTTYPE_MOUSE_DOWN && ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
        sapp_lock_mouse(true);
    }
    if (ev->type == SAPP_EVENTTYPE_MOUSE_MOVE && sapp_mouse_locked()) {
        // Locked deltas are raw movement, not Retina framebuffer pixels.
        float sensitivity = state.zoom ? 0.0003125f : 0.0025f;
        state.yaw = remainderf(state.yaw + ev->mouse_dx*sensitivity, 6.283185307f);
        state.pitch = fmaxf(-1.55f, fminf(1.55f, state.pitch - ev->mouse_dy*sensitivity));
        if (ev->mouse_dx != 0 || ev->mouse_dy != 0) state.track_moon = false;
    }
    if ((ev->type == SAPP_EVENTTYPE_KEY_DOWN || ev->type == SAPP_EVENTTYPE_KEY_UP) &&
        ev->key_code > 0 && static_cast<int>(ev->key_code) < SAPP_MAX_KEYCODES) {
        state.keys[ev->key_code] = ev->type == SAPP_EVENTTYPE_KEY_DOWN;
    }
    if (ev->type != SAPP_EVENTTYPE_KEY_DOWN || ev->key_repeat) return;
    if (ev->key_code == SAPP_KEYCODE_COMMA || ev->key_code == SAPP_KEYCODE_PERIOD) {
        yard_camera_step_exposure(&state.camera, ev->key_code == SAPP_KEYCODE_PERIOD ? 1 : -1);
        state.title_minute = INT64_MIN;
    }
    if (ev->key_code == SAPP_KEYCODE_MINUS || ev->key_code == SAPP_KEYCODE_EQUAL) {
        yard_camera_step_gain(&state.camera, ev->key_code == SAPP_KEYCODE_EQUAL ? 1 : -1);
        state.title_minute = INT64_MIN;
    }
    if (ev->key_code == SAPP_KEYCODE_V) {
        state.grass_volume=!state.grass_volume;
        state.title_minute=INT64_MIN;
        printf("Yard: grass volume %s\n",state.grass_volume ? "on" : "off");
    }
    if (ev->key_code == SAPP_KEYCODE_C) {
        state.no_culling = !state.no_culling;
        state.title_minute = INT64_MIN;
        printf("Yard: frustum culling %s\n",state.no_culling ? "off" : "on");
    }
    if (ev->key_code == SAPP_KEYCODE_M) state.track_moon = !state.track_moon;
    if (ev->key_code == SAPP_KEYCODE_Z) state.zoom = !state.zoom;
    if (ev->key_code == SAPP_KEYCODE_W || ev->key_code == SAPP_KEYCODE_A ||
        ev->key_code == SAPP_KEYCODE_S || ev->key_code == SAPP_KEYCODE_D) state.track_moon = false;
    if (ev->key_code == SAPP_KEYCODE_LEFT_BRACKET || ev->key_code == SAPP_KEYCODE_RIGHT_BRACKET) {
        state.utc = yard_local_hour(state.utc, -1, ev->key_code == SAPP_KEYCODE_RIGHT_BRACKET ? 1 : -1);
        state.paused = true;
    }
    if (ev->key_code == SAPP_KEYCODE_SPACE) state.paused = !state.paused;
    if (ev->key_code == SAPP_KEYCODE_ESCAPE) {
        if (sapp_mouse_locked()) {
            sapp_lock_mouse(false);
            memset(state.keys, 0, sizeof(state.keys));
        } else sapp_request_quit();
    }
    if (ev->key_code >= SAPP_KEYCODE_1 && ev->key_code <= SAPP_KEYCODE_4) {
        const double hours[] = {7, 12, 19, 0};
        state.utc = yard_local_hour(state.utc, hours[ev->key_code - SAPP_KEYCODE_1], 0);
        state.paused = true;
    }
    if (ev->key_code == SAPP_KEYCODE_R) {
        state.position[0] = 0;
        state.position[2] = 8;
        state.yaw = 0;
        state.pitch = -0.10f;
        state.utc = (double)time(nullptr);
        state.track_moon = false;
        state.zoom = false;
        state.paused = false;
    }
}

static void cleanup(void) { sapp_lock_mouse(false); sg_shutdown(); yard_draw_layout_destroy(&state.layout); yard_terrain_destroy(&state.terrain); }

sapp_desc sokol_main(int argc, char *argv[]) {
    sapp_desc app{};
    if (setenv("TZ", "America/New_York", 1) != 0) {
        perror("TZ"); exit(EXIT_FAILURE);
    }
    tzset();
    state.site = yard_default_site;
    yard_camera_init(&state.camera, &yard_ov2640_svga);
    state.ssaa = 8;
    state.msaa = 1;
    state.grass_stride = 1;
    state.lod_start=3;
    state.lod_end=6;
    state.eye_height = 1.6f;
    state.vertical_fov = 60.0f; // XIAO Sense OV2640 stock lens FOV is not yet calibrated.
    state.utc = (double)time(nullptr);
    state.position[2] = 8;
    state.pitch = -0.10f;
    state.title_minute = INT64_MIN;
    const char *requested_date = nullptr;
    double requested_hour = -1;
    const char *profile_path = nullptr;
    double exposure_ms = -1, gain = -1, exposure_lines = -1, gain_index = -1;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--smoke-test") == 0) state.smoke_test = true;
        else if (strcmp(argv[i], "--terrain-smoke-test") == 0) state.terrain_smoke_test = true;
        else if (strcmp(argv[i], "--grass-volume") == 0) state.grass_volume=true;
        else if ((strcmp(argv[i],"--lod-start")==0 || strcmp(argv[i],"--lod-end")==0) && i+1<argc) {
            bool start=strcmp(argv[i],"--lod-start")==0;
            char *end; const char *value=argv[++i];
            float distance=strtof(value,&end);
            if (end==value || *end || !isfinite(distance) || distance<0 || distance>100) goto usage;
            if (start) state.lod_start=distance; else state.lod_end=distance;
        }
        else if (strcmp(argv[i], "--no-culling") == 0) state.no_culling = true;
        else if (strcmp(argv[i], "--no-grass") == 0) state.no_grass = true;
        else if ((strcmp(argv[i], "--msaa") == 0 || strcmp(argv[i], "--ssaa") == 0 || strcmp(argv[i], "--grass-stride") == 0) && i+1 < argc) {
            bool msaa = strcmp(argv[i], "--msaa") == 0;
            bool ssaa = strcmp(argv[i], "--ssaa") == 0;
            const char *argument = argv[++i];
            char *end;
            long value = strtol(argument, &end, 10);
            if (end == argument || *end != '\0' || value < 1 || value > 64) goto usage;
            if (msaa) {
                if (value != 1 && value != 4) goto usage;
                state.msaa = (int)value;
            } else if (ssaa) {
                if (value != 1 && value != 8) goto usage;
                state.ssaa = (int)value;
            } else state.grass_stride = (int)value;
        }
        else if (strcmp(argv[i], "--geometry-demo") == 0) state.geometry_demo=true;
        else if (strcmp(argv[i], "--tree") == 0 && i+1<argc) state.tree_species=argv[++i];
        else if (strcmp(argv[i], "--moon") == 0) state.track_moon = true;
        else if (strcmp(argv[i], "--zoom") == 0) state.zoom = true;
        else if (strcmp(argv[i], "--camera-profile") == 0 && i+1 < argc) profile_path = argv[++i];
        else if ((strcmp(argv[i], "--exposure-ms") == 0 || strcmp(argv[i], "--gain") == 0 ||
                  strcmp(argv[i], "--aec-value") == 0 || strcmp(argv[i], "--agc-gain") == 0) && i+1 < argc) {
            const char *option = argv[i], *argument = argv[++i];
            char *end;
            double value = strtod(argument, &end);
            if (!isfinite(value) || value < 0 || end == argument || *end != '\0') goto usage;
            if (strcmp(option, "--exposure-ms") == 0) exposure_ms = value;
            else if (strcmp(option, "--gain") == 0) gain = value;
            else if (strcmp(option, "--aec-value") == 0) exposure_lines = value;
            else gain_index = value;
        }
        else if (strcmp(argv[i], "--site") == 0 && i+1 < argc) {
            const char *path = argv[++i];
            if (!yard_site_load(path, &state.site)) {
                fprintf(stderr, "Cannot load skyglow site: %s\n", path);
                goto usage;
            }
        }
        else if (strcmp(argv[i], "--eye-height") == 0 && i+1 < argc) {
            const char *argument = argv[++i];
            char *end;
            state.eye_height = strtof(argument, &end);
            if (!isfinite(state.eye_height) || end == argument || *end != '\0' ||
                state.eye_height < 0.1f || state.eye_height > 10.0f) goto usage;
        }
        else if (strcmp(argv[i], "--vfov") == 0 && i+1 < argc) {
            const char *argument = argv[++i];
            char *end = nullptr;
            state.vertical_fov = strtof(argument, &end);
            if (!isfinite(state.vertical_fov) || end == argument || *end != '\0' ||
                state.vertical_fov < 1 || state.vertical_fov > 170) goto usage;
        }
        else if (strcmp(argv[i], "--date") == 0 && i+1 < argc) requested_date = argv[++i];
        else if (strcmp(argv[i], "--time") == 0 && i+1 < argc) {
            char *end = nullptr;
            const char *argument = argv[++i];
            requested_hour = strtod(argument, &end);
            if (!isfinite(requested_hour) || end == argument || *end != '\0' ||
                requested_hour < 0 || requested_hour >= 24) goto usage;
        } else goto usage;
    }
    if (state.smoke_test && state.terrain_smoke_test) goto usage;
    if (profile_path) {
        yard_camera_profile profile;
        if (!yard_camera_profile_load(profile_path, &profile)) {
            fprintf(stderr, "Cannot load camera profile: %s\n", profile_path);
            goto usage;
        }
        yard_camera_init(&state.camera, &profile);
    }
    if ((exposure_ms >= 0 && exposure_lines >= 0) || (gain >= 0 && gain_index >= 0)) goto usage;
    if (exposure_ms >= 0 && !yard_camera_set_exposure_ms(&state.camera, exposure_ms)) goto usage;
    if (gain >= 0 && !yard_camera_set_gain(&state.camera, gain)) goto usage;
    if (exposure_lines >= 0 && (exposure_lines > state.camera.profile.max_exposure_lines ||
        floor(exposure_lines) != exposure_lines || !yard_camera_set_lines(&state.camera, (int)exposure_lines))) goto usage;
    if (gain_index >= 0 && (gain_index >= state.camera.profile.gain_count || floor(gain_index) != gain_index ||
        !yard_camera_set_gain_index(&state.camera, (int)gain_index))) goto usage;
    if (requested_date || requested_hour >= 0) {
        struct tm local;
        char today[16];
        yard_local_calendar(state.utc, &local);
        strftime(today, sizeof(today), "%Y-%m-%d", &local);
        double hour = requested_hour >= 0 ? requested_hour : local.tm_hour+local.tm_min/60.0;
        if (!yard_local_datetime(requested_date ? requested_date : today, hour, &state.utc)) goto usage;
        state.paused = true;
    }
    printf("Yard: camera %s, %dx%d %.1f fps, manual %.4f ms %.2fx (AEC %d, gain %d), render multiplier %.3f\n",
           state.camera.profile.name, state.camera.profile.width, state.camera.profile.height, state.camera.profile.fps,
           yard_camera_exposure_ms(&state.camera), yard_camera_gain(&state.camera), state.camera.exposure_lines,
           state.camera.gain_index, yard_camera_multiplier(&state.camera));
    printf("Yard: skyglow atlas %d, lat %.7f lon %.7f, artificial/natural %.4f\n",
           state.site.year, state.site.latitude, state.site.longitude, state.site.artificial_ratio);
    if (state.lod_end<=state.lod_start) goto usage;
    app.init_cb = init;
    app.frame_cb = frame;
    app.cleanup_cb = cleanup;
    app.event_cb = event;
    app.width = state.camera.profile.width;
    app.height = state.camera.profile.height;
    app.sample_count = 1;
    app.high_dpi = true;
    app.window_title = "Yard | Click: mouse look | WASD: move | Esc: release/quit";
    app.logger.func = slog_func;
    return app;
usage:
    fprintf(stderr, "Usage: %s [--date YYYY-MM-DD] [--time local-hour] [--moon] [--zoom] [--vfov degrees] [--eye-height metres] [--smoke-test | --terrain-smoke-test] [--site profile] [--tree species] [--geometry-demo]\n"
                    "Grass LOD: [--grass-volume] [--lod-start metres] [--lod-end metres] (V toggles)\n"
                    "Rendering: [--ssaa 1|8] [--msaa 1|4] [--no-culling] [--no-grass] [--grass-stride 1..64] (diagnostic density reduction)\n"
                    "Camera: [--camera-profile FILE] [--exposure-ms MS | --aec-value LINES] [--gain MULTIPLIER | --agc-gain INDEX]\n"
                    "OV2640 default: manual shutter 0-33.333333 ms (AEC 0-1200, frame-capped), gain 1-31x (index 0-30).\n"
                    "Keys: comma/period = shutter -/+ 1/3 stop; minus/equal = gain -/+ one step.\n"
                    "Dates: 1900-2100; hours: [0,24), America/New_York. Invalid dates and DST gaps are rejected.\n", argv[0]);
    exit(EXIT_FAILURE);
}
