# Hardware camera reference collection

Status: USB/Wi-Fi firmware and collection tooling have been verified against a connected
OV2640, including a 360-frame indoor static sweep. See the
[indoor bring-up record](../data/camera-reference/INDOOR_BRINGUP.md) and
[Wi-Fi validation](../data/camera-reference/WIFI_BRINGUP.md). A
[partial outdoor twilight session](../data/camera-reference/OUTDOOR_TWILIGHT.md)
has also been collected, with limitations recorded. The first camera TODO
remains open until the outdoor and controlled-motion reference set below exists. This procedure targets the original XIAO ESP32-S3
Sense with OV2640, using its installed lens and 800×600 SVGA output.

## Before collection

Identify the board, sensor, and lens physically. Save the exact capture firmware
source revision, local modifications, build configuration, and camera-driver
revision with the session. The [USB/Wi-Fi capture utility](../tools/camera-reference/README.md)
provides firmware and a collector for bring-up and static sweeps, saving original
camera buffers, settings, and timestamps. The Yard renderer itself does not capture
hardware images.
Record how that firmware is built and run so another session can reproduce it.

Use a fixed mount overlooking a static scene with matte light, middle-gray, and
dark patches, fine detail, and a sharp vertical edge. Measure target dimensions
and camera distance/height, and photograph the setup separately. Keep composition
and focus fixed across the daylight, twilight, and night sessions. Record weather,
cloud cover, nearby lamps, location, and UTC start/end times. Do not infer scene
luminance from patch color; record a meter reading only if actually measured.

Disable automatic exposure, secondary exposure control, and automatic gain for
the manual series, and record their reported states. Record white-balance state
and all accessible image-processing settings, including brightness, contrast,
saturation, gamma, corrections, mirroring, and JPEG quality where applicable.
Keep processing settings fixed within a comparison. If a setting cannot be read
back, label it unknown rather than assuming the requested value took effect.

Record configured XCLK and accessible clock-divider, blanking, and timing
registers, including register bank/address/value and the method used to read them.
Keep requested settings, driver readback, and instrument measurements separate.
The simulator's 49.6031746 µs line period is inferred, so do not use it to label
hardware exposures in milliseconds. Measuring timing is the next TODO item.

## Collection matrix

Repeat these two series in each lighting condition: daylight, twilight, and night.
The following values are proposed starting points, not calibrated exposures or a
claim that a particular firmware applies them correctly.

| Series | Requested manual settings | Capture |
| --- | --- | --- |
| Static shutter sweep | AEC 50, 100, 202, 400, 672; AGC index 0 | At least 30 consecutive frames per setting |
| Static gain sweep | AEC 202; AGC index 0, 3, 7, 15, 30 | At least 30 consecutive frames per setting |
| Controlled motion | AEC 50, 202, 672; AGC index fixed within the series | At least 3 repeat passes per setting, with stationary lead-in/out |

Preserve clipped and nearly black captures, but add a usable setting if the
initial series contains no visible target detail. Record any additions. Repeat
one fixed baseline setting at the beginning and end of each series to reveal
lighting drift, especially during twilight. Record exact UTC times per run;
"twilight" alone is insufficient to compare brightness.

After each setting change, drain queued old frames and wait for the new settings
to settle. Log the number of discarded frames and the acceptance criterion used;
do not assign the new settings to frames still captured under the previous ones.
Record the applied settings with each run and split a run whenever they change.

For motion, use a repeatable camera pan or translation past the static edge, with
a measured angle or travel distance and a recorded duration. Record axis,
direction, speed or trajectory, mount, and how motion was measured. Keep target
distance and lighting fixed. An unmeasured hand-held sweep may be useful context
but does not satisfy controlled motion. These frames will support later shutter
integration work; they do not establish rolling-shutter timing by themselves.

## Files and metadata

Copy [the session template](../data/camera-reference/session-template.json) into
the capture directory. Replace null fields with observed values; retain null for
unknowns and explain missing measurements in `notes`. Duplicate the run and frame
records as needed. The template is a recording aid, not a validated interchange
format or a hardware capture program.

Keep original JPEG bytes or raw pixel buffers without resizing, recompression,
or screenshotting. For raw formats, record byte order, row stride, and packing.
Keep previews separately. Every frame record must name its original file and
include its SHA-256, byte count, sequence number, and timestamp. State whether
timestamps describe sensor capture, driver delivery, or host receipt, including
clock units, origin, and UTC synchronization uncertainty. Log dropped frames and
capture failures; host arrival intervals are not measured sensor frame periods.

Use one directory per session, containing the completed metadata, original
images, firmware/configuration snapshot, settings/register logs, and setup notes.
Record firmware artifacts and register logs by relative path in the metadata.
Store large capture sets outside Git and add a durable archive location and its
checksum to a small repository index once real captures are available. Do not
commit placeholder images or treat generated simulator output as hardware data.

## Completion review

Before checking off the TODO, verify that all three lighting conditions contain
both static and measured-motion series, every original file is readable and has
the recorded checksum, and decoded images are 800×600. Check that each run links
to exposure/gain settings, firmware/driver revisions, pixel format, and clock
configuration. Inspect representative images for focus, target visibility,
clipping, and whether the intended motion occurred. Record missing settings or
uncertain readbacks explicitly and collect them before calling the set complete.

Record observed frame intervals as observations only. Leave the timing and render
brightness calibration TODOs open until their separate measurements and analysis
are complete; this reference collection does not change Yard's camera profile.
