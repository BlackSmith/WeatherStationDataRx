#pragma once

#include <Arduino.h>

namespace esphome::weather_station {

// Based on WeatherStationDataRx v0.5.2. Shared indices are accessed by the ISR.
template<typename T, uint16_t SIZE> class Ringbuffer {
 public:
  bool push(const T *value) __attribute__((noinline)) {
    if (this->size_ == SIZE)
      return false;
    uint16_t write_pos = this->read_pos_ + this->size_;
    if (write_pos >= SIZE)
      write_pos -= SIZE;
    this->buffer_[write_pos] = *value;
    this->size_ = this->size_ + 1;
    return true;
  }

  bool pull(T &value) __attribute__((noinline)) {
    if (this->size_ == 0)
      return false;
    value = this->buffer_[this->read_pos_];
    this->read_pos_ = this->read_pos_ + 1;
    this->size_ = this->size_ - 1;
    if (this->read_pos_ == SIZE)
      this->read_pos_ = 0;
    return true;
  }

  bool contains(const T *value) __attribute__((noinline)) { return this->counterEqual(value) != 0; }

  uint16_t counterEqual(const T *value) {
    uint16_t count = 0;
    for (uint16_t i = 0, j = this->read_pos_; i < this->size_; i++) {
      if (j == SIZE)
        j = 0;
      if (this->buffer_[j++] == *value)
        count++;
    }
    return count;
  }

  void clear() { this->size_ = 0; }
  uint16_t currentSize() const { return this->size_; }

 protected:
  T buffer_[SIZE]{};
  volatile uint16_t read_pos_{0};
  volatile uint16_t size_{0};
};

}  // namespace esphome::weather_station
