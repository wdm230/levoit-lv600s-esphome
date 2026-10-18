#include "lv600s_switch.h"

#include "esphome/core/log.h"

namespace esphome {
namespace lv600s_humidifier {

static const char *const TAG = "lv600s_humidifier.switch";

void LV600SSwitch::setup() {
  this->parent_->add_on_state_callback([this](bool force) { this->update_(force); });
}

void LV600SSwitch::dump_config() { LOG_SWITCH("", "LV600S Switch", this); }

void LV600SSwitch::write_state(bool state) {
  if (this->type_ == LV600SSwitchType::SWITCH_POWER) {
    this->parent_->set_power(state);
  } else {
    this->parent_->set_display(state);
  }
}

void LV600SSwitch::update_(bool force) {
  if (!this->parent_->has_status()) {
    return;
  }
  const bool state =
      this->type_ == LV600SSwitchType::SWITCH_POWER ? this->parent_->is_power_on() : this->parent_->is_display_on();
  if (force || !this->published_ || state != this->state) {
    this->published_ = true;
    this->publish_state(state);
  }
}

}  // namespace lv600s_humidifier
}  // namespace esphome
