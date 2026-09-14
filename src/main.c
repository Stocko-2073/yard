#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"
#include "cube_shader.h"

static struct {
    sg_pipeline pipeline;
    sg_bindings bindings;
    float angle;
    bool paused;
    bool smoke_test;
    unsigned frames;
} state;

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });

    // Separate vertices per face let each face have a solid color.
    static const float vertices[][6] = {
        {-1,-1, 1, .35f,.75f,.45f}, { 1,-1, 1, .35f,.75f,.45f},
        { 1, 1, 1, .35f,.75f,.45f}, {-1, 1, 1, .35f,.75f,.45f},
        { 1,-1,-1, .25f,.45f,.65f}, {-1,-1,-1, .25f,.45f,.65f},
        {-1, 1,-1, .25f,.45f,.65f}, { 1, 1,-1, .25f,.45f,.65f},
        { 1,-1, 1, .85f,.55f,.25f}, { 1,-1,-1, .85f,.55f,.25f},
        { 1, 1,-1, .85f,.55f,.25f}, { 1, 1, 1, .85f,.55f,.25f},
        {-1,-1,-1, .65f,.35f,.45f}, {-1,-1, 1, .65f,.35f,.45f},
        {-1, 1, 1, .65f,.35f,.45f}, {-1, 1,-1, .65f,.35f,.45f},
        {-1, 1, 1, .75f,.85f,.55f}, { 1, 1, 1, .75f,.85f,.55f},
        { 1, 1,-1, .75f,.85f,.55f}, {-1, 1,-1, .75f,.85f,.55f},
        {-1,-1,-1, .40f,.30f,.25f}, { 1,-1,-1, .40f,.30f,.25f},
        { 1,-1, 1, .40f,.30f,.25f}, {-1,-1, 1, .40f,.30f,.25f},
    };
    static const uint16_t indices[] = {
        0,1,2, 0,2,3, 4,5,6, 4,6,7, 8,9,10, 8,10,11,
        12,13,14, 12,14,15, 16,17,18, 16,18,19, 20,21,22, 20,22,23,
    };
    state.bindings.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices), .label = "cube vertices",
    });
    state.bindings.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .usage.index_buffer = true,
        .data = SG_RANGE(indices), .label = "cube indices",
    });
    sg_shader shader = sg_make_shader(&(sg_shader_desc){
        .vertex_func = {.source = cube_shader_source, .entry = "cube_vs"},
        .fragment_func = {.source = cube_shader_source, .entry = "cube_fs"},
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_VERTEX, .size = 16, .msl_buffer_n = 0,
        },
        .label = "cube shader",
    });
    state.pipeline = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shader,
        .layout.attrs = {
            [0].format = SG_VERTEXFORMAT_FLOAT3,
            [1].format = SG_VERTEXFORMAT_FLOAT3,
        },
        .index_type = SG_INDEXTYPE_UINT16,
        .cull_mode = SG_CULLMODE_BACK,
        .face_winding = SG_FACEWINDING_CCW,
        .depth = {.write_enabled = true, .compare = SG_COMPAREFUNC_LESS_EQUAL},
        .label = "cube pipeline",
    });
}

static void frame(void) {
    if (!state.paused) {
        double dt = sapp_frame_duration();
        if (dt > 0.1) dt = 0.1; // Avoid jumps after a long pause or window drag.
        state.angle += (float)dt * 0.7f;
    }
    if (sapp_width() <= 0 || sapp_height() <= 0) return;
    const float uniforms[4] = {
        state.angle, sapp_widthf() / sapp_heightf(), 0, 0,
    };
    sg_begin_pass(&(sg_pass){
        .action.colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = {.r = .055f, .g = .075f, .b = .09f, .a = 1},
        },
        .swapchain = sglue_swapchain(),
    });
    sg_apply_pipeline(state.pipeline);
    sg_apply_bindings(&state.bindings);
    sg_apply_uniforms(0, &SG_RANGE(uniforms));
    sg_draw(0, 36, 1);
    sg_end_pass();
    sg_commit();
    if (++state.frames == 120 && state.smoke_test) {
        puts("Yard: rendered 120 frames on Metal.");
        sapp_request_quit();
    }
}

static void event(const sapp_event *ev) {
    if (ev->type != SAPP_EVENTTYPE_KEY_DOWN || ev->key_repeat) return;
    if (ev->key_code == SAPP_KEYCODE_SPACE) state.paused = !state.paused;
    if (ev->key_code == SAPP_KEYCODE_ESCAPE) sapp_request_quit();
}

static void cleanup(void) { sg_shutdown(); }

sapp_desc sokol_main(int argc, char *argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--smoke-test") == 0) state.smoke_test = true;
    }
    return (sapp_desc){
        .init_cb = init, .frame_cb = frame, .cleanup_cb = cleanup, .event_cb = event,
        .width = 960, .height = 640, .sample_count = 4, .high_dpi = true,
        .window_title = "Yard | Space: pause | Esc: quit",
        .logger.func = slog_func,
    };
}
