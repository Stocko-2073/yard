#!/usr/bin/env python3
"""Collect Yard USB/Wi-Fi reference firmware frames on macOS/Linux (stdlib only)."""
import argparse
import contextlib
import datetime
import hashlib
import json
import os
from pathlib import Path
import select
import shutil
import socket
import termios
import time
import tty


def utc():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


class Stream:
    def __init__(self, fd):
        self.fd = fd
        self.buffer = bytearray()

    def read(self, size):
        deadline = time.monotonic() + 20
        while len(self.buffer) < size:
            remaining = deadline - time.monotonic()
            if remaining <= 0 or not select.select([self.fd], [], [], remaining)[0]:
                raise TimeoutError('capture read timed out; reconnect (reset the board for USB) before retrying')
            chunk = os.read(self.fd, 65536)
            if not chunk:
                raise EOFError('capture connection closed')
            self.buffer.extend(chunk)
        data = bytes(self.buffer[:size])
        del self.buffer[:size]
        return data

    def record(self):
        line = bytearray()
        while len(line) < 16384:
            c = self.read(1)
            if c == b'\n':
                result = json.loads(line)
                if result.get('type') == 'error':
                    raise RuntimeError(result['message'])
                return result
            line.extend(c)
        raise ValueError('oversized protocol record')

    def command(self, text):
        data = (text + '\n').encode('ascii')
        while data:
            count = os.write(self.fd, data)
            if not count:
                raise EOFError('capture connection write failed')
            data = data[count:]


@contextlib.contextmanager
def connect(port):
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    old = None
    try:
        old = termios.tcgetattr(fd)
        tty.setraw(fd)
        attrs = termios.tcgetattr(fd)
        attrs[4] = attrs[5] = termios.B115200
        attrs[2] |= termios.CLOCAL | termios.CREAD
        termios.tcsetattr(fd, termios.TCSANOW, attrs)
        # Opening USB can reset some boards; clear boot output before INFO.
        time.sleep(2)
        termios.tcflush(fd, termios.TCIOFLUSH)
        yield Stream(fd)
    finally:
        if old is not None:
            try:
                termios.tcsetattr(fd, termios.TCSANOW, old)
            except termios.error:
                pass
        os.close(fd)


class TCPStream(Stream):
    def __init__(self, connection):
        super().__init__(connection.fileno())
        self.connection = connection

    def command(self, text):
        self.connection.sendall((text + '\n').encode('ascii'))


def load_token(path):
    token = path.read_text().strip()
    if len(token) != 64 or any(c not in '0123456789abcdefABCDEF' for c in token):
        raise ValueError('capture token must be 64 hexadecimal characters')
    return token


@contextlib.contextmanager
def connect_tcp(host, port, token):
    with socket.create_connection((host, port), timeout=20) as connection:
        stream = TCPStream(connection)
        stream.command('AUTH ' + token)
        if stream.record().get('type') != 'authenticated':
            raise ValueError('camera authentication was not accepted')
        yield stream


def readback(settings):
    """Decode selected OV2640 registers, independently of the driver cache."""
    regs = settings['registers_bank_address_hex']
    for key in ('104', '110', '145', '100', '113', '0c2', '0c3'):
        if not isinstance(regs[key], int) or not 0 <= regs[key] <= 255:
            raise ValueError('invalid hardware register readback')
    return dict(aec_value=((regs['145'] & 0x3f) << 10) | (regs['110'] << 2) | (regs['104'] & 3),
                gain_register=regs['100'], automatic_exposure=bool(regs['113'] & 1),
                automatic_gain=bool(regs['113'] & 4),
                secondary_exposure_control=not bool(regs['0c2'] & 0x40),
                automatic_white_balance=bool(regs['0c3'] & 8),
                white_balance_gain=bool(regs['0c3'] & 4))


