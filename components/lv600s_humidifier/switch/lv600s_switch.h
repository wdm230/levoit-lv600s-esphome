#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "../lv600s_humidifier.h"

namespace esphome {
namespace lv600s_humidifier {

class LV600SSwitch : public switch_::Switch, public Component, public Parented<LV600SHumidifier> {
 public:
  explicit LV600SSwitch(LV600SSwitchType type) : type_(type) {}
  void setup() override;
  void dump_config() override;

 protected:
  // The new state is published once the MCU reports it, not optimistically.
  void write_state(bool state) override;
  void update_(bool force);

  LV600SSwitchType type_;
  bool published_{false};
};

}  // namespace lv600s_humidifier
}  // namespace esphome
