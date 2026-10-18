#include "lv600s_humidifier.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_API
#include "esphome/components/api/api_server.h"
#endif
#ifdef USE_WIFI
#include "esphome/components/wifi/wifi_component.h"
#endif
#ifdef USE_CAPTIVE_PORTAL
#include "esphome/components/captive_portal/captive_portal.h"
#endif

#include <algorithm>
#include <cinttypes>
#include <cstdio>

namespace esphome {
namespace lv600s_humidifier {

static const char *const TAG = "lv600s_humidifier";

static constexpr uint32_t RX_BYTE_TIMEOUT_MS = 100;
static constexpr uint32_t ACK_TIMEOUT_MS = 500;
static constexpr uint8_t MAX_RETRIES = 2;
static constexpr size_t MAX_QUEUE_LEN = 16;
static constexpr uint32_t MAX_TIMER_SECONDS = 43200;
static constexpr uint32_t TIMER_PUBLISH_INTERVAL_MS = 60000;
static constexpr uint32_t WIFI_LED_CHECK_INTERVAL_MS = 1000;
// Grace period for the soft AP to come up before its state is trusted.
static constexpr uint32_t HOTSPOT_START_GRACE_MS = 5000;

#if defined(USE_WIFI) && defined(USE_WIFI_AP)
// WiFiComponent is final and keeps AP start/stop protected. Explicit instantiation is exempt from access checks,
// which lets us bind pointers to exactly the members ESPHome itself uses for its fallback AP.
namespace wifi_access {
template<typename Tag, typename Tag::type M> struct Binder {
  friend typename Tag::type get(Tag /*unused*/) { return M; }
};
struct SetupApConfig {
  using type = void (wifi::WiFiComponent::*)();
  friend type get(SetupApConfig);
};
struct WifiMode {
  using type = bool (wifi::WiFiComponent::*)(optional<bool>, optional<bool>);
  friend type get(WifiMode);
};
struct ApSetup {
  using type = bool wifi::WiFiComponent::*;
  friend type get(ApSetup);
};
template struct Binder<SetupApConfig, &wifi::WiFiComponent::setup_ap_config_>;
template struct Binder<WifiMode, &wifi::WiFiComponent::wifi_mode_>;
template struct Binder<ApSetup, &wifi::WiFiComponent::ap_setup_>;
}  // namespace wifi_access
#endif

void LV600SHumidifier::setup() {
  this->rx_frame_.reserve(64);
  this->last_status_query_ms_ = millis();
  this->query_status();
}

void LV600SHumidifier::loop() {
  const uint32_t now = millis();

  uint8_t byte;
  while (this->available() > 0 && this->read_byte(&byte)) {
    this->last_rx_byte_ms_ = now;
    this->process_byte_(byte);
  }
  if (!this->rx_frame_.empty() && now - this->last_rx_byte_ms_ > RX_BYTE_TIMEOUT_MS) {
    ESP_LOGW(TAG, "Discarding incomplete %u-byte frame after RX timeout",
             static_cast<unsigned>(this->rx_frame_.size()));
    this->reset_rx_();
  }

  if (this->status_interval_ms_ > 0 && now - this->last_status_query_ms_ >= this->status_interval_ms_) {
    this->query_status();
  }

  this->process_tx_(now);
  this->update_timer_(now);
  this->update_wifi_led_(now);
  this->update_hotspot_(now);
}

void LV600SHumidifier::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Levoit LV600S Humidifier\n"
                "  Status interval: %" PRIu32 " ms\n"
                "  Wi-Fi LED: %s\n"
                "  Hotspot on long press: %s (timeout %" PRIu32 " s)",
                this->status_interval_ms_, YESNO(this->wifi_led_enabled_), YESNO(this->hotspot_on_long_press_),
                this->hotspot_timeout_ms_ / 1000);
}

void LV600SHumidifier::query_status() {
  this->last_status_query_ms_ = millis();
  this->enqueue_command_(CMD_STATUS, nullptr, 0);
}

void LV600SHumidifier::set_power(bool on) {
  const uint8_t payload = on ? 0x01 : 0x00;
  this->enqueue_command_(CMD_POWER, &payload, 1);
  this->query_status();
}

