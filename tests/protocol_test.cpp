#include "rf_receiver.h"
#include "weather_station.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace esphome::weather_station;

unsigned long mock_time_us = 10000000;
bool mock_pin_high = true;
void (*mock_interrupt)() = nullptr;

static void expect(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}

static void expect_value(float actual, float expected) {
  if (std::fabs(actual - expected) > 0.001f)
    throw std::runtime_error("Expected " + std::to_string(expected) + ", got " + std::to_string(actual));
}

// Emulate the PDF's ~0.5 ms HIGH pulse followed by a LOW data/sync interval.
static void interval(unsigned long duration) {
  mock_time_us += 500;
  mock_pin_high = false;
  mock_interrupt();
  mock_time_us += duration;
  mock_pin_high = true;
  mock_interrupt();
}

static void send_frame(uint64_t frame, uint8_t syncs = 1) {
  for (uint8_t i = 0; i < syncs; i++)
    interval(9000);
  for (uint8_t bit = 0; bit < 36; bit++)
    interval((frame & (uint64_t{1} << bit)) ? 4000 : 2000);
}

static uint64_t with_checksum(uint64_t payload, bool rain = false) {
  uint8_t sum = 0;
  for (uint8_t nibble = 0; nibble < 8; nibble++)
    sum += (payload >> (nibble * 4)) & 0x0f;
  const uint64_t checksum = (rain ? 7 + sum : 15 - sum) & 0x0f;
  return payload | (checksum << 32);
}

static uint64_t temperature_frame(int16_t tenths, uint8_t humidity, uint8_t type = 0, uint8_t id = 234,
                                  bool low_battery = false) {
  return with_checksum(uint64_t{id} | (uint64_t{low_battery} << 8) | (uint64_t{type} << 9) |
                       (uint64_t{static_cast<uint16_t>(tenths) & 0x0fffU} << 12) |
                       (uint64_t{humidity % 10U} << 24) | (uint64_t{humidity / 10U} << 28));
}

static uint64_t speed_frame(uint8_t raw, bool low_battery = false) {
  return with_checksum(234 | (uint64_t{low_battery} << 8) | (uint64_t{3} << 9) | (uint64_t{1} << 12) |
                       (uint64_t{raw} << 24));
}

static uint64_t direction_frame(uint16_t direction, uint8_t gust) {
  return with_checksum(234 | (uint64_t{3} << 9) | (uint64_t{7} << 12) | (uint64_t{direction} << 15) |
                       (uint64_t{gust} << 24));
}

static uint64_t rain_frame(uint16_t ticks, bool low_battery = false) {
  return with_checksum(204 | (uint64_t{low_battery} << 8) | (uint64_t{3} << 9) | (uint64_t{3} << 12) |
                           (uint64_t{ticks} << 16),
                       true);
}

static void test_temperature() {
  WeatherStationDataRx receiver(27, ARMPass);
  receiver.begin();
  for (uint8_t type = 0; type < 3; type++) {
    for (int16_t tenths : {-2048, -123, -1, 0, 123, 2047}) {
      send_frame(temperature_frame(tenths, 56, type));
      expect(receiver.readData() == (NDBattState | NDTemperature | NDHumidity), "Temperature packet not decoded");
      expect_value(receiver.readTemperature(), tenths / 10.0f);
      expect(receiver.readHumidity() == 56, "BCD humidity not decoded");
      expect(receiver.sensorID() == 234, "Station ID not decoded");
    }
  }
}

static void test_wind() {
  WeatherStationDataRx receiver(27, ARMPass);
  receiver.begin();
  send_frame(speed_frame(255));
  expect(receiver.readData() == (NDBattState | NDWindSpeed), "Wind speed packet not decoded");
  expect_value(receiver.readWindSpeed(), 51.0f);
  for (uint16_t direction : {0, 45, 90, 135, 180, 225, 270, 315, 360}) {
    send_frame(direction_frame(direction, 13));
    expect(receiver.readData() == (NDBattState | NDWindDirection | NDWindGust), "Wind direction packet not decoded");
    expect(receiver.readWindDirection() == direction, "Wrong wind direction");
    expect_value(receiver.readWindGust(), 2.6f);
  }
}

static void test_rain() {
  WeatherStationDataRx receiver(27, ARMPass);
  receiver.begin();
  const uint16_t ticks[] = {0, 1, 2, 16, 2621, 2622, 4000, 65535};
  const float millimeters[] = {0, 0.25f, 0.5f, 4, 655.25f, 655.5f, 1000, 16383.75f};
  for (unsigned i = 0; i < sizeof(ticks) / sizeof(ticks[0]); i++) {
    send_frame(rain_frame(ticks[i]));
    expect(receiver.readData() == (NDBattState | NDRainVolume), "Rain packet not decoded");
    expect_value(receiver.readRainVolume(), millimeters[i]);
  }
}

