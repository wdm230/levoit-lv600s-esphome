#include "lv600s_number.h"

#include "esphome/core/log.h"

#include <cmath>

namespace esphome {
namespace lv600s_humidifier {

static const char *const TAG = "lv600s_humidifier.number";

void LV600SNumber::setup() {
  this->parent_->add_on_state_callback([this](bool force) { this->update_(force); });
}

void LV600SNumber::dump_config() { LOG_NUMBER("", "LV600S Number", this); }

void LV600SNumber::control(float value) {
  const long rounded = std::lround(value);
  switch (this->type_) {
    case LV600SNumberType::NUMBER_TARGET_HUMIDITY:
      this->parent_->set_target_humidity(static_cast<uint8_t>(rounded));
      break;
    case LV600SNumberType::NUMBER_MIST_LEVEL:
      this->parent_->set_mist_level(static_cast<uint8_t>(rounded));
      break;
    case LV600SNumberType::NUMBER_WARM_LEVEL:
      this->parent_->set_warm_level(static_cast<uint8_t>(rounded));
      break;
    case LV600SNumberType::NUMBER_TIMER_MINUTES:
      this->parent_->set_timer_seconds(static_cast<uint32_t>(rounded) * 60);
      break;
  }
}

void LV600SNumber::update_(bool force) {
  float value;
  switch (this->type_) {
    case LV600SNumberType::NUMBER_TARGET_HUMIDITY:
      if (!this->parent_->has_status())
        return;
      value = this->parent_->get_target_humidity();
      break;
    case LV600SNumberType::NUMBER_MIST_LEVEL:
      if (!this->parent_->has_status())
        return;
      value = this->parent_->get_mist_level();
      break;
    case LV600SNumberType::NUMBER_WARM_LEVEL:
      if (!this->parent_->has_status())
        return;
      value = this->parent_->get_warm_level();
      break;
    case LV600SNumberType::NUMBER_TIMER_MINUTES:
      // Duration of the running timer, 0 when no timer is active.
      if (!this->parent_->has_status())
        return;
      value = std::round(this->parent_->get_timer_total_seconds() / 60.0f);
      break;
    default:
      return;
  }
  if (force || !this->published_ || value != this->state) {
    this->published_ = true;
    this->publish_state(value);
  }
}

}  // namespace lv600s_humidifier
}  // namespace esphome
