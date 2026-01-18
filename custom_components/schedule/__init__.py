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
CONF_SCHEDULED_DATA_ITEMS = "scheduled_data_items"
CONF_ITEM_LABEL = "label"
CONF_ITEM_TYPE = "item_type"
# Note: The following options are only applicable to state-based schedules (switch, climate, etc.)
# Event-based schedules (button) don't have OFF states or manual modes
CONF_OFF_BEHAVIOR = "item_behavior_when_off"
CONF_OFF_VALUE = "item_off_value"
CONF_MANUAL_BEHAVIOR = "item_behavior_in_manual_on"
CONF_MANUAL_VALUE = "item_manual_on_value"

# Define the namespace and the C++ class name
schedule_ns = cg.esphome_ns.namespace("schedule")
Schedule = schedule_ns.class_("Schedule", cg.Component, cg.EntityBase)
StateBasedSchedulable = schedule_ns.class_("StateBasedSchedulable", Schedule)
EventBasedSchedulable = schedule_ns.class_("EventBasedSchedulable", Schedule)
DataSensor = schedule_ns.class_("DataSensor", sensor.Sensor)
ArrayPreference = schedule_ns.class_("ArrayPreference", cg.Component)

# Storage type enum for schedule components
ScheduleStorageType = schedule_ns.enum("ScheduleStorageType")
STORAGE_TYPES = {
    "STATE_BASED": ScheduleStorageType.STORAGE_TYPE_STATE_BASED,
    "EVENT_BASED": ScheduleStorageType.STORAGE_TYPE_EVENT_BASED,
}

def calculate_schedule_array_size(max_entries, storage_type="state"):
# Calculate array preference size based on storage type.
    if storage_type == 'state' or storage_type == 'state_based':
        # State-based: [ON, OFF] pairs + [0xFFFF, 0xFFFF] terminator
        multiplier = 2
    elif storage_type == 'event' or storage_type == 'event_based':
        # Event-based: [EVENT] singles + [0xFFFF] terminator  
        multiplier = 1
    else:
        raise ValueError(f"Unknown storage type: {storage_type}. Use 'state' or 'event'")
    
    # Each entry is multiplier * 2 bytes (uint16_t)
    # Plus terminator is also multiplier * 2 bytes
    return (max_entries * multiplier * 2) + (multiplier * 2)

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

# Off behavior modes for data sensors (state-based schedules only)
# Controls what value the sensor shows when schedule is in OFF state
OFF_BEHAVIORS = {
    "NAN": 0,
    "LAST_ON_VALUE": 1,
    "OFF_VALUE": 2,
}

# Manual behavior modes for data sensors (state-based schedules only)
# Controls what value the sensor shows when schedule is in Manual On mode
MANUAL_BEHAVIORS = {
    "NAN": 0,
    "LAST_ON_VALUE": 1,
    "MANUAL_VALUE": 2,
}

def validate_off_value(config):
    # Validate that off_value is present when off_behavior is OFF_VALUE.
    if config.get(CONF_OFF_BEHAVIOR) == "OFF_VALUE":
        if CONF_OFF_VALUE not in config:
            raise cv.Invalid(f"{CONF_OFF_VALUE} is required when {CONF_OFF_BEHAVIOR} is OFF_VALUE")
    return config

def validate_manual_value(config):
    # Validate that manual_value is present when manual_behavior is MANUAL_VALUE.
    if config.get(CONF_MANUAL_BEHAVIOR) == "MANUAL_VALUE":
        if CONF_MANUAL_VALUE not in config:
            raise cv.Invalid(f"{CONF_MANUAL_VALUE} is required when {CONF_MANUAL_BEHAVIOR} is MANUAL_VALUE")
    return config

# Base schema for data sensors (common to all schedule types)
_DATA_SENSOR_BASE_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(DataSensor),
    cv.Required(CONF_ITEM_LABEL): cv.string,
    cv.Required(CONF_ITEM_TYPE): cv.enum(ITEM_TYPES, lower=True, space="_"),
}).extend(cv.COMPONENT_SCHEMA.extend({
    cv.Optional(CONF_ICON): cv.icon,
    cv.Optional(CONF_ENTITY_CATEGORY): cv.entity_category,
})).extend(sensor.sensor_schema(
    DataSensor,
    accuracy_decimals=1,
).extend({
    cv.Optional(sensor.CONF_FILTERS): cv.invalid("Filters are not supported on schedule data sensors")
}))

# Schema for data sensors in STATE-BASED schedules (switch, climate, etc.)
# Includes OFF and MANUAL behavior options since these schedules have continuous state
DATA_SENSOR_SCHEMA_STATE_BASED = cv.All(
    _DATA_SENSOR_BASE_SCHEMA.extend({
        cv.Optional(CONF_OFF_BEHAVIOR, default="NAN"): cv.enum(OFF_BEHAVIORS, upper=True, space="_"),
        cv.Optional(CONF_OFF_VALUE): cv.float_,
        cv.Optional(CONF_MANUAL_BEHAVIOR, default="NAN"): cv.enum(MANUAL_BEHAVIORS, upper=True, space="_"),
        cv.Optional(CONF_MANUAL_VALUE): cv.float_,
    }),
    validate_off_value,
    validate_manual_value,
)

# Schema for data sensors in EVENT-BASED schedules (button, etc.)
# Excludes OFF and MANUAL behavior options since event-based schedules don't have continuous state
DATA_SENSOR_SCHEMA_EVENT_BASED = _DATA_SENSOR_BASE_SCHEMA.extend({
    cv.Optional(CONF_OFF_BEHAVIOR): cv.invalid("OFF behavior not applicable to event-based schedules"),
    cv.Optional(CONF_OFF_VALUE): cv.invalid("OFF value not applicable to event-based schedules"),
    cv.Optional(CONF_MANUAL_BEHAVIOR): cv.invalid("Manual behavior not applicable to event-based schedules"),
    cv.Optional(CONF_MANUAL_VALUE): cv.invalid("Manual value not applicable to event-based schedules"),
})

# Empty schema - schedule is a base library component, platforms extend it
CONFIG_SCHEMA = cv.Schema({})

async def to_code(config):
    # Note: state_based_schedulable.cpp and event_based_schedulable.h contain
    # the implementations for StateBasedSchedulable and EventBasedSchedulable classes.
    # These files should be automatically compiled by ESPHome's build system.
    pass
