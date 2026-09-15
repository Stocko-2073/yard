import json
from pathlib import Path
import socket
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).parents[1]))
import capture
import configure_wifi


class WiFiTest(unittest.TestCase):
    def test_tcp_auth_and_capture_protocol(self):
        client, peer = socket.socketpair()
        self.addCleanup(peer.close)
        peer.sendall(b'{"type":"authenticated"}\n{"type":"info"}\n')
        with mock.patch.object(capture.socket, 'create_connection', return_value=client):
            with capture.connect_tcp('camera.local', 4765, 'a' * 64) as stream:
                self.assertEqual(peer.recv(100), b'AUTH ' + b'a' * 64 + b'\n')
                stream.command('INFO')
                self.assertEqual(peer.recv(100), b'INFO\n')
                self.assertEqual(stream.record()['type'], 'info')
        self.assertEqual(client.fileno(), -1)

    def test_tcp_auth_failure_closes_connection(self):
        client, peer = socket.socketpair()
        self.addCleanup(peer.close)
        peer.sendall(b'{"type":"error","message":"authentication failed"}\n')
        with mock.patch.object(capture.socket, 'create_connection', return_value=client):
            with self.assertRaisesRegex(RuntimeError, 'authentication failed'):
                with capture.connect_tcp('camera.local', 4765, 'a' * 64):
                    self.fail('authentication should fail')
        self.assertEqual(client.fileno(), -1)

    def test_tcp_disconnect_mid_frame(self):
        client, peer = socket.socketpair()
        self.addCleanup(client.close)
        peer.sendall(b'\xff\xd8partial')
        peer.close()
        with self.assertRaises(EOFError):
            capture.TCPStream(client).read(100)

    def test_setup_escapes_credentials_and_checks_ack(self):
        stream = mock.Mock()
        stream.record.return_value = {'type': 'wifi_saved'}
        ssid, password = 'Wi-Fi "yard"', 'a"b\\cdef'
        configure_wifi.configure(stream, ssid, password, 'a' * 64)
        command = stream.command.call_args.args[0]
        self.assertTrue(command.startswith('WIFI_CONFIG '))
        self.assertEqual(json.loads(command[12:]), dict(ssid=ssid, password=password, token='a' * 64))
        stream.record.return_value = {'type': 'info'}
        with self.assertRaisesRegex(ValueError, 'acknowledge'):
            configure_wifi.configure(stream, ssid, password, 'a' * 64)

    def test_utf8_byte_limits_and_nul(self):
        for ssid, password in [('', 'password'), ('é' * 17, 'password'),
                               ('ssid', 'short'), ('ssid', 'é' * 32), ('a\0b', 'password')]:
            with self.assertRaises(ValueError):
                configure_wifi.validate_credentials(ssid, password)
        configure_wifi.validate_credentials('é' * 16, 'password')
        configure_wifi.validate_credentials('Open network', '')

    def test_token_is_private_and_reused(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'token'
            token = configure_wifi.token_for(path)
            self.assertEqual(len(token), 64)
            self.assertEqual(path.stat().st_mode & 0o777, 0o600)
            self.assertEqual(configure_wifi.token_for(path), token)
            path.write_text('not a valid token')
            with self.assertRaises(ValueError):
                configure_wifi.token_for(path)


if __name__ == '__main__':
    unittest.main()
