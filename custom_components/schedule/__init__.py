import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_NAME
)
from esphome.components import sensor

# Define a configuration key for the array size
CONF_MAX_SCHEDULE_SIZE = "max_schedule_size"

CODEOWNERS = ["@pebblebed-tech"]
DEPENDENCIES = ["api"]

CONF_HA_SCHEDULE_ENTITY_ID = "ha_schedule_entity_id"
CONF_SCHEDULE_VARS = "schedule_variables"
CONF_ITEM_LABEL = "label"
CONF_ITEM_TYPE = "item_type"

# Define the namespace and the C++ class name
schedule_ns = cg.esphome_ns.namespace("schedule")
Schedule = schedule_ns.class_("Schedule", cg.Component, cg.EntityBase)
DataSensor = schedule_ns.class_("DataSensor", sensor.Sensor)

ITEM_TYPES = {
    "uint8_t": 0,
    "uint16_t": 1,
    "int32_t": 2,
    "float": 3,
}

# Schema for individual data sensor
DATA_SENSOR_SCHEMA = sensor.sensor_schema(
    DataSensor,
    accuracy_decimals=1,
).extend({
    cv.GenerateID(): cv.declare_id(DataSensor),
    cv.Required(CONF_ITEM_LABEL): cv.string,
    cv.Required(CONF_ITEM_TYPE): cv.enum(ITEM_TYPES, lower=True, space="_"),
})

# Define configuration schema, requiring an entity_id
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Schedule),
            cv.Optional(CONF_NAME): cv.string,
            cv.Required(CONF_HA_SCHEDULE_ENTITY_ID): cv.string,
            cv.Optional(CONF_MAX_SCHEDULE_SIZE, default=21): cv.int_,
            cv.Optional(CONF_SCHEDULE_VARS): cv.ensure_list(DATA_SENSOR_SCHEMA),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)

async def to_code(config):
    # Instantiate the C++ class
    var = cg.new_Pvariable(config[CONF_ID])
    # Pass the entity_id from YAML config into the C++ instance
    cg.add(var.set_schedule_entity_id(config[CONF_HA_SCHEDULE_ENTITY_ID]))
    # Pass the size to the C++ component using a setter method
    cg.add_build_flag(f'-DSCHEDULE_COMPONENT_MAX_SIZE={config[CONF_MAX_SCHEDULE_SIZE]}')
    cg.add(var.set_max_schedule_entries(config[CONF_MAX_SCHEDULE_SIZE]))
    cg.add(var.set_name(config[CONF_NAME] if CONF_NAME in config else "*schedule*"))
    cg.add(var.set_object_id(config[CONF_ID].id))
   # cg.add(var.set_comp_id(config[CONF_ID].id))
    # Process schedule variables
    if CONF_SCHEDULE_VARS in config:
        for sensor_config in config[CONF_SCHEDULE_VARS]:
            label = sensor_config[CONF_ITEM_LABEL]
            item_type = ITEM_TYPES[sensor_config[CONF_ITEM_TYPE]]
            
            # Create DataSensor
            sens = cg.new_Pvariable(sensor_config[CONF_ID])
            await sensor.register_sensor(sens, sensor_config)
            
            # Set the label and item type
            cg.add(sens.set_label(label))
            cg.add(sens.set_item_type(item_type))
            cg.add(sens.set_max_schedule_data_entries(config[CONF_MAX_SCHEDULE_SIZE]))
            
            # Add data item to schedule component
            cg.add(var.add_data_item(label, item_type))
            # Register sensor with schedule component
            cg.add(var.register_data_sensor(sens))

    await cg.register_component(var, config)