import importlib.util
import json
import os
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('capture', Path(__file__).parents[1] / 'capture.py')
capture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(capture)


REGS = {'104': 2, '110': 50, '145': 0, '100': 0, '113': 0xe0, '0c2': 0x40, '0c3': 0}


class CaptureTest(unittest.TestCase):
    def stream(self, data):
        read, write = os.pipe()
        os.write(write, data)
        os.close(write)
        self.addCleanup(os.close, read)
        return capture.Stream(read)

    def test_binary_does_not_consume_next_record(self):
        stream = self.stream(b'{"type":"frame"}\n\xff\xd8\n\x00\xff\xd9{"type":"done"}\n')
        self.assertEqual(stream.record()['type'], 'frame')
        self.assertEqual(stream.read(6), b'\xff\xd8\n\x00\xff\xd9')
        self.assertEqual(stream.record()['type'], 'done')

    def test_truncated_payload(self):
        with self.assertRaises(EOFError):
            self.stream(b'ab').read(3)

    def test_device_error(self):
        with self.assertRaisesRegex(RuntimeError, 'sensor failed'):
            self.stream(b'{"type":"error","message":"sensor failed"}\n').record()

    def test_collect_preserves_bytes_and_checksum(self):
        jpeg = b'\xff\xd8\x00\n\xff\xd9'
        records = [dict(type='settings', requested_aec=202, requested_gain_index=0, registers_bank_address_hex=REGS),
                   dict(type='frame', sequence=0, bytes=len(jpeg), width=800, height=600)]
        data = b''.join(json.dumps(r).encode() + b'\n' for r in records)
        stream = self.stream(data + jpeg + b'{"type":"done"}\n')
        stream.command = lambda text: self.assertEqual(text, 'CAP 202 0 1')
        with tempfile.TemporaryDirectory() as directory:
            manifest = {'frames': []}
            capture.collect(stream, Path(directory), 202, 0, 1, manifest)
            self.assertEqual((Path(directory) / '0000.jpg').read_bytes(), jpeg)
            self.assertEqual(manifest['frames'][0]['sha256'], capture.hashlib.sha256(jpeg).hexdigest())

    def test_readback_high_exposure_bits_and_auto_flags(self):
        regs = dict(REGS, **{'104': 0, '110': 44, '145': 1, '113': 5, '0c2': 0, '0c3': 12})
        result = capture.readback({'registers_bank_address_hex': regs})
        self.assertEqual(result['aec_value'], 1200)
        for key in ('automatic_exposure', 'automatic_gain', 'secondary_exposure_control',
                    'automatic_white_balance', 'white_balance_gain'):
            self.assertTrue(result[key])

    def test_bad_frame_sequence(self):
        records = [dict(type='settings', requested_aec=202, requested_gain_index=0, registers_bank_address_hex=REGS),
                   dict(type='frame', sequence=1, bytes=6, width=800, height=600)]
        stream = self.stream(b''.join(json.dumps(r).encode() + b'\n' for r in records))
        stream.command = lambda text: None
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, 'sequence'):
                capture.collect(stream, Path(directory), 202, 0, 1, {'frames': []})
            self.assertFalse(list(Path(directory).iterdir()))


if __name__ == '__main__':
    unittest.main()
