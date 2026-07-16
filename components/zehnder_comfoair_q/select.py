import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select

from . import (
    CONF_ZEHNDER_COMFOAIR_Q_ID,
    ZehnderComfoAirQ,
    zehnder_comfoair_q_ns,
)

DEPENDENCIES = ["zehnder_comfoair_q"]

ComfoAirQSelect = zehnder_comfoair_q_ns.class_("ComfoAirQSelect", select.Select)
SelectPurpose = zehnder_comfoair_q_ns.enum("SelectPurpose", is_class=True)

# key -> (purpose, options, state PDO id or None, icon)
# Option order must match the PDO / enum value range (0-based). Selects without
# a state PDO (RMI properties) publish optimistically after sending.
SELECTS = {
    "fan_level": (
        SelectPurpose.FAN_LEVEL,
        ["0", "1", "2", "3"],
        65,
        "mdi:fan",
    ),
    "bypass_mode": (
        SelectPurpose.BYPASS_MODE,
        ["Auto", "Activated", "Deactivated"],
        66,
        "mdi:valve",
    ),
    "temperature_profile": (
        SelectPurpose.TEMPERATURE_PROFILE,
        ["Normal", "Cold", "Warm"],
        67,
        "mdi:thermometer-lines",
    ),
    "passive_temperature": (
        SelectPurpose.PASSIVE_TEMPERATURE,
        ["Off", "Auto", "On"],
        None,
        "mdi:home-thermometer",
    ),
    "humidity_comfort": (
        SelectPurpose.HUMIDITY_COMFORT,
        ["Off", "Auto", "On"],
        None,
        "mdi:water-percent",
    ),
    "humidity_protection": (
        SelectPurpose.HUMIDITY_PROTECTION,
        ["Off", "Auto", "On"],
        None,
        "mdi:water-alert",
    ),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ZEHNDER_COMFOAIR_Q_ID): cv.use_id(ZehnderComfoAirQ),
        **{
            cv.Optional(key): select.select_schema(ComfoAirQSelect, icon=icon)
            for key, (_, _, _, icon) in SELECTS.items()
        },
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ZEHNDER_COMFOAIR_Q_ID])
    for key, (purpose, options, pdo_id, _) in SELECTS.items():
        if key in config:
            var = await select.new_select(config[key], options=options)
            await cg.register_parented(var, config[CONF_ZEHNDER_COMFOAIR_Q_ID])
            cg.add(var.set_purpose(purpose))
            if pdo_id is not None:
                cg.add(hub.register_select(pdo_id, var))
