import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, sensor, text_sensor, uart
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_PROBLEM,
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    UNIT_SECOND,
)

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["binary_sensor", "sensor", "text_sensor"]
CODEOWNERS = ["@codex"]

lv600s_ns = cg.esphome_ns.namespace("lv600s_humidifier")
LV600SHumidifier = lv600s_ns.class_(
    "LV600SHumidifier", cg.Component, uart.UARTDevice
)

CONF_STATUS_INTERVAL = "status_interval"
CONF_POWER = "power"
CONF_DISPLAY = "display"
CONF_WATER_LACKS = "water_lacks"
CONF_TANK_REMOVED = "tank_removed"
CONF_HUMIDIFYING = "humidifying"
CONF_CURRENT_HUMIDITY = "current_humidity"
CONF_CURRENT_TEMPERATURE = "current_temperature"
CONF_TARGET_HUMIDITY = "target_humidity"
CONF_MIST_LEVEL = "mist_level"
CONF_WARM_LEVEL = "warm_level"
CONF_TIMER_REMAINING = "timer_remaining"
CONF_MODE = "mode"
CONF_FOG_STATUS = "fog_status"
CONF_CONTAINER_STATE = "container_state"
CONF_OTHER_EXCEPTION = "other_exception"
CONF_MCU_VERSION = "mcu_version"
CONF_LAST_FRAME = "last_frame"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(LV600SHumidifier),
        cv.Optional(CONF_STATUS_INTERVAL, default="5s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_POWER): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_POWER,
        ),
        cv.Optional(CONF_DISPLAY): binary_sensor.binary_sensor_schema(),
        cv.Optional(CONF_WATER_LACKS): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_PROBLEM,
        ),
        cv.Optional(CONF_TANK_REMOVED): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_PROBLEM,
        ),
        cv.Optional(CONF_HUMIDIFYING): binary_sensor.binary_sensor_schema(),
        cv.Optional(CONF_CURRENT_HUMIDITY): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_HUMIDITY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_CURRENT_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_TARGET_HUMIDITY): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_HUMIDITY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_MIST_LEVEL): sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_WARM_LEVEL): sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_TIMER_REMAINING): sensor.sensor_schema(
            unit_of_measurement=UNIT_SECOND,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_MODE): sensor.sensor_schema(
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_FOG_STATUS): sensor.sensor_schema(
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_CONTAINER_STATE): sensor.sensor_schema(
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_OTHER_EXCEPTION): sensor.sensor_schema(
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_MCU_VERSION): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_LAST_FRAME): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
).extend(cv.COMPONENT_SCHEMA).extend(uart.UART_DEVICE_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_status_interval(config[CONF_STATUS_INTERVAL].total_milliseconds))

    if CONF_POWER in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_POWER])
        cg.add(var.set_power_sensor(sens))
    if CONF_DISPLAY in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_DISPLAY])
        cg.add(var.set_display_sensor(sens))
    if CONF_WATER_LACKS in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_WATER_LACKS])
        cg.add(var.set_water_lacks_sensor(sens))
    if CONF_TANK_REMOVED in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_TANK_REMOVED])
        cg.add(var.set_tank_removed_sensor(sens))
    if CONF_HUMIDIFYING in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_HUMIDIFYING])
        cg.add(var.set_humidifying_sensor(sens))

    if CONF_CURRENT_HUMIDITY in config:
        sens = await sensor.new_sensor(config[CONF_CURRENT_HUMIDITY])
        cg.add(var.set_current_humidity_sensor(sens))
    if CONF_CURRENT_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_CURRENT_TEMPERATURE])
        cg.add(var.set_current_temperature_sensor(sens))
    if CONF_TARGET_HUMIDITY in config:
        sens = await sensor.new_sensor(config[CONF_TARGET_HUMIDITY])
        cg.add(var.set_target_humidity_sensor(sens))
    if CONF_MIST_LEVEL in config:
        sens = await sensor.new_sensor(config[CONF_MIST_LEVEL])
        cg.add(var.set_mist_level_sensor(sens))
    if CONF_WARM_LEVEL in config:
        sens = await sensor.new_sensor(config[CONF_WARM_LEVEL])
        cg.add(var.set_warm_level_sensor(sens))
    if CONF_TIMER_REMAINING in config:
        sens = await sensor.new_sensor(config[CONF_TIMER_REMAINING])
        cg.add(var.set_timer_remaining_sensor(sens))
    if CONF_MODE in config:
        sens = await sensor.new_sensor(config[CONF_MODE])
        cg.add(var.set_mode_sensor(sens))
    if CONF_FOG_STATUS in config:
        sens = await sensor.new_sensor(config[CONF_FOG_STATUS])
        cg.add(var.set_fog_status_sensor(sens))
    if CONF_CONTAINER_STATE in config:
        sens = await sensor.new_sensor(config[CONF_CONTAINER_STATE])
        cg.add(var.set_container_state_sensor(sens))
    if CONF_OTHER_EXCEPTION in config:
        sens = await sensor.new_sensor(config[CONF_OTHER_EXCEPTION])
        cg.add(var.set_other_exception_sensor(sens))

    if CONF_MCU_VERSION in config:
        sens = await text_sensor.new_text_sensor(config[CONF_MCU_VERSION])
        cg.add(var.set_mcu_version_sensor(sens))
    if CONF_LAST_FRAME in config:
        sens = await text_sensor.new_text_sensor(config[CONF_LAST_FRAME])
        cg.add(var.set_last_frame_sensor(sens))
