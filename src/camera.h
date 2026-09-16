#ifndef YARD_CAMERA_H
#define YARD_CAMERA_H

enum { YARD_CAMERA_MAX_GAINS = 64 };
typedef struct {
    char name[32];
    int width, height;
    double fps, line_us;
    int max_exposure_lines, default_exposure_lines;
    double reference_exposure_ms; // Render calibration, not sensor sensitivity.
    int gain_count;
    double gains[YARD_CAMERA_MAX_GAINS]; // Discrete linear multipliers.
} yard_camera_profile;

typedef struct {
    yard_camera_profile profile;
    int exposure_lines, gain_index;
} yard_camera;

extern const yard_camera_profile yard_ov2640_svga;
bool yard_camera_profile_load(const char *path, yard_camera_profile *out);
void yard_camera_init(yard_camera *camera, const yard_camera_profile *profile);
bool yard_camera_set_lines(yard_camera *camera, int lines);
bool yard_camera_set_exposure_ms(yard_camera *camera, double ms);
bool yard_camera_set_gain_index(yard_camera *camera, int index);
bool yard_camera_set_gain(yard_camera *camera, double gain);
void yard_camera_step_exposure(yard_camera *camera, int direction);
void yard_camera_step_gain(yard_camera *camera, int direction);
double yard_camera_exposure_ms(const yard_camera *camera);
double yard_camera_gain(const yard_camera *camera);
float yard_camera_multiplier(const yard_camera *camera);
#endif
