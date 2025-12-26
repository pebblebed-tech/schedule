import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@pebblebed-tech"]

CONF_BUFFER_SIZE = "buffer_size"
CONF_HASH = "hash"

dynamic_preference_ns = cg.esphome_ns.namespace("dynamic_preference")
DynamicPreference = dynamic_preference_ns.class_("DynamicPreference", cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(DynamicPreference),
    cv.Required(CONF_BUFFER_SIZE): cv.int_range(min=1, max=4096),
    cv.Optional(CONF_HASH): cv.hex_uint32_t,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    buffer_size = config[CONF_BUFFER_SIZE]
    
    # Create template instance with size
    template_args = cg.TemplateArguments(buffer_size)
    var = cg.new_Pvariable(config[CONF_ID], template_args)
    
    # Set hash if provided
    if CONF_HASH in config:
        cg.add(var.set_hash(config[CONF_HASH]))
    
    await cg.register_component(var, config)