#include "rf_receiver.h"
#include "ring_buffer.h"

namespace esphome::weather_station {

volatile uint32_t weather_station_rf_edges = 0;
volatile uint32_t weather_station_rf_frames = 0;

static uint64_t rx_buffer;
static Ringbuffer<uint64_t, 5> *unconfirmed_buffer = nullptr;
static Ringbuffer<uint64_t, 20> data_buffer;
static constexpr uint32_t IGNORE_REPEATED_MESSAGES_TIME = 3000;
WeatherStationDataRx *WeatherStationDataRx::instance_ = nullptr;

WeatherStationDataRx::WeatherStationDataRx(uint8_t pin, ActionOnRepeatedMessage repeated_message)
    : pin_(pin), repeated_message_(repeated_message) {
  instance_ = this;
  if (this->repeated_message_ != ARMPass && unconfirmed_buffer == nullptr)
    unconfirmed_buffer = new Ringbuffer<uint64_t, 5>();
}

WeatherStationDataRx::~WeatherStationDataRx() {
  this->end();
  if (instance_ == this) {
    instance_ = nullptr;
    delete unconfirmed_buffer;
    unconfirmed_buffer = nullptr;
  }
}

void WeatherStationDataRx::begin() {
  pinMode(this->pin_, INPUT);
  attachInterrupt(digitalPinToInterrupt(this->pin_), isr_, CHANGE);
}

void WeatherStationDataRx::end() { detachInterrupt(digitalPinToInterrupt(this->pin_)); }

void IRAM_ATTR WeatherStationDataRx::isr_() {
  if (instance_ != nullptr)
    instance_->rx_handler_();
}

void IRAM_ATTR WeatherStationDataRx::rx_handler_() {
  static unsigned long rx_low = 0;
  static bool sync_bit = false, data_bit = false;
  static uint8_t rx_counter = 0;
  weather_station_rf_edges = weather_station_rf_edges + 1;
  if (!digitalRead(this->pin_)) {
    rx_low = micros();
    return;
  }

  const unsigned long duration = micros() - rx_low;
  if (duration > 8500) {
    // A sync interval starts a new 36-bit, LSB-first packet.
    sync_bit = true;
    rx_buffer = 0;
    rx_counter = 0;
    return;
  }
  if (!sync_bit)
    return;
  if (duration > 1500)
    data_bit = false;
  if (duration > 3500)
    data_bit = true;
  if (rx_counter < 36)
    rx_buffer |= static_cast<uint64_t>(data_bit) << rx_counter++;
  if (rx_counter != 36)
    return;

  weather_station_rf_frames = weather_station_rf_frames + 1;
  this->buffer_write_lock_ = true;
  if (this->buffer_read_lock_) {
    this->buffer_write_lock_ = false;
    return;
  }
  rx_counter = 0;
  sync_bit = false;

  if (this->repeated_message_ != ARMPass && millis() - this->last_data_time_ > IGNORE_REPEATED_MESSAGES_TIME)
    unconfirmed_buffer->clear();

  if (this->repeated_message_ == ARMUseAsConfirmation2x) {
    if (unconfirmed_buffer->counterEqual(&rx_buffer) >= 2)
      data_buffer.push(&rx_buffer);
    else
      unconfirmed_buffer->push(&rx_buffer);
  } else if (this->repeated_message_ == ARMUseAsConfirmation) {
    if (unconfirmed_buffer->contains(&rx_buffer))
      data_buffer.push(&rx_buffer);
    else
      unconfirmed_buffer->push(&rx_buffer);
  } else if (this->repeated_message_ == ARMIgnore) {
    if (!unconfirmed_buffer->contains(&rx_buffer)) {
      unconfirmed_buffer->push(&rx_buffer);
      data_buffer.push(&rx_buffer);
    }
  } else {
    data_buffer.push(&rx_buffer);
  }
  this->last_data_time_ = millis();
  this->buffer_write_lock_ = false;
}

bool WeatherStationDataRx::checksum_valid_(uint64_t data, uint8_t initial, bool add) const {
  uint32_t checksum = initial;
  for (uint8_t bit = 0; bit < 32; bit += 4) {
    if (add)
      checksum += (data >> bit) & 0x0f;
    else
      checksum -= (data >> bit) & 0x0f;
  }
  return (checksum & 0x0f) == ((data >> 32) & 0x0f);
}

void WeatherStationDataRx::set_battery_(uint8_t mask, bool low) {
  if (low)
    this->battery_state_ |= mask;
  else
    this->battery_state_ &= static_cast<uint8_t>(~mask);
}

uint8_t WeatherStationDataRx::readData() {
  if (data_buffer.currentSize() == 0)
    return 0;
  uint64_t data;
  // Preserve the v0.5.2 ISR/loop buffer coordination.
  this->buffer_read_lock_ = true;
  while (this->buffer_write_lock_)
    delay(10);
  const bool received = data_buffer.pull(data);
  this->buffer_read_lock_ = false;
  if (!received)
    return 0;

  this->station_id_ = data & 0xff;
  const bool battery_low = (data >> 8) & 0x01;
  const uint8_t type = (data >> 9) & 0x03;
  // The documented sensors have bit 4 of their random station ID cleared.
  if (this->station_id_ & 0x10)
    return 0;

  if (type < 3) {
    if (!this->checksum_valid_(data, 0x0f, false))
      return 0;
    this->set_battery_(0x01, battery_low);
    const uint16_t temperature = (data >> 12) & 0x0fff;
    this->temperature_ = temperature & 0x0800 ? static_cast<int16_t>(temperature) - 4096 : temperature;
    this->humidity_ = ((data >> 24) & 0x0f) + ((data >> 28) & 0x0f) * 10;
    return NDBattState | NDTemperature | NDHumidity;
  }

  const uint8_t sub_id = (data >> 12) & 0x07;
  if (sub_id == 1 || sub_id == 7) {
    if (!this->checksum_valid_(data, 0x0f, false))
      return 0;
    this->set_battery_(0x01, battery_low);
    if (sub_id == 1) {
      this->wind_speed_ = ((data >> 24) & 0xff) * 2;
      return NDBattState | NDWindSpeed;
    }
    const uint16_t direction = (data >> 15) & 0x01ff;
    if (direction <= 360)
      this->wind_direction_ = direction;
    this->wind_gust_ = ((data >> 24) & 0xff) * 2;
    return NDBattState | NDWindDirection | NDWindGust;
  }
  if (sub_id == 3) {
    if (!this->checksum_valid_(data, 0x07, true))
      return 0;
    this->set_battery_(0x02, battery_low);
    this->rain_ticks_ = (data >> 16) & 0xffff;
    return NDBattState | NDRainVolume;
  }
  return 0;
}

}  // namespace esphome::weather_station
