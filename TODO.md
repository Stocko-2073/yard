# TODO

## Camera

Keep the original XIAO ESP32-S3 Sense **OV2640 at 800×600 SVGA** as the
medium-term target. Preserve configurable camera profiles for other sensors.
Manual shutter/gain controls and profiles are already implemented; the items
below are future work. See [camera design and current limits](design/CAMERA.md).

### Next

- [ ] Capture a hardware reference set across daylight, twilight, and night,
      with recorded exposure/gain, firmware revision, pixel format, and clock
      settings. Include static scenes and controlled camera motion.
      [Collection protocol and metadata template](design/CAMERA_REFERENCE.md) are
      prepared. USB/Wi-Fi capture tooling and a 360-frame indoor bring-up sweep are
      verified; the outdoor reference set and controlled motion remain outstanding.
- [ ] Verify SVGA line timing and the exposure/frame-period relationship on the
      actual module. Replace inferred profile timings with measured values.
- [ ] Calibrate render brightness against the reference captures across shutter
      and gain settings, replacing the arbitrary 10 ms exposure reference.
- [ ] Add image-based automatic exposure and gain control with independent
      enable/disable switches, exposure lock, adjustable target brightness, and
      profile-specific limits. Check settling and stability through day/night
      transitions; distinguish an approximation from the OV2640's real algorithm.
- [ ] Add captured-image regression checks for exposure changes, clipping, and
      nighttime visibility. Cover manual controls while paused and confirm that
      window resizing, Retina, and preview zoom do not alter the sensor image.

### Further fidelity

- [ ] Measure stock-lens intrinsics/FOV and distortion; store calibrated lens
      settings separately from sensor timing and response.
- [ ] Model shot/read noise, black level, saturation, and quantization using
      hardware measurements, including the loss of usable detail at high gain.
      Support deterministic noise seeds for repeatable simulation runs.
- [ ] Integrate scene motion over the exposure interval and model rolling-shutter
      readout. Keep sensor timing independent of display refresh and render stalls.
- [ ] Characterize and approximate the selected hardware output pipeline:
      white balance, color response, tone/gamma response, and JPEG or RGB565
      artifacts. Record the relevant hardware settings with each reference capture.
- [ ] Extend camera profiles as these features arrive, keeping lens, sensor,
      and image-processing parameters separate from yard lighting. Validate a
      second sensor profile before treating the model as generally calibrated.
