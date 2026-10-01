#pragma once

#include <cstdint>

namespace esphome {
class Component {
 public:
  virtual ~Component() = default;
  virtual void setup() {}
  virtual void loop() {}
  virtual void dump_config() {}
  virtual void on_shutdown() {}
  template<typename Callback> void set_interval(const char *, uint32_t, Callback) {}
};
}  // namespace esphome
