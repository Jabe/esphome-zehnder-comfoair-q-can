import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button

from . import (
    CONF_ZEHNDER_COMFOAIR_Q_ID,
    ZehnderComfoAirQ,
    zehnder_comfoair_q_ns,
)

DEPENDENCIES = ["zehnder_comfoair_q"]

ComfoAirQBoostButton = zehnder_comfoair_q_ns.class_(
    "ComfoAirQBoostButton", button.Button
)

# key -> boost duration in seconds (0 = off). Fan level, manual mode and the
# temperature/humidity settings are select/switch entities with state sync.
BUTTONS = {
    "boost_15min": 15 * 60,
    "boost_30min": 30 * 60,
    "boost_off": 0,
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ZEHNDER_COMFOAIR_Q_ID): cv.use_id(ZehnderComfoAirQ),
        **{
            cv.Optional(key): button.button_schema(
                ComfoAirQBoostButton, icon="mdi:fan-plus"
            )
            for key in BUTTONS
        },
    }
)


async def to_code(config):
    for key, duration_secs in BUTTONS.items():
        if key in config:
            var = await button.new_button(config[key])
            await cg.register_parented(var, config[CONF_ZEHNDER_COMFOAIR_Q_ID])
            cg.add(var.set_duration(duration_secs))
