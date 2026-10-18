import esphome.codegen as cg
from esphome.components import event
import esphome.config_validation as cv

from .. import CONF_LV600S_ID, LV600SHumidifier, lv600s_ns

LV600SPanelEvent = lv600s_ns.class_("LV600SPanelEvent", event.Event, cg.Component)

CONF_PANEL_BUTTON = "panel_button"

# Panel button actions reported by the MCU (0xD102/0xD103); stock firmware starts Wi-Fi pairing on 3 and
# performs a factory reset on 4. The meaning of 1 and 2 is not known.
EVENT_TYPES = ["action_1", "action_2", "pair_wifi", "factory_reset"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LV600S_ID): cv.use_id(LV600SHumidifier),
        cv.Optional(CONF_PANEL_BUTTON): event.event_schema(
            LV600SPanelEvent, icon="mdi:gesture-tap-button"
        ).extend(cv.COMPONENT_SCHEMA),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_LV600S_ID])
    if conf := config.get(CONF_PANEL_BUTTON):
        var = await event.new_event(conf, event_types=EVENT_TYPES)
        await cg.register_component(var, conf)
        await cg.register_parented(var, parent)
