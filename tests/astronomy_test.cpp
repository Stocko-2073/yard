#define _POSIX_C_SOURCE 200809L
#include "astronomy.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static const double RAD = 0.017453292519943295;
static double utc(const char *date, double hour) {
    double result;
    assert(yard_local_datetime(date, hour, &result));
    return result;
}
static void close_to(double actual, double expected, double tolerance) {
    if (fabs(actual-expected) > tolerance) {
        fprintf(stderr, "Expected %.8f +/- %.8f, got %.8f\n", expected, tolerance, actual);
        abort();
    }
}
int main(void) {
    assert(setenv("TZ", "UTC", 1) == 0); tzset();
    // Independent phase-event reference: https://aa.usno.navy.mil/calculated/moon/phases?year=2026
    const char *dates[] = {"2026-01-18", "2026-01-26", "2026-02-01", "2026-02-09"};
    const double hours[] = {19+52/60.0, 4+47/60.0, 22+9/60.0, 12+43/60.0};
    const double fractions[] = {0, 0.5, 1, 0.5};
    for (int i=0; i<4; ++i) {
        yard_ephemeris sky;
        yard_astronomy(utc(dates[i], hours[i]), YARD_LATITUDE, YARD_LONGITUDE, &sky);
        // Local parallax makes observed quarters differ slightly from geocentric 50%.
        close_to(sky.illuminated, fractions[i], 0.015);
        if (i == 1) assert(sky.waxing);
        if (i == 3) assert(!sky.waxing);
        assert(sky.moon_radius/RAD > 0.24 && sky.moon_radius/RAD < 0.29);
        printf("%s: %.3f%% illuminated, radius %.4f degrees\n", dates[i], sky.illuminated*100, sky.moon_radius/RAD);
    }
    // Published worked topocentric example, 1990-04-19 00:00 UTC at 60 N, 15 E:
    // https://stjarnhimlen.se/comp/tutorial.html sections 6 and 9.
    yard_ephemeris sky;
    yard_astronomy(utc("1990-04-19", 0), 60, 15, &sky);
    close_to(asin(sky.sun[1])/RAD, -17.9570, 0.02);
    close_to(atan2(sky.sun[0],-sky.sun[2])/RAD, 15.6767, 0.02);
    double ha = (221.8388-310.0017)*RAD, dec = -19.8790*RAD;
    double expected_alt = asin(sin(60*RAD)*sin(dec)+cos(60*RAD)*cos(dec)*cos(ha));
    double expected_az = atan2(-cos(dec)*sin(ha), cos(60*RAD)*sin(dec)-sin(60*RAD)*cos(dec)*cos(ha));
    close_to(asin(sky.moon[1])/RAD, expected_alt/RAD, 0.03);
    close_to(atan2(sky.moon[0],-sky.moon[2])/RAD, expected_az/RAD, 0.03);
    yard_ephemeris elsewhere;
    yard_astronomy(utc("1990-04-19", 0), -30, -84, &elsewhere);
    assert(fabs(elsewhere.moon[1]-sky.moon[1]) > 0.2);

    assert(setenv("TZ", "America/New_York", 1) == 0); tzset();
    double t;
    assert(!yard_local_datetime("2026-02-29", 12, &t));
    assert(!yard_local_datetime("2026-04-31", 12, &t));
    assert(!yard_local_datetime("2026-03-08", 2.5, &t)); // DST gap.
    assert(!yard_local_datetime("2026-01-01", 24, &t));
    assert(!yard_local_datetime("2026-01-01", NAN, &t));
    assert(!yard_local_datetime("2101-01-01", 0, &t));
    struct tm repeated;
    yard_local_calendar(utc("2026-11-01", 1.5), &repeated);
    assert(repeated.tm_hour == 1 && repeated.tm_min == 30 && repeated.tm_isdst == 0);
    assert(!yard_local_datetime("2026- 1-01", 12, &t));
    t = utc("2026-03-07", 12);
    close_to(yard_local_hour(t, -1, 1)-t, 23*3600, 0);
    t = utc("2026-10-31", 12);
    close_to(yard_local_hour(t, -1, 1)-t, 25*3600, 0);
    struct tm local;
    yard_local_calendar(utc("2024-02-28", 23)+3600, &local);
    assert(local.tm_mon == 1 && local.tm_mday == 29 && local.tm_hour == 0);
    yard_local_calendar(utc("2026-12-31", 23)+3600, &local);
    assert(local.tm_year == 127 && local.tm_mon == 0 && local.tm_mday == 1);
    // UTC simulation remains monotonic even as the local fall clock repeats.
    yard_local_calendar(utc("2026-11-01", 0.5)+3*3600, &local);
    assert(local.tm_hour == 2 && local.tm_min == 30 && local.tm_isdst == 0);
    puts("Astronomy and local calendar checks passed.");
}
