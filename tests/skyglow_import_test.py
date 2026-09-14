import importlib.util
import math
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('skyglow', Path(__file__).parents[1] / 'tools/fetch-skyglow.py')
skyglow = importlib.util.module_from_spec(spec)
spec.loader.exec_module(skyglow)


class AtlasImportTest(unittest.TestCase):
    def test_coordinates(self):
        self.assertEqual(skyglow.tile_location(32.8908277, -84.3271342), (20, 20, 80, 346))
        self.assertEqual(skyglow.tile_location(-65, -180), (1, 1, 0, 0))
        self.assertEqual(skyglow.tile_location(-65, 180), (1, 1, 0, 0))
        self.assertEqual(skyglow.tile_location(74.999999, 179.999999), (72, 28, 599, 599))
        self.assertEqual(skyglow.tile_location(30, -85), (20, 20, 0, 0))
        for lat, lon in [(75, 0), (-66, 0), (0, 181), (float('nan'), 0), (0, float('inf'))]:
            with self.assertRaises(ValueError):
                skyglow.tile_location(lat, lon)

    def test_signed_deltas_and_row_orientation(self):
        # Encoded origin=130, next row's origin=127, second column=129.
        raw = bytearray(360001)
        raw[0], raw[1], raw[601], raw[602] = 1, 2, 253, 2
        self.assertAlmostEqual(skyglow.decode_sample(raw, 0, 0), (5/195)*(math.exp(.0195*130)-1))
        self.assertAlmostEqual(skyglow.decode_sample(raw, 1, 1), (5/195)*(math.exp(.0195*129)-1))
        self.assertEqual(skyglow.decode_sample(bytes(360001), 599, 599), 0)
        with self.assertRaises(ValueError):
            skyglow.decode_sample(b'bad', 0, 0)


if __name__ == '__main__':
    unittest.main()
