# ESPHome example

The [example YAML](weather-station.yaml) loads `weather_station` directly from
this repository's `components` directory. It targets an ESP32 `nodemcu-32s` and
requires the Arduino framework. No `esphome.libraries`, `includes`, custom sensor
lambda, or second repository is required.

Create `secrets.yaml` beside the example using `secrets.example.yaml` as a template.
Replace the sample Wi-Fi, API and OTA values before uploading to a real device.
From the repository root:

```sh
esphome config examples/esphome/weather-station.yaml
esphome compile examples/esphome/weather-station.yaml
```

Both station IDs are initially 0. This enables scan-only logging and deliberately
publishes no measurements. Use the logs to identify the wind/temperature/humidity
transmitter and the rain transmitter, then set their IDs in the YAML and rebuild.
IDs may change after battery replacement. Other temperature sensors can be in
range; choose the IDs of your own weather station rather than accepting every ID.

All eight readings are optional. The component supplies their units, device
classes, state classes and display precision. Filters and other standard ESPHome
sensor options can be added to each nested reading.

When using a configuration outside this repository, adjust `external_components`
to point to the repository's `components` directory, or use its Git source after
the component has been published. See the [main README](../../README.md).
