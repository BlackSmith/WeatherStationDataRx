# Project context

- The repository targets ESPHome only. `components/weather_station` is the
  self-contained external component; there is no standalone Arduino library API
  or separate RF library to download during code generation.
- ESP32 still requires `framework.type: arduino` because the receiver uses
  Arduino GPIO/interrupt APIs. This works with ESPHome's native ESP-IDF toolchain;
  the toolchain and framework choices are distinct.
- The receiving logic is derived from Zwer2k WeatherStationDataRx v0.5.2 commit
  `7c82596485d7e3b5d63aeb3af3e7e9ec24a7adc5`. Preserve the existing timing
  thresholds, duplicate modes and ISR/loop coordination when maintaining it.
  The component uses `ARMIgnore` to match the deployed integration.
- RX buffers are shared and the low-level receiver has one active instance.
  `FINAL_VALIDATE_SCHEMA` deliberately rejects multiple weather_station platform
  entries in one ESPHome configuration.
- The RF protocol is TFD Auriol/Ventus v2.0: 36 LSB-first bits; combined-sensor
  checksum `(15 - sum(n0..n7)) & 15`; rain checksum `(7 + sum(n0..n7)) & 15`.
- Store rainfall in raw 16-bit quarter-millimeter ticks and convert with `0.25f`.
  The old `uint16_t ticks * 25 / 100` representation overflowed at 655.5 mm,
  well below the protocol limit of 16383.75 mm.
- Battery packets carry flags, not percentages. Clearing a normal battery flag
  must preserve the other station's flag. Percentage sensors retain the legacy
  wind 100/0 and rain 100/5 normal/low convention for entity compatibility.
- Zero station IDs mean scan-only logging, not wildcard publication. IDs can
  change after battery replacement; log rejected packets so ID mismatches can
  be distinguished from missing RF input.
- Temperature/humidity arrive approximately every 186 seconds; rainfall can
  arrive every 148 seconds. Filters may further delay publication.
- Host tests replay timed frames through the production ISR and decoder with
  GPIO/sensor stubs. They do not prove physical RF reception or concurrent ISR
  behavior. Run `python3 -m unittest discover -s tests -v`; installing ESPHome
  also enables schema validation tests. Compile the example or actual device YAML
  to verify real ESP32 headers and linking.
