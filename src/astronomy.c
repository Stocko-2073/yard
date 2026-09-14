#define _POSIX_C_SOURCE 200809L
#include "astronomy.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static const double RAD = 0.017453292519943295;
static double sind(double x) { return sin(x * RAD); }
static double cosd(double x) { return cos(x * RAD); }
static double norm(double v[3]) {
    double length = sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
    for (int i=0; i<3; ++i) v[i] /= length;
    return length;
}
static double dot(const double a[3], const double b[3]) {
    return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
}
static double anomaly(double mean, double eccentricity) {
    double m = remainder(mean, 360.0)*RAD, e = m;
    for (int i=0; i<8; ++i) e -= (e-eccentricity*sin(e)-m)/(1-eccentricity*cos(e));
    return e;
}
static void equatorial(double longitude, double latitude, double radius, double obliquity, double out[3]) {
    double y = radius*sind(longitude)*cosd(latitude), z = radius*sind(latitude);
    out[0] = radius*cosd(longitude)*cosd(latitude);
    out[1] = y*cosd(obliquity)-z*sind(obliquity);
    out[2] = y*sind(obliquity)+z*cosd(obliquity);
}
static void horizontal(const double eq[3], double sidereal, double latitude, double out[3]) {
    double meridian = cosd(sidereal)*eq[0]+sind(sidereal)*eq[1];
    out[0] = -sind(sidereal)*eq[0]+cosd(sidereal)*eq[1];
    out[1] = cosd(latitude)*meridian+sind(latitude)*eq[2];
    out[2] = sind(latitude)*meridian-cosd(latitude)*eq[2];
}

void yard_astronomy(double unix_seconds, double latitude, double longitude, yard_ephemeris *out) {
    // Low precision orbital model with lunar perturbations: Paul Schlyter,
    // https://stjarnhimlen.se/comp/ppcomp.html . All orbital angles in degrees.
    // Epoch is 2000 Jan 0.0 UT; UTC approximates UT1/TT at this visual accuracy.
    double jd = unix_seconds/86400.0+2440587.5, d = jd-2451543.5;
    double obliquity = 23.4393-3.563e-7*d;
    double ms = 356.0470+0.9856002585*d, ws = 282.9404+4.70935e-5*d;
    double es = 0.016709-1.151e-9*d, eccentric = anomaly(ms, es);
    double x = cos(eccentric)-es, y = sqrt(1-es*es)*sin(eccentric);
    double sun_lon = atan2(y,x)/RAD+ws, sun_distance = hypot(x,y)*149597870.7;
    double sun_eq[3];
    equatorial(sun_lon, 0, sun_distance, obliquity, sun_eq);

    double node = 125.1228-0.0529538083*d, peri = 318.0634+0.1643573223*d;
    double mm = 115.3654+13.0649929509*d;
    eccentric = anomaly(mm, 0.0549);
    x = cos(eccentric)-0.0549;
    y = sqrt(1-0.0549*0.0549)*sin(eccentric);
    double v = atan2(y,x)/RAD, radius = hypot(x,y)*60.2666;
    double mx = cosd(node)*cosd(v+peri)-sind(node)*sind(v+peri)*cosd(5.1454);
    double my = sind(node)*cosd(v+peri)+cosd(node)*sind(v+peri)*cosd(5.1454);
    double mz = sind(v+peri)*sind(5.1454);
    double lon = atan2(my,mx)/RAD, lat = atan2(mz,hypot(mx,my))/RAD;
    double elongation = mm+peri+node-ms-ws, f = mm+peri;
    lon += -1.274*sind(mm-2*elongation)+0.658*sind(2*elongation)-0.186*sind(ms)
        -0.059*sind(2*mm-2*elongation)-0.057*sind(mm-2*elongation+ms)
        +0.053*sind(mm+2*elongation)+0.046*sind(2*elongation-ms)+0.041*sind(mm-ms)
        -0.035*sind(elongation)-0.031*sind(mm+ms)-0.015*sind(2*f-2*elongation)
        +0.011*sind(mm-4*elongation);
    lat += -0.173*sind(f-2*elongation)-0.055*sind(mm-f-2*elongation)
        -0.046*sind(mm+f-2*elongation)+0.033*sind(f+2*elongation)+0.017*sind(2*mm+f);
    radius += -0.58*cosd(mm-2*elongation)-0.46*cosd(2*elongation);
    double moon_eq[3];
    equatorial(lon, lat, radius*6378.137, obliquity, moon_eq);

    double sidereal = remainder(280.46061837+360.98564736629*(jd-2451545.0)+longitude, 360.0);
    // WGS84 sea-level observer: subtract before normalizing to retain lunar parallax.
    double e2 = 0.00669437999014;
    double n = 6378.137/sqrt(1-e2*sind(latitude)*sind(latitude));
    double observer[3] = {n*cosd(latitude)*cosd(sidereal), n*cosd(latitude)*sind(sidereal),
                          n*(1-e2)*sind(latitude)};
    double sun_top[3], moon_top[3], illumination[3];
    for (int i=0; i<3; ++i) {
        sun_top[i] = sun_eq[i]-observer[i];
        moon_top[i] = moon_eq[i]-observer[i];
        illumination[i] = sun_eq[i]-moon_eq[i];
    }
    norm(sun_top);
    double distance = norm(moon_top);
    norm(illumination);
    horizontal(sun_top, sidereal, latitude, out->sun);
    horizontal(moon_top, sidereal, latitude, out->moon);
    horizontal(illumination, sidereal, latitude, out->moon_to_sun);
    const double pole[3] = {0,0,1};
    horizontal(pole, sidereal, latitude, out->celestial_north);
    out->moon_radius = asin(1737.4/distance);
    out->illuminated = fmax(0, fmin(1, (1-dot(moon_top,illumination))*0.5));
    out->waxing = sind(lon-sun_lon) >= 0;
}