void LV600SHumidifier::set_display(bool on) {
  const uint8_t payload = on ? 0x64 : 0x00;
  this->enqueue_command_(CMD_DISPLAY, &payload, 1);
  this->query_status();
}

void LV600SHumidifier::set_target_humidity(uint8_t target) {
  target = clamp<uint8_t>(target, 40, 80);
  const uint8_t payload[2] = {0x00, target};
  this->enqueue_command_(CMD_TARGET_HUMIDITY, payload, sizeof(payload));
  this->query_status();
}

void LV600SHumidifier::set_mist_level(uint8_t level) {
  level = clamp<uint8_t>(level, 1, 9);
  const uint8_t payload[4] = {level, 0x00, 0x00, 0x00};
  this->enqueue_command_(CMD_MIST_LEVEL, payload, sizeof(payload));
  this->query_status();
}

void LV600SHumidifier::set_warm_level(uint8_t level) {
  level = clamp<uint8_t>(level, 0, 3);
  const uint8_t payload[5] = {level, 0x00, 0x00, 0x00, 0x00};
  this->enqueue_command_(CMD_WARM_LEVEL, payload, sizeof(payload));
  this->query_status();
}

void LV600SHumidifier::set_timer_seconds(uint32_t seconds) {
  seconds = std::min<uint32_t>(seconds, MAX_TIMER_SECONDS);
  const uint8_t payload[4] = {
      static_cast<uint8_t>(seconds & 0xFF),
      static_cast<uint8_t>((seconds >> 8) & 0xFF),
      static_cast<uint8_t>((seconds >> 16) & 0xFF),
      static_cast<uint8_t>((seconds >> 24) & 0xFF),
  };
  this->enqueue_command_(CMD_TIMER, payload, sizeof(payload));
  this->query_status();
}

void LV600SHumidifier::set_manual_mode_level(uint8_t warm_enable, uint8_t mist_enable, uint8_t level) {
  level = clamp<uint8_t>(level, 0, 9);
  const uint8_t payload[3] = {
      static_cast<uint8_t>(warm_enable != 0),
      static_cast<uint8_t>(mist_enable != 0),
      level,
  };
  this->enqueue_command_(CMD_MANUAL_MODE_LEVEL, payload, sizeof(payload));
  this->query_status();
}

void LV600SHumidifier::set_humidity_mode_raw(uint8_t value) {
  const uint8_t payload[4] = {value, 0x00, 0x00, 0x00};
  this->enqueue_command_(CMD_HUMIDITY_MODE, payload, sizeof(payload));
  this->query_status();
}

void LV600SHumidifier::set_sleep_mode_raw(uint8_t value) {
  const uint8_t payload[6] = {value, 0x00, 0x00, 0x00, 0x00, 0x00};
  this->enqueue_command_(CMD_SLEEP_MODE, payload, sizeof(payload));
  this->query_status();
}

void LV600SHumidifier::set_manual_mode() {
  const uint8_t level = clamp<uint8_t>(this->mist_level_ == 0 ? 1 : this->mist_level_, 1, 9);
  this->set_manual_mode_level(0, 1, level);
}

void LV600SHumidifier::set_target_humidity_mode() {
  const uint8_t target = this->target_humidity_or_default_();
  this->set_humidity_mode_raw(target);
}

void LV600SHumidifier::set_sleep_mode() {
  const uint8_t target = this->target_humidity_or_default_();
  this->set_sleep_mode_raw(target);
}

void LV600SHumidifier::reboot_mcu(uint8_t value) {
  this->enqueue_command_(CMD_REBOOT_MCU, &value, 1);
  this->query_status();
}

void LV600SHumidifier::uart_test() { this->enqueue_command_(CMD_UART_TEST, nullptr, 0); }

std::string LV600SHumidifier::get_mode_name() const {
  switch (static_cast<MCUMode>(this->mode_)) {
    case MCUMode::MODE_HUMIDITY:
      return "Target Humidity";
    case MCUMode::MODE_SLEEP:
      return "Sleep";
    case MCUMode::MODE_MANUAL:
      return "Manual";
    default:
      return "Auto";
  }
}

