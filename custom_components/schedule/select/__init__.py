import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG, CONF_NAME

from .. import CONF_SCHEDULE_ID, Schedule, schedule_ns

ScheduleModeSelect = schedule_ns.class_("ScheduleModeSelect", select.Select)

CONF_MODE_SELECT = "schedule_mode_select"

SCHEDULE_MODE_OPTIONS = [
    "Manual Off",
    "Early Off",
    "Auto",
    "Manual On",
    "Boost On"
]

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_SCHEDULE_ID): cv.use_id(Schedule),
    cv.Required(CONF_MODE_SELECT): cv.maybe_simple_value(
        select.select_schema(
            ScheduleModeSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        key=CONF_NAME,
    ),
}


async def to_code(config):
    schedule_component = await cg.get_variable(config[CONF_SCHEDULE_ID])
    mode_select_config = config[CONF_MODE_SELECT]
    s = await select.new_select(
        mode_select_config,
        options=SCHEDULE_MODE_OPTIONS,
    )
    await cg.register_parented(s, config[CONF_SCHEDULE_ID])
    cg.add(schedule_component.set_mode_select(s))
