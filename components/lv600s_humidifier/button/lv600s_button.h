#pragma once

#include "esphome/components/button/button.h"
#include "../lv600s_humidifier.h"

namespace esphome {
namespace lv600s_humidifier {

class LV600SButton : public button::Button, public Parented<LV600SHumidifier> {
 public:
  explicit LV600SButton(LV600SButtonType type) : type_(type) {}

 protected:
  void press_action() override {
    switch (this->type_) {
      case LV600SButtonType::BUTTON_QUERY_STATUS:
        this->parent_->query_status();
        break;
      case LV600SButtonType::BUTTON_CLEAR_TIMER:
        this->parent_->clear_timer();
        break;
      case LV600SButtonType::BUTTON_START_HOTSPOT:
        this->parent_->start_hotspot();
        break;
    }
  }

  LV600SButtonType type_;
};

}  // namespace lv600s_humidifier
}  // namespace esphome