// ---------------------------------------------------------------------------------------------------------------
// RX

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

  const uint8_t ctrl = frame[1];
  if ((ctrl & FRAME_VERSION_MASK) != FRAME_VERSION) {
    ESP_LOGW(TAG, "Unsupported A5 protocol version in ctrl 0x%02X", ctrl);
    return;
  }

  if (ctrl & FRAME_FLAG_ACK) {
    this->handle_reply_(ctrl, frame[2], &frame[6], body_len);
  } else {
    this->handle_request_(ctrl, frame[2], &frame[6], body_len);
  }
}

void LV600SHumidifier::handle_reply_(uint8_t ctrl, uint8_t seq, const uint8_t *body, uint16_t len) {
  const bool has_command = len >= 4;
  const uint16_t command = has_command ? static_cast<uint16_t>(body[1]) | (static_cast<uint16_t>(body[2]) << 8) : 0;
  const uint8_t result = has_command ? body[3] : 0;
  const bool error = (ctrl & FRAME_FLAG_ERROR) != 0 || result != 0;
  const uint8_t *payload = has_command ? &body[4] : nullptr;
  const uint16_t payload_len = has_command ? len - 4 : 0;

  if (has_command) {
    this->publish_last_frame_(command, payload_len);
  }

  if (this->tx_inflight_ && (seq == this->tx_current_seq_ || (has_command && command == this->tx_current_.command))) {
    this->tx_inflight_ = false;
    this->on_command_complete_(this->tx_current_, !error);
  } else {
    ESP_LOGD(TAG, "Unmatched reply seq=%u command=0x%04X", seq, command);
  }

  if (error) {
    ESP_LOGW(TAG, "MCU rejected command 0x%04X (ctrl 0x%02X, result %u)", command, ctrl, result);
    this->publish_last_ack_(command, payload, payload_len);
    return;
  }

  if (command == CMD_STATUS) {
    this->process_status_(payload, payload_len);
  } else if (command == CMD_TIMER_REMAINING) {
    this->process_timer_remaining_(payload, payload_len);
    this->publish_last_ack_(command, payload, payload_len);
  } else {
    ESP_LOGD(TAG, "ACK command=0x%04X payload_len=%u", command, payload_len);
    this->publish_last_ack_(command, payload, payload_len);
  }
}

void LV600SHumidifier::handle_request_(uint8_t ctrl, uint8_t seq, const uint8_t *body, uint16_t len) {
  if (len < 4) {
    ESP_LOGW(TAG, "Short MCU request body len=%u", len);
    return;
  }

  const uint16_t command = static_cast<uint16_t>(body[1]) | (static_cast<uint16_t>(body[2]) << 8);
  const uint8_t *payload = &body[4];
  const uint16_t payload_len = len - 4;
  const bool wants_ack = (ctrl & FRAME_FLAG_ACK_REQUEST) != 0;
  this->publish_last_frame_(command, payload_len);

  switch (command) {
    // Like the stock firmware, status pushes and timer reports are consumed without an ACK.
    case CMD_STATUS_PUSH:
      this->process_status_(payload, payload_len);
      break;
    case CMD_TIMER_REPORT:
      this->process_timer_report_(payload, payload_len);
      break;
    case CMD_BUTTON_ACTION:
    case CMD_BUTTON_ACTION_ALT:
      if (wants_ack)
        this->send_ack_(seq, command, 0, false);
      if (payload_len >= 1)
        this->handle_panel_button_(payload[0]);
      break;
    case CMD_SELF_TEST:
      if (wants_ack)
        this->send_ack_(seq, command, 0, false);
      ESP_LOGI(TAG, "Ignoring factory self-test request %u from MCU", payload_len >= 1 ? payload[0] : 0);
      break;
    default:
      ESP_LOGD(TAG, "Unsupported MCU request command=0x%04X payload_len=%u", command, payload_len);
      if (wants_ack)
        this->send_ack_(seq, command, RESULT_UNSUPPORTED, true);
      break;
  }
}

