import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv

from .. import CONF_LV600S_ID, LV600SHumidifier, LV600SSwitchType, lv600s_ns

LV600SSwitch = lv600s_ns.class_("LV600SSwitch", switch.Switch, cg.Component)

CONF_POWER = "power"
CONF_DISPLAY = "display"

TYPES = {
    CONF_POWER: LV600SSwitchType.SWITCH_POWER,
    CONF_DISPLAY: LV600SSwitchType.SWITCH_DISPLAY,
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LV600S_ID): cv.use_id(LV600SHumidifier),
        cv.Optional(CONF_POWER): switch.switch_schema(
            LV600SSwitch, icon="mdi:power", default_restore_mode="DISABLED"
        ).extend(cv.COMPONENT_SCHEMA),
        cv.Optional(CONF_DISPLAY): switch.switch_schema(
            LV600SSwitch, icon="mdi:monitor", default_restore_mode="DISABLED"
        ).extend(cv.COMPONENT_SCHEMA),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_LV600S_ID])
    for key, switch_type in TYPES.items():
        if conf := config.get(key):
            var = await switch.new_switch(conf, switch_type)
            await cg.register_component(var, conf)
            await cg.register_parented(var, parent)
