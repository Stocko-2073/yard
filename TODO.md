# TODO

## Procedural geometry and trees

Implement in two stages, with tree generation consuming the geometry engine.

- [ ] Build a geometry engine that converts higher-level primitives, similar to
      Blender's curves, beveled/swept profiles, and polygon surfaces, into indexed
      triangle meshes with normals and UVs for Yard's renderer. First research
      performant open-source solutions we could reuse; compare primitive support,
      mesh quality, tessellation controls, generation time, memory use, licensing,
      and C++/macOS integration. Keep the geometry engine independent of tree
      generation and expose detail controls suitable for producing LOD meshes.
- [ ] Port [tree-gen](https://github.com/friggog/tree-gen)'s generation algorithm
      and species presets to C++ against that geometry engine, replacing Blender
      geometry operations. Preserve its GPLv3 license and attribution. Compare
      representative species against Blender output and measure generation time
      and mesh size. Keep the branch skeleton separate from render meshes so
      seasonal foliage, pruning, and collision representations can build on it;
      persistent biological growth remains additional work.

## Camera

Keep the original XIAO ESP32-S3 Sense **OV2640 at 800×600 SVGA** as the
medium-term target. Preserve configurable camera profiles for other sensors.
Manual shutter/gain controls and profiles are already implemented. Capture
progress and future work are listed below. See
[camera design and current limits](design/CAMERA.md).

### Completed

- [x] Add USB and authenticated Wi-Fi capture firmware, serial network setup,
      and a collector that retains original SVGA images, metadata, and firmware
      provenance. Verify indoor acquisition, Wi-Fi reconnection, and initial
      outdoor capture. See the [collection protocol](design/CAMERA_REFERENCE.md),
      [indoor bring-up](data/camera-reference/INDOOR_BRINGUP.md),
      [Wi-Fi checks](data/camera-reference/WIFI_BRINGUP.md), and
      [partial twilight session](data/camera-reference/OUTDOOR_TWILIGHT.md).

### Next — future work

- [ ] Add selectable hardware white-balance correction and record its settings.
      Compare automatic/settled correction with the current disabled configuration
      before judging camera color quality. Define a repeatable way to retain
      corrected color settings for reference captures.
- [ ] Investigate outdoor AEC 672 acquisition failures and baseline brightness
      drift at identical register settings. Check sensor timing, settling,
      processing, power, and transport separately before assigning a cause.
- [ ] Complete the hardware reference set across daylight, twilight, and full
      night, including a repeatable target scene and controlled camera motion.
      Record exposure/gain, firmware revision, pixel format, clock settings,
      weather, and nearby lights. Initial indoor and twilight captures are
      retained, but are not an absolute brightness calibration dataset.
- [ ] Compare the original and replacement wide-lens OV2640 modules under matched
      lighting, framing, exposure, and color settings. Keep module/lens identity
      separate in metadata. Use an opaque lens cap for dark-frame measurements;
      hand-covered images cannot distinguish leakage from transmitted light.
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