void LV600SHumidifier::process_status_(const uint8_t *status, uint16_t len) {
  if (len < 16) {
    ESP_LOGW(TAG, "Short status payload len=%u", len);
    return;
  }

  const bool was_display_on = this->display_on_;
  const bool first_status = !this->has_status_;
  this->has_status_ = true;
  this->mcu_version_[0] = status[0];
  this->mcu_version_[1] = status[1];
  this->mcu_version_[2] = status[2];
  this->power_on_ = status[3] != 0;
  this->container_state_ = status[4];
  this->water_lacks_ = status[5] != 0;
  this->display_config_ = status[6];
  this->fog_status_ = status[7];
  this->target_humidity_ = status[8];
  this->current_humidity_ = status[9];
  this->current_temperature_ = status[10];
  this->mode_ = status[11];
  this->mist_level_ = status[12];
  this->warm_level_ = status[13];
  this->status_byte_14_ = status[14];
  this->other_exception_ = status[15];
  this->display_on_ = this->power_on_ && this->display_config_ != 0 &&
                      static_cast<MCUMode>(this->mode_) != MCUMode::MODE_SLEEP;
  this->humidifying_ = this->power_on_ && !this->water_lacks_ && this->container_state_ == 0 && this->fog_status_ == 0;
  this->warm_enabled_ = this->warm_level_ != 0;

  if (first_status)
    this->publish_timer_();
  // The MCU blanks the Wi-Fi icon with the rest of the panel, so re-assert it after boot and whenever the panel lights
  // up again (power on, display on, sleep mode left), like stock does after power on, display on and leaving sleep.
  if (first_status || (this->display_on_ && !was_display_on))
    this->wifi_led_dirty_ = true;

  if (this->power_sensor_ != nullptr) this->power_sensor_->publish_state(this->power_on_);
  if (this->display_sensor_ != nullptr) this->display_sensor_->publish_state(this->display_on_);
  if (this->water_lacks_sensor_ != nullptr) this->water_lacks_sensor_->publish_state(this->water_lacks_);
  if (this->tank_removed_sensor_ != nullptr) this->tank_removed_sensor_->publish_state(this->container_state_ != 0);
  if (this->humidifying_sensor_ != nullptr) this->humidifying_sensor_->publish_state(this->humidifying_);
  if (this->warm_enabled_sensor_ != nullptr) this->warm_enabled_sensor_->publish_state(this->warm_enabled_);
  if (this->current_humidity_sensor_ != nullptr) this->current_humidity_sensor_->publish_state(this->current_humidity_);
  if (this->current_temperature_sensor_ != nullptr)
    this->current_temperature_sensor_->publish_state(this->current_temperature_);
  if (this->target_humidity_sensor_ != nullptr) this->target_humidity_sensor_->publish_state(this->target_humidity_);
  if (this->mist_level_sensor_ != nullptr) this->mist_level_sensor_->publish_state(this->mist_level_);
  if (this->warm_level_sensor_ != nullptr) this->warm_level_sensor_->publish_state(this->warm_level_);
  if (this->display_config_sensor_ != nullptr) this->display_config_sensor_->publish_state(this->display_config_);
  if (this->mode_sensor_ != nullptr) this->mode_sensor_->publish_state(this->mode_);
  if (this->fog_status_sensor_ != nullptr) this->fog_status_sensor_->publish_state(this->fog_status_);
  if (this->container_state_sensor_ != nullptr) this->container_state_sensor_->publish_state(this->container_state_);
  if (this->status_byte_14_sensor_ != nullptr) this->status_byte_14_sensor_->publish_state(this->status_byte_14_);
  if (this->other_exception_sensor_ != nullptr) this->other_exception_sensor_->publish_state(this->other_exception_);

  if (this->mcu_version_sensor_ != nullptr) {
    char version[16];
    std::snprintf(version, sizeof(version), "%u.%u.%u", this->mcu_version_[0], this->mcu_version_[1],
                  this->mcu_version_[2]);
    this->mcu_version_sensor_->publish_state(version);
  }

  ESP_LOGD(TAG,
           "Status power=%u display=%u display_config=%u water=%u fog=%u humidifying=%u hum=%u temp=%u target=%u "
           "mist=%u warm=%u mode=%u",
           this->power_on_, this->display_on_, this->display_config_, this->water_lacks_,
           this->fog_status_, this->humidifying_, this->current_humidity_, this->current_temperature_,
           this->target_humidity_, this->mist_level_, this->warm_level_, this->mode_);

  this->notify_state_();
}

