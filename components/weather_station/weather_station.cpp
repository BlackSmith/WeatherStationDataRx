#include "weather_station.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome::weather_station {

static const char *const TAG = "weather_station";

void WeatherStation::setup() {
  this->receiver_.begin();
  this->set_interval("rf_diagnostics", 60000, [this]() { this->log_reception_(); });
}

void WeatherStation::dump_config() {
  ESP_LOGCONFIG(TAG, "Weather station receiver:");
  ESP_LOGCONFIG(TAG, "  GPIO: %u", this->pin_);
  ESP_LOGCONFIG(TAG, "  Wind station ID: %u", this->wind_id_);
  ESP_LOGCONFIG(TAG, "  Rain station ID: %u", this->rain_id_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Wind speed", this->wind_speed_sensor_);
  LOG_SENSOR("  ", "Wind direction", this->wind_direction_sensor_);
  LOG_SENSOR("  ", "Wind gust", this->wind_gust_sensor_);
  LOG_SENSOR("  ", "Rainfall", this->rain_volume_sensor_);
  LOG_SENSOR("  ", "Wind station battery", this->battery_wind_station_sensor_);
  LOG_SENSOR("  ", "Rain station battery", this->battery_rain_station_sensor_);
}

void WeatherStation::on_shutdown() { this->receiver_.end(); }

void WeatherStation::log_reception_() {
  const uint32_t edges = weather_station_rf_edges;
  const uint32_t frames = weather_station_rf_frames;
  const uint32_t new_edges = edges - this->last_rf_edges_;
  const uint32_t new_frames = frames - this->last_rf_frames_;
  ESP_LOGI(TAG, "RF last 60s: GPIO edges=%" PRIu32 ", frames=%" PRIu32 ", decoded=%" PRIu32 ", matched=%" PRIu32,
           new_edges, new_frames, this->decoded_packets_, this->matched_packets_);
  if (new_edges == 0) {
    ESP_LOGW(TAG, "No GPIO edges received; check receiver power, wiring and GPIO%u", this->pin_);
  } else if (this->decoded_packets_ == 0) {
    ESP_LOGW(TAG, "No valid weather packets decoded; check transmitters, RF reception and protocol");
  } else if (this->matched_packets_ == 0 && this->wind_id_ > 0 && this->rain_id_ > 0) {
    ESP_LOGW(TAG, "Decoded packets do not match configured station IDs; check received IDs in DEBUG logs");
  }
  this->last_rf_edges_ = edges;
  this->last_rf_frames_ = frames;
  this->decoded_packets_ = 0;
  this->matched_packets_ = 0;
}

void WeatherStation::publish_or_log_(sensor::Sensor *sensor, float value, const char *label, const char *unit,
                                    uint8_t station_id, uint8_t configured_id) {
  if (configured_id == 0) {
    ESP_LOGI(TAG, "%s: %.2f %s (station ID %u)", label, value, unit, station_id);
  } else if (sensor != nullptr) {
    sensor->publish_state(value);
  }
}

void WeatherStation::loop() {
  const uint8_t data = this->receiver_.readData();
  if (data == 0)
    return;

  const uint8_t station_id = this->receiver_.sensorID();
  this->decoded_packets_++;
  ESP_LOGD(TAG, "Decoded RF packet: station ID=%u, data=0x%02X", station_id, data);
  const bool wind_data = (data & (NDTemperature | NDHumidity | NDWindSpeed | NDWindDirection | NDWindGust)) != 0;
  const bool rain_data = (data & NDRainVolume) != 0;
  if ((wind_data && this->wind_id_ > 0 && station_id == this->wind_id_) ||
      (rain_data && this->rain_id_ > 0 && station_id == this->rain_id_)) {
    this->matched_packets_++;
  } else {
    ESP_LOGD(TAG, "Packet not published: configured wind ID=%u, rain ID=%u (ID 0 enables scan-only mode)",
             this->wind_id_, this->rain_id_);
  }
  if (this->wind_id_ == 0 || station_id == this->wind_id_) {
    if (this->receiver_.dataHas(data, NDTemperature)) {
      this->publish_or_log_(this->temperature_sensor_, this->receiver_.readTemperature(), "Temperature", "C",
                            station_id, this->wind_id_);
    }
    if (this->receiver_.dataHas(data, NDHumidity)) {
      this->publish_or_log_(this->humidity_sensor_, this->receiver_.readHumidity(), "Humidity", "%", station_id,
                            this->wind_id_);
    }
    if (this->receiver_.dataHas(data, NDWindSpeed)) {
      this->publish_or_log_(this->wind_speed_sensor_, this->receiver_.readWindSpeed(), "Wind speed", "m/s", station_id,
                            this->wind_id_);
    }
    if (this->receiver_.dataHas(data, NDWindDirection)) {
      this->publish_or_log_(this->wind_direction_sensor_, this->receiver_.readWindDirection(), "Wind direction", "deg",
                            station_id, this->wind_id_);
    }
    if (this->receiver_.dataHas(data, NDWindGust)) {
      this->publish_or_log_(this->wind_gust_sensor_, this->receiver_.readWindGust(), "Wind gust", "m/s", station_id,
                            this->wind_id_);
    }
    if (wind_data && this->wind_id_ > 0 && this->receiver_.dataHas(data, NDBattState) &&
        this->battery_wind_station_sensor_ != nullptr) {
      this->battery_wind_station_sensor_->publish_state((this->receiver_.batteryStatus() & 0x01) ? 0 : 100);
    }
  }
  if (this->rain_id_ == 0 || station_id == this->rain_id_) {
    if (this->receiver_.dataHas(data, NDRainVolume)) {
      this->publish_or_log_(this->rain_volume_sensor_, this->receiver_.readRainVolume(), "Rainfall", "mm", station_id,
                            this->rain_id_);
    }
    if (rain_data && this->rain_id_ > 0 && this->receiver_.dataHas(data, NDBattState) &&
        this->battery_rain_station_sensor_ != nullptr) {
      this->battery_rain_station_sensor_->publish_state((this->receiver_.batteryStatus() & 0x02) ? 5 : 100);
    }
  }
}

}  // namespace esphome::weather_station
