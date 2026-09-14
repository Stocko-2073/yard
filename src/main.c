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

enum { CAMERA_WIDTH = 800, CAMERA_HEIGHT = 600 };
static const double CAMERA_INTERVAL = 1.0 / 30.0;

static struct {
    sg_pipeline preview_pipeline;
    sg_bindings preview_bindings;
    sg_attachments camera_attachments;
    float vertical_fov;
    double capture_elapsed;
    unsigned captures;
    sg_pipeline pipeline;
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
    float angle;
    bool paused;
    bool smoke_test;
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

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });

    static const float sky_vertices[][2] = {{-1,-1}, {3,-1}, {-1,3}};
    state.sky_bindings.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(sky_vertices), .label = "sky triangle",
    });
    sg_image camera_color = sg_make_image(&(sg_image_desc){
        .usage.color_attachment = true,
        .width = CAMERA_WIDTH, .height = CAMERA_HEIGHT,
        .pixel_format = SG_PIXELFORMAT_RGBA8, .sample_count = 1,
        .label = "SVGA camera color",
    });
    sg_image camera_depth = sg_make_image(&(sg_image_desc){
        .usage.depth_stencil_attachment = true,
        .width = CAMERA_WIDTH, .height = CAMERA_HEIGHT,
        .pixel_format = SG_PIXELFORMAT_DEPTH, .sample_count = 1,
        .label = "SVGA camera depth",
    });
    state.camera_attachments = (sg_attachments){
        .colors[0] = sg_make_view(&(sg_view_desc){.color_attachment.image = camera_color}),
        .depth_stencil = sg_make_view(&(sg_view_desc){.depth_stencil_attachment.image = camera_depth}),
    };
    state.preview_bindings = (sg_bindings){
        .vertex_buffers[0] = state.sky_bindings.vertex_buffers[0],
        .views[VIEW_camera_image] = sg_make_view(&(sg_view_desc){.texture.image = camera_color}),
        .samplers[SMP_camera_sampler] = sg_make_sampler(&(sg_sampler_desc){
            .min_filter = SG_FILTER_NEAREST, .mag_filter = SG_FILTER_NEAREST,
            .wrap_u = SG_WRAP_CLAMP_TO_EDGE, .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        }),
    };
    state.preview_pipeline = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(preview_shader_desc(sg_query_backend())),
        .layout.attrs[ATTR_preview_position].format = SG_VERTEXFORMAT_FLOAT2,
        .depth = {.compare = SG_COMPAREFUNC_ALWAYS},
        .label = "camera preview",
    });
    state.sky_pipeline = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(sky_shader_desc(sg_query_backend())),
        .layout.attrs[ATTR_sky_position].format = SG_VERTEXFORMAT_FLOAT2,
        .depth = {.pixel_format = SG_PIXELFORMAT_DEPTH, .write_enabled = false, .compare = SG_COMPAREFUNC_ALWAYS},
        .colors[0].pixel_format = SG_PIXELFORMAT_RGBA8,
        .sample_count = 1,
        .label = "atmosphere and ground",
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
    sg_shader shader = sg_make_shader(cube_shader_desc(sg_query_backend()));
    state.pipeline = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shader,
        .layout.attrs = {
            [ATTR_cube_position].format = SG_VERTEXFORMAT_FLOAT3,
            [ATTR_cube_color].format = SG_VERTEXFORMAT_FLOAT3,
        },
        .index_type = SG_INDEXTYPE_UINT16,
        .cull_mode = SG_CULLMODE_BACK,
        .face_winding = SG_FACEWINDING_CCW,
        .depth = {.pixel_format = SG_PIXELFORMAT_DEPTH, .write_enabled = true, .compare = SG_COMPAREFUNC_LESS_EQUAL},
        .colors[0].pixel_format = SG_PIXELFORMAT_RGBA8,
        .sample_count = 1,
        .label = "cube pipeline",
    });
}

