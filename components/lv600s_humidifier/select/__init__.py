import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv

from .. import CONF_LV600S_ID, LV600SHumidifier, lv600s_ns

LV600SModeSelect = lv600s_ns.class_("LV600SModeSelect", select.Select, cg.Component)

CONF_MODE = "mode"

# Option index == raw MCU mode byte (status offset 11).
MODE_OPTIONS = ["Auto", "Manual", "Sleep", "Target Humidity"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LV600S_ID): cv.use_id(LV600SHumidifier),
        cv.Optional(CONF_MODE): select.select_schema(
            LV600SModeSelect, icon="mdi:tune-variant"
        ).extend(cv.COMPONENT_SCHEMA),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_LV600S_ID])
    if conf := config.get(CONF_MODE):
        var = await select.new_select(conf, options=MODE_OPTIONS)
        await cg.register_component(var, conf)
        await cg.register_parented(var, parent)
