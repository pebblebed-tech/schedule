from logging import config
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.components import time
from esphome.const import (
    CONF_ID,
    CONF_ICON,
    CONF_ENTITY_CATEGORY,
    CONF_TIME_ID,
)

CODEOWNERS = ["@pebblebed-tech"]
DEPENDENCIES = ["api", "time"]

# Define a configuration key for the array size
CONF_SCHEDULE_ID = "schedule_id"
CONF_MAX_SCHEDULE_SIZE = "max_schedule_size"
CONF_HA_SCHEDULE_ENTITY_ID = "ha_schedule_entity_id"
CONF_SCHEDULE_VARS = "schedule_variables"
CONF_ITEM_LABEL = "label"
CONF_ITEM_TYPE = "item_type"
CONF_OFF_BEHAVIOR = "variable_behavior_when_off"
CONF_OFF_VALUE = "variable_off_value"
CONF_MANUAL_BEHAVIOR = "variable_behavior_in_manual_on"
CONF_MANUAL_VALUE = "variable_manual_on_value"

# Define the namespace and the C++ class name
schedule_ns = cg.esphome_ns.namespace("schedule")
Schedule = schedule_ns.class_("Schedule", cg.Component, cg.EntityBase)
DataSensor = schedule_ns.class_("DataSensor", sensor.Sensor)
ArrayPreference = schedule_ns.class_("ArrayPreference", cg.Component)

ITEM_TYPES = {
    "uint8_t": 0,
    "uint16_t": 1,
    "int32_t": 2,
    "float": 3,
}

# Map item types to their byte sizes
ITEM_TYPE_BYTES = {
    0: 1,  # uint8_t
    1: 2,  # uint16_t
    2: 4,  # int32_t
    3: 4,  # float
}

# Off behavior modes for data sensors
OFF_BEHAVIORS = {
    "NAN": 0,
    "LAST_ON_VALUE": 1,
    "OFF_VALUE": 2,
}

# Manual behavior modes for data sensors
MANUAL_BEHAVIORS = {
    "NAN": 0,
    "LAST_ON_VALUE": 1,
    "MANUAL_VALUE": 2,
}

def validate_manual_value(config):
    """Validate that manual_value is present when manual_behavior is MANUAL_VALUE."""
    if config.get(CONF_MANUAL_BEHAVIOR) == "MANUAL_VALUE":
        if CONF_MANUAL_VALUE not in config:
            raise cv.Invalid(f"{CONF_MANUAL_VALUE} is required when {CONF_MANUAL_BEHAVIOR} is MANUAL_VALUE")
    return config

# Schema for individual data sensor
DATA_SENSOR_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(DataSensor),
    cv.Required(CONF_ITEM_LABEL): cv.string,
    cv.Required(CONF_ITEM_TYPE): cv.enum(ITEM_TYPES, lower=True, space="_"),
    cv.Optional(CONF_OFF_BEHAVIOR, default="NAN"): cv.enum(OFF_BEHAVIORS, upper=True, space="_"),
    cv.Optional(CONF_OFF_VALUE, default=0.0): cv.float_,
    cv.Optional(CONF_MANUAL_BEHAVIOR, default="NAN"): cv.enum(MANUAL_BEHAVIORS, upper=True, space="_"),
    cv.Optional(CONF_MANUAL_VALUE): cv.float_,
}).extend(cv.COMPONENT_SCHEMA.extend({
    cv.Optional(CONF_ICON): cv.icon,
    cv.Optional(CONF_ENTITY_CATEGORY): cv.entity_category,
})).extend(sensor.sensor_schema(
    DataSensor,
    accuracy_decimals=1,
).extend({
    cv.Optional(sensor.CONF_FILTERS): cv.invalid("Filters are not supported on schedule data sensors")
})).add_extra(validate_manual_value)

# Empty schema - schedule is a base library component, platforms extend it
CONFIG_SCHEMA = cv.Schema({})

async def to_code(config):
    # Nothing to do - this is a library component
    pass
