#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uart/uart.h"

#include <cstdint>
#include <string>
#include <vector>

namespace esphome {
namespace lv600s_humidifier {

class LV600SHumidifier : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_status_interval(uint32_t interval_ms) { this->status_interval_ms_ = interval_ms; }

  void set_power_sensor(binary_sensor::BinarySensor *sensor) { this->power_sensor_ = sensor; }
  void set_display_sensor(binary_sensor::BinarySensor *sensor) { this->display_sensor_ = sensor; }
  void set_water_lacks_sensor(binary_sensor::BinarySensor *sensor) { this->water_lacks_sensor_ = sensor; }
  void set_tank_removed_sensor(binary_sensor::BinarySensor *sensor) { this->tank_removed_sensor_ = sensor; }
  void set_humidifying_sensor(binary_sensor::BinarySensor *sensor) { this->humidifying_sensor_ = sensor; }
  void set_warm_enabled_sensor(binary_sensor::BinarySensor *sensor) { this->warm_enabled_sensor_ = sensor; }

  void set_current_humidity_sensor(sensor::Sensor *sensor) { this->current_humidity_sensor_ = sensor; }
  void set_current_temperature_sensor(sensor::Sensor *sensor) { this->current_temperature_sensor_ = sensor; }
  void set_target_humidity_sensor(sensor::Sensor *sensor) { this->target_humidity_sensor_ = sensor; }
  void set_mist_level_sensor(sensor::Sensor *sensor) { this->mist_level_sensor_ = sensor; }
  void set_warm_level_sensor(sensor::Sensor *sensor) { this->warm_level_sensor_ = sensor; }
  void set_timer_remaining_sensor(sensor::Sensor *sensor) { this->timer_remaining_sensor_ = sensor; }
  void set_display_config_sensor(sensor::Sensor *sensor) { this->display_config_sensor_ = sensor; }
  void set_mode_sensor(sensor::Sensor *sensor) { this->mode_sensor_ = sensor; }
  void set_fog_status_sensor(sensor::Sensor *sensor) { this->fog_status_sensor_ = sensor; }
  void set_container_state_sensor(sensor::Sensor *sensor) { this->container_state_sensor_ = sensor; }
  void set_status_byte_14_sensor(sensor::Sensor *sensor) { this->status_byte_14_sensor_ = sensor; }
  void set_other_exception_sensor(sensor::Sensor *sensor) { this->other_exception_sensor_ = sensor; }

  void set_mcu_version_sensor(text_sensor::TextSensor *sensor) { this->mcu_version_sensor_ = sensor; }
  void set_last_frame_sensor(text_sensor::TextSensor *sensor) { this->last_frame_sensor_ = sensor; }
  void set_last_ack_sensor(text_sensor::TextSensor *sensor) { this->last_ack_sensor_ = sensor; }

  void query_status();
  void set_power(bool on);
  void set_display(bool on);
  void set_target_humidity(uint8_t target);
  void set_mist_level(uint8_t level);
  void set_warm_level(uint8_t level);
  void set_timer_seconds(uint32_t seconds);
  void set_manual_mode_level(uint8_t warm_enable, uint8_t mist_enable, uint8_t level);
  void set_humidity_mode_raw(uint8_t value);
  void set_sleep_mode_raw(uint8_t value);
  void set_sleep_auto_mode_raw(uint8_t value) { this->set_sleep_mode_raw(value); }
  void set_manual_mode();
  void set_target_humidity_mode();
  void set_sleep_mode();
  void set_sleep_auto_mode() { this->set_sleep_mode(); }
  void reboot_mcu(uint8_t value);
  void uart_test();
  void clear_timer() { this->set_timer_seconds(0); }

  bool has_status() const { return this->has_status_; }
  bool is_power_on() const { return this->power_on_; }
  bool is_display_on() const { return this->display_on_; }
  bool water_lacks() const { return this->water_lacks_; }
  bool is_tank_removed() const { return this->container_state_ != 0; }
  bool is_humidifying() const { return this->humidifying_; }
  bool is_warm_enabled() const { return this->warm_enabled_; }
  float get_current_humidity() const { return this->current_humidity_; }
  float get_current_temperature() const { return this->current_temperature_; }
  float get_target_humidity() const { return this->target_humidity_; }
  float get_mist_level() const { return this->mist_level_; }
  float get_warm_level() const { return this->warm_level_; }
  float get_timer_remaining() const { return this->timer_remaining_; }
  float get_display_config() const { return this->display_config_; }
  float get_mode() const { return this->mode_; }
  std::string get_mode_name() const;