def collect(stream, output, aec, gain, count, manifest):
    stream.command(f'CAP {aec} {gain} {count}')
    settings = stream.record()
    if settings.get('type') != 'settings':
        raise ValueError(f'expected settings, got {settings}')
    if (settings.get('requested_aec'), settings.get('requested_gain_index')) != (aec, gain):
        raise ValueError('settings do not match request')
    manifest['settings'] = settings
    hardware = manifest['hardware_readback'] = readback(settings)
    if hardware['aec_value'] != aec or any(hardware[k] for k in (
            'automatic_exposure', 'automatic_gain', 'secondary_exposure_control',
            'automatic_white_balance', 'white_balance_gain')):
        raise ValueError('manual exposure/processing readback does not match request')
    for sequence in range(count):
        frame = stream.record()
        if frame.get('type') != 'frame' or frame.get('sequence') != sequence:
            raise ValueError('unexpected frame sequence')
        if (frame.get('width'), frame.get('height')) != (800, 600):
            raise ValueError('expected SVGA frame')
        size = frame.get('bytes')
        if not isinstance(size, int) or not 4 <= size <= 2_000_000:
            raise ValueError('invalid frame length')
        data = stream.read(size)
        if not data.startswith(b'\xff\xd8') or not data.endswith(b'\xff\xd9'):
            # Some drivers can include padding after EOI; fail for review rather
            # than trimming the original camera buffer silently.
            raise ValueError('invalid JPEG framing')
        name = f'{sequence:04d}.jpg'
        (output / name).write_bytes(data)
        frame.update(file=name, sha256=hashlib.sha256(data).hexdigest(), host_received_utc=utc())
        manifest['frames'].append(frame)
    if stream.record().get('type') != 'done':
        raise ValueError('missing completion record')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    transport = parser.add_mutually_exclusive_group(required=True)
    transport.add_argument('--port', help='USB serial device')
    transport.add_argument('--host', help='camera IP address or .local hostname')
    parser.add_argument('--tcp-port', type=int, default=4765)
    parser.add_argument('--token-file', type=Path, help='capture token saved by configure_wifi.py')
    parser.add_argument('--output', type=Path, required=True, help='new directory; never overwritten')
    parser.add_argument('--aec', type=int, default=202)
    parser.add_argument('--gain-index', type=int, default=0)
    parser.add_argument('--frames', type=int, default=30)
    parser.add_argument('--scene', required=True, help='actual lighting/setup/motion description')
    parser.add_argument('--build-dir', type=Path, required=True, help='Arduino build directory for firmware provenance')
    args = parser.parse_args()
    if not (0 <= args.aec <= 1200 and 0 <= args.gain_index <= 30 and 1 <= args.frames <= 300):
        parser.error('AEC 0..1200, gain index 0..30, frames 1..300 required')
    if not 1 <= args.tcp_port <= 65535:
        parser.error('TCP port must be 1..65535')
    if bool(args.host) != bool(args.token_file):
        parser.error('--host requires --token-file; tokens are not used with --port')
    token = load_token(args.token_file) if args.host else None
    elf = args.build_dir / 'yard_capture.ino.elf'
    binary = args.build_dir / 'yard_capture.ino.bin'
    provenance = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in (elf, binary)}
    args.output.mkdir(parents=True, exist_ok=False)
    artifacts = args.output / 'firmware'
    artifacts.mkdir()
    for artifact in (elf, binary, args.build_dir / 'build.options.json',
                     args.build_dir / 'sketch' / 'yard_capture.ino.cpp'):
        shutil.copy2(artifact, artifacts / artifact.name)
    manifest = dict(schema='yard-camera-capture-v1', status='incomplete', utc_start=utc(),
                    scene=args.scene, firmware_artifact_sha256=provenance,
                    transport='tcp' if args.host else 'usb',
                    timestamp_basis='driver first DMA buffer, microseconds since boot; UTC is host receipt only',
                    utc_sync_uncertainty_ms=None, sensor_dropped_frames=None,
                    cadence='single buffer, transport backpressure; not a continuous sensor-rate recording',
                    frames=[])
    try:
        connection = connect_tcp(args.host, args.tcp_port, token) if args.host else connect(args.port)
        with connection as stream:
            stream.command('INFO')
            info = stream.record()
            manifest['device'] = info
            if info.get('type') != 'info' or info.get('protocol') != 1 or info.get('camera_error') != 0:
                raise ValueError(f'camera not ready: {info}')
            if info.get('app_elf_sha256') != provenance[elf.name]:
                raise ValueError('running firmware does not match supplied build ELF')
            if info.get('sensor_pid') != 0x26:
                raise ValueError(f'OV2640 required, detected PID {info.get("sensor_pid")}')
            collect(stream, args.output, args.aec, args.gain_index, args.frames, manifest)
            manifest['status'] = 'captured-unreviewed'
    except Exception as exc:
        manifest['error'] = str(exc)
        raise
    finally:
        manifest['utc_end'] = utc()
        (args.output / 'capture.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'Saved {len(manifest["frames"])} original SVGA JPEGs to {args.output}')


if __name__ == '__main__':
    main()
