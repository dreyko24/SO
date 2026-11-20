import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


CONVERTER = Path(__file__).resolve().parents[1] / 'tools' / 'xwd2png.py'


class ExpectedColorTest(unittest.TestCase):
    def convert(self, pixels):
        width, height = 64, 32
        name = b'test\0'
        header = struct.pack('>25I', 100 + len(name), 7, 2, 24, width, height,
                             0, 0, 32, 0, 32, 32, width * 4, 4,
                             0xff0000, 0x00ff00, 0x0000ff, 8, 256, 0,
                             width, height, 0, 0, 0)
        with tempfile.TemporaryDirectory() as directory:
            src = Path(directory) / 'screen.xwd'
            dst = Path(directory) / 'screen.png'
            src.write_bytes(header + name + pixels)
            result = subprocess.run([sys.executable, str(CONVERTER), str(src), str(dst),
                                     '--expect-rgb', '26a69a'],
                                    capture_output=True, text=True)
            self.assertTrue(dst.read_bytes().startswith(b'\x89PNG\r\n\x1a\n'))
        return result

    def test_rejects_visible_frame_without_page_color(self):
        result = self.convert(b'\xff\xff\xff\x00' * 2048)
        self.assertNotEqual(result.returncode, 0)

    def test_accepts_frame_with_page_color(self):
        result = self.convert(b'\xff\xff\xff\x00' * 1536 + b'\x9a\xa6\x26\x00' * 512)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_rejects_a_single_pixel_of_page_color(self):
        result = self.convert(b'\xff\xff\xff\x00' * 2047 + b'\x9a\xa6\x26\x00')
        self.assertNotEqual(result.returncode, 0)


if __name__ == '__main__':
    unittest.main()