static void frame(void) {
    float dt = (float)fmin(sapp_frame_duration(), 0.1);
    if (!state.paused) state.utc += dt * 360.0; // Four minutes per day.
    state.utc += dt * 7200.0 * ((int)state.keys[SAPP_KEYCODE_RIGHT] - (int)state.keys[SAPP_KEYCODE_LEFT]);
    // Keep the calendar within the documented range of this visual ephemeris.
    state.utc = fmax(-2208970800.0, fmin(4133998799.0, state.utc));
    // Ground-plane navigation at fixed eye height, independent of solar time.
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
    yard_astronomy(state.utc, state.site.latitude, state.site.longitude, &state.ephemeris);
    if (state.track_moon) {
        state.yaw = (float)atan2(state.ephemeris.moon[0], -state.ephemeris.moon[2]);
        state.pitch = (float)asin(state.ephemeris.moon[1]);
    }
    state.capture_elapsed += dt;
    int64_t minute = (int64_t)floor(state.utc / 60);
    if (minute != state.title_minute) {
        char title[256], date[64];
        struct tm local;
        yard_local_calendar(state.utc, &local);
        strftime(date, sizeof(date), "%Y-%m-%d %H:%M %Z", &local);
        snprintf(title, sizeof(title), "Yard | 800x600 | Lat %.5f, Lon %.5f | %s | Moon %.0f%% %s%s",
                 state.site.latitude, state.site.longitude, date, state.ephemeris.illuminated*100, state.ephemeris.waxing ? "waxing" : "waning",
                 state.ephemeris.moon[1] < 0 ? " (below horizon)" : "");
        sapp_set_window_title(title);
        state.title_minute = minute;
    }
    if (sapp_width() <= 0 || sapp_height() <= 0) return;
    if (state.captures == 0 || state.capture_elapsed >= CAMERA_INTERVAL) {
        state.capture_elapsed = fmod(state.capture_elapsed, CAMERA_INTERVAL);
        const vs_params_t uniforms = {
            .view = {state.yaw, state.pitch, (float)CAMERA_WIDTH / CAMERA_HEIGHT, state.angle},
            .lens = {1.0f / tanf(state.vertical_fov * 0.00872664626f), 0, 0, 0},
            .camera_position = {state.position[0], state.position[1], state.position[2], 0},
        };
        light_params_t light = {0};
        sunlight(state.ephemeris.sun, light.sun_color, 3.0);
        yard_night_light(&state.site, state.ephemeris.sun[1], light.night_radiance);
        for (int i=0; i<3; ++i) light.sun_direction[i] = (float)state.ephemeris.sun[i];
        sky_params_t sky = {0};
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
        sg_begin_pass(&(sg_pass){
            .action.colors[0] = {
                .load_action = SG_LOADACTION_CLEAR,
                .clear_value = {.r = .055f, .g = .075f, .b = .09f, .a = 1},
            },
            .attachments = state.camera_attachments,
        });
        sg_apply_pipeline(state.sky_pipeline);
        sg_apply_bindings(&state.sky_bindings);
        sg_apply_uniforms(UB_sky_params, &SG_RANGE(sky));
        sg_draw(0, 3, 1);
        sg_apply_pipeline(state.pipeline);
        sg_apply_bindings(&state.bindings);
        sg_apply_uniforms(UB_vs_params, &SG_RANGE(uniforms));
        sg_apply_uniforms(UB_light_params, &SG_RANGE(light));
        sg_draw(0, 36, 1);
        sg_end_pass();
        ++state.captures;
    }
    // Letterbox the fixed sensor image. Window size never changes its intrinsics.
    int window_width = sapp_width(), window_height = sapp_height();
    float scale = fminf((float)window_width/CAMERA_WIDTH, (float)window_height/CAMERA_HEIGHT);
    int width = (int)(CAMERA_WIDTH*scale), height = (int)(CAMERA_HEIGHT*scale);
    if (width < 1) width = 1;
    if (height < 1) height = 1;
    sg_begin_pass(&(sg_pass){
        .action.colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0,0,0,1}},
        .swapchain = sglue_swapchain(),
    });
    sg_apply_viewport((window_width-width)/2, (window_height-height)/2, width, height, true);
    sg_apply_pipeline(state.preview_pipeline);
    sg_apply_bindings(&state.preview_bindings);
    const preview_params_t preview = {.preview_settings = {state.zoom ? 8.0f : 1.0f,0,0,0}};
    sg_apply_uniforms(UB_preview_params, &SG_RANGE(preview));
    sg_draw(0, 3, 1);
    sg_end_pass();
    sg_commit();
    if (state.captures == 120 && state.smoke_test) {
        puts("Yard: rendered 120 SVGA 800x600 camera frames on Metal at a maximum of 30 fps (four lunar phases).");
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
        ev->key_code > 0 && ev->key_code < SAPP_MAX_KEYCODES) {
        state.keys[ev->key_code] = ev->type == SAPP_EVENTTYPE_KEY_DOWN;
    }
    if (ev->type != SAPP_EVENTTYPE_KEY_DOWN || ev->key_repeat) return;
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
        state.position[1] = 2.5f;
        state.position[2] = 8;
        state.yaw = 0;
        state.pitch = -0.10f;
        state.utc = (double)time(NULL);
        state.track_moon = false;
        state.zoom = false;
        state.paused = false;
    }
}