void LV600SHumidifier::process_timer_remaining_(const uint8_t *payload, uint16_t len) {
  uint32_t remaining;
  if (len >= 4) {
    remaining = encode_uint32(payload[3], payload[2], payload[1], payload[0]);
  } else if (len >= 2) {
    remaining = encode_uint16(payload[1], payload[0]);
  } else {
    ESP_LOGD(TAG, "Timer remaining reply without payload");
    return;
  }
  ESP_LOGD(TAG, "Timer remaining=%" PRIu32, remaining);
  this->apply_timer_(remaining, std::max(this->timer_total_s_, remaining));
}

void LV600SHumidifier::process_timer_report_(const uint8_t *payload, uint16_t len) {
  if (len < 8) {
    ESP_LOGW(TAG, "Short timer report payload len=%u", len);
    return;
  }
  const uint32_t remaining = encode_uint32(payload[3], payload[2], payload[1], payload[0]);
  const uint32_t total = encode_uint32(payload[7], payload[6], payload[5], payload[4]);
  ESP_LOGD(TAG, "Timer report remaining=%" PRIu32 " total=%" PRIu32, remaining, total);
  this->apply_timer_(remaining, total);
}

void LV600SHumidifier::handle_panel_button_(uint8_t action) {
  ESP_LOGI(TAG, "Panel button action %u", action);
  this->panel_button_callback_.call(action);
  if (action == static_cast<uint8_t>(PanelButtonAction::PANEL_PAIR_WIFI) && this->hotspot_on_long_press_) {
    this->start_hotspot();
  }
}

void LV600SHumidifier::on_command_complete_(const PendingCommand &command, bool success) {
  if (!success) {
    // Make entities snap back to the MCU's real state instead of showing the rejected value. Queries and the
    // Wi-Fi LED are not retried this way, so an unresponsive MCU does not cause a request loop.
    if (command.command != CMD_STATUS && command.command != CMD_TIMER_REMAINING && command.command != CMD_WIFI_LED) {
      this->force_publish_ = true;
      this->query_status();
    }
    return;
  }
  if (command.command == CMD_TIMER && command.payload.size() >= 4) {
    const auto &p = command.payload;
    const uint32_t seconds = encode_uint32(p[3], p[2], p[1], p[0]);
    this->apply_timer_(seconds, seconds);
  }
}

// ---------------------------------------------------------------------------------------------------------------
// TX

void LV600SHumidifier::enqueue_command_(uint16_t command, const uint8_t *payload, uint16_t payload_len) {
  // Status queries always move to the back so they observe every command queued before them.
  if (command == CMD_STATUS) {
    this->tx_queue_.erase(std::remove_if(this->tx_queue_.begin(), this->tx_queue_.end(),
                                         [](const PendingCommand &c) { return c.command == CMD_STATUS; }),
                          this->tx_queue_.end());
  } else {
    // Coalesce repeated settings (e.g. a dragged slider): only the latest value is sent.
    for (auto &pending : this->tx_queue_) {
      if (pending.command == command) {
        pending.payload.assign(payload, payload + payload_len);
        return;
      }
    }
  }

  if (this->tx_queue_.size() >= MAX_QUEUE_LEN) {
    ESP_LOGW(TAG, "TX queue full, dropping command 0x%04X", this->tx_queue_.front().command);
    this->tx_queue_.pop_front();
  }
  this->tx_queue_.push_back(PendingCommand{command, std::vector<uint8_t>(payload, payload + payload_len)});
}

