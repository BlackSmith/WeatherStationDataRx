import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


COMPONENTS = Path(__file__).resolve().parents[1] / "components"
HAS_ESPHOME = importlib.util.find_spec("esphome") is not None


@unittest.skipUnless(HAS_ESPHOME, "Install ESPHome to run configuration validation tests")
class ESPHomeConfigTest(unittest.TestCase):
    def validate(self, *, wind_id=234, rain_id=204, framework="arduino", second_receiver=False, quiet=True):
        config = f"""esphome:
  name: weather-station-test
esp32:
  board: nodemcu-32s
  framework:
    type: {framework}
external_components:
  - source:
      type: local
      path: {COMPONENTS}
    components: [weather_station]
sensor:
  - platform: weather_station
    pin: GPIO27
    wind_station_id: {wind_id}
    rain_station_id: {rain_id}
    temperature:
      name: Temperature
    rain_volume:
      name: Rainfall Total
"""
        if second_receiver:
            config += """  - platform: weather_station
    pin: GPIO26
    wind_station_id: 1
    rain_station_id: 2
    humidity:
      name: Second Humidity
"""
        with tempfile.TemporaryDirectory(prefix="weather-config-test-") as directory:
            path = Path(directory) / "test.yaml"
            path.write_text(config, encoding="utf-8")
            args = [sys.executable, "-m", "esphome"]
            if quiet:
                args.append("--quiet")
            return subprocess.run(args + ["config", str(path)], capture_output=True, text=True, timeout=30)

    def test_minimal_readings_have_ha_metadata(self):
        import yaml

        result = self.validate(quiet=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        config = yaml.safe_load(result.stdout)
        temperature = config["sensor"][0]["temperature"]
        rain = config["sensor"][0]["rain_volume"]
        self.assertEqual(temperature["unit_of_measurement"], "°C")
        self.assertEqual(temperature["device_class"], "temperature")
        self.assertEqual(rain["unit_of_measurement"], "mm")
        self.assertEqual(rain["device_class"], "precipitation")
        self.assertEqual(rain["state_class"], "total_increasing")

    def test_scan_ids_are_valid(self):
        result = self.validate(wind_id=0, rain_id=0)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_ids_outside_byte_range_are_rejected(self):
        for field in ("wind_id", "rain_id"):
            for value in (-1, 256):
                with self.subTest(field=field, value=value):
                    result = self.validate(**{field: value})
                    self.assertNotEqual(result.returncode, 0)

    def test_native_idf_framework_is_rejected(self):
        result = self.validate(framework="esp-idf")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("arduino", result.stdout + result.stderr)

    def test_multiple_receivers_are_rejected(self):
        result = self.validate(second_receiver=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Only one weather_station receiver", result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
