import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor

from . import CONF_ZEHNDER_COMFOAIR_Q_ID, ZehnderComfoAirQ

DEPENDENCIES = ["zehnder_comfoair_q"]

# key -> PDO id; values are mapped to strings inside the component
TEXT_SENSORS = {
    "operating_mode": 49,
    "bypass_activation_mode": 66,
    "temperature_profile": 67,
    "next_fan_change_in": 81,
    "next_bypass_change_in": 82,
    # decoded 64-bit bitset: which comfort functions / constraints currently
    # steer the ventilation (e.g. "TemperatureComfort, HumidityComfort")
    "airflow_constraints": 230,
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ZEHNDER_COMFOAIR_Q_ID): cv.use_id(ZehnderComfoAirQ),
        **{
            cv.Optional(key): text_sensor.text_sensor_schema()
            for key in TEXT_SENSORS
        },
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ZEHNDER_COMFOAIR_Q_ID])
    for key, pdo_id in TEXT_SENSORS.items():
        if key in config:
            tsens = await text_sensor.new_text_sensor(config[key])
            cg.add(hub.register_text_sensor(pdo_id, tsens))
