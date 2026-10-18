import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG, ENTITY_CATEGORY_DIAGNOSTIC

from .. import CONF_LV600S_ID, LV600SButtonType, LV600SHumidifier, lv600s_ns

LV600SButton = lv600s_ns.class_("LV600SButton", button.Button)

CONF_QUERY_STATUS = "query_status"
CONF_CLEAR_TIMER = "clear_timer"
CONF_START_HOTSPOT = "start_hotspot"

TYPES = {
    CONF_QUERY_STATUS: LV600SButtonType.BUTTON_QUERY_STATUS,
    CONF_CLEAR_TIMER: LV600SButtonType.BUTTON_CLEAR_TIMER,
    CONF_START_HOTSPOT: LV600SButtonType.BUTTON_START_HOTSPOT,
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LV600S_ID): cv.use_id(LV600SHumidifier),
        cv.Optional(CONF_QUERY_STATUS): button.button_schema(
            LV600SButton, icon="mdi:refresh", entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
        cv.Optional(CONF_CLEAR_TIMER): button.button_schema(
            LV600SButton, icon="mdi:timer-off-outline"
        ),
        cv.Optional(CONF_START_HOTSPOT): button.button_schema(
            LV600SButton, icon="mdi:access-point", entity_category=ENTITY_CATEGORY_CONFIG
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_LV600S_ID])
    for key, button_type in TYPES.items():
        if conf := config.get(key):
            var = await button.new_button(conf, button_type)
            await cg.register_parented(var, parent)