static void cleanup(void) { sapp_lock_mouse(false); sg_shutdown(); }

sapp_desc sokol_main(int argc, char *argv[]) {
    if (setenv("TZ", "America/New_York", 1) != 0) {
        perror("TZ"); exit(EXIT_FAILURE);
    }
    tzset();
    state.site = yard_default_site;
    state.vertical_fov = 60.0f; // XIAO Sense OV2640 stock lens FOV is not yet calibrated.
    state.utc = (double)time(NULL);
    state.position[1] = 2.5f;
    state.position[2] = 8;
    state.pitch = -0.10f;
    state.angle = 0.4f;
    state.title_minute = INT64_MIN;
    const char *requested_date = NULL;
    double requested_hour = -1;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--smoke-test") == 0) state.smoke_test = true;
        else if (strcmp(argv[i], "--moon") == 0) state.track_moon = true;
        else if (strcmp(argv[i], "--zoom") == 0) state.zoom = true;
        else if (strcmp(argv[i], "--site") == 0 && i+1 < argc) {
            const char *path = argv[++i];
            if (!yard_site_load(path, &state.site)) {
                fprintf(stderr, "Cannot load skyglow site: %s\n", path);
                goto usage;
            }
        }
        else if (strcmp(argv[i], "--vfov") == 0 && i+1 < argc) {
            const char *argument = argv[++i];
            char *end = NULL;
            state.vertical_fov = strtof(argument, &end);
            if (!isfinite(state.vertical_fov) || end == argument || *end != '\0' ||
                state.vertical_fov < 1 || state.vertical_fov > 170) goto usage;
        }
        else if (strcmp(argv[i], "--date") == 0 && i+1 < argc) requested_date = argv[++i];
        else if (strcmp(argv[i], "--time") == 0 && i+1 < argc) {
            char *end = NULL;
            const char *argument = argv[++i];
            requested_hour = strtod(argument, &end);
            if (!isfinite(requested_hour) || end == argument || *end != '\0' ||
                requested_hour < 0 || requested_hour >= 24) goto usage;
        } else goto usage;
    }
    if (requested_date || requested_hour >= 0) {
        struct tm local;
        char today[16];
        yard_local_calendar(state.utc, &local);
        strftime(today, sizeof(today), "%Y-%m-%d", &local);
        double hour = requested_hour >= 0 ? requested_hour : local.tm_hour+local.tm_min/60.0;
        if (!yard_local_datetime(requested_date ? requested_date : today, hour, &state.utc)) goto usage;
        state.paused = true;
    }
    printf("Yard: skyglow atlas %d, lat %.7f lon %.7f, artificial/natural %.4f\n",
           state.site.year, state.site.latitude, state.site.longitude, state.site.artificial_ratio);
    return (sapp_desc){
        .init_cb = init, .frame_cb = frame, .cleanup_cb = cleanup, .event_cb = event,
        .width = CAMERA_WIDTH, .height = CAMERA_HEIGHT, .sample_count = 1, .high_dpi = true,
        .window_title = "Yard | Click: mouse look | WASD: move | Esc: release/quit",
        .logger.func = slog_func,
    };
usage:
    fprintf(stderr, "Usage: %s [--date YYYY-MM-DD] [--time local-hour] [--moon] [--zoom] [--vfov degrees] [--smoke-test] [--site profile]\n"
                    "Dates: 1900-2100; hours: [0,24), America/New_York. Invalid dates and DST gaps are rejected.\n", argv[0]);
    exit(EXIT_FAILURE);
}
