import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import (
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_CONFIG,
    UNIT_CELSIUS,
    UNIT_CUBIC_METER_PER_HOUR,
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
UNIT_VENTILATIONCONFIG = 0x1E

# key -> (unit, subunit, property, scale, min, max, step)
# Temperature profile target temperatures (INT16, 0.1 degC), see the
# TEMPHUMCONTROL unit in the comfoconnect PROTOCOL-RMI docs.
# Note: in the (default) adaptive temperature profile mode the unit derives
# its active setpoint from the running mean outdoor temperature — these
# targets only take direct effect in fixed profile mode.
TEMPERATURE_NUMBERS = {
    "profile_target_warm": (UNIT_TEMPHUMCONTROL, 0x01, 0x0A, 0.1, 15.0, 28.0, 0.5),
    "profile_target_normal": (UNIT_TEMPHUMCONTROL, 0x01, 0x0B, 0.1, 15.0, 28.0, 0.5),
    "profile_target_cool": (UNIT_TEMPHUMCONTROL, 0x01, 0x0C, 0.1, 15.0, 28.0, 0.5),
}

# Fan flow setpoints per ventilation level (INT16, m³/h): the constant-volume
# target the unit regulates each level to (level 0 = away). Level 3 is also
# what the native boost ("Party Timer") runs. The unit persists these values
# and clamps them to the model's range — the read-back shows what it accepted.
# Meant for tuning levels (e.g. for noise), not for continuous automation.
FLOW_NUMBERS = {
    "fan_flow_level_0": (UNIT_VENTILATIONCONFIG, 0x01, 0x03, 1.0, 30.0, 600.0, 5.0),
    "fan_flow_level_1": (UNIT_VENTILATIONCONFIG, 0x01, 0x04, 1.0, 30.0, 600.0, 5.0),
    "fan_flow_level_2": (UNIT_VENTILATIONCONFIG, 0x01, 0x05, 1.0, 30.0, 600.0, 5.0),
    "fan_flow_level_3": (UNIT_VENTILATIONCONFIG, 0x01, 0x06, 1.0, 30.0, 600.0, 5.0),
}

NUMBERS = {**TEMPERATURE_NUMBERS, **FLOW_NUMBERS}

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
            for key in TEMPERATURE_NUMBERS
        },
        **{
            cv.Optional(key): number.number_schema(
                ComfoAirQPropertyNumber,
                unit_of_measurement=UNIT_CUBIC_METER_PER_HOUR,
                entity_category=ENTITY_CATEGORY_CONFIG,
                icon="mdi:fan",
            )
            for key in FLOW_NUMBERS
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
