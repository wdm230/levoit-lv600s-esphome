#pragma once

#include "esphome/core/component.h"
#include "esphome/components/number/number.h"
#include "../lv600s_humidifier.h"

namespace esphome {
namespace lv600s_humidifier {

class LV600SNumber : public number::Number, public Component, public Parented<LV600SHumidifier> {
 public:
  explicit LV600SNumber(LV600SNumberType type) : type_(type) {}
  void setup() override;
  void dump_config() override;

 protected:
  // The new value is published once the MCU reports it, not optimistically.
  void control(float value) override;
  void update_(bool force);

  LV600SNumberType type_;
  bool published_{false};
};

}  // namespace lv600s_humidifier
}  // namespace esphome
