# Outdoor twilight session — 2026-09-14

Collected an initial outdoor reference session over Wi-Fi with the OV2640 powered
by the user's phone. The user described the camera lying on a deck table, pointed
at a partly cloudy sky, while the user was indoors. Exact coordinates, the
user-reported local time, and final scene notes are retained in the local session
metadata. Nearby artificial lighting, camera height/orientation, lens intrinsics,
and scene luminance were not measured or fully characterized.

Acquisition ran from **20:07:49 to 20:13:20 EDT** (September 15, 00:07:49–00:13:20
UTC). At the supplied coordinates, Yard's geometric sun model put the sun between
approximately −6.29° and −7.41° elevation: twilight, not a full-night reference.
The firmware and capture tools were at commit `6670469`; the installed Arduino
core was `esp32:esp32@3.3.10`. Original firmware artifacts and driver provenance
are retained with the session.

## Retained data and verification

- Eleven complete 30-frame runs: beginning/end baselines at AEC 202/gain index 0;
  AEC 50, 100, 202, and 400 at gain index 0; and gain indices 0, 3, 7, 15, and 30
  at AEC 202.
- One preview image and two images from an interrupted AEC 672 run.
- **333 retained original JPEGs** in total. All fully decoded at 800×600, matched
  recorded byte counts and SHA-256 hashes, and had increasing DMA timestamps
  within each run. Available exposure/gain and manual-control register readbacks
  matched requests. No sensor-rate frame continuity is claimed.
- Three incomplete AEC 672 attempts: two failed while discarding settling frames
  (gain indices 0 and 30); a gain-index-0 retry retained two frames before another
  acquisition failure. Error records are preserved. The cause is unmeasured;
  this does not establish a hardware exposure limit or a network failure.

## Observations and limits

Initial low-gain images were nearly black. Inspected higher-gain frames showed
substantial colored noise without clear cloud detail. This is evidence for the
need to characterize low-light noise, not a calibrated noise model.

The baseline's mean encoded grayscale value rose from approximately **1.01 to
10.51** on a 0–255 scale, despite matching AEC/gain readbacks. These values include
the camera processing pipeline and JPEG encoding, not linear radiance. Neither
the baseline change nor brightness differences between settings can be assigned
to a single cause from this dataset. Illumination, processing/settling behavior,
and sensor effects need separate investigation. The sky-only scene lacks a
calibrated target or measured pose and is unsuitable for absolute brightness
calibration.

The full reference set remains future work: this is a **partial twilight reference**.
Daylight, full-night, controlled-motion, and a repeatable target scene remain
outstanding, as do hardware timing and render-brightness calibration. Outdoor
transfers also took longer than the indoor checks; capture timestamps, rather
than requested cadence, must be used when analyzing this set.

## Local retention

The session directory includes `session.json`, `scene.json`, requested run
settings, per-run metadata, all original images and matching firmware snapshots,
driver provenance, and a reproducible Pillow review script with its JSON results.
Images and precise location metadata are kept locally, outside Git.

The archive is stored under the base checkout at
`.worktrees/artifacts/camera-reference/2026-09-14-outdoor-twilight.tar.gz`, outside
the task worktree. SHA-256:
`458881a30aec1ae1066df8d423ba8b1c6b4fc221f4c8bb888ca9e5d8c3b3550f`.
This is local retention, not a published dataset or off-machine backup.
