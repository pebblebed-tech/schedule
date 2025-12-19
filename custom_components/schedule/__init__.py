import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_NAME
)
# Define a configuration key for the array size
CONF_MAX_SCHEDULE_SIZE = "max_schedule_size"

CODEOWNERS = ["@pebblebed-tech"]
DEPENDENCIES = ["api"]

CONF_HA_SCHEDULE_ENTITY_ID = "ha_schedule_entity_id"
CONF_SCHEDULE_VARS= "schedule_variables"
CONF_DATA_ITEM = "data_item"
CONF_ITEM_LABEL = "data_item_label"
CONF_ITEM_TYPE = "data_item_type"
# Define the namespace and the C++ class name
schedule_ns = cg.esphome_ns.namespace("schedule")
Schedule = schedule_ns.class_("Schedule", cg.Component, cg.EntityBase)
ITEM_TYPES = {
    "byte": 0,
    "int": 1,
    "float": 2,
}

DATA_ITEM_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ITEM_LABEL): cv.string,
        cv.Required(CONF_ITEM_TYPE): cv.enum(ITEM_TYPES, lower=True, space="_"),
    }
)
DATA_ITEMS_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_DATA_ITEM): cv.ensure_list(DATA_ITEM_SCHEMA)

    }
)
# Define configuration schema, requiring an entity_id
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Schedule),
            cv.Optional(CONF_NAME): cv.string,
            cv.Required(CONF_HA_SCHEDULE_ENTITY_ID): cv.string, # Add required entity_id
            cv.Optional(CONF_MAX_SCHEDULE_SIZE, default=21): cv.int_, # Validate the size as an integer
            cv.Optional(CONF_SCHEDULE_VARS): cv.ensure_list(DATA_ITEMS_SCHEMA),
            

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
    cg.add(var.set_max_schedule_size(config[CONF_MAX_SCHEDULE_SIZE]))
    await  cg.register_component(var, config)