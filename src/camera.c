#include "camera.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

// OV2640 SVGA: nominal 672 line periods/frame. 30 fps is a simulator assumption.
// Gains decode the Espressif OV2640 table via the datasheet GAIN formula.
// Sources, timing assumptions, and render calibration: design/CAMERA.md.
const yard_camera_profile yard_ov2640_svga = {
    .name = "ov2640-svga", .width = 800, .height = 600,
    .fps = 30, .line_us = 1000000.0 / (30.0 * 672.0),
    .max_exposure_lines = 1200, .default_exposure_lines = 202,
    .reference_exposure_ms = 10, .gain_count = 31,
    .gains = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,
              17,18,19,20,21,22,23,24,25,26,27,28,29,30,31},
};

static bool read_integer(FILE *file, int *out) {
    char token[64], *end;
    if (fscanf(file, "%63s", token) != 1) return false;
    errno = 0;
    long value = strtol(token, &end, 10);
    if (errno || end == token || *end || value < INT_MIN || value > INT_MAX) return false;
    *out = (int)value;
    return true;
}

bool yard_camera_profile_load(const char *path, yard_camera_profile *out) {
    FILE *f = fopen(path, "r");
    if (!f) return false;
    yard_camera_profile p = {0};
    char tag[32], extra;
    bool valid = fscanf(f, "%31s %31s", tag, p.name) == 2 &&
        read_integer(f, &p.width) && read_integer(f, &p.height) &&
        fscanf(f, "%lf %lf", &p.fps, &p.line_us) == 2 &&
        read_integer(f, &p.max_exposure_lines) && read_integer(f, &p.default_exposure_lines) &&
        fscanf(f, "%lf", &p.reference_exposure_ms) == 1 && read_integer(f, &p.gain_count) &&
        strcmp(tag, "YARD_CAMERA_V1") == 0 &&
        p.width >= 1 && p.width <= 4096 && p.height >= 1 && p.height <= 4096 &&
        isfinite(p.fps) && p.fps >= 1 && p.fps <= 120 &&
        isfinite(p.line_us) && p.line_us >= 0.01 && p.line_us <= 1000000.0/p.fps &&
        p.max_exposure_lines >= 1 && p.max_exposure_lines <= 65535 &&
        p.default_exposure_lines >= 1 && p.default_exposure_lines <= p.max_exposure_lines &&
        isfinite(p.reference_exposure_ms) && p.reference_exposure_ms >= 0.001 &&
        p.reference_exposure_ms <= 1000 && p.gain_count >= 1 && p.gain_count <= YARD_CAMERA_MAX_GAINS;
    for (int i = 0; valid && i < p.gain_count; ++i) {
        valid = fscanf(f, "%lf", &p.gains[i]) == 1 && isfinite(p.gains[i]) &&
            p.gains[i] >= 1 && p.gains[i] <= 1024 && (i == 0 || p.gains[i] > p.gains[i-1]);
    }
    valid = valid && fscanf(f, " %c", &extra) == EOF && !ferror(f);
    fclose(f);
    if (valid) *out = p;
    return valid;
}

void yard_camera_init(yard_camera *c, const yard_camera_profile *p) {
    c->profile = *p;
    c->exposure_lines = p->default_exposure_lines;
    c->gain_index = 0;
}

bool yard_camera_set_lines(yard_camera *c, int lines) {
    if (lines < 0 || lines > c->profile.max_exposure_lines) return false;
    c->exposure_lines = lines;
    return true;
}

bool yard_camera_set_exposure_ms(yard_camera *c, double ms) {
    double max_ms = fmin(1000.0/c->profile.fps,
                        c->profile.max_exposure_lines*c->profile.line_us/1000.0);
    if (!isfinite(ms) || ms < 0 || ms > max_ms + 1e-8) return false;
    int lines = (int)lround(ms*1000.0/c->profile.line_us);
    if (lines > c->profile.max_exposure_lines) lines = c->profile.max_exposure_lines;
    return yard_camera_set_lines(c, lines);
}

bool yard_camera_set_gain_index(yard_camera *c, int index) {
    if (index < 0 || index >= c->profile.gain_count) return false;
    c->gain_index = index;
    return true;
}

bool yard_camera_set_gain(yard_camera *c, double gain) {
    if (!isfinite(gain) || gain < c->profile.gains[0] || gain > c->profile.gains[c->profile.gain_count-1]) return false;
    int best = 0;
    for (int i = 1; i < c->profile.gain_count; ++i) {
        if (fabs(c->profile.gains[i]-gain) < fabs(c->profile.gains[best]-gain)) best = i;
    }
    c->gain_index = best;
    return true;
}

double yard_camera_exposure_ms(const yard_camera *c) {
    // Sensor integration never exceeds a frame, even if the register does.
    return fmin(c->exposure_lines*c->profile.line_us/1000.0, 1000.0/c->profile.fps);
}

double yard_camera_gain(const yard_camera *c) { return c->profile.gains[c->gain_index]; }

float yard_camera_multiplier(const yard_camera *c) {
    return (float)(yard_camera_exposure_ms(c)/c->profile.reference_exposure_ms * yard_camera_gain(c));
}

void yard_camera_step_exposure(yard_camera *c, int direction) {
    // One-third stop, quantized to lines. Start from effective time when capped.
    int current = (int)lround(yard_camera_exposure_ms(c)*1000.0/c->profile.line_us);
    int limit = (int)ceil(1000000.0/(c->profile.fps*c->profile.line_us));
    if (limit > c->profile.max_exposure_lines) limit = c->profile.max_exposure_lines;
    int next = (int)lround(current * exp2(direction/3.0));
    if (direction > 0 && next <= current) next = current + 1;
    if (direction < 0 && next >= current) next = current - 1;
    c->exposure_lines = next < 0 ? 0 : next > limit ? limit : next;
}

void yard_camera_step_gain(yard_camera *c, int direction) {
    int next = c->gain_index + direction;
    c->gain_index = next < 0 ? 0 : next >= c->profile.gain_count ? c->profile.gain_count-1 : next;
}
