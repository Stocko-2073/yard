# OV2640 reference capture

An Arduino sketch and a Python collector for capturing original frames from a
XIAO ESP32-S3 Sense **with OV2640**, over USB or Wi-Fi. They are separate from the
Yard renderer and need no SD card or third-party Python packages. The collector
runs on macOS or Linux with Python 3. The firmware reports other sensor IDs but
refuses to capture with them.

## Build, flash, and collect over USB

Build with Arduino CLI and the `esp32:esp32` core **3.3.10**, FQBN
`esp32:esp32:XIAO_ESP32S3:PSRAM=opi`; the pin mapping is the XIAO entry from that
core's CameraWebServer example. Ordinary Yard builds never build firmware. From
the repository root:

```sh
arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32S3:PSRAM=opi \
  --build-path build/camera-firmware tools/camera-reference/yard_capture
arduino-cli board list
# Upload overwrites the connected board's firmware; select its actual port.
arduino-cli upload --port /dev/cu.usbmodem2101 \
  --fqbn esp32:esp32:XIAO_ESP32S3:PSRAM=opi \
  --input-dir build/camera-firmware tools/camera-reference/yard_capture
python3 tools/camera-reference/capture.py --port /dev/cu.usbmodem2101 \
  --build-dir build/camera-firmware --output captures/indoor-aec202-gain0 \
  --aec 202 --gain-index 0 --frames 30 \
  --scene 'Describe the actual scene, lighting, and motion here'
```

If the upload cannot reach the bootloader, hold BOOT, tap RESET, release BOOT and
retry, rechecking the port after USB re-enumeration. Close serial monitors before
uploading or collecting. If the board stays in download mode after flashing, tap
RESET alone. See [Espressif's boot mode instructions](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/advanced-topics/boot-mode-selection.html).

## Configure Wi-Fi over USB

Attach the external antenna with power disconnected, reconnect USB, then run the
setup script in a terminal. It prompts for the SSID and a hidden password:

```sh
python3 tools/camera-reference/configure_wifi.py --port /dev/cu.usbmodem2101
```

Use a **2.4 GHz** network with a personal password (8–63 UTF-8 bytes) or none;
enterprise authentication and captive portals are unsupported. The script reports
the camera's address and its `yard-camera-XXXXXX.local` hostname; use the address
if mDNS does not resolve. Settings survive resets and firmware uploads, and the
board reconnects on its own, so a USB power bank is enough outdoors. A failed join
keeps the settings for later retries; the script gives up after 30 seconds, and
rerunning it replaces the credentials. Provisioning is accepted only over USB.

The Wi-Fi password is never written to disk or embedded in firmware. The script
creates a capture token in ignored `local/camera-token.txt` with owner-only
permissions, or reuses it; `--token-file PATH` chooses another file. Keep the
token out of image archives. To check or erase the saved settings:

```sh
python3 tools/camera-reference/configure_wifi.py --port /dev/cu.usbmodem2101 --status
python3 tools/camera-reference/configure_wifi.py --port /dev/cu.usbmodem2101 --clear
```

The firmware uses the core's [Wi-Fi station API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/wifi.html).

## Collect over Wi-Fi

From a network that can reach the camera:

```sh
python3 tools/camera-reference/capture.py --host CAMERA_ADDRESS \
  --token-file local/camera-token.txt --build-dir build/camera-firmware \
  --output captures/outdoor-daylight-aec202-gain0 \
  --aec 202 --gain-index 0 --frames 30 \
  --scene 'Describe the actual scene, lighting, and motion here'
```

The camera listens on TCP port 4765 (`--tcp-port` changes the collector's
destination) and serves one client, which must present the token first. There is
no TLS: use a trusted network and never expose the port to the Internet. Guest
networks that isolate clients can block the connection. A broken connection
aborts the capture and frees the frame buffer; retry into a new directory.

## Output

Each run needs a new output directory; the collector refuses to overwrite. It
saves the original JPEG bytes with their SHA-256 and writes `capture.json`, even
when the capture fails. It checks the running firmware's ELF digest against
`--build-dir` and copies the ELF, binary, build options and generated sketch
source into `firmware/`. The core's library bundle does not identify an
esp32-camera commit, so keep the core version, its `versions.txt` and a hash of
`libespressif__esp32-camera.a` with archived sessions instead. `captures/` is
ignored by Git and survives `make clean`.

Captures use 800×600 JPEG at quality 10, a 20 MHz XCLK, one PSRAM frame buffer
and `CAMERA_GRAB_WHEN_EMPTY`. AEC, AEC2, AGC, AWB and AWB gain are disabled before
the requested AEC value and gain index are set; other processing keeps the
driver's defaults. The recorded `driver_status_cache` is the driver's cache, not
hardware readback. Disabled white balance leaves a color cast.

After each settings change, eight frames are discarded to drain the buffer; this
is not a measured settling time. The register snapshot covers timing, exposure,
gain and DSP control registers (bank in the high byte, address in the low byte),
not the indirect DSP tables. The collector rejects a run whose exposure readback
differs from the request or whose automatic controls are enabled.

Frame timestamps are the driver's first-DMA time in microseconds since boot;
UTC is host receipt time, with unknown synchronization error. Sequence numbers
count delivered frames, not sensor frames. The single buffer and the transfer
apply backpressure, so a run is not a continuous 30 fps recording: don't infer
line periods, shutter times or frame rate from its intervals.

## Protocol

Over TCP, send `AUTH token` first and expect an `authenticated` record; USB needs
no authentication. TCP clients idle for ten seconds are disconnected.

Commands are one ASCII line: `INFO`, or `CAP aec gain_index count` with AEC
0–1200, gain index 0–30 and 1–300 frames. USB also accepts `WIFI_STATUS`,
`WIFI_CLEAR` and `WIFI_CONFIG json`. Replies are newline-delimited JSON. A
`frame` record is followed immediately by exactly `bytes` bytes of JPEG, then the
next record with no separator. `done` ends a run and `error` aborts it. After an
interrupted USB transfer, reset the board before reconnecting so leftover payload
is not parsed as JSON; TCP retries just reconnect. Capture metadata uses the
`yard-camera-capture-v1` schema with a `transport` of `usb` or `tcp`. The
collector checks JPEG markers and reported dimensions only; decode the files to
confirm image integrity.
