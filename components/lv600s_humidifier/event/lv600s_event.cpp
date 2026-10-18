#include "lv600s_event.h"

#include "esphome/core/log.h"

namespace esphome {
namespace lv600s_humidifier {

static const char *const TAG = "lv600s_humidifier.event";

void LV600SPanelEvent::setup() {
  this->parent_->add_on_panel_button_callback([this](uint8_t action) {
    switch (static_cast<PanelButtonAction>(action)) {
      case PanelButtonAction::PANEL_ACTION_1:
        this->trigger("action_1");
        break;
      case PanelButtonAction::PANEL_ACTION_2:
        this->trigger("action_2");
        break;
      case PanelButtonAction::PANEL_PAIR_WIFI:
        this->trigger("pair_wifi");
        break;
      case PanelButtonAction::PANEL_FACTORY_RESET:
        this->trigger("factory_reset");
        break;
      default:
        ESP_LOGD(TAG, "Unknown panel button action %u", action);
        break;
    }
  });
}

void LV600SPanelEvent::dump_config() { LOG_EVENT("", "LV600S Panel Button", this); }

}  // namespace lv600s_humidifier
}  // namespace esphome
