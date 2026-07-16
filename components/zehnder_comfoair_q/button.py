import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button

from . import (
    CONF_ZEHNDER_COMFOAIR_Q_ID,
    ZehnderComfoAirQ,
    zehnder_comfoair_q_ns,
)

DEPENDENCIES = ["zehnder_comfoair_q"]

ComfoAirQButton = zehnder_comfoair_q_ns.class_("ComfoAirQButton", button.Button)
ButtonAction = zehnder_comfoair_q_ns.enum("ButtonAction", is_class=True)

# key -> (action, value); OffAutoOn values: 0 = off, 1 = auto, 2 = on
BUTTONS = {
    "boost_15min": (ButtonAction.BOOST, 15 * 60),
    "boost_30min": (ButtonAction.BOOST, 30 * 60),
    "boost_off": (ButtonAction.BOOST, 0),
    "fan_level_0": (ButtonAction.FAN_LEVEL, 0),
    "fan_level_1": (ButtonAction.FAN_LEVEL, 1),
    "fan_level_2": (ButtonAction.FAN_LEVEL, 2),
    "fan_level_3": (ButtonAction.FAN_LEVEL, 3),
    "manual_mode_on": (ButtonAction.MANUAL_MODE, 1),
    "manual_mode_off": (ButtonAction.MANUAL_MODE, 0),
    "passive_temperature_auto": (ButtonAction.PASSIVE_TEMPERATURE, 1),
    "passive_temperature_off": (ButtonAction.PASSIVE_TEMPERATURE, 0),
    "humidity_comfort_auto": (ButtonAction.HUMIDITY_COMFORT, 1),
    "humidity_comfort_off": (ButtonAction.HUMIDITY_COMFORT, 0),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ZEHNDER_COMFOAIR_Q_ID): cv.use_id(ZehnderComfoAirQ),
        **{
            cv.Optional(key): button.button_schema(ComfoAirQButton)
            for key in BUTTONS
        },
    }
)


async def to_code(config):
    for key, (action, value) in BUTTONS.items():
        if key in config:
            var = await button.new_button(config[key])
            await cg.register_parented(var, config[CONF_ZEHNDER_COMFOAIR_Q_ID])
            cg.add(var.set_action(action, value))
