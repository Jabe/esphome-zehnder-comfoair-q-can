import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import (
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_CONFIG,
    UNIT_CELSIUS,
)

from . import (
    CONF_ZEHNDER_COMFOAIR_Q_ID,
    ZehnderComfoAirQ,
    zehnder_comfoair_q_ns,
)

DEPENDENCIES = ["zehnder_comfoair_q"]

ComfoAirQPropertyNumber = zehnder_comfoair_q_ns.class_(
    "ComfoAirQPropertyNumber", number.Number
)

UNIT_TEMPHUMCONTROL = 0x1D

# key -> (unit, subunit, property, scale, min, max, step)
# Temperature profile target temperatures (INT16, 0.1 degC), see the
# TEMPHUMCONTROL unit in the comfoconnect PROTOCOL-RMI docs.
NUMBERS = {
    "profile_target_warm": (UNIT_TEMPHUMCONTROL, 0x01, 0x0A, 0.1, 15.0, 28.0, 0.5),
    "profile_target_normal": (UNIT_TEMPHUMCONTROL, 0x01, 0x0B, 0.1, 15.0, 28.0, 0.5),
    "profile_target_cool": (UNIT_TEMPHUMCONTROL, 0x01, 0x0C, 0.1, 15.0, 28.0, 0.5),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ZEHNDER_COMFOAIR_Q_ID): cv.use_id(ZehnderComfoAirQ),
        **{
            cv.Optional(key): number.number_schema(
                ComfoAirQPropertyNumber,
                unit_of_measurement=UNIT_CELSIUS,
                device_class=DEVICE_CLASS_TEMPERATURE,
                entity_category=ENTITY_CATEGORY_CONFIG,
                icon="mdi:thermometer-check",
            )
            for key in NUMBERS
        },
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ZEHNDER_COMFOAIR_Q_ID])
    for key, (unit, subunit, prop, scale, min_v, max_v, step) in NUMBERS.items():
        if key in config:
            var = await number.new_number(
                config[key], min_value=min_v, max_value=max_v, step=step
            )
            await cg.register_parented(var, config[CONF_ZEHNDER_COMFOAIR_Q_ID])
            cg.add(var.set_property(unit, subunit, prop, scale))
            cg.add(hub.register_property_number(unit, subunit, prop, var, scale))
