import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch as esphome_switch
from esphome.components import sensor
from esphome.components import button
from esphome.components import binary_sensor
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
    Schedule,
    DataSensor,
    DATA_SENSOR_SCHEMA,
    CONF_SCHEDULE_VARS,
    CONF_ITEM_LABEL,
    CONF_ITEM_TYPE,
    CONF_OFF_BEHAVIOR,
    CONF_OFF_VALUE,
    CONF_MANUAL_BEHAVIOR,
    CONF_MANUAL_VALUE,
    CONF_HA_SCHEDULE_ENTITY_ID,
    CONF_MAX_SCHEDULE_SIZE,
    ITEM_TYPES,
    ITEM_TYPE_BYTES,
)

CODEOWNERS = ["@pebblebed-tech"]
DEPENDENCIES = []
AUTO_LOAD = ["select", "schedule"]

# Switch-specific configuration keys
CONF_UPDATE_BUTTON = "schedule_update_button"
CONF_SCHEDULE_SWITCH_IND = "schedule_switch_indicator"
CONF_CURRENT_EVENT = "current_event"
CONF_NEXT_EVENT = "next_event"
CONF_MODE_SELECT = "mode_selector"
CONF_UPDATE_ON_RECONNECT = "update_schedule_from_ha_on_reconnect"

# C++ classes
ScheduleSwitch = schedule_ns.class_("ScheduleSwitch", esphome_switch.Switch, Schedule)
UpdateScheduleButton = schedule_ns.class_("UpdateScheduleButton", button.Button, cg.Component)
ScheduleSwitchIndicator = schedule_ns.class_("ScheduleSwitchIndicator", binary_sensor.BinarySensor, cg.Component)
ScheduleModeSelect = schedule_ns.class_("ScheduleModeSelect", select.Select, cg.Component)

SCHEDULE_MODE_OPTIONS = [
    "Manual Off",
    "Early Off",
    "Auto",
    "Manual On",
    "Boost On"
]

CONFIG_SCHEMA = esphome_switch.switch_schema(
    ScheduleSwitch,
    default_restore_mode="RESTORE_DEFAULT_OFF",
).extend({
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
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    # Create the switch (which extends Schedule)
    var = await esphome_switch.new_switch(config)
    await cg.register_component(var, config)
    
    # Set up base Schedule properties
    cg.add(var.set_schedule_entity_id(config[CONF_HA_SCHEDULE_ENTITY_ID]))
    cg.add(var.set_max_schedule_entries(config[CONF_MAX_SCHEDULE_SIZE]))
    
    # Calculate and create array preference for schedule times
    size = (config[CONF_MAX_SCHEDULE_SIZE] * 2 * 2) + 4
    array_pref = cg.RawExpression(f'new esphome::schedule::ArrayPreference<{size}>()')
    cg.add(var.sched_add_pref(array_pref))
    
    # Set internal to true by default
    cg.add(var.set_internal(True))
    
    # Set restore mode to always restore OFF
    cg.add(var.set_restore_mode(esphome_switch.SwitchRestoreMode.SWITCH_ALWAYS_OFF))
    
    # Generate button ID
    button_var = await button.new_button(config[CONF_UPDATE_BUTTON])
    await cg.register_component(button_var, config[CONF_UPDATE_BUTTON])
    cg.add(button_var.set_schedule(var))
    
    # Create binary sensor for schedule switch indicator if configured
    if CONF_SCHEDULE_SWITCH_IND in config:
        binary_sensor_var = await binary_sensor.new_binary_sensor(config[CONF_SCHEDULE_SWITCH_IND])
        await cg.register_component(binary_sensor_var, config[CONF_SCHEDULE_SWITCH_IND])
        cg.add(binary_sensor_var.set_schedule(var))
        cg.add(var.set_switch_indicator(binary_sensor_var))
    
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
    
    # Set time component
    if CONF_TIME_ID in config:
        time_var = await cg.get_variable(config[CONF_TIME_ID])
        cg.add(var.set_time(time_var))
    
    # Set update on reconnect flag
    cg.add(var.set_update_schedule_on_reconnect(config[CONF_UPDATE_ON_RECONNECT]))
    
    # Process schedule variables
    if CONF_SCHEDULE_VARS in config:
        for sensor_config in config[CONF_SCHEDULE_VARS]:
            label = sensor_config[CONF_ITEM_LABEL]
            item_type = ITEM_TYPES[sensor_config[CONF_ITEM_TYPE]]
            
            # Get max_schedule_entries - use config value directly
            max_entries = config[CONF_MAX_SCHEDULE_SIZE]
            
            # Calculate bytes needed for this sensor's data
            bytes_per_item = ITEM_TYPE_BYTES[item_type]
            default_bytes = max_entries * bytes_per_item
            sensor_array_pref = cg.RawExpression(f'new esphome::schedule::ArrayPreference<{default_bytes}>()')
            
            # Create DataSensor
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
