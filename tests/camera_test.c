#include "camera.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <unistd.h>

static void invalid_profile(const char *text) {
    const char *path = "build/invalid-camera.txt";
    FILE *f = fopen(path, "w");
    assert(f);
    fputs(text, f);
    fclose(f);
    yard_camera_profile p = yard_ov2640_svga;
    assert(!yard_camera_profile_load(path, &p));
    assert(p.width == 800 && p.fps == 30);
    unlink(path);
}

int main(void) {
    yard_camera_profile p;
    assert(yard_camera_profile_load("data/cameras/ov2640-svga.txt", &p));
    assert(p.width == 800 && p.height == 600 && p.fps == 30);
    assert(fabs(p.line_us - yard_ov2640_svga.line_us) < 1e-10);
    assert(p.max_exposure_lines == 1200 && p.default_exposure_lines == 202);
    assert(p.reference_exposure_ms == 10 && p.gain_count == 31);
    // Independent register-formula checks at driver table boundaries.
    assert(p.gains[0] == 1 && p.gains[1] == 2 && p.gains[7] == 8);
    assert(p.gains[15] == 16 && p.gains[30] == 16*(1+15.0/16));
    for (int i=0; i<p.gain_count; ++i) assert(p.gains[i] == yard_ov2640_svga.gains[i]);
    yard_camera c;
    yard_camera_init(&c, &p);
    assert(yard_camera_set_lines(&c, 336));
    assert(fabs(yard_camera_exposure_ms(&c)-1000.0/60) < 1e-10);
    double baseline = yard_camera_multiplier(&c);
    assert(yard_camera_set_lines(&c, 672));
    assert(fabs(yard_camera_multiplier(&c)/baseline - 2) < 1e-6);
    assert(yard_camera_set_lines(&c, 1200));
    assert(fabs(yard_camera_exposure_ms(&c)-1000.0/30) < 1e-10);
    yard_camera_step_exposure(&c, -1);
    assert(yard_camera_exposure_ms(&c) < 1000.0/30);
    assert(yard_camera_set_lines(&c, 672));
    assert(yard_camera_set_gain_index(&c, 30));
    assert(fabs(yard_camera_multiplier(&c) - 103.333333333) < 1e-4);
    assert(yard_camera_set_exposure_ms(&c, 10));
    assert(c.exposure_lines == 202);
    assert(yard_camera_set_gain(&c, 8.3) && yard_camera_gain(&c) == 8);
    assert(!yard_camera_set_exposure_ms(&c, 34));
    assert(!yard_camera_set_exposure_ms(&c, NAN));
    assert(!yard_camera_set_exposure_ms(&c, -1));
    assert(!yard_camera_set_gain(&c, INFINITY));
    assert(!yard_camera_set_gain(&c, .5));
    assert(!yard_camera_set_gain_index(&c, 31));
    assert(!yard_camera_set_lines(&c, 1201));
    assert(!yard_camera_set_lines(&c, -1));
    assert(yard_camera_set_lines(&c, 0));
    assert(yard_camera_multiplier(&c) == 0);
    yard_camera_step_exposure(&c, 1);
    assert(c.exposure_lines == 1);
    for (int i=0; i<100; ++i) { yard_camera_step_exposure(&c, 1); yard_camera_step_gain(&c, 1); }
    assert(fabs(yard_camera_exposure_ms(&c)-1000.0/30) < 1e-8);
    assert(c.gain_index == 30);
    for (int i=0; i<100; ++i) { yard_camera_step_exposure(&c, -1); yard_camera_step_gain(&c, -1); }
    assert(c.exposure_lines == 0 && c.gain_index == 0);

    // Different camera timing/gains must affect limits and response, not just its label.
    p.fps = 60; p.line_us = 20; p.max_exposure_lines = 500;
    p.gain_count = 3; p.gains[0] = 1; p.gains[1] = 1.5; p.gains[2] = 4;
    yard_camera_init(&c, &p);
    assert(yard_camera_set_exposure_ms(&c, 10));
    assert(!yard_camera_set_exposure_ms(&c, 11));
    assert(yard_camera_set_gain(&c, 1.6) && yard_camera_gain(&c) == 1.5);
    assert(fabs(yard_camera_multiplier(&c)-1.5) < 1e-6);

    invalid_profile("YARD_CAMERA_V1 test 99999999999999999999 600 30 50 1200 202 10 1\n1\n");
    invalid_profile("YARD_CAMERA_V1 test 800 600 0 50 1200 202 10 1\n1\n");
    invalid_profile("YARD_CAMERA_V1 test 800 600 30 nan 1200 202 10 1\n1\n");
    invalid_profile("YARD_CAMERA_V1 test 800 600 30 50 1200 202 0 1\n1\n");
    invalid_profile("YARD_CAMERA_V1 test 800 600 30 50 1200 202 10 65\n1\n");
    invalid_profile("YARD_CAMERA_V1 test 800 600 30 50 1200 202 10 2\n2 1\n");
    invalid_profile("YARD_CAMERA_V1 test 800 600 30 50 1200 202 10 1\ninf\n");
    invalid_profile("YARD_CAMERA_V1 test 800 600 30 50 1200 202 10 2\n1\n");
    invalid_profile("YARD_CAMERA_V1 test 800 600 30 50 1200 202 10 1\n1 extra\n");
    puts("Camera: profile, shutter quantization, frame limits, gain, and exposure checks passed.");
}
