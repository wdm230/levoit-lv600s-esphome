#include "lv600s_humidifier.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cstdio>

namespace esphome {
namespace lv600s_humidifier {

static const char *const TAG = "lv600s_humidifier";

void LV600SHumidifier::setup() {
  this->rx_frame_.reserve(64);
  this->last_status_query_ms_ = millis();
}

void LV600SHumidifier::loop() {
  while (this->available() > 0) {
    uint8_t byte;
    if (this->read_byte(&byte)) {
      this->process_byte_(byte);
    }
  }

  const uint32_t now = millis();
  if (this->status_interval_ms_ > 0 && now - this->last_status_query_ms_ >= this->status_interval_ms_) {
    this->query_status();
    this->last_status_query_ms_ = now;
  }
}

void LV600SHumidifier::dump_config() {
  ESP_LOGCONFIG(TAG, "Levoit LV600S Humidifier");
  ESP_LOGCONFIG(TAG, "  Status interval: %u ms", this->status_interval_ms_);
}

void LV600SHumidifier::query_status() { this->send_command_(CMD_STATUS, nullptr, 0); }

void LV600SHumidifier::set_power(bool on) {
  const uint8_t payload = on ? 0x01 : 0x00;
  this->send_command_(CMD_POWER, &payload, 1);
  this->power_on_ = on;
}

void LV600SHumidifier::set_display(bool on) {
  const uint8_t payload = on ? 0x64 : 0x00;
  this->send_command_(CMD_DISPLAY, &payload, 1);
  this->display_on_ = on;
}

void LV600SHumidifier::set_target_humidity(uint8_t target) {
  target = clamp<uint8_t>(target, 40, 80);
  const uint8_t payload[2] = {0x00, target};
  this->send_command_(CMD_TARGET_HUMIDITY, payload, sizeof(payload));
  this->target_humidity_ = target;
}

void LV600SHumidifier::set_mist_level(uint8_t level) {
  level = clamp<uint8_t>(level, 1, 9);
  const uint8_t payload[4] = {level, 0x00, 0x00, 0x00};
  this->send_command_(CMD_MIST_LEVEL, payload, sizeof(payload));
  this->mist_level_ = level;
}

void LV600SHumidifier::set_warm_level(uint8_t level) {
  level = clamp<uint8_t>(level, 0, 3);
  const uint8_t payload[5] = {level, 0x00, 0x00, 0x00, 0x00};
  this->send_command_(CMD_WARM_LEVEL, payload, sizeof(payload));
  this->warm_level_ = level;
}

void LV600SHumidifier::set_timer_seconds(uint32_t seconds) {
  seconds = std::min<uint32_t>(seconds, 43200);
  const uint8_t payload[4] = {
      static_cast<uint8_t>(seconds & 0xFF),
      static_cast<uint8_t>((seconds >> 8) & 0xFF),
      static_cast<uint8_t>((seconds >> 16) & 0xFF),
      static_cast<uint8_t>((seconds >> 24) & 0xFF),
  };
  this->send_command_(CMD_TIMER, payload, sizeof(payload));
}

void LV600SHumidifier::set_manual_mode_level(uint8_t warm_enable, uint8_t mist_enable, uint8_t level) {
  level = clamp<uint8_t>(level, 0, 9);
  const uint8_t payload[3] = {
      static_cast<uint8_t>(warm_enable != 0),
      static_cast<uint8_t>(mist_enable != 0),
      level,
  };
  this->send_command_(CMD_MANUAL_MODE_LEVEL, payload, sizeof(payload));
}

void LV600SHumidifier::set_humidity_mode_raw(uint8_t value) {
  const uint8_t payload[4] = {value, 0x00, 0x00, 0x00};
  this->send_command_(CMD_HUMIDITY_MODE, payload, sizeof(payload));
}

void LV600SHumidifier::set_sleep_auto_mode_raw(uint8_t value) {
  const uint8_t payload[6] = {value, 0x00, 0x00, 0x00, 0x00, 0x00};
  this->send_command_(CMD_SLEEP_AUTO_MODE, payload, sizeof(payload));
}

void LV600SHumidifier::set_manual_mode() {
  const uint8_t level = clamp<uint8_t>(this->mist_level_ == 0 ? 1 : this->mist_level_, 1, 9);
  this->set_manual_mode_level(0, 1, level);
  this->mode_ = 0;
}

void LV600SHumidifier::set_target_humidity_mode() {
  const uint8_t target = this->target_humidity_or_default_();
  this->set_humidity_mode_raw(target);
  this->mode_ = 3;
}

void LV600SHumidifier::set_sleep_auto_mode() {
  const uint8_t target = this->target_humidity_or_default_();
  this->set_sleep_auto_mode_raw(target);
  this->mode_ = 1;
}

void LV600SHumidifier::reboot_mcu(uint8_t value) {
  this->send_command_(CMD_REBOOT_MCU, &value, 1);
}

void LV600SHumidifier::uart_test() { this->send_command_(CMD_UART_TEST, nullptr, 0); }

std::string LV600SHumidifier::get_mode_name() const {
  switch (this->mode_) {
    case 1:
    case 2:
      return "Sleep / Auto";
    case 3:
      return "Target Humidity";
    default:
      return "Manual";
  }
}

void LV600SHumidifier::process_byte_(uint8_t byte) {
  if (this->rx_frame_.empty()) {
    if (byte != FRAME_HEADER) {
      return;
    }
    this->rx_frame_.push_back(byte);
    this->rx_expected_len_ = 0;
    return;
  }

  this->rx_frame_.push_back(byte);

  if (this->rx_frame_.size() == 5) {
    const uint16_t body_len = static_cast<uint16_t>(this->rx_frame_[3]) |
                              (static_cast<uint16_t>(this->rx_frame_[4]) << 8);
    if (body_len > MAX_BODY_LEN) {
      ESP_LOGW(TAG, "Dropping overlong frame body: %u bytes", body_len);
      this->reset_rx_();
      return;
    }
    this->rx_expected_len_ = 6 + body_len;
  }

  if (this->rx_expected_len_ != 0 && this->rx_frame_.size() >= this->rx_expected_len_) {
    this->process_frame_(this->rx_frame_);
    this->reset_rx_();
  }
}

void LV600SHumidifier::process_frame_(const std::vector<uint8_t> &frame) {
  if (frame.size() < 6) {
    return;
  }

  const uint8_t expected_checksum = this->frame_checksum_(frame);
  if (frame[5] != expected_checksum) {
    ESP_LOGW(TAG, "Bad A5 checksum: got 0x%02X expected 0x%02X len=%u", frame[5], expected_checksum,
             static_cast<unsigned>(frame.size()));
    return;
  }

  const uint16_t body_len = static_cast<uint16_t>(frame[3]) | (static_cast<uint16_t>(frame[4]) << 8);
  if (body_len + 6 != frame.size()) {
    ESP_LOGW(TAG, "A5 length mismatch: body=%u frame=%u", body_len, static_cast<unsigned>(frame.size()));
    return;
  }

  this->process_body_(&frame[6], body_len);
}

void LV600SHumidifier::process_body_(const uint8_t *body, uint16_t len) {
  if (len < 4) {
    ESP_LOGD(TAG, "Short body len=%u", len);
    return;
  }

  const uint16_t command = static_cast<uint16_t>(body[1]) | (static_cast<uint16_t>(body[2]) << 8);
  const uint16_t payload_len = len - 4;
  this->publish_last_frame_(command, payload_len);

  if (command == CMD_STATUS) {
    this->process_status_(&body[4], payload_len);
  } else {
    ESP_LOGD(TAG, "RX command=0x%04X payload_len=%u", command, payload_len);
  }
}

void LV600SHumidifier::process_status_(const uint8_t *status, uint16_t len) {
  if (len < 15) {
    ESP_LOGW(TAG, "Short status payload len=%u", len);
    return;
  }

  this->has_status_ = true;
  this->mcu_version_[0] = status[0];
  this->mcu_version_[1] = status[1];
  this->mcu_version_[2] = status[2];
  this->power_on_ = status[3] != 0;
  this->container_state_ = status[4];
  this->water_lacks_ = status[5] != 0;
  this->display_on_ = status[6] != 0;
  this->fog_status_ = status[7];
  this->target_humidity_ = status[8];
  this->current_humidity_ = status[9];
  this->current_temperature_ = status[10];
  this->mode_ = status[11];
  this->mist_level_ = status[12];
  this->warm_level_ = status[13];
  this->other_exception_ = status[14];

  if (this->power_sensor_ != nullptr) this->power_sensor_->publish_state(this->power_on_);
  if (this->display_sensor_ != nullptr) this->display_sensor_->publish_state(this->display_on_);
  if (this->water_lacks_sensor_ != nullptr) this->water_lacks_sensor_->publish_state(this->water_lacks_);
  if (this->current_humidity_sensor_ != nullptr) this->current_humidity_sensor_->publish_state(this->current_humidity_);
  if (this->current_temperature_sensor_ != nullptr)
    this->current_temperature_sensor_->publish_state(this->current_temperature_);
  if (this->target_humidity_sensor_ != nullptr) this->target_humidity_sensor_->publish_state(this->target_humidity_);
  if (this->mist_level_sensor_ != nullptr) this->mist_level_sensor_->publish_state(this->mist_level_);
  if (this->warm_level_sensor_ != nullptr) this->warm_level_sensor_->publish_state(this->warm_level_);
  if (this->mode_sensor_ != nullptr) this->mode_sensor_->publish_state(this->mode_);
  if (this->fog_status_sensor_ != nullptr) this->fog_status_sensor_->publish_state(this->fog_status_);
  if (this->container_state_sensor_ != nullptr) this->container_state_sensor_->publish_state(this->container_state_);
  if (this->other_exception_sensor_ != nullptr) this->other_exception_sensor_->publish_state(this->other_exception_);

  if (this->mcu_version_sensor_ != nullptr) {
    char version[16];
    std::snprintf(version, sizeof(version), "%u.%u.%u", this->mcu_version_[0], this->mcu_version_[1],
                  this->mcu_version_[2]);
    this->mcu_version_sensor_->publish_state(version);
  }

  ESP_LOGD(TAG, "Status power=%u display=%u water=%u hum=%u temp=%u target=%u mist=%u warm=%u mode=%u",
           this->power_on_, this->display_on_, this->water_lacks_, this->current_humidity_,
           this->current_temperature_, this->target_humidity_, this->mist_level_, this->warm_level_, this->mode_);
}

void LV600SHumidifier::send_command_(uint16_t command, const uint8_t *payload, uint16_t payload_len,
                                     bool request_ack) {
  const uint16_t body_len = payload_len + 4;
  std::vector<uint8_t> frame;
  frame.reserve(6 + body_len);
  frame.push_back(FRAME_HEADER);
  frame.push_back(request_ack ? FRAME_CTRL_ACK_REQUEST : 0x02);
  frame.push_back(this->tx_sequence_++);
  frame.push_back(static_cast<uint8_t>(body_len & 0xFF));
  frame.push_back(static_cast<uint8_t>((body_len >> 8) & 0xFF));
  frame.push_back(0x00);
  frame.push_back(0x01);
  frame.push_back(static_cast<uint8_t>(command & 0xFF));
  frame.push_back(static_cast<uint8_t>((command >> 8) & 0xFF));
  frame.push_back(0x00);

  for (uint16_t i = 0; i < payload_len; i++) {
    frame.push_back(payload[i]);
  }

  frame[5] = this->frame_checksum_(frame);
  for (uint8_t byte : frame) {
    this->write_byte(byte);
  }
  this->flush();
  this->publish_last_frame_(command, payload_len);
  ESP_LOGD(TAG, "TX command=0x%04X payload_len=%u", command, payload_len);
}

uint8_t LV600SHumidifier::frame_checksum_(const std::vector<uint8_t> &frame) const {
  uint8_t sum = 0;
  for (size_t i = 0; i < frame.size(); i++) {
    sum += (i == 5) ? 0 : frame[i];
  }
  return static_cast<uint8_t>(~sum);
}

void LV600SHumidifier::publish_last_frame_(uint16_t command, uint16_t payload_len) {
  if (this->last_frame_sensor_ == nullptr) {
    return;
  }
  char text[48];
  std::snprintf(text, sizeof(text), "cmd=0x%04X len=%u", command, payload_len);
  this->last_frame_sensor_->publish_state(text);
}

void LV600SHumidifier::reset_rx_() {
  this->rx_frame_.clear();
  this->rx_expected_len_ = 0;
}

uint8_t LV600SHumidifier::target_humidity_or_default_() const {
  if (this->target_humidity_ >= 40 && this->target_humidity_ <= 80) {
    return this->target_humidity_;
  }
  return 50;
}

}  // namespace lv600s_humidifier
}  // namespace esphome