void LV600SHumidifier::process_tx_(uint32_t now) {
  if (this->tx_inflight_) {
    if (now - this->tx_sent_ms_ < ACK_TIMEOUT_MS) {
      return;
    }
    if (this->tx_retries_ < MAX_RETRIES) {
      this->tx_retries_++;
      ESP_LOGD(TAG, "No ACK for 0x%04X, retry %u", this->tx_current_.command, this->tx_retries_);
      this->write_frame_(FRAME_CTRL_REQUEST, this->tx_current_seq_, this->tx_current_.command, 0x00,
                         this->tx_current_.payload.data(), this->tx_current_.payload.size());
      this->tx_sent_ms_ = now;
      return;
    }
    ESP_LOGW(TAG, "MCU did not acknowledge command 0x%04X", this->tx_current_.command);
    this->tx_inflight_ = false;
    this->on_command_complete_(this->tx_current_, false);
  }

  if (this->tx_queue_.empty()) {
    return;
  }

  this->tx_current_ = std::move(this->tx_queue_.front());
  this->tx_queue_.pop_front();
  this->tx_current_seq_ = this->tx_sequence_++;
  this->tx_retries_ = 0;
  this->write_frame_(FRAME_CTRL_REQUEST, this->tx_current_seq_, this->tx_current_.command, 0x00,
                     this->tx_current_.payload.data(), this->tx_current_.payload.size());
  this->tx_inflight_ = true;
  this->tx_sent_ms_ = now;
  this->publish_last_frame_(this->tx_current_.command, this->tx_current_.payload.size());
  ESP_LOGD(TAG, "TX command=0x%04X seq=%u payload_len=%u", this->tx_current_.command, this->tx_current_seq_,
           static_cast<unsigned>(this->tx_current_.payload.size()));
}

void LV600SHumidifier::write_frame_(uint8_t ctrl, uint8_t seq, uint16_t command, uint8_t param,
                                    const uint8_t *payload, uint16_t payload_len) {
  const uint16_t body_len = payload_len + 4;
  std::vector<uint8_t> frame;
  frame.reserve(6 + body_len);
  frame.push_back(FRAME_HEADER);
  frame.push_back(ctrl);
  frame.push_back(seq);
  frame.push_back(static_cast<uint8_t>(body_len & 0xFF));
  frame.push_back(static_cast<uint8_t>((body_len >> 8) & 0xFF));
  frame.push_back(0x00);
  frame.push_back(0x01);
  frame.push_back(static_cast<uint8_t>(command & 0xFF));
  frame.push_back(static_cast<uint8_t>((command >> 8) & 0xFF));
  frame.push_back(param);
  for (uint16_t i = 0; i < payload_len; i++) {
    frame.push_back(payload[i]);
  }

  frame[5] = this->frame_checksum_(frame);
  this->write_array(frame);
  this->flush();
}

void LV600SHumidifier::send_ack_(uint8_t seq, uint16_t command, uint8_t result, bool error) {
  const uint8_t ctrl = FRAME_CTRL_ACK | (error ? FRAME_FLAG_ERROR : 0);
  this->write_frame_(ctrl, seq, command, result, nullptr, 0);
  ESP_LOGD(TAG, "ACK command=0x%04X seq=%u result=%u", command, seq, result);
}

uint8_t LV600SHumidifier::frame_checksum_(const std::vector<uint8_t> &frame) const {
  uint8_t sum = 0;
  for (size_t i = 0; i < frame.size(); i++) {
    sum += (i == 5) ? 0 : frame[i];
  }
  return static_cast<uint8_t>(~sum);
}

// ---------------------------------------------------------------------------------------------------------------
// Timer

uint32_t LV600SHumidifier::get_timer_remaining_seconds() const {
  if (this->timer_remaining_at_sync_s_ == 0) {
    return 0;
  }
  const uint32_t elapsed_s = (millis() - this->timer_sync_ms_) / 1000;
  return elapsed_s >= this->timer_remaining_at_sync_s_ ? 0 : this->timer_remaining_at_sync_s_ - elapsed_s;
}

void LV600SHumidifier::apply_timer_(uint32_t remaining_s, uint32_t total_s) {
  this->timer_remaining_at_sync_s_ = remaining_s;
  this->timer_total_s_ = remaining_s == 0 ? 0 : std::max(total_s, remaining_s);
  this->timer_sync_ms_ = millis();
  this->publish_timer_();
  this->notify_state_();
}

