from logging import config
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    ENTITY_CATEGORY_CONFIG,
    ICON_RULER,
    CONF_ICON,
    CONF_ENTITY_CATEGORY,
    CONF_TIME_ID,
)
from esphome.components import sensor
from esphome.components import binary_sensor
from esphome.components import switch
from esphome.components import text_sensor
from esphome.components import select
from esphome.components import time




CODEOWNERS = ["@pebblebed-tech"]
DEPENDENCIES = ["api", "switch", "binary_sensor", "button", "sensor", "text_sensor"]
# For some reason select is not recognized as a dependency unless explicitly added
AUTO_LOAD = ["select"]
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
CONF_UPDATE_BUTTON = "schedule_update_button"
CONF_SCHEDULE_SWITCH_IND = "schedule_switch_indicator"
CONF_SCHEDULE_SWITCH = "schedule_switch"
CONF_CURRENT_EVENT = "current_event"
CONF_NEXT_EVENT = "next_event"
CONF_MODE_SELECT = "mode_selector"
CONF_UPDATE_ON_RECONNECT = "update_schedule_from_ha_on_reconnect"

# Define the namespace and the C++ class name
schedule_ns = cg.esphome_ns.namespace("schedule")
Schedule = schedule_ns.class_("Schedule", cg.Component, cg.EntityBase)
DataSensor = schedule_ns.class_("DataSensor", sensor.Sensor)
ArrayPreference = schedule_ns.class_("ArrayPreference", cg.Component)
UpdateScheduleButton = schedule_ns.class_("UpdateScheduleButton", button.Button, cg.Component)
ScheduleSwitchIndicator = schedule_ns.class_("ScheduleSwitchIndicator", binary_sensor.BinarySensor, cg.Component)
ScheduleSwitch = schedule_ns.class_("ScheduleSwitch", switch.Switch, cg.Component)
ScheduleModeSelect = schedule_ns.class_("ScheduleModeSelect", select.Select, cg.Component)



SCHEDULE_MODE_OPTIONS = [
    "Manual Off",
    "Early Off",
    "Auto",
    "Manual On",
    "Boost On"
]



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
}).extend(sensor.sensor_schema(
    DataSensor,
    accuracy_decimals=1,
)).add_extra(validate_manual_value)

