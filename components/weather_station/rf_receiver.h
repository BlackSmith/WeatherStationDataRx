#pragma once

#include <Arduino.h>

namespace esphome::weather_station {

enum NewDataType : uint8_t {
  NDBattState = 0b00000001,
  NDTemperature = 0b00000010,
  NDHumidity = 0b00000100,
  NDWindSpeed = 0b00001000,
  NDWindDirection = 0b00010000,
  NDWindGust = 0b00100000,
  NDRainVolume = 0b01000000,
  NDError = 0b10000000,
};

enum ActionOnRepeatedMessage {
  ARMUseAsConfirmation2x,
  ARMUseAsConfirmation,
  ARMIgnore,
  ARMPass,
};

extern volatile uint32_t weather_station_rf_edges;
extern volatile uint32_t weather_station_rf_frames;

// Internal Arduino-backed RF decoder derived from WeatherStationDataRx v0.5.2.
// ESPHome owns the entities and station ID selection; this is not a public library API.
class WeatherStationDataRx {
 public:
  WeatherStationDataRx(uint8_t pin, ActionOnRepeatedMessage repeated_message = ARMIgnore);
  ~WeatherStationDataRx();
  void begin();
  void end();
  uint8_t readData();
  bool dataHas(uint8_t state, NewDataType type) const { return (state & type) == type; }
  float readTemperature() const { return this->temperature_ / 10.0f; }
  uint8_t readHumidity() const { return this->humidity_; }
  float readWindSpeed() const { return this->wind_speed_ / 10.0f; }
  uint16_t readWindDirection() const { return this->wind_direction_; }
  float readWindGust() const { return this->wind_gust_ / 10.0f; }
  float readRainVolume() const { return this->rain_ticks_ * 0.25f; }
  uint8_t batteryStatus() const { return this->battery_state_; }
  uint8_t sensorID() const { return this->station_id_; }

 protected:
  static void isr_();
  void rx_handler_();
  bool checksum_valid_(uint64_t data, uint8_t initial, bool add) const;
  void set_battery_(uint8_t mask, bool low);

  static WeatherStationDataRx *instance_;
  uint8_t pin_;
  ActionOnRepeatedMessage repeated_message_;
  unsigned long last_data_time_{0};
  volatile bool buffer_read_lock_{false};
  volatile bool buffer_write_lock_{false};
  int16_t temperature_{0};
  uint8_t humidity_{0};
  uint16_t wind_speed_{0};
  uint16_t wind_direction_{0};
  uint16_t wind_gust_{0};
  // Store the raw protocol counter; multiplying by 25 in uint16_t overflowed at 655.5 mm.
  uint16_t rain_ticks_{0};
  uint8_t battery_state_{0};
  uint8_t station_id_{0};
};

}  // namespace esphome::weather_station
