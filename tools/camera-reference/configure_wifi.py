#!/usr/bin/env python3
"""Provision camera Wi-Fi over USB without putting credentials in the firmware."""
import argparse
import getpass
import json
import os
from pathlib import Path
import secrets
import time
import warnings

from capture import connect, load_token

DEFAULT_TOKEN = Path(__file__).resolve().parents[2] / 'local' / 'camera-token.txt'


def validate_credentials(ssid, password):
    if '\0' in ssid or not 1 <= len(ssid.encode('utf-8')) <= 32:
        raise ValueError('SSID must be 1..32 UTF-8 bytes, without NUL')
    password_bytes = len(password.encode('utf-8'))
    if '\0' in password or not (password_bytes == 0 or 8 <= password_bytes <= 63):
        raise ValueError('Password must be 8..63 UTF-8 bytes, or empty for an open network')


def token_for(path):
    if path.exists():
        return load_token(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    token = secrets.token_hex(32)
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, 'w') as output:
        output.write(token + '\n')
    return token


def configure(stream, ssid, password, token):
    validate_credentials(ssid, password)
    payload = json.dumps(dict(ssid=ssid, password=password, token=token), separators=(',', ':'))
    if len(('WIFI_CONFIG ' + payload).encode('ascii')) > 1024:
        raise ValueError('Encoded configuration exceeds the firmware command limit')
    stream.command('WIFI_CONFIG ' + payload)
    if stream.record().get('type') != 'wifi_saved':
        raise ValueError('camera did not acknowledge saved configuration')


def status(stream):
    stream.command('WIFI_STATUS')
    result = stream.record()
    if result.get('type') != 'wifi_status':
        raise ValueError('expected Wi-Fi status; flash the Wi-Fi firmware first')
    return result


def show_status(result):
    print('Wi-Fi connected.' if result['connected'] else 'Wi-Fi not connected.')
    if result['connected']:
        print(f"Camera: {result['hostname']}.local ({result['ip']}), TCP port {result['port']}")
    elif result['configured']:
        print('Settings are saved; the board will keep trying to connect.')
    else:
        print('No Wi-Fi configuration is saved.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True, help='USB serial device, e.g. /dev/cu.usbmodem2101')
    parser.add_argument('--ssid', help='prompted if omitted')
    parser.add_argument('--token-file', type=Path, default=DEFAULT_TOKEN)
    action = parser.add_mutually_exclusive_group()
    action.add_argument('--status', action='store_true', help='show connection status without changing settings')
    action.add_argument('--clear', action='store_true', help='erase saved camera Wi-Fi settings and disable Wi-Fi')
    args = parser.parse_args()
    if args.status or args.clear:
        with connect(args.port) as stream:
            if args.clear:
                stream.command('WIFI_CLEAR')
                result = stream.record()
                if result.get('type') != 'wifi_status':
                    raise ValueError('camera did not acknowledge clearing Wi-Fi')
            else:
                result = status(stream)
            show_status(result)
        return
    ssid = args.ssid if args.ssid is not None else input('Wi-Fi network name (2.4 GHz): ')
    with warnings.catch_warnings():
        warnings.simplefilter('error', getpass.GetPassWarning)
        password = getpass.getpass('Wi-Fi password (hidden; empty for open network): ')
    validate_credentials(ssid, password)
    token = token_for(args.token_file)
    with connect(args.port) as stream:
        configure(stream, ssid, password, token)
        print('Wi-Fi settings saved on the camera. Waiting for connection...')
        deadline = time.monotonic() + 30
        while True:
            result = status(stream)
            if result['connected'] or time.monotonic() >= deadline:
                break
            time.sleep(1)
        show_status(result)
    print(f'Capture token file: {args.token_file.resolve()}')
    print('The Wi-Fi password was not saved on this computer.')
    if not result['connected']:
        print('Check the SSID/password and 2.4 GHz coverage; rerun this script to change them.')
        raise SystemExit(1)


if __name__ == '__main__':
    try:
        main()
    except (EOFError, KeyboardInterrupt):
        raise SystemExit('\nSetup cancelled.')
    except getpass.GetPassWarning:
        raise SystemExit('Run setup in an interactive terminal that supports hidden password input.')
    except (OSError, ValueError, RuntimeError) as exc:
        raise SystemExit(f'Wi-Fi setup failed: {exc}')
