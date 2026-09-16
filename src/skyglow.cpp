#include "skyglow.h"
#include "astronomy.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// Reproduce with tools/fetch-skyglow.py; provenance in data/default-site.txt.
const yard_site yard_default_site = {YARD_LATITUDE, YARD_LONGITUDE, 2025, 5.44249805476};

bool yard_site_load(const char *path, yard_site *site) {
    FILE *file = fopen(path, "r");
    if (!file) return false;
    yard_site candidate = {};
    char line[512], tag[32], extra;
    bool valid = fgets(line, sizeof(line), file) &&
        sscanf(line, "%31s %lf %lf %d %lf %c", tag, &candidate.latitude,
               &candidate.longitude, &candidate.year, &candidate.artificial_ratio, &extra) == 5 &&
        strcmp(tag, "YARD_SKYGLOW_V1") == 0 &&
        isfinite(candidate.latitude) && candidate.latitude >= -65 && candidate.latitude < 75 &&
        isfinite(candidate.longitude) && fabs(candidate.longitude) <= 180 &&
        candidate.year >= 2016 && candidate.year <= 2025 &&
        isfinite(candidate.artificial_ratio) && candidate.artificial_ratio >= 0 &&
        candidate.artificial_ratio <= 100000;
    while (valid && fgets(line, sizeof(line), file)) {
        valid = line[0] == '#' || line[0] == '\n';
    }
    valid = valid && !ferror(file);
    fclose(file);
    if (valid) *site = candidate;
    return valid;
}

void yard_night_light(const yard_site *site, double sun_y, float rgb[4]) {
    // Full night below -18 degrees, off by -6 degrees; smooth astronomical twilight.
    double t = (sun_y - sin(-18.0 * 0.0174532925199433)) /
               (sin(-6.0 * 0.0174532925199433) - sin(-18.0 * 0.0174532925199433));
    t = fmax(0, fmin(1, t));
    double night = 1 - t*t*(3 - 2*t);
    // Preserve the natural sky floor's luminance; no spectrum is supplied by the atlas.
    const double natural[3] = {0.0005, 0.0008, 0.0016};
    const double luminance = 0.2126*0.0005 + 0.7152*0.0008 + 0.0722*0.0016;
    for (int i = 0; i < 3; ++i) {
        rgb[i] = (float)(night * (natural[i] + luminance*site->artificial_ratio));
    }
    rgb[3] = 0;
}
