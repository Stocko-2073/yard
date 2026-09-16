# Camera target and exposure

The original XIAO ESP32-S3 Sense with **OV2640 at 800×600 SVGA** is Yard's
medium-term camera target. The built-in `ov2640-svga` profile remains the default.
Resolution, capture cadence, line timing, shutter range, gain steps, and render
calibration live in `src/camera.cpp` rather than in scene lighting. Other cameras
can use a profile file without changing the shaders. The lens remains separate:
`--vfov` supplies vertical FOV; the current 60° default is uncalibrated.

## Controls

Yard currently implements **manual** exposure and analog-gain response:

| Control | Default OV2640 profile |
| --- | --- |
| `--exposure-ms MS` | 0–33.333333… ms, rounded to sensor-line periods |
| `--aec-value N` | Integer 0–1200, matching Espressif's manual AEC value |
| `--gain X` | 1–31×, rounded to the nearest supported gain |
| `--agc-gain N` | Integer index 0–30, matching Espressif's gain control |
| Comma / period | Decrease / increase shutter by about 1/3 stop |
| Minus / equals | Decrease / increase gain by one supported step |

Use either milliseconds or AEC value, and either gain multiplier or gain index.
Conflicting units and invalid inputs are rejected. The title and startup log show
effective milliseconds, linear gain, and the requested line count/gain index.
Changing shutter or gain works while time is paused. R resets navigation and time
but preserves camera settings. Z changes only preview magnification.

Start with the existing daylight appearance (default: 202 lines, about 10.02 ms,
1× gain), or try this manual night setting:

```sh
./build/yard --date 2026-01-21 --time 0 --aec-value 672 --agc-gain 15
# Approximately 33.33 ms, 16x gain. Equivalent physical-unit controls:
./build/yard --date 2026-01-21 --time 0 --exposure-ms 33.333333 --gain 16
```

The same night setting will overexpose daylight. Automatic exposure/gain control
is not implemented; manual control corresponds to disabling sensor AEC/AGC on
hardware. No claim is made that Yard reproduces the OV2640's automatic algorithm,
DSP AEC2, metering, gamma, or other image-processing controls.

## Hardware basis and assumptions

Primary references:

- OmniVision [OV2640 datasheet v1.6](https://files.waveshare.com/wiki/common/OV2640DS_en.pdf),
  Figure 16 (page 15), GAIN register (page 22), and AEC registers (page 23).
- Espressif [OV2640 driver](https://github.com/espressif/esp32-camera/blob/3fb41a99d61a853313d1cd5543ebf2c109ef7c0e/sensors/ov2640.c)
  and [gain table](https://github.com/espressif/esp32-camera/blob/3fb41a99d61a853313d1cd5543ebf2c109ef7c0e/sensors/private_include/ov2640_settings.h),
  inspected at commit `3fb41a99d61a853313d1cd5543ebf2c109ef7c0e` on September 14, 2026.

The datasheet gives `exposure = line period × AEC`, capped to one frame. Its
nominal SVGA timing has 672 line periods per frame. Yard assumes 30 fps and thus
49.6031746 µs per line. This is a timing model, **not a verified XIAO clock setup**;
real XCLK, divider, blanking, and pixel-format settings affect timing. Values
above 672 remain accepted up to the driver's 1200 limit, but cannot increase
integration beyond 33.33 ms in this profile. Zero produces zero signal in Yard.

The gain-register formula, applied to Espressif's 31 table entries, produces
1× through 31× in unit increments. Thus driver index 0 means 1×, and index 15
means 16×; the index is neither dB nor the raw register byte. These are manual
control steps, not the automatic gain-ceiling setting.

## Rendering model and calibration

All sky and surface radiance is multiplied by

```
(effective exposure milliseconds / reference exposure milliseconds) * gain
```

before the existing ACES-style tone curve, sRGB encoding, and RGBA8 camera output.
The default reference is **10 ms at 1× gain**; this is an explicit arbitrary
mapping to the scaffold's existing render units, not an OV2640 sensitivity
measurement. Atlas skyglow ratios and sun/moon geometry remain independent of
exposure. Shutter is capped using the configured sensor frame period, never
window refresh time or delayed rendering frames.

This adds exposure response, not temporal integration: longer shutter currently
brightens the instantaneous image without motion blur or rolling-shutter skew.
Noise, full-well capacity, ADC response, sensor spectral response, white balance,
lens transmission, and the real camera's JPEG/RGB565 pipeline are still absent.
High gain therefore produces an unrealistically clean image. Absolute night
performance needs calibration against captures from the actual camera and lens.
Use the [hardware reference collection protocol](CAMERA_REFERENCE.md) and its
session metadata template for that first capture set. An
[indoor hardware bring-up sweep](../data/camera-reference/INDOOR_BRINGUP.md) verifies
USB collection and manual controls; the outdoor reference set remains outstanding.

## Other camera profiles

`--camera-profile FILE` loads a validated profile before applying CLI exposure
settings, regardless of argument order. `data/cameras/ov2640-svga.txt` reproduces
the built-in default; normal builds and launches require no external profile.
The whitespace-delimited format is:

```
YARD_CAMERA_V1 name width height fps line_us max_lines default_lines reference_ms gain_count
gain_0 gain_1 ... gain_N
```

Names contain no whitespace (31 characters maximum). Gains must be finite,
strictly increasing linear multipliers. No trailing tokens or comments are
accepted. The loader supports dimensions 1–4096, rates 1–120 fps, up to 65535
exposure lines, and 1–64 gain steps from 1× to 1024×. Timing/reference values
must be finite and positive; defaults must fit the profile's register range.
These are parser bounds, not guarantees of achievable performance.

Changing width/height selects a different fixed sensor image; window resizing,
Retina, and preview zoom still cannot change its resolution or projection.
The default OV2640 profile remains SVGA at up to 30 fps. Future profiles can
specify different timings and nonuniform gain steps; sensor-specific nonlinear
response and automatic metering will require extending the camera model.
