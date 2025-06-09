#include "hdc302x.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace hdc302x {

static const char *const TAG = "hdc302x";

static const uint8_t HDC302x_CMD_READ[2] = {0x24, 0x00};

void HDC302xComponent::setup() {
  ESP_LOGCONFIG(TAG, "Running setup");
}

void HDC302xComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "HDC302x:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "ESP_LOG_MSG_COMM_FAIL");
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Humidity", this->humidity_);
}
void HDC302xComponent::update() {
  uint8_t read_buffer[6];
  if (this->write(HDC302x_CMD_READ, 2) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }
  delay(20);
  if (this->read(reinterpret_cast<uint8_t *>(&read_buffer), 6) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }
  uint16_t humidity_raw = read_buffer[3] << 8;   
  humidity_raw = (humidity_raw + read_buffer[4]);
  float humidity = (((float)(humidity_raw)) / 65535) * 100;  // conversion on the HDC3x datasheet
  
  uint16_t temp_raw = (read_buffer[0] << 8);
  temp_raw = (temp_raw + read_buffer[1]);   
  float temperature = ((float)(temp_raw) / 65535) * (175) - 45;

  this->temperature_->publish_state(temperature);
  this->humidity_->publish_state(humidity);

  ESP_LOGD(TAG, "Got temperature=%.1f°C humidity=%.1f%%", temperature, humidity);
  this->status_clear_warning();
}
float HDC302xComponent::get_setup_priority() const { return setup_priority::DATA; }

}  // namespace hdc302x
}  // namespace esphome
