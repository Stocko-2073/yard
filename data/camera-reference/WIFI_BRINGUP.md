# Wi-Fi capture verification — 2026-09-14

The connected XIAO ESP32-S3 Sense OV2640 was updated with the USB/Wi-Fi capture
firmware. The user attached the external antenna while power was disconnected,
then ran `configure_wifi.py` locally to provision the network over USB. The
Wi-Fi password was not shared with the agent or added to firmware/capture files.
The camera joined the existing network and was reachable through its advertised
`.local` hostname. The Python collector now uses either USB or authenticated TCP
with the same frame/metadata protocol; network setup is USB-only.

Hardware verification:

- Captured 30 SVGA JPEGs over Wi-Fi and checked all file hashes and full decoding.
- Rejected an invalid capture token before serving metadata or frames.
- Deliberately disconnected during a frame payload, then captured another 30
  frames over a fresh TCP connection without resetting the board.
- Captured three frames over USB with Wi-Fi enabled.
- Rebooted the board without changing flash and captured three more frames over
  Wi-Fi, confirming saved credentials and reconnection without reprovisioning.

All 66 retained JPEGs decoded at 800×600, matched their SHA-256 records, and passed
manual AEC/automatic-control readback checks at AEC 202 and gain index 0. These
were indoor transport checks, not additional outdoor calibration references.
No Wi-Fi range, power-bank runtime, or continuous sensor frame rate was measured.
Single-buffer acquisition still experiences transport backpressure.

The firmware compiled and its uploaded flash hashes verified. Twelve host tests
passed, covering binary framing, disconnects, authentication, credential escaping
and byte limits, private token creation/reuse, and sensor readback. The additional
`--warnings all` build completed with diagnostics confined to installed ESP32
HAL/TinyUSB headers, with none in the sketch. The simulator was not changed and
its graphical smoke check was not run.

Original images and matching firmware snapshots remain local. The capture token
is stored separately with owner-only permissions, never inside the capture
archive. Setup and collection commands are in the
[capture utility README](../../tools/camera-reference/README.md).

The local archive is outside the task worktree at
`.worktrees/artifacts/camera-reference/2026-09-14-wifi.tar.gz` in the base checkout.
SHA-256: `14c6ec93bedc601cf3413b0f300a2edd56ee6012754d7baf35f2a8103a725172`.
It is a local retention copy, not a published dataset or off-machine backup.
