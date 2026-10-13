#ifndef YARD_SKYGLOW_H
#define YARD_SKYGLOW_H

typedef struct {
    double latitude, longitude;
    int year;
    double artificial_ratio; // Artificial / natural zenith sky brightness (LPI).
} yard_site;

extern const yard_site yard_default_site;
bool yard_site_load(const char *path, yard_site *site);
// Render-space RGB radiance: the natural sky floor plus achromatic skyglow at the
// atlas ratio. Relative brightness only, not calibrated photometry.
void yard_night_light(const yard_site *site, double sun_y, float rgb[4]);
#endif
