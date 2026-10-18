import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_HUMIDITY, UNIT_MINUTE, UNIT_PERCENT

from .. import CONF_LV600S_ID, LV600SHumidifier, LV600SNumberType, lv600s_ns

LV600SNumber = lv600s_ns.class_("LV600SNumber", number.Number, cg.Component)

CONF_TARGET_HUMIDITY = "target_humidity"
CONF_MIST_LEVEL = "mist_level"
CONF_WARM_LEVEL = "warm_level"
CONF_TIMER_MINUTES = "timer_minutes"

# key: (type, min, max, step)
TYPES = {
    CONF_TARGET_HUMIDITY: (LV600SNumberType.NUMBER_TARGET_HUMIDITY, 40, 80, 5),
    CONF_MIST_LEVEL: (LV600SNumberType.NUMBER_MIST_LEVEL, 1, 9, 1),
    CONF_WARM_LEVEL: (LV600SNumberType.NUMBER_WARM_LEVEL, 0, 3, 1),
    CONF_TIMER_MINUTES: (LV600SNumberType.NUMBER_TIMER_MINUTES, 0, 720, 1),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LV600S_ID): cv.use_id(LV600SHumidifier),
        cv.Optional(CONF_TARGET_HUMIDITY): number.number_schema(
            LV600SNumber,
            icon="mdi:water-percent",
            device_class=DEVICE_CLASS_HUMIDITY,
            unit_of_measurement=UNIT_PERCENT,
        ).extend(cv.COMPONENT_SCHEMA),
        cv.Optional(CONF_MIST_LEVEL): number.number_schema(
            LV600SNumber, icon="mdi:weather-fog"
        ).extend(cv.COMPONENT_SCHEMA),
        cv.Optional(CONF_WARM_LEVEL): number.number_schema(
            LV600SNumber, icon="mdi:heat-wave"
        ).extend(cv.COMPONENT_SCHEMA),
        cv.Optional(CONF_TIMER_MINUTES): number.number_schema(
            LV600SNumber, icon="mdi:timer-outline", unit_of_measurement=UNIT_MINUTE
        ).extend(cv.COMPONENT_SCHEMA),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_LV600S_ID])
    for key, (number_type, min_value, max_value, step) in TYPES.items():
        if conf := config.get(key):
            var = await number.new_number(
                conf, number_type, min_value=min_value, max_value=max_value, step=step
            )
            await cg.register_component(var, conf)
            await cg.register_parented(var, parent)