void yard_local_calendar(double unix_seconds, struct tm *out) {
    time_t seconds = (time_t)floor(unix_seconds);
    localtime_r(&seconds, out);
}

bool yard_local_datetime(const char *date, double hour, double *unix_seconds) {
    if (strlen(date) != 10) return false;
    for (int i=0; i<10; ++i) {
        if (i == 4 || i == 7) { if (date[i] != '-') return false; }
        else if (date[i] < '0' || date[i] > '9') return false;
    }
    int year, month, day, consumed = 0;
    if (strlen(date) != 10 || sscanf(date, "%4d-%2d-%2d%n", &year, &month, &day, &consumed) != 3 ||
        consumed != 10 || year < 1900 || year > 2100 || !isfinite(hour) || hour < 0 || hour >= 24) return false;
    int seconds = (int)floor(hour*3600);
    struct tm tm = {.tm_year=year-1900, .tm_mon=month-1, .tm_mday=day,
                    .tm_hour=seconds/3600, .tm_min=(seconds/60)%60, .tm_sec=seconds%60, .tm_isdst=-1};
    time_t result = mktime(&tm);
    // Reject invalid dates and nonexistent local times at the spring DST transition.
    if (tm.tm_year != year-1900 || tm.tm_mon != month-1 || tm.tm_mday != day ||
        tm.tm_hour != seconds/3600 || tm.tm_min != (seconds/60)%60 || tm.tm_sec != seconds%60) return false;
    // Select the standard-time occurrence deterministically if the fall hour repeats.
    if (tm.tm_isdst > 0) {
        struct tm standard = tm;
        standard.tm_isdst = 0;
        time_t candidate = mktime(&standard);
        if (standard.tm_year == tm.tm_year && standard.tm_yday == tm.tm_yday &&
            standard.tm_hour == tm.tm_hour && standard.tm_min == tm.tm_min) result = candidate;
    }
    *unix_seconds = (double)result;
    return true;
}

double yard_local_hour(double unix_seconds, double hour, int day_offset) {
    struct tm tm;
    yard_local_calendar(unix_seconds, &tm);
    tm.tm_mday += day_offset;
    if (hour >= 0) {
        int seconds = (int)floor(hour*3600);
        tm.tm_hour = seconds/3600; tm.tm_min = (seconds/60)%60; tm.tm_sec = seconds%60;
    }
    tm.tm_isdst = -1;
    return (double)mktime(&tm);
}
