# USB and Wi-Fi hardware camera capture

This is a separate Arduino sketch and Python collector, not part of the Yard
renderer. It supports USB and Wi-Fi collection without an SD card or third-party
Python packages. Wi-Fi credentials are provisioned over USB and stored on the
board, separately from the firmware.
The collector runs on macOS/Linux with Python 3. The initial supported hardware
is XIAO ESP32-S3 Sense **with OV2640**. The firmware reports other sensor IDs but
refuses manual capture with an unsupported sensor.

## Build, flash, and collect

The verified compile configuration is Arduino CLI with `esp32:esp32` core
**3.3.10**, FQBN `esp32:esp32:XIAO_ESP32S3:PSRAM=opi`. Install that core explicitly
if absent; ordinary simulator builds do not install or build firmware. The pin
mapping matches the XIAO entry in that core's CameraWebServer example.

From the task worktree root:

```sh
arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32S3:PSRAM=opi \
  --build-path build/camera-firmware-wifi tools/camera-reference/yard_capture
arduino-cli board list
# Upload overwrites the connected board's firmware; select its actual port.
arduino-cli upload --port /dev/cu.usbmodem2101 \
  --fqbn esp32:esp32:XIAO_ESP32S3:PSRAM=opi \
  --input-dir build/camera-firmware-wifi tools/camera-reference/yard_capture
python3 tools/camera-reference/capture.py --port /dev/cu.usbmodem2101 \
  --build-dir build/camera-firmware-wifi --output captures/indoor-aec202-gain0 \
  --aec 202 --gain-index 0 --frames 30 \
  --scene 'Indoor desk, stationary camera pointing at ceiling; bring-up only'
```