 protected:
  static constexpr uint8_t FRAME_HEADER = 0xA5;
  static constexpr uint8_t FRAME_CTRL_ACK_REQUEST = 0x22;
  static constexpr uint16_t MAX_BODY_LEN = 512;

  static constexpr uint16_t CMD_STATUS = 0x4110;
  static constexpr uint16_t CMD_WARM_LEVEL = 0x4112;
  static constexpr uint16_t CMD_MIST_LEVEL = 0x4113;
  static constexpr uint16_t CMD_HUMIDITY_MODE = 0x4114;
  static constexpr uint16_t CMD_SLEEP_MODE = 0x4082;
  static constexpr uint16_t CMD_POWER = 0xA000;
  static constexpr uint16_t CMD_ACK_RESULT = 0xA003;
  static constexpr uint16_t CMD_MANUAL_MODE_LEVEL = 0xA260;
  static constexpr uint16_t CMD_DISPLAY = 0xA105;
  static constexpr uint16_t CMD_TIMER = 0xA264;
  static constexpr uint16_t CMD_TIMER_REMAINING = 0xA265;
  static constexpr uint16_t CMD_TARGET_HUMIDITY = 0xA2E8;
  static constexpr uint16_t CMD_REBOOT_MCU = 0xD101;
  static constexpr uint16_t CMD_UART_TEST = 0xD007;

  void process_byte_(uint8_t byte);
  void process_frame_(const std::vector<uint8_t> &frame);
  void process_body_(const uint8_t *body, uint16_t len);
  void process_status_(const uint8_t *status, uint16_t len);
  void process_timer_remaining_(const uint8_t *payload, uint16_t len);
  void process_ack_(uint16_t command, const uint8_t *payload, uint16_t len);
  void send_command_(uint16_t command, const uint8_t *payload, uint16_t payload_len, bool request_ack = true);
  uint8_t frame_checksum_(const std::vector<uint8_t> &frame) const;
  void publish_last_frame_(uint16_t command, uint16_t payload_len);
  void publish_last_ack_(uint16_t command, const uint8_t *payload, uint16_t len);
  void schedule_status_query_(uint32_t delay_ms = 300);
  void reset_rx_();
  uint8_t target_humidity_or_default_() const;

  uint32_t status_interval_ms_{5000};
  uint32_t last_status_query_ms_{0};
  uint32_t next_status_query_ms_{0};
  bool status_query_pending_{false};
  uint8_t tx_sequence_{0};

  std::vector<uint8_t> rx_frame_;
  uint16_t rx_expected_len_{0};

  bool has_status_{false};
  bool power_on_{false};
  bool display_on_{false};
  bool water_lacks_{false};
  bool humidifying_{false};
  bool warm_enabled_{false};
  uint8_t current_humidity_{0};
  uint8_t current_temperature_{0};
  uint8_t target_humidity_{0};
  uint8_t mist_level_{0};
  uint8_t warm_level_{0};
  uint8_t display_config_{0};
  uint8_t mode_{0};
  uint8_t fog_status_{0};
  uint8_t container_state_{0};
  uint8_t status_byte_14_{0};
  uint8_t other_exception_{0};
  uint16_t timer_remaining_{0};
  uint8_t mcu_version_[3]{0, 0, 0};

  binary_sensor::BinarySensor *power_sensor_{nullptr};
  binary_sensor::BinarySensor *display_sensor_{nullptr};
  binary_sensor::BinarySensor *water_lacks_sensor_{nullptr};
  binary_sensor::BinarySensor *tank_removed_sensor_{nullptr};
  binary_sensor::BinarySensor *humidifying_sensor_{nullptr};
  binary_sensor::BinarySensor *warm_enabled_sensor_{nullptr};

  sensor::Sensor *current_humidity_sensor_{nullptr};
  sensor::Sensor *current_temperature_sensor_{nullptr};
  sensor::Sensor *target_humidity_sensor_{nullptr};
  sensor::Sensor *mist_level_sensor_{nullptr};
  sensor::Sensor *warm_level_sensor_{nullptr};
  sensor::Sensor *timer_remaining_sensor_{nullptr};
  sensor::Sensor *display_config_sensor_{nullptr};
  sensor::Sensor *mode_sensor_{nullptr};
  sensor::Sensor *fog_status_sensor_{nullptr};
  sensor::Sensor *container_state_sensor_{nullptr};
  sensor::Sensor *status_byte_14_sensor_{nullptr};
  sensor::Sensor *other_exception_sensor_{nullptr};

  text_sensor::TextSensor *mcu_version_sensor_{nullptr};
  text_sensor::TextSensor *last_frame_sensor_{nullptr};
  text_sensor::TextSensor *last_ack_sensor_{nullptr};
};

}  // namespace lv600s_humidifier
}  // namespace esphome
