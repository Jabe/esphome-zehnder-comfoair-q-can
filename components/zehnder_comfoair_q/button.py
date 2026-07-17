import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import CONF_DURATION, CONF_LEVEL

from . import (
    CONF_ZEHNDER_COMFOAIR_Q_ID,
    ZehnderComfoAirQ,
    zehnder_comfoair_q_ns,
)

DEPENDENCIES = ["zehnder_comfoair_q"]

ComfoAirQFanLevelTimerButton = zehnder_comfoair_q_ns.class_(
    "ComfoAirQFanLevelTimerButton", button.Button
)

CONF_FAN_LEVEL_TIMERS = "fan_level_timers"
CONF_FAN_LEVEL_TIMER_OFF = "fan_level_timer_off"

# Fan level timer buttons ("Party Timer" on the unit's display): run a fan
# level for an explicit duration, with a countdown on the display. The classic
# boost is simply level 3. fan_level_timer_off cancels a running timer.
_TIMER_SCHEMA = button.button_schema(
    ComfoAirQFanLevelTimerButton, icon="mdi:fan-clock"
).extend(
    {
        cv.Optional(CONF_LEVEL, default=3): cv.int_range(min=0, max=3),
        cv.Required(CONF_DURATION): cv.positive_time_period_seconds,
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ZEHNDER_COMFOAIR_Q_ID): cv.use_id(ZehnderComfoAirQ),
        cv.Optional(CONF_FAN_LEVEL_TIMERS): cv.ensure_list(_TIMER_SCHEMA),
        cv.Optional(CONF_FAN_LEVEL_TIMER_OFF): button.button_schema(
            ComfoAirQFanLevelTimerButton, icon="mdi:fan-off"
        ),
    }
)


async def to_code(config):
    for conf in config.get(CONF_FAN_LEVEL_TIMERS, []):
        var = await button.new_button(conf)
        await cg.register_parented(var, config[CONF_ZEHNDER_COMFOAIR_Q_ID])
        cg.add(var.set_timer(conf[CONF_LEVEL], conf[CONF_DURATION]))
    if timer_off := config.get(CONF_FAN_LEVEL_TIMER_OFF):
        var = await button.new_button(timer_off)
        await cg.register_parented(var, config[CONF_ZEHNDER_COMFOAIR_Q_ID])
        cg.add(var.set_timer(0, 0))