If automatic upload cannot contact the bootloader, hold BOOT, tap RESET, release
BOOT, then retry. Recheck the port after USB re-enumeration. Close serial monitors
before uploading or collecting. If the board remains in download mode after a
successful flash, tap RESET without BOOT. See
[Espressif's boot mode instructions](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/advanced-topics/boot-mode-selection.html).

## Configure Wi-Fi once over USB

Attach the board's external antenna with power disconnected, then reconnect USB.
Run the setup script in a terminal; it prompts for the SSID and a hidden password:

```sh
python3 tools/camera-reference/configure_wifi.py --port /dev/cu.usbmodem2101
```

Use a **2.4 GHz** network. The script supports personal Wi-Fi passwords (8–63
UTF-8 bytes), or an empty password for an open network; enterprise authentication
and captive portals are not implemented. The SSID is limited to 32 UTF-8 bytes.
The script reports the camera's DHCP address and its unique
`yard-camera-XXXXXX.local` hostname. If mDNS does not resolve on your network,
use the reported IP address. Credentials remain on the board across resets and
ordinary firmware uploads. On startup it tries to reconnect without needing a
USB host; a USB power bank can supply power outdoors.

The script generates a separate capture token in ignored `local/camera-token.txt`
with owner-only permissions, or reuses that file if it exists. The Wi-Fi password
is not written to disk or embedded in the firmware. Keep the token for collection
and out of image archives; preserve it outside the task worktree before removing
that worktree. `--token-file PATH` selects another token location.
Network provisioning is accepted only over USB. To check or erase saved settings:

```sh
python3 tools/camera-reference/configure_wifi.py --port /dev/cu.usbmodem2101 --status
python3 tools/camera-reference/configure_wifi.py --port /dev/cu.usbmodem2101 --clear
```

A failed join leaves settings saved so the board can retry when coverage returns.
The setup script exits unsuccessfully after 30 seconds without a connection;
rerun it to correct credentials. USB capture remains available without Wi-Fi.
The implementation uses the installed core's
[Wi-Fi station API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/wifi.html).

## Collect over Wi-Fi

Keep the computer on a network that can reach the camera, then substitute the
address reported by setup:

```sh
python3 tools/camera-reference/capture.py --host CAMERA_ADDRESS \
  --token-file local/camera-token.txt --build-dir build/camera-firmware-wifi \
  --output captures/outdoor-daylight-aec202-gain0 \
  --aec 202 --gain-index 0 --frames 30 \
  --scene 'Describe the actual scene, lighting, and motion here'
```

The TCP server uses port 4765; `--tcp-port` can override the collector destination.
It accepts one active client and requires the token before exposing capture or
metadata. This is a local-network protocol without TLS; use it on a trusted
network and do not expose the port to the Internet. Guest-network client isolation
can prevent the laptop from reaching the camera even if both joined successfully.
A broken TCP connection aborts the current capture and releases the frame buffer;
a fresh connection starts cleanly and can retry into a new output directory.

Use a new output directory for every run. The collector rejects overwrites, saves
original JPEG bytes and SHA-256 hashes, and writes `capture.json` even when a
capture fails. It verifies the running app's ELF digest against the provided
build, and copies the ELF, binary, build options, and generated sketch source to
`firmware/` in the capture directory. Retain these artifacts when archiving a set.
Also save the installed core version and library bundle's `versions.txt`; the
3.3.10 bundle does not identify a separate esp32-camera commit, so do not invent
one. For stronger driver provenance, hash/archive the installed
`libespressif__esp32-camera.a` alongside that version record.

`captures/` is ignored by Git and survives `make clean`. Archive real sessions
outside the worktree before removing it. Do not push private indoor images as
part of the PR. The [collection protocol](../../design/CAMERA_REFERENCE.md)
defines the eventual outdoor reference set and its review requirements. The
collector's per-run records supplement the human session template; they do not
fill in scene measurements or establish that the collection matrix is complete.

## What is recorded and what is unmeasured

Configuration: 800×600 JPEG, quality setting 10, configured XCLK 20 MHz, one frame
buffer in PSRAM, `CAMERA_GRAB_WHEN_EMPTY`. Manual capture disables AEC, AEC2, AGC,
AWB, and AWB gain, checks setter return codes, and requests the specified AEC
value and gain index. Other processing settings use driver initialization defaults
and are recorded as the **driver status cache**, which is not full hardware
readback. Disabling white balance is a repeatable initial configuration, not a
color-calibrated output pipeline.

Eight frames are discarded after each setting change. This drains the single
buffer and provides a provisional settling interval; it is not a measured sensor
settling guarantee. Inspect image stability before using a series for calibration.
The register snapshot uses the driver's `get_reg` with bank in the high byte and
address in the low byte. It includes timing, exposure, gain, and DSP control
registers; it is not a full dump of indirect DSP tables. Negative reads fail the
run. The collector separately decodes AEC and automatic-control bits, rejecting
exposure readback mismatches or enabled automatic controls before saving frames. The conventions follow
[Espressif's OV2640 driver](https://github.com/espressif/esp32-camera/blob/master/sensors/ov2640.c);
requested values and register bytes remain separately available for analysis.

Frame timestamps are the camera driver's first DMA-buffer timestamp in
microseconds since boot. UTC is host receipt time, with unknown synchronization
uncertainty. Sequence numbers count delivered frames within the run, not sensor
VSYNCs. A single buffer and image transfer impose backpressure: this is **not a
30 fps continuous recording**, and unobserved sensor frames are not counted.
Do not infer line periods, shutter milliseconds, or free-running frame rate from
these intervals. Future motion/timing collection needs a capture path with
measured cadence. This utility currently supports bring-up and static sweeps.

Over TCP, send `AUTH token` first and expect `authenticated` JSON; USB requires
no authentication. TCP clients idle for ten seconds between commands are closed.

The capture protocol is one ASCII command (`INFO` or `CAP aec gain_index count`) followed
by newline. Replies are newline-delimited JSON. A `frame` record is immediately
followed by exactly `bytes` binary JPEG bytes; the next JSON record follows with
no separator. `done` ends a successful run; `error` aborts it. On interrupted
USB transfer, reset the board before reconnecting to avoid parsing leftover payload
as JSON. TCP retries use a new connection and need no board reset. Accepted ranges are AEC 0–1200, gain index 0–30, and 1–300 frames.
New captures use `yard-camera-capture-v1` metadata with an explicit `transport`
field (`usb` or `tcp`); older USB-only runs retain `yard-usb-capture-v1`.
JPEG markers and reported dimensions are checked during collection; decode the
original files separately to establish actual dimensions and image integrity.

Run collector transport tests without hardware:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover \
  -s tools/camera-reference/tests -v
```
