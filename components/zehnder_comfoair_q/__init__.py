import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.canbus import CONF_CANBUS_ID, CanbusComponent, CanSpeed
from esphome.const import CONF_ID

CODEOWNERS = ["@felixstorm"]
DEPENDENCIES = ["canbus"]

CONF_ZEHNDER_COMFOAIR_Q_ID = "zehnder_comfoair_q_id"

# we don't use UPDATE_INTERVAL as it's only the requests we send, but updates can come in any time (if the value changes)
CONF_REQUEST_INTERVAL = "request_interval"
CONF_REQUEST_IDS = "request_ids"
CONF_REQUEST_DELAY = "request_delay"
CONF_LOCAL_NODE_ID = "local_node_id"
# RMI properties (e.g. the temperature/humidity control selects) don't push
# changes like PDOs do, so they are polled; 0s disables polling
CONF_PROPERTY_POLL_INTERVAL = "property_poll_interval"

zehnder_comfoair_q_ns = cg.esphome_ns.namespace("zehnder_comfoair_q")
ZehnderComfoAirQ = zehnder_comfoair_q_ns.class_("ZehnderComfoAirQ", cg.PollingComponent)
ComputedSensor = zehnder_comfoair_q_ns.enum("ComputedSensor", is_class=True)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ZehnderComfoAirQ),
        cv.GenerateID(CONF_CANBUS_ID): cv.use_id(CanbusComponent),
        cv.Optional(CONF_REQUEST_INTERVAL, default="1h"): cv.update_interval,
        # extra PDO ids to request in addition to the ones implied by configured entities
        cv.Optional(CONF_REQUEST_IDS, default=[]): cv.ensure_list(
            cv.int_range(min=0, max=0x3FF)
        ),
        cv.Optional(CONF_REQUEST_DELAY, default="100ms"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_LOCAL_NODE_ID, default=0x2A): cv.int_range(min=0x01, max=0x3F),
        cv.Optional(CONF_PROPERTY_POLL_INTERVAL, default="60s"): cv.update_interval,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    # also set CAN bus parameters here (since they're fixed anyway)
    canbus_var = await cg.get_variable(config[CONF_CANBUS_ID])
    cg.add(canbus_var.set_use_extended_id(True))
    cg.add(canbus_var.set_bitrate(CanSpeed.CAN_50KBPS))

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_canbus(canbus_var))
    cg.add(var.set_update_interval(config[CONF_REQUEST_INTERVAL]))
    cg.add(var.set_request_delay(config[CONF_REQUEST_DELAY]))
    cg.add(var.set_local_node_id(config[CONF_LOCAL_NODE_ID]))
    cg.add(var.set_property_poll_interval(config[CONF_PROPERTY_POLL_INTERVAL]))
    for pdo_id in config[CONF_REQUEST_IDS]:
        cg.add(var.add_request_id(pdo_id))
