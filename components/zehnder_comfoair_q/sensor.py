import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_CELSIUS,
    UNIT_CUBIC_METER_PER_HOUR,
    UNIT_KELVIN,
    UNIT_KILOWATT,
    UNIT_KILOWATT_HOURS,
    UNIT_PERCENT,
    UNIT_SECOND,
    UNIT_WATT,
)

from . import (
    CONF_ZEHNDER_COMFOAIR_Q_ID,
    ComputedSensor,
    ZehnderComfoAirQ,
)

DEPENDENCIES = ["zehnder_comfoair_q"]

# PDO values are pushed by the unit on every change, so smooth the noisy ones by default
_THROTTLE_FILTERS = [
    {"throttle_average": "10s"},
    {"filter_out": "nan"},
    {"sliding_window_moving_average": {"send_every": 1, "window_size": 3}},
]
_THROTTLE_ROUND_TENS_FILTERS = [
    *_THROTTLE_FILTERS,
    {"round_to_multiple_of": 10},
]


def _temperature_schema():
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    )


def _humidity_schema():
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_PERCENT,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_HUMIDITY,
        state_class=STATE_CLASS_MEASUREMENT,
    )


def _energy_total_schema():
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_KILOWATT_HOURS,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_ENERGY,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    )


def _avoided_power_schema():
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_KILOWATT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
        filters=_THROTTLE_FILTERS,
    )


# key -> (PDO id, schema)
PDO_SENSORS = {
    "fan_level": (
        65,
        sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    ),
    "next_fan_change_in_seconds": (
        81,
        sensor.sensor_schema(
            unit_of_measurement=UNIT_SECOND,
            accuracy_decimals=0,
        ),
    ),
    "next_bypass_change_in_seconds": (
        82,
        sensor.sensor_schema(
            unit_of_measurement=UNIT_SECOND,
            accuracy_decimals=0,
        ),
    ),
    "exhaust_fan_duty": (
        117,
        sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            filters=_THROTTLE_FILTERS,
        ),
    ),
    "supply_fan_duty": (
        118,
        sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            filters=_THROTTLE_FILTERS,
        ),
    ),
    "exhaust_fan_flow": (
        119,
        sensor.sensor_schema(
            unit_of_measurement=UNIT_CUBIC_METER_PER_HOUR,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            filters=_THROTTLE_FILTERS,
        ),
    ),
    "supply_fan_flow": (
        120,
        sensor.sensor_schema(
            unit_of_measurement=UNIT_CUBIC_METER_PER_HOUR,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            filters=_THROTTLE_FILTERS,
        ),
    ),
    "exhaust_fan_speed": (
        121,
        sensor.sensor_schema(
            unit_of_measurement="rpm",
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            filters=_THROTTLE_ROUND_TENS_FILTERS,
        ),
    ),
    "supply_fan_speed": (
        122,
        sensor.sensor_schema(
            unit_of_measurement="rpm",
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            filters=_THROTTLE_ROUND_TENS_FILTERS,
        ),
    ),
    "power_consumption_current": (
        128,
        sensor.sensor_schema(
            unit_of_measurement=UNIT_WATT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
            filters=_THROTTLE_FILTERS,
        ),
    ),
    "power_consumption_ytd": (129, _energy_total_schema()),
    "power_consumption_since_start": (130, _energy_total_schema()),
    "pre_heater_power_cons_current": (
        144,
        sensor.sensor_schema(
            unit_of_measurement=UNIT_WATT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
            filters=_THROTTLE_FILTERS,
        ),
    ),
    "pre_heater_power_cons_ytd": (145, _energy_total_schema()),
    "pre_heater_power_cons_since_start": (146, _energy_total_schema()),
    "filter_replacement_remaining_days": (
        192,
        sensor.sensor_schema(
            unit_of_measurement="d",
            accuracy_decimals=0,
        ),
    ),
    "running_mean_outdoor_temp": (209, _temperature_schema()),
    "profile_target_temp": (212, _temperature_schema()),
    "avoided_heating_actual": (213, _avoided_power_schema()),
    "avoided_heating_ytd": (214, _energy_total_schema()),
    "avoided_heating_total": (215, _energy_total_schema()),
    "avoided_cooling_actual": (216, _avoided_power_schema()),
    "avoided_cooling_ytd": (217, _energy_total_schema()),
    "avoided_cooling_total": (218, _energy_total_schema()),
    "pre_heater_temp_before": (220, _temperature_schema()),
    "post_heater_temp_after": (221, _temperature_schema()),
    # semantics of 225/226 are not fully confirmed (see PROTOCOL-PDO.md):
    # 225 looks like a sensor-based ventilation mode, 226 like the fan speed
    # target modulated by the comfort functions (0-300 scale) — please report
    "sensor_ventilation_mode": (
        225,
        sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    ),
    "fan_speed_modulated": (
        226,
        sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            filters=_THROTTLE_FILTERS,
        ),
    ),
    "bypass_state": (
        227,
        sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    ),
    "extract_air_temp": (274, _temperature_schema()),
    "exhaust_air_temp": (275, _temperature_schema()),
    "outdoor_air_temp": (276, _temperature_schema()),
    "pre_heater_temp_after": (277, _temperature_schema()),
    "supply_air_temp": (278, _temperature_schema()),
    "extract_air_humidity": (290, _humidity_schema()),
    "exhaust_air_humidity": (291, _humidity_schema()),
    "outdoor_air_humidity": (292, _humidity_schema()),
    "pre_heater_humidity_after": (293, _humidity_schema()),
    "supply_air_humidity": (294, _humidity_schema()),
    "ghe_outdoor_temp": (416, _temperature_schema()),
    "ghe_sole_temp": (417, _temperature_schema()),
    "ghe_state": (
        418,
        sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    ),
}


