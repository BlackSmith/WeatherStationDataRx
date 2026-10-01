from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
COMPONENT = ROOT / "components" / "weather_station"


class ProtocolTest(unittest.TestCase):
    """Replay protocol frames through the production ISR, decoder and sensors."""

    @classmethod
    def setUpClass(cls):
        compiler = shutil.which("g++")
        if compiler is None:
            raise RuntimeError("g++ is required for the protocol tests")
        cls.build = tempfile.TemporaryDirectory(prefix="weather-station-test-")
        cls.addClassCleanup(cls.build.cleanup)
        cls.executable = Path(cls.build.name) / "protocol_test"
        subprocess.run(
            [
                compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                "-I", str(ROOT / "tests" / "stubs"), "-I", str(COMPONENT),
                str(ROOT / "tests" / "protocol_test.cpp"),
                str(COMPONENT / "rf_receiver.cpp"),
                str(COMPONENT / "weather_station.cpp"),
                "-o", str(cls.executable),
            ],
            check=True,
            capture_output=True,
            text=True,
        )

    def test_protocol_and_publication(self):
        for case in (
            "temperature", "wind", "rain", "battery", "rejection",
            "ignore_repeats", "confirm_once", "confirm_twice", "pass_repeats",
            "station_filter", "scan",
        ):
            with self.subTest(case=case):
                result = subprocess.run(
                    [str(self.executable), case], capture_output=True, text=True,
                    timeout=10,
                )
                self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
