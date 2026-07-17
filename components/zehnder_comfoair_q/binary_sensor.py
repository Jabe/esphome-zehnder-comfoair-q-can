import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor

from . import CONF_ZEHNDER_COMFOAIR_Q_ID, ZehnderComfoAirQ

DEPENDENCIES = ["zehnder_comfoair_q"]

# key -> PDO id
BINARY_SENSORS = {
    "away_indicator": 16,
    "heating_season": 210,
    "cooling_season": 211,
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ZEHNDER_COMFOAIR_Q_ID): cv.use_id(ZehnderComfoAirQ),
        **{
            cv.Optional(key): binary_sensor.binary_sensor_schema()
            for key in BINARY_SENSORS
        },
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ZEHNDER_COMFOAIR_Q_ID])
    for key, pdo_id in BINARY_SENSORS.items():
        if key in config:
            bsens = await binary_sensor.new_binary_sensor(config[key])
            cg.add(hub.register_binary_sensor(pdo_id, bsens))