def _temperature_diff_schema():
    # deliberately no temperature device_class: HA's automatic unit conversion
    # would apply the K -> degC offset, which is wrong for a difference
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_KELVIN,
        accuracy_decimals=1,
        state_class=STATE_CLASS_MEASUREMENT,
        icon="mdi:delta",
    )


def _recovery_ratio_schema():
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_PERCENT,
        accuracy_decimals=1,
        state_class=STATE_CLASS_MEASUREMENT,
    )


def _thermal_power_schema():
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_WATT,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
    )


# key -> (ComputedSensor kind, schema); derived from other PDO values, updated every 60s
COMPUTED_SENSORS = {
    "indoor_air_temp_diff": (
        ComputedSensor.INDOOR_AIR_TEMP_DIFF,
        _temperature_diff_schema(),
    ),
    "outdoor_air_temp_diff": (
        ComputedSensor.OUTDOOR_AIR_TEMP_DIFF,
        _temperature_diff_schema(),
    ),
    "heat_recovery_ratio": (
        ComputedSensor.HEAT_RECOVERY_RATIO,
        _recovery_ratio_schema(),
    ),
    # only meaningful on units with an enthalpy exchanger (ERV)
    "enthalpy_recovery_ratio": (
        ComputedSensor.ENTHALPY_RECOVERY_RATIO,
        _recovery_ratio_schema(),
    ),
    "humidity_recovery_ratio": (
        ComputedSensor.HUMIDITY_RECOVERY_RATIO,
        _recovery_ratio_schema(),
    ),
    # thermal power the ventilation adds to (+) or removes from (-) the rooms
    "supply_thermal_power": (
        ComputedSensor.SUPPLY_THERMAL_POWER,
        _thermal_power_schema(),
    ),
    # heat lost to the outside despite the exchanger
    "ventilation_heat_loss": (
        ComputedSensor.VENTILATION_HEAT_LOSS,
        _thermal_power_schema(),
    ),
    # power recovered as moisture; only meaningful on ERV units
    "latent_recovery_power": (
        ComputedSensor.LATENT_RECOVERY_POWER,
        _thermal_power_schema(),
    ),
    # electrical power per air volume; rises when the filters clog
    "specific_fan_power": (
        ComputedSensor.SPECIFIC_FAN_POWER,
        sensor.sensor_schema(
            unit_of_measurement="Wh/m³",
            accuracy_decimals=2,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:air-filter",
        ),
    ),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ZEHNDER_COMFOAIR_Q_ID): cv.use_id(ZehnderComfoAirQ),
        **{cv.Optional(key): schema for key, (_, schema) in PDO_SENSORS.items()},
        **{cv.Optional(key): schema for key, (_, schema) in COMPUTED_SENSORS.items()},
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ZEHNDER_COMFOAIR_Q_ID])
    for key, (pdo_id, _) in PDO_SENSORS.items():
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(hub.register_sensor(pdo_id, sens))
    for key, (kind, _) in COMPUTED_SENSORS.items():
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(hub.register_computed_sensor(kind, sens))
