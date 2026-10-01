#pragma once

#include <vector>

namespace esphome::sensor {
class Sensor {
 public:
  void publish_state(float value) { this->values.push_back(value); }
  std::vector<float> values;
};
}  // namespace esphome::sensor

#define SUB_SENSOR(name) \
 protected: \
  esphome::sensor::Sensor *name##_sensor_{nullptr}; \
 public: \
  void set_##name##_sensor(esphome::sensor::Sensor *value) { this->name##_sensor_ = value; }
