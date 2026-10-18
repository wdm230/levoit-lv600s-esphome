#pragma once

#include "esphome/core/component.h"
#include "esphome/components/select/select.h"
#include "../lv600s_humidifier.h"

namespace esphome {
namespace lv600s_humidifier {

class LV600SModeSelect : public select::Select, public Component, public Parented<LV600SHumidifier> {
 public:
  void setup() override;
  void dump_config() override;

 protected:
  void control(size_t index) override;
  void update_(bool force);

  bool published_{false};
  size_t last_index_{0};
};

}  // namespace lv600s_humidifier
}  // namespace esphome
