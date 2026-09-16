#include "skyglow.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <unistd.h>

static void malformed(const char *text) {
    const char *path = "build/invalid-site.txt";
    FILE *f = fopen(path, "w");
    assert(f);
    fputs(text, f);
    fclose(f);
    yard_site site = yard_default_site;
    assert(!yard_site_load(path, &site));
    assert(site.artificial_ratio == yard_default_site.artificial_ratio);
    unlink(path);
}

int main(void) {
    yard_site site;
    assert(yard_site_load("data/default-site.txt", &site));
    assert(site.latitude == yard_default_site.latitude);
    assert(site.longitude == yard_default_site.longitude);
    assert(site.artificial_ratio == yard_default_site.artificial_ratio);
    assert(site.year == yard_default_site.year);
    assert(!yard_site_load("build/missing-site.txt", &site));
    malformed("YARD_SKYGLOW_V1 nan 0 2025 1\n");
    malformed("YARD_SKYGLOW_V1 75 0 2025 1\n");
    malformed("YARD_SKYGLOW_V1 30 181 2025 1\n");
    malformed("YARD_SKYGLOW_V1 30 0 2025 -1\n");
    malformed("YARD_SKYGLOW_V1 30 0 2025 inf\n");
    malformed("YARD_SKYGLOW_V1 30 0 2025 100001\n");
    malformed("YARD_SKYGLOW_V1 30 0 2025 1 extra\n");
    malformed("YARD_SKYGLOW_V1 30 0 2025 1\nextra\n");
    malformed("YARD_SKYGLOW_V2 30 0 2025 1\n");
    malformed("YARD_SKYGLOW_V1 30 0\n");

    yard_site dark = site, city = site;
    dark.artificial_ratio = 0;
    city.artificial_ratio = 30;
    float a[4], b[4], c[4];
    yard_night_light(&dark, -1, a);
    yard_night_light(&site, -1, b);
    yard_night_light(&city, -1, c);
    double ya = .2126*a[0]+.7152*a[1]+.0722*a[2];
    double yb = .2126*b[0]+.7152*b[1]+.0722*b[2];
    assert(fabs(yb/ya - (1+site.artificial_ratio)) < 1e-5);
    for (int i=0; i<3; ++i) assert(a[i] < b[i] && b[i] < c[i]);
    float previous = c[0];
    for (int degree=-90; degree<=90; ++degree) {
        yard_night_light(&city, sin(degree*0.0174532925199433), c);
        assert(c[0] <= previous + 1e-8f);
        assert(isfinite(c[0]) && c[0] >= 0);
        previous = c[0];
    }
    yard_night_light(&city, 0, c);
    for (int i=0; i<4; ++i) assert(c[i] == 0);
    puts("Skyglow: profile validation, brightness ratios, and twilight checks passed.");
}
