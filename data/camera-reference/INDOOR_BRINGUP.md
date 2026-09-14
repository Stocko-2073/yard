# Indoor OV2640 bring-up — 2026-09-14

The USB capture firmware was flashed onto the connected XIAO ESP32-S3 Sense and
verified by the uploader. Manual BOOT/RESET entry was needed for the initial
upload, followed by a RESET tap to start the app. Camera identification returned
PID `0x26`, version `0x42` (OV2640). Three initial test images and a subsequent
360-frame sweep decoded successfully at 800×600.

Setup: board on an indoor desk pointing at the ceiling, as described by the user
and observed in the images. There was no calibrated target, measured illumination,
or controlled motion. This is a bring-up check, **not the outdoor reference set**.
Original images and firmware artifacts stay local; no indoor images are included
in Git. The collection tools are in `tools/camera-reference/`.

## Configuration and provenance

- Arduino core `esp32:esp32@3.3.10`, ESP-IDF `v5.5.4`.
- FQBN `esp32:esp32:XIAO_ESP32S3:PSRAM=opi`.
- JPEG 800×600, quality 10, configured XCLK 20 MHz, one PSRAM frame buffer.
- AEC, AEC2, AGC, AWB, and AWB gain disabled. Other processing defaults retained.
- Eight frames discarded after each setting change, then 30 delivered frames per
  run. No calibrated settling time or continuous sensor-rate capture is claimed.
- Running app ELF SHA-256:
  `55cab2177576de957c2f4832a50824d99ec342146e3570620f2a4086b29f5953`.
- App binary SHA-256:
  `2379a2be87241bfed170e7dfd28e371bef8a782bf2bfc60d45a23f395c7e700e`.
- Installed `libespressif__esp32-camera.a` SHA-256:
  `9f25728226978bdf68fe92df6c6fe9cc6283efdc237a4c1e6927b3d13713992a`.
  The bundle's `versions.txt` leaves its camera-driver revision blank; the library
  binary and version record were archived locally rather than assigning a commit.

Each run contains original JPEGs, timestamps, checksums, requested settings,
driver status, register snapshots, and matching firmware artifacts. Device
first-DMA timestamps and host receipt UTC remain separate; synchronization
uncertainty and unobserved sensor frame counts are unknown.

## Verification and observations

The 12-run sweep covered AEC values 50, 100, 202, 400, 672 at gain index 0;
gain indices 0, 3, 7, 15, 30 at AEC 202; and beginning/end baselines at 202/0.
All 360 files were fully decoded using Pillow, checked for 800×600 dimensions,
and checked against their recorded SHA-256. Exposure readbacks matched every
request, and hardware automatic-control bits remained disabled. Gain-register
bytes were 0, 48, 112, 240, and 255 for the five requested gain steps.

Mean encoded grayscale values (0–255) increased from about 21 to 138 over the
shutter sweep, and from about 63 to 246 over the gain sweep. Representative
images were visually inspected: the ceiling is visible and high gain clips much
of the image. The beginning/end baseline means were 60.42 and 61.24; the separate
202/0 gain run averaged 63.26, showing some scene/lighting drift between runs.
These encoded values include the sensor's processing and JPEG output and are
not linear radiance or absolute sensitivity measurements. Disabled white balance
also leaves a visible color cast.

Timing, brightness calibration, outdoor daylight/twilight/night series, and
controlled motion remain open. USB backpressure and single-buffer acquisition
make delivered frame intervals unsuitable for deriving free-running sensor timing.
The board was left running the capture firmware at AEC 202 and gain index 0.

## Local archive and build checks

The archive is stored under the base checkout at
`.worktrees/artifacts/camera-reference/2026-09-14-indoor.tar.gz`, outside the task
worktree and its cleanable build directory. SHA-256:
`1cf00e15711065d765b62625e5527761c9556550fb3a206b0f330d11bce665c9`.
It includes the original captures, per-run firmware snapshots, driver provenance,
collector/firmware sources, and the local review JSON. This is a local retention
location, not a published dataset or off-machine backup.

The firmware compiled and uploaded successfully; six host transport/readback
tests passed. An additional `--warnings all` build succeeded without diagnostics
in the sketch, but reported missing-field-initializer warnings in the installed
ESP32 core's HAL and TinyUSB headers. The installed core was not modified.
No simulator rendering changes were made, so the graphical smoke check was not run.