void LV600SHumidifier::update_timer_(uint32_t now) {
  if (this->timer_remaining_at_sync_s_ == 0) {
    return;
  }
  if (this->get_timer_remaining_seconds() == 0) {
    ESP_LOGD(TAG, "Timer expired");
    this->apply_timer_(0, 0);
    this->query_status();
    return;
  }
  if (now - this->last_timer_publish_ms_ >= TIMER_PUBLISH_INTERVAL_MS) {
    this->publish_timer_();
  }
}

void LV600SHumidifier::publish_timer_() {
  this->last_timer_publish_ms_ = millis();
  if (this->timer_remaining_sensor_ != nullptr) {
    this->timer_remaining_sensor_->publish_state(this->get_timer_remaining_seconds());
  }
}

// ---------------------------------------------------------------------------------------------------------------
// Wi-Fi LED (0xA129 payload: state, on_ms LE16, off_ms LE16, 0; state 0 = off, 1 = on, 2 = blink)

LV600SHumidifier::WifiLedState LV600SHumidifier::desired_wifi_led_state_() const {
#if defined(USE_WIFI) && defined(USE_WIFI_AP)
  if (wifi::global_wifi_component != nullptr && wifi::global_wifi_component->is_ap_active()) {
    return WifiLedState::LED_BLINK_FAST;
  }
#endif
#ifdef USE_API
  if (api::global_api_server != nullptr) {
    return api::global_api_server->is_connected() ? WifiLedState::LED_ON : WifiLedState::LED_BLINK_SLOW;
  }
#endif
#ifdef USE_WIFI
  if (wifi::global_wifi_component != nullptr) {
    return wifi::global_wifi_component->is_connected() ? WifiLedState::LED_ON : WifiLedState::LED_BLINK_SLOW;
  }
#endif
  return WifiLedState::LED_OFF;
}

void LV600SHumidifier::update_wifi_led_(uint32_t now) {
  // Stock never sends 0xA129 while the appliance is off. Go one step further and leave the LED alone whenever the panel
  // is dark (power off, display off or sleep mode), so the icon cannot light it up; process_status_() re-asserts the
  // current state once the display is back on.
  if (!this->wifi_led_enabled_ || !this->has_status_ || !this->display_on_) {
    return;
  }
  if (!this->wifi_led_dirty_ && now - this->last_wifi_led_check_ms_ < WIFI_LED_CHECK_INTERVAL_MS) {
    return;
  }
  this->last_wifi_led_check_ms_ = now;

  const WifiLedState state = this->desired_wifi_led_state_();
  if (!this->wifi_led_dirty_ && state == this->wifi_led_state_) {
    return;
  }
  this->wifi_led_dirty_ = false;
  this->wifi_led_state_ = state;

  uint8_t mode = 0;
  uint16_t on_ms = 0;
  uint16_t off_ms = 0;
  const char *name = "off";
  switch (state) {
    case WifiLedState::LED_ON:
      mode = 1;
      name = "on";
      break;
    case WifiLedState::LED_BLINK_SLOW:
      mode = 2;
      on_ms = off_ms = 1000;
      name = "slow blink";
      break;
    case WifiLedState::LED_BLINK_FAST:
      mode = 2;
      on_ms = off_ms = 250;
      name = "fast blink";
      break;
    default:
      break;
  }
  const uint8_t payload[6] = {
      mode,
      static_cast<uint8_t>(on_ms & 0xFF),
      static_cast<uint8_t>(on_ms >> 8),
      static_cast<uint8_t>(off_ms & 0xFF),
      static_cast<uint8_t>(off_ms >> 8),
      0x00,
  };
  ESP_LOGD(TAG, "Wi-Fi LED -> %s", name);
  this->enqueue_command_(CMD_WIFI_LED, payload, sizeof(payload));
}

// ---------------------------------------------------------------------------------------------------------------
// Setup hotspot (started from the panel long press or the Start Hotspot button)