static void test_battery() {
  WeatherStationDataRx receiver(27, ARMPass);
  receiver.begin();
  send_frame(speed_frame(1, true));
  receiver.readData();
  expect(receiver.batteryStatus() == 1, "Wind low-battery flag missing");
  send_frame(rain_frame(1, false));
  receiver.readData();
  expect(receiver.batteryStatus() == 1, "Healthy rain packet cleared wind low-battery flag");
  send_frame(rain_frame(2, true));
  receiver.readData();
  expect(receiver.batteryStatus() == 3, "Independent battery flags not preserved");
  send_frame(speed_frame(2, false));
  receiver.readData();
  expect(receiver.batteryStatus() == 2, "Healthy wind packet cleared rain low-battery flag");
  send_frame(rain_frame(3, false));
  receiver.readData();
  expect(receiver.batteryStatus() == 0, "Rain low-battery flag not cleared");
}

static void test_rejection() {
  WeatherStationDataRx receiver(27, ARMPass);
  receiver.begin();
  send_frame(temperature_frame(200, 60) ^ (uint64_t{1} << 32));
  expect(receiver.readData() == 0, "Bad combined-sensor checksum accepted");
  send_frame(rain_frame(123) ^ (uint64_t{1} << 32));
  expect(receiver.readData() == 0, "Bad rain checksum accepted");
  send_frame(temperature_frame(200, 60, 0, 250));
  expect(receiver.readData() == 0, "Unsupported station ID bit 4 accepted");
  send_frame(with_checksum(234 | (uint64_t{3} << 9) | (uint64_t{2} << 12)));
  expect(receiver.readData() == 0, "Unsupported packet type accepted");
}

static void test_repeats(ActionOnRepeatedMessage mode, unsigned expected) {
  WeatherStationDataRx receiver(27, mode);
  receiver.begin();
  const uint64_t frame = temperature_frame(200, 60);
  unsigned decoded = 0;
  for (unsigned i = 0; i < 6; i++) {
    send_frame(frame, 4);
    decoded += receiver.readData() != 0;
  }
  expect(decoded == expected, "Wrong duplicate confirmation policy");
  mock_time_us += 4000000;
  decoded = 0;
  for (unsigned i = 0; i < 6; i++) {
    send_frame(frame, 4);
    decoded += receiver.readData() != 0;
  }
  expect(decoded == expected, "Confirmation buffer did not reset for next burst");
  expect(weather_station_rf_frames == 12, "RF frame diagnostics incorrect");
  expect(weather_station_rf_edges == 12 * (4 + 36) * 2, "RF edge diagnostics incorrect");
}

static void test_station(bool scan) {
  WeatherStation station(27, scan ? 0 : 234, scan ? 0 : 204);
  esphome::sensor::Sensor temperature, humidity, rain, wind, direction, gust, wind_battery, rain_battery;
  station.set_temperature_sensor(&temperature);
  station.set_humidity_sensor(&humidity);
  station.set_rain_volume_sensor(&rain);
  station.set_wind_speed_sensor(&wind);
  station.set_wind_direction_sensor(&direction);
  station.set_wind_gust_sensor(&gust);
  station.set_battery_wind_station_sensor(&wind_battery);
  station.set_battery_rain_station_sensor(&rain_battery);
  station.setup();
  send_frame(temperature_frame(300, 10, 0, 230));
  station.loop();
  expect(temperature.values.empty(), "Foreign station temperature published");
  send_frame(temperature_frame(-123, 56));
  station.loop();
  send_frame(speed_frame(4));
  station.loop();
  send_frame(direction_frame(225, 13));
  station.loop();
  send_frame(rain_frame(2622));
  station.loop();
  if (scan) {
    expect(temperature.values.empty() && humidity.values.empty() && rain.values.empty() && wind.values.empty() &&
               direction.values.empty() && gust.values.empty() && wind_battery.values.empty() && rain_battery.values.empty(),
           "Scan mode published measurements");
  } else {
    expect_value(temperature.values.back(), -12.3f);
    expect_value(humidity.values.back(), 56);
    expect_value(wind.values.back(), 0.8f);
    expect_value(direction.values.back(), 225);
    expect_value(gust.values.back(), 2.6f);
    expect_value(rain.values.back(), 655.5f);
    expect_value(wind_battery.values.back(), 100);
    expect_value(rain_battery.values.back(), 100);
  }
  station.on_shutdown();
  expect(mock_interrupt == nullptr, "RF interrupt not detached on shutdown");
}

int main(int argc, char **argv) {
  try {
    expect(argc == 2, "Test case required");
    const std::string test = argv[1];
    if (test == "temperature") test_temperature();
    else if (test == "wind") test_wind();
    else if (test == "rain") test_rain();
    else if (test == "battery") test_battery();
    else if (test == "rejection") test_rejection();
    else if (test == "ignore_repeats") test_repeats(ARMIgnore, 1);
    else if (test == "confirm_once") test_repeats(ARMUseAsConfirmation, 5);
    else if (test == "confirm_twice") test_repeats(ARMUseAsConfirmation2x, 4);
    else if (test == "pass_repeats") test_repeats(ARMPass, 6);
    else if (test == "station_filter") test_station(false);
    else if (test == "scan") test_station(true);
    else throw std::runtime_error("Unknown test case");
    std::cout << test << " passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
