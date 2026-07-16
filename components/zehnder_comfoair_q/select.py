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

UNIT_TEMPHUMCONTROL = 0x1D

# key -> (purpose, options, state source, icon)
# State source is either ("pdo", pdo_id) or ("property", unit, subunit, property).
# Option order must match the PDO / property value range (0-based). Property
# state is read back from the unit on boot, per request cycle and after a set.
SELECTS = {
    "fan_level": (
        SelectPurpose.FAN_LEVEL,
        ["0", "1", "2", "3"],
        ("pdo", 65),
        "mdi:fan",
    ),
    "bypass_mode": (
        SelectPurpose.BYPASS_MODE,
        ["Auto", "Activated", "Deactivated"],
        ("pdo", 66),
        "mdi:valve",
    ),
    "temperature_profile": (
        SelectPurpose.TEMPERATURE_PROFILE,
        ["Normal", "Cold", "Warm"],
        ("pdo", 67),
        "mdi:thermometer-lines",
    ),
    "passive_temperature": (
        SelectPurpose.PASSIVE_TEMPERATURE,
        ["Off", "Auto", "On"],
        ("property", UNIT_TEMPHUMCONTROL, 0x01, 0x04),
        "mdi:home-thermometer",
    ),
    "humidity_comfort": (
        SelectPurpose.HUMIDITY_COMFORT,
        ["Off", "Auto", "On"],
        ("property", UNIT_TEMPHUMCONTROL, 0x01, 0x06),
        "mdi:water-percent",
    ),
    "humidity_protection": (
        SelectPurpose.HUMIDITY_PROTECTION,
        ["Off", "Auto", "On"],
        ("property", UNIT_TEMPHUMCONTROL, 0x01, 0x07),
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
    for key, (purpose, options, state_source, _) in SELECTS.items():
        if key in config:
            var = await select.new_select(config[key], options=options)
            await cg.register_parented(var, config[CONF_ZEHNDER_COMFOAIR_Q_ID])
            cg.add(var.set_purpose(purpose))
            if state_source[0] == "pdo":
                cg.add(hub.register_select(state_source[1], var))
            else:
                cg.add(hub.register_property_select(*state_source[1:], var))
