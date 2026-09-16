#ifndef YARD_ASTRONOMY_H
#define YARD_ASTRONOMY_H
#include <time.h>

#define YARD_LATITUDE 32.8908277
#define YARD_LONGITUDE (-84.3271342)

typedef struct {
    // World coordinates: east +X, up +Y, north -Z. Directions are topocentric.
    double sun[3], moon[3], moon_to_sun[3], celestial_north[3];
    double moon_radius; // Angular radius in radians.
    double illuminated;
    bool waxing;
} yard_ephemeris;

void yard_astronomy(double unix_seconds, double latitude, double longitude, yard_ephemeris *out);
// Calendar helpers use the process timezone, set once to America/New_York.
bool yard_local_datetime(const char *date, double hour, double *unix_seconds);
double yard_local_hour(double unix_seconds, double hour, int day_offset);
void yard_local_calendar(double unix_seconds, struct tm *out);
#endif
