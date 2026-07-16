import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch

from . import (
    CONF_ZEHNDER_COMFOAIR_Q_ID,
    ZehnderComfoAirQ,
    zehnder_comfoair_q_ns,
)

DEPENDENCIES = ["zehnder_comfoair_q"]

ComfoAirQManualModeSwitch = zehnder_comfoair_q_ns.class_(
    "ComfoAirQManualModeSwitch", switch.Switch
)

# key -> (class, state PDO id, icon)
SWITCHES = {
    "manual_mode": (ComfoAirQManualModeSwitch, 49, "mdi:hand-back-right"),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ZEHNDER_COMFOAIR_Q_ID): cv.use_id(ZehnderComfoAirQ),
        **{
            cv.Optional(key): switch.switch_schema(class_, icon=icon)
            for key, (class_, _, icon) in SWITCHES.items()
        },
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ZEHNDER_COMFOAIR_Q_ID])
    for key, (_, pdo_id, _) in SWITCHES.items():
        if key in config:
            var = await switch.new_switch(config[key])
            await cg.register_parented(var, config[CONF_ZEHNDER_COMFOAIR_Q_ID])
            cg.add(hub.register_switch(pdo_id, var))