void LV600SHumidifier::start_hotspot() {
#if defined(USE_WIFI) && defined(USE_WIFI_AP)
  auto *wifi_component = wifi::global_wifi_component;
  if (wifi_component == nullptr || !wifi_component->has_ap()) {
    ESP_LOGW(TAG, "Cannot start hotspot: no wifi.ap configured");
    return;
  }
  ESP_LOGI(TAG, "Starting setup hotspot for %" PRIu32 " s", this->hotspot_timeout_ms_ / 1000);
  (wifi_component->*get(wifi_access::SetupApConfig{}))();
#ifdef USE_CAPTIVE_PORTAL
  if (captive_portal::global_captive_portal != nullptr && !captive_portal::global_captive_portal->is_active()) {
    captive_portal::global_captive_portal->start();
  }
#endif
  this->hotspot_active_ = true;
  this->hotspot_started_ms_ = millis();
  this->wifi_led_dirty_ = true;
#else
  ESP_LOGW(TAG, "Cannot start hotspot: firmware built without wifi.ap");
#endif
}

void LV600SHumidifier::update_hotspot_(uint32_t now) {
  if (!this->hotspot_active_) {
    return;
  }
#if defined(USE_WIFI) && defined(USE_WIFI_AP)
  // ESPHome closes the AP itself once new credentials from the captive portal connect.
  if (now - this->hotspot_started_ms_ > HOTSPOT_START_GRACE_MS && !wifi::global_wifi_component->is_ap_active()) {
    ESP_LOGI(TAG, "Setup hotspot closed by Wi-Fi component");
    this->hotspot_active_ = false;
    this->wifi_led_dirty_ = true;
    return;
  }
  if (now - this->hotspot_started_ms_ >= this->hotspot_timeout_ms_) {
    this->stop_hotspot_();
  }
#endif
}

void LV600SHumidifier::stop_hotspot_() {
  this->hotspot_active_ = false;
  this->wifi_led_dirty_ = true;
#if defined(USE_WIFI) && defined(USE_WIFI_AP)
  auto *wifi_component = wifi::global_wifi_component;
  if (!wifi_component->is_connected()) {
    ESP_LOGI(TAG, "Hotspot timeout reached while Wi-Fi is disconnected; leaving the fallback hotspot running");
    return;
  }
#ifdef USE_CAPTIVE_PORTAL
  if (captive_portal::global_captive_portal != nullptr && captive_portal::global_captive_portal->is_active()) {
    captive_portal::global_captive_portal->end();
  }
#endif
  (wifi_component->*get(wifi_access::WifiMode{}))(optional<bool>{}, optional<bool>(false));
  // Allow ESPHome's own fallback AP to be set up again later.
  wifi_component->*get(wifi_access::ApSetup{}) = false;
  ESP_LOGI(TAG, "Setup hotspot closed after timeout");
#endif
}

// ---------------------------------------------------------------------------------------------------------------
// Helpers

void LV600SHumidifier::notify_state_() {
  const bool force = this->force_publish_;
  this->force_publish_ = false;
  this->state_callback_.call(force);
}

void LV600SHumidifier::publish_last_frame_(uint16_t command, uint16_t payload_len) {
  if (this->last_frame_sensor_ == nullptr) {
    return;
  }
  char text[48];
  std::snprintf(text, sizeof(text), "cmd=0x%04X len=%u", command, payload_len);
  this->last_frame_sensor_->publish_state(text);
}

void LV600SHumidifier::publish_last_ack_(uint16_t command, const uint8_t *payload, uint16_t len) {
  if (this->last_ack_sensor_ == nullptr) {
    return;
  }

  char text[96];
  int used = std::snprintf(text, sizeof(text), "cmd=0x%04X len=%u", command, len);
  if (used < 0) {
    return;
  }
  size_t pos = std::min<size_t>(static_cast<size_t>(used), sizeof(text) - 1);
  const uint16_t bytes_to_show = std::min<uint16_t>(len, 12);
  for (uint16_t i = 0; i < bytes_to_show && pos < sizeof(text) - 4; i++) {
    used = std::snprintf(text + pos, sizeof(text) - pos, "%s%02X", i == 0 ? " payload=" : " ", payload[i]);
    if (used < 0) {
      break;
    }
    pos += static_cast<size_t>(used);
  }
  if (len > bytes_to_show && pos < sizeof(text) - 4) {
    std::snprintf(text + pos, sizeof(text) - pos, " ...");
  }
  this->last_ack_sensor_->publish_state(text);
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