# Define configuration schema, requiring an entity_id
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Schedule),
            cv.Optional(CONF_NAME): cv.string,
            cv.Required(CONF_HA_SCHEDULE_ENTITY_ID): cv.string,
            cv.Optional(CONF_MAX_SCHEDULE_SIZE, default=21): cv.int_,
            cv.Optional(CONF_SCHEDULE_VARS): cv.ensure_list(DATA_SENSOR_SCHEMA),
            cv.Required(CONF_UPDATE_BUTTON): cv.maybe_simple_value(
                button.button_schema(
                    UpdateScheduleButton,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_SCHEDULE_SWITCH_IND): cv.maybe_simple_value(
                binary_sensor.binary_sensor_schema(
                    ScheduleSwitchIndicator,
                ),
                key=CONF_NAME,
            ),
            cv.Required(CONF_SCHEDULE_SWITCH): switch.switch_schema(
                ScheduleSwitch,
                default_restore_mode="RESTORE_DEFAULT_OFF",
            ),
            cv.Optional(CONF_CURRENT_EVENT): cv.maybe_simple_value(
                text_sensor.text_sensor_schema(),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_NEXT_EVENT): cv.maybe_simple_value(
                text_sensor.text_sensor_schema(),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_MODE_SELECT): cv.maybe_simple_value(
                select.select_schema(
                    ScheduleModeSelect,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
                key=CONF_NAME,
            ),
            cv.GenerateID(CONF_TIME_ID): cv.All(
                        cv.requires_component("time"), cv.use_id(time.RealTimeClock)
            ),
            cv.Optional(CONF_UPDATE_ON_RECONNECT, default=False): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)

async def to_code(config):
    # Instantiate the C++ class
    var = cg.new_Pvariable(config[CONF_ID])
    # Pass the entity_id from YAML config into the C++ instance
    cg.add(var.set_schedule_entity_id(config[CONF_HA_SCHEDULE_ENTITY_ID]))

    cg.add(var.set_max_schedule_entries(config[CONF_MAX_SCHEDULE_SIZE]))
    cg.add(var.set_name(config[CONF_NAME] if CONF_NAME in config else "*schedule*"))
    cg.add(var.set_object_id(config[CONF_ID].id))

    size = (config[CONF_MAX_SCHEDULE_SIZE] * 2 * 2) + 4  # Each entry has a start and end time so actual size is double and is 16bit so *2, plus 4 for terminators
    # Create a NEW ID object for the child 
    array_pref = cg.RawExpression(f'new esphome::schedule::ArrayPreference<{size}>()')
    cg.add(var.sched_add_pref(array_pref))
    # Generate button ID
    button_var = await button.new_button(config[CONF_UPDATE_BUTTON])
    await cg.register_component(button_var, config[CONF_UPDATE_BUTTON])
    # Link button to schedule component
    cg.add(button_var.set_schedule(var))
    # Process schedule variables
    if CONF_SCHEDULE_VARS in config:
        for sensor_config in config[CONF_SCHEDULE_VARS]:
            label = sensor_config[CONF_ITEM_LABEL]
            item_type = ITEM_TYPES[sensor_config[CONF_ITEM_TYPE]]
            
            # Calculate bytes needed for this sensor's data
            bytes_per_item = ITEM_TYPE_BYTES[item_type]
            total_bytes = config[CONF_MAX_SCHEDULE_SIZE] * bytes_per_item
            
            # Create ArrayPreference for this sensor's data
            sensor_array_pref = cg.RawExpression(f'new esphome::schedule::ArrayPreference<{total_bytes}>()')
            
            # Create DataSensor
            sens = cg.new_Pvariable(sensor_config[CONF_ID])
            await sensor.register_sensor(sens, sensor_config)
            
            # Set the label and item type
            cg.add(sens.set_label(label))
            cg.add(sens.set_item_type(item_type))
            cg.add(sens.set_max_schedule_data_entries(config[CONF_MAX_SCHEDULE_SIZE]))
            cg.add(sens.set_array_preference(sensor_array_pref))
            
            # Set off behavior and off value
            off_behavior_name = sensor_config[CONF_OFF_BEHAVIOR]
            # Map YAML names to C++ enum values
            off_behavior_enum_map = {
                "NAN": "DATA_SENSOR_OFF_BEHAVIOR_NAN",
                "LAST_ON_VALUE": "DATA_SENSOR_OFF_BEHAVIOR_LAST_ON_VALUE",
                "OFF_VALUE": "DATA_SENSOR_OFF_BEHAVIOR_OFF_VALUE",
            }
            cg.add(sens.set_off_behavior(cg.RawExpression(f'esphome::schedule::{off_behavior_enum_map[off_behavior_name]}')))
            cg.add(sens.set_off_value(sensor_config[CONF_OFF_VALUE]))
            
            # Set manual behavior and manual value
            manual_behavior_name = sensor_config[CONF_MANUAL_BEHAVIOR]
            manual_behavior_enum_map = {
                "NAN": "DATA_SENSOR_MANUAL_BEHAVIOR_NAN",
                "LAST_ON_VALUE": "DATA_SENSOR_MANUAL_BEHAVIOR_LAST_ON_VALUE",
                "MANUAL_VALUE": "DATA_SENSOR_MANUAL_BEHAVIOR_MANUAL_VALUE",
            }
            cg.add(sens.set_manual_behavior(cg.RawExpression(f'esphome::schedule::{manual_behavior_enum_map[manual_behavior_name]}')))
            if CONF_MANUAL_VALUE in sensor_config:
                cg.add(sens.set_manual_value(sensor_config[CONF_MANUAL_VALUE]))
            
            # Add data item to schedule component
            cg.add(var.add_data_item(label, item_type))
            # Register sensor with schedule component
            cg.add(var.register_data_sensor(sens))

    # Create binary sensor for schedule switch indicator if configured
    if CONF_SCHEDULE_SWITCH_IND in config:
        binary_sensor_var = await binary_sensor.new_binary_sensor(config[CONF_SCHEDULE_SWITCH_IND])
        await cg.register_component(binary_sensor_var, config[CONF_SCHEDULE_SWITCH_IND])
        
        # Link binary sensor to schedule component
        cg.add(binary_sensor_var.set_schedule(var))
        cg.add(var.set_switch_indicator(binary_sensor_var))

    # Create required schedule switch
    switch_var = await switch.new_switch(config[CONF_SCHEDULE_SWITCH])
    await cg.register_component(switch_var, config[CONF_SCHEDULE_SWITCH])
    
    # Set internal to true by default
    cg.add(switch_var.set_internal(True))
    
    # Set restore mode to always restore OFF
    cg.add(switch_var.set_restore_mode(switch.SwitchRestoreMode.SWITCH_ALWAYS_OFF))
    
    # Link switch to schedule component
    cg.add(switch_var.set_schedule(var))
    cg.add(var.set_schedule_switch(switch_var))

    # Create current_event text sensor if configured
    if CONF_CURRENT_EVENT in config:
        current_event_var = await text_sensor.new_text_sensor(config[CONF_CURRENT_EVENT])
        cg.add(var.set_current_event_sensor(current_event_var))
    
    # Create next_event text sensor if configured
    if CONF_NEXT_EVENT in config:
        next_event_var = await text_sensor.new_text_sensor(config[CONF_NEXT_EVENT])
        cg.add(var.set_next_event_sensor(next_event_var))

    # Create mode_select if configured
    if CONF_MODE_SELECT in config:
        mode_select_conf = config[CONF_MODE_SELECT]
        mode_select_var = await select.new_select(mode_select_conf, options=SCHEDULE_MODE_OPTIONS)
        await cg.register_component(mode_select_var, mode_select_conf)
        cg.add(var.set_mode_select(mode_select_var))

    # Set time component if configured
    if CONF_TIME_ID in config:
        time_var = await cg.get_variable(config[CONF_TIME_ID])
        cg.add(var.set_time(time_var))
    
    # Set update on reconnect flag
    cg.add(var.set_update_schedule_on_reconnect(config[CONF_UPDATE_ON_RECONNECT]))
        
    await cg.register_component(var, config)

