#include "lv600s_select.h"

#include "esphome/core/log.h"

namespace esphome {
namespace lv600s_humidifier {

static const char *const TAG = "lv600s_humidifier.select";

void LV600SModeSelect::setup() {
  this->parent_->add_on_state_callback([this](bool force) { this->update_(force); });
}

void LV600SModeSelect::dump_config() { LOG_SELECT("", "LV600S Mode Select", this); }

void LV600SModeSelect::control(size_t index) {
  switch (static_cast<MCUMode>(index)) {
    case MCUMode::MODE_MANUAL:
      this->parent_->set_manual_mode();
      break;
    case MCUMode::MODE_SLEEP:
      this->parent_->set_sleep_mode();
      break;
    case MCUMode::MODE_HUMIDITY:
      this->parent_->set_target_humidity_mode();
      break;
    default:
      // No direct MCU command for auto mode is known (stock firmware only sets it app-side).
      ESP_LOGW(TAG, "Auto mode can only be selected on the humidifier panel");
      this->update_(true);
      break;
  }
}

void LV600SModeSelect::update_(bool force) {
  if (!this->parent_->has_status()) {
    return;
  }
  size_t index = this->parent_->get_raw_mode();
  if (index >= this->size()) {
    index = static_cast<size_t>(MCUMode::MODE_AUTO);  // stock firmware maps unknown raw modes to auto
  }
  if (force || !this->published_ || index != this->last_index_) {
    this->published_ = true;
    this->last_index_ = index;
    this->publish_state(index);
  }
}

}  // namespace lv600s_humidifier
}  // namespace esphome
