import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button as esphome_button
from esphome.components import sensor
from esphome.components import button
from esphome.components import text_sensor
from esphome.components import select
from esphome.components import time
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_ICON,
    CONF_ENTITY_CATEGORY,
    CONF_TIME_ID,
    ENTITY_CATEGORY_CONFIG,
)

# Import from parent __init__.py
from .. import (
    schedule_ns,
    EventBasedSchedulable,
    DataSensor,
    DATA_SENSOR_SCHEMA_EVENT_BASED,  # Use event-based schema (no OFF/Manual behaviors)
    CONF_SCHEDULE_VARS,
    CONF_ITEM_LABEL,
    CONF_ITEM_TYPE,
    CONF_HA_SCHEDULE_ENTITY_ID,
    CONF_MAX_SCHEDULE_SIZE,
    ITEM_TYPES,
    ITEM_TYPE_BYTES,
    calculate_schedule_array_size,
)

CODEOWNERS = ["@pebblebed-tech"]
DEPENDENCIES = []
AUTO_LOAD = ["select", "schedule"]

# Button-specific configuration keys
CONF_UPDATE_BUTTON = "schedule_update_button"
CONF_CURRENT_EVENT = "current_event"
CONF_NEXT_EVENT = "next_event"
CONF_MODE_SELECT = "mode_selector"
CONF_UPDATE_ON_RECONNECT = "update_schedule_from_ha_on_reconnect"

# C++ classes
ScheduleButton = schedule_ns.class_("ScheduleButton", esphome_button.Button, EventBasedSchedulable)
UpdateScheduleButton = schedule_ns.class_("UpdateScheduleButton", button.Button, cg.Component)
ScheduleEventModeSelect = schedule_ns.class_("ScheduleEventModeSelect", select.Select, cg.Component)

# Simplified mode options for event-based components (no state to maintain)
SCHEDULE_BUTTON_MODE_OPTIONS = [
    "Disabled",
    "Enabled"
]

CONFIG_SCHEMA = esphome_button.button_schema(
    ScheduleButton,
).extend({
    cv.Required(CONF_HA_SCHEDULE_ENTITY_ID): cv.string,
    cv.Optional(CONF_MAX_SCHEDULE_SIZE, default=21): cv.int_,
    cv.Optional(CONF_SCHEDULE_VARS): cv.ensure_list(DATA_SENSOR_SCHEMA_EVENT_BASED),
    cv.Required(CONF_UPDATE_BUTTON): cv.maybe_simple_value(
        button.button_schema(
            UpdateScheduleButton,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        key=CONF_NAME,
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
            ScheduleEventModeSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        key=CONF_NAME,
    ),
    cv.GenerateID(CONF_TIME_ID): cv.All(
        cv.requires_component("time"), cv.use_id(time.RealTimeClock)
    ),
    cv.Optional(CONF_UPDATE_ON_RECONNECT, default=False): cv.boolean,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    # Create the button (which extends EventBasedSchedulable)
    var = await esphome_button.new_button(config)
    await cg.register_component(var, config)
    
    # Set up base Schedule properties
    cg.add(var.set_schedule_entity_id(config[CONF_HA_SCHEDULE_ENTITY_ID]))
    cg.add(var.set_max_schedule_entries(config[CONF_MAX_SCHEDULE_SIZE]))
    
    # Calculate and create array preference for schedule times
    # ScheduleButton is event-based (stores EVENT times only, not ON/OFF pairs)
    size = calculate_schedule_array_size(config[CONF_MAX_SCHEDULE_SIZE], 'event')
    array_pref = cg.RawExpression(f'new esphome::schedule::ArrayPreference<{size}>()')
    cg.add(var.sched_add_pref(array_pref))
    
    # Set internal to true by default
    cg.add(var.set_internal(True))
    
    # Generate update button
    button_var = await button.new_button(config[CONF_UPDATE_BUTTON])
    await cg.register_component(button_var, config[CONF_UPDATE_BUTTON])
    cg.add(button_var.set_schedule(var))
    
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
        mode_select_var = await select.new_select(config[CONF_MODE_SELECT])
        await cg.register_component(mode_select_var, config[CONF_MODE_SELECT])
        cg.add(mode_select_var.set_schedule(var))
        cg.add(var.set_mode_select(mode_select_var))
        
        # Set mode options (simplified for event-based: Disabled, Enabled)
        cg.add(mode_select_var.traits.set_options(SCHEDULE_BUTTON_MODE_OPTIONS))
    
    # Handle time component
    time_var = await cg.get_variable(config[CONF_TIME_ID])
    cg.add(var.set_time(time_var))
    
    # Set update on reconnect flag
    if config[CONF_UPDATE_ON_RECONNECT]:
        cg.add(var.set_update_on_reconnect(True))
    
    # Handle schedule variables (data sensors)
    if CONF_SCHEDULE_VARS in config:
        for sensor_config in config[CONF_SCHEDULE_VARS]:
            label = sensor_config[CONF_ITEM_LABEL]
            item_type = sensor_config[CONF_ITEM_TYPE]
            
            # Calculate max entries for data sensor
            max_entries = config[CONF_MAX_SCHEDULE_SIZE]
            item_type_bytes = ITEM_TYPE_BYTES[item_type]
            sensor_array_size = max_entries * item_type_bytes
            sensor_array_pref = cg.RawExpression(f'new esphome::schedule::ArrayPreference<{sensor_array_size}>()')
            
            sens = cg.new_Pvariable(sensor_config[CONF_ID])
            await sensor.register_sensor(sens, sensor_config)
            
            # Set the label and item type
            cg.add(sens.set_label(label))
            cg.add(sens.set_item_type(item_type))
            cg.add(sens.set_max_schedule_data_entries(max_entries))
            cg.add(sens.set_array_preference(sensor_array_pref))
            
            # Set off behavior and off value
            off_behavior_name = sensor_config[CONF_OFF_BEHAVIOR]
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
