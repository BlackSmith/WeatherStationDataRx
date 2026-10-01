#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "rf_receiver.h"

namespace esphome::weather_station {

class WeatherStation final : public Component {
 public:
  WeatherStation(uint8_t pin, uint8_t wind_id, uint8_t rain_id)
      : receiver_(pin, ARMIgnore), pin_(pin), wind_id_(wind_id), rain_id_(rain_id) {}

  void setup() override;
  void loop() override;
  void dump_config() override;
  void on_shutdown() override;

  SUB_SENSOR(temperature)
  SUB_SENSOR(humidity)
  SUB_SENSOR(wind_speed)
  SUB_SENSOR(wind_direction)
  SUB_SENSOR(wind_gust)
  SUB_SENSOR(rain_volume)
  SUB_SENSOR(battery_wind_station)
  SUB_SENSOR(battery_rain_station)

 protected:
  void log_reception_();
  void publish_or_log_(sensor::Sensor *sensor, float value, const char *label, const char *unit,
                       uint8_t station_id, uint8_t configured_id);

  WeatherStationDataRx receiver_;
  uint8_t pin_;
  uint8_t wind_id_;
  uint8_t rain_id_;
  uint32_t last_rf_edges_{0};
  uint32_t last_rf_frames_{0};
  uint32_t decoded_packets_{0};
  uint32_t matched_packets_{0};
};

}  // namespace esphome::weather_station
