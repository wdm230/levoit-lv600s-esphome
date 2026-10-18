#pragma once

#include "esphome/core/component.h"
#include "esphome/components/event/event.h"
#include "../lv600s_humidifier.h"

namespace esphome {
namespace lv600s_humidifier {

class LV600SPanelEvent : public event::Event, public Component, public Parented<LV600SHumidifier> {
 public:
  void setup() override;
  void dump_config() override;
};

}  // namespace lv600s_humidifier
}  // namespace esphome
