# Adding New Scheduled Components

This guide provides step-by-step instructions for adding new state-based or event-based schedule platforms.

## Table of Contents
- [Decision: State-Based vs Event-Based](#decision-state-based-vs-event-based)
- [Adding a State-Based Platform](#adding-a-state-based-platform)
- [Adding an Event-Based Platform](#adding-an-event-based-platform)
- [Testing Your Implementation](#testing-your-implementation)
- [Common Patterns](#common-patterns)

---

## Decision: State-Based vs Event-Based

### Use State-Based When:
- Component needs to **maintain state over time**
- Has distinct ON and OFF states
- Examples: Switch, Climate, Light, Fan, Number

**Storage:** [ON_TIME, OFF_TIME] pairs = 4 bytes per entry

### Use Event-Based When:
- Component responds to **discrete events**
- Doesn't maintain continuous state
- Action is triggered once per event
- Examples: Button, Cover, Lock, Script

**Storage:** [EVENT_TIME] singles = 2 bytes per entry (**50% savings!**)

---

## Adding a State-Based Platform

### Example: Adding a Climate Platform

#### Step 1: Create Platform Directory Structure

```
custom_components/schedule/
├── climate/
│   ├── __init__.py
│   ├── schedule_climate.h
│   └── schedule_climate.cpp
```

#### Step 2: Create Header File (`climate/schedule_climate.h`)

```cpp
#pragma once
#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#include "../state_based_schedulable.h"

namespace esphome {
namespace schedule {

class ScheduleClimate : public climate::Climate, public StateBasedSchedulable {
 public:
  // Setup - sync entity info and call parent setup
  void setup() override {
    this->sync_from_entity(this->EntityBase::get_object_id(), 
                           this->EntityBase::get_name().c_str());
    StateBasedSchedulable::setup();
  }
  
  // Dump configuration for logging
  void dump_config() override;
  
 protected:
  // Apply the scheduled ON/OFF state to the climate component
  void apply_scheduled_state(bool on) override {
    auto call = this->make_call();
    
    if (on) {
      // Turn ON - set to HEAT mode and target temperature from data sensor
      call.set_mode(climate::CLIMATE_MODE_HEAT);
      
      // Get temperature from data sensor if available
      for (auto *sensor : this->data_sensors_) {
        if (sensor->get_label() == "temperature") {
          call.set_target_temperature(sensor->state);
          ESP_LOGD("schedule.climate", "Setting target temp to %.1f°C", sensor->state);
          break;
        }
      }
    } else {
      // Turn OFF - set to OFF mode
      call.set_mode(climate::CLIMATE_MODE_OFF);
    }
    
    call.perform();
  }
  
  // Handle manual climate changes
  void control(const climate::ClimateCall &call) override {
    // When user manually changes climate, update the internal state
    if (call.get_mode().has_value()) {
      this->mode = *call.get_mode();
    }
    if (call.get_target_temperature().has_value()) {
      this->target_temperature = *call.get_target_temperature();
    }
    this->publish_state();
  }
};

} // namespace schedule
} // namespace esphome
```

#### Step 3: Create Implementation File (`climate/schedule_climate.cpp`)

```cpp
#include "esphome.h"
#include "schedule_climate.h"

namespace esphome {
namespace schedule {

static const char *const TAG = "schedule.climate";

void ScheduleClimate::dump_config() {
  LOG_CLIMATE("", "Schedule Climate", this);
  // Call base schedule configuration
  this->dump_config_base();
}

} // namespace schedule
} // namespace esphome
```

#### Step 4: Create Python Configuration (`climate/__init__.py`)

```python
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate
from esphome.components import sensor
from esphome.components import button
from esphome.components import binary_sensor
from esphome.components import text_sensor
from esphome.components import select
from esphome.components import time
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_TIME_ID,
    ENTITY_CATEGORY_CONFIG,
)

# Import from parent __init__.py
from .. import (
    schedule_ns,
    StateBasedSchedulable,
    DataSensor,
    DATA_SENSOR_SCHEMA,
    CONF_SCHEDULED_DATA_ITEMS,
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
    calculate_schedule_array_size,
)

CODEOWNERS = ["@pebblebed-tech"]
DEPENDENCIES = []
AUTO_LOAD = ["select", "schedule"]

# Climate-specific configuration keys
CONF_UPDATE_BUTTON = "schedule_update_button"
CONF_CURRENT_EVENT = "current_event"
CONF_NEXT_EVENT = "next_event"
CONF_MODE_SELECT = "mode_selector"
CONF_UPDATE_ON_RECONNECT = "update_schedule_from_ha_on_reconnect"

# C++ classes
ScheduleClimate = schedule_ns.class_("ScheduleClimate", climate.Climate, StateBasedSchedulable)
UpdateScheduleButton = schedule_ns.class_("UpdateScheduleButton", button.Button, cg.Component)
ScheduleStateModeSelect = schedule_ns.class_("ScheduleStateModeSelect", select.Select, cg.Component)

SCHEDULE_MODE_OPTIONS = [
    "Manual Off",
    "Early Off",
    "Auto",
    "Manual On",
    "Boost On"
]

def add_default_ids(config):
    """Add default IDs for optional components based on the climate ID."""
    climate_id = config[CONF_ID]
    base_id = climate_id.id
    
    if CONF_CURRENT_EVENT in config and CONF_ID not in config[CONF_CURRENT_EVENT]:
        config[CONF_CURRENT_EVENT][CONF_ID] = cv.declare_id(text_sensor.TextSensor)(f"{base_id}_current_event_sensor")
    
    if CONF_NEXT_EVENT in config and CONF_ID not in config[CONF_NEXT_EVENT]:
        config[CONF_NEXT_EVENT][CONF_ID] = cv.declare_id(text_sensor.TextSensor)(f"{base_id}_next_event_sensor")
    
    if CONF_ID not in config[CONF_MODE_SELECT]:
        config[CONF_MODE_SELECT][CONF_ID] = cv.declare_id(ScheduleStateModeSelect)(f"{base_id}_mode_select")
    
    if CONF_ID not in config[CONF_UPDATE_BUTTON]:
        config[CONF_UPDATE_BUTTON][CONF_ID] = cv.declare_id(UpdateScheduleButton)(f"{base_id}_update_button")
    
    return config

CONFIG_SCHEMA = climate.climate_schema(
    ScheduleClimate,
).extend({
    cv.Required(CONF_HA_SCHEDULE_ENTITY_ID): cv.string,
    cv.Optional(CONF_MAX_SCHEDULE_SIZE, default=21): cv.int_,
    cv.Optional(CONF_SCHEDULED_DATA_ITEMS): cv.ensure_list(DATA_SENSOR_SCHEMA),
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
    cv.Required(CONF_MODE_SELECT): cv.maybe_simple_value(
        select.select_schema(
            ScheduleStateModeSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        key=CONF_NAME,
    ),
    cv.GenerateID(CONF_TIME_ID): cv.All(
        cv.requires_component("time"), cv.use_id(time.RealTimeClock)
    ),
    cv.Optional(CONF_UPDATE_ON_RECONNECT, default=False): cv.boolean,
}).extend(cv.COMPONENT_SCHEMA)

# Apply default ID generation
CONFIG_SCHEMA = cv.All(CONFIG_SCHEMA, add_default_ids)

async def to_code(config):
    # Create the climate component (which extends StateBasedSchedulable)
    var = await climate.new_climate(config)
    await cg.register_component(var, config)
    
    # Set up base Schedule properties
    cg.add(var.set_schedule_entity_id(config[CONF_HA_SCHEDULE_ENTITY_ID]))
    cg.add(var.set_max_schedule_entries(config[CONF_MAX_SCHEDULE_SIZE]))
    
    # Calculate and create array preference for schedule times
    # ScheduleClimate is state-based (stores ON/OFF pairs)
    size = calculate_schedule_array_size(config[CONF_MAX_SCHEDULE_SIZE], 'state')
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
    
    # Create mode_select (required)
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
    
    # Process schedule data items
    if CONF_SCHEDULED_DATA_ITEMS in config:
        for sensor_config in config[CONF_SCHEDULED_DATA_ITEMS]:
            label = sensor_config[CONF_ITEM_LABEL]
            item_type = ITEM_TYPES[sensor_config[CONF_ITEM_TYPE]]
            
            # Get max_schedule_entries
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
```

#### Step 5: YAML Configuration Example

```yaml
climate:
  - platform: schedule
    id: heating
    name: "Heating Schedule"
    ha_schedule_entity_id: "schedule.heating"
    max_schedule_entries: 21
    
    schedule_update_button:
      name: "Update Heating Schedule"
    
    mode_selector:
      name: "Heating Mode"
    
    scheduled_data_items:
      - id: heating_temp
        label: "temperature"
        item_type: float
        off_behavior: OFF_VALUE
        off_value: 15.0
        manual_behavior: MANUAL_VALUE
        manual_value: 20.0
```

---

## Adding an Event-Based Platform

### Example: Adding a Cover Platform

#### Step 1: Create Platform Directory Structure

```
custom_components/schedule/
├── cover/
│   ├── __init__.py
│   ├── schedule_cover.h
│   └── schedule_cover.cpp
```

#### Step 2: Create Header File (`cover/schedule_cover.h`)

```cpp
#pragma once
#include "esphome/core/component.h"
#include "esphome/components/cover/cover.h"
#include "../event_based_schedulable.h"

namespace esphome {
namespace schedule {

class ScheduleCover : public cover::Cover, public EventBasedSchedulable {
 public:
  void setup() override {
    this->sync_from_entity(this->EntityBase::get_object_id(), 
                           this->EntityBase::get_name().c_str());
    EventBasedSchedulable::setup();
  }
  
  void dump_config() override;
  
 protected:
  // Apply the scheduled event (event-based: only ON events matter)
  void apply_scheduled_state(bool on) override {
    // Event-based components only respond to triggers (on=true)
    if (!on) return;
    
    ESP_LOGD("schedule.cover", "Scheduled cover event triggered");
    
    // Get position from data sensor if available
    float position = 0.5f;  // Default to 50%
    for (auto *sensor : this->data_sensors_) {
      if (sensor->get_label() == "position") {
        position = sensor->state;
        ESP_LOGD("schedule.cover", "Moving to position %.0f%%", position * 100);
        break;
      }
    }
    
    // Trigger cover movement
    auto call = this->make_call();
    call.set_position(position);
    call.perform();
  }
  
  // Handle cover trait requirements
  cover::CoverTraits get_traits() override {
    auto traits = cover::CoverTraits();
    traits.set_is_assumed_state(false);
    traits.set_supports_position(true);
    traits.set_supports_tilt(false);
    return traits;
  }
  
  void control(const cover::CoverCall &call) override {
    // Handle manual cover control
    if (call.get_position().has_value()) {
      this->position = *call.get_position();
      this->publish_state();
    }
    if (call.get_stop()) {
      // Stop the cover
      this->publish_state();
    }
  }
};

} // namespace schedule
} // namespace esphome
```

#### Step 3: Create Implementation File (`cover/schedule_cover.cpp`)

```cpp
#include "esphome.h"
#include "schedule_cover.h"

namespace esphome {
namespace schedule {

static const char *const TAG = "schedule.cover";

void ScheduleCover::dump_config() {
  LOG_COVER("", "Schedule Cover", this);
  // Call base schedule configuration
  this->dump_config_base();
}

} // namespace schedule
} // namespace esphome
```

#### Step 4: Create Python Configuration (`cover/__init__.py`)

```python
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import cover
from esphome.components import sensor
from esphome.components import button
from esphome.components import text_sensor
from esphome.components import select
from esphome.components import time
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_TIME_ID,
    ENTITY_CATEGORY_CONFIG,
)

# Import from parent __init__.py
from .. import (
    schedule_ns,
    EventBasedSchedulable,
    DataSensor,
    DATA_SENSOR_SCHEMA_EVENT_BASED,  # Use event-based schema (no OFF/Manual behaviors)
    CONF_SCHEDULED_DATA_ITEMS,
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

# Cover-specific configuration keys
CONF_UPDATE_BUTTON = "schedule_update_button"
CONF_CURRENT_EVENT = "current_event"
CONF_NEXT_EVENT = "next_event"
CONF_MODE_SELECT = "mode_selector"
CONF_UPDATE_ON_RECONNECT = "update_schedule_from_ha_on_reconnect"

# C++ classes
ScheduleCover = schedule_ns.class_("ScheduleCover", cover.Cover, EventBasedSchedulable)
UpdateScheduleButton = schedule_ns.class_("UpdateScheduleButton", button.Button, cg.Component)
ScheduleEventModeSelect = schedule_ns.class_("ScheduleEventModeSelect", select.Select, cg.Component)

SCHEDULE_COVER_MODE_OPTIONS = [
    "Disabled",
    "Enabled"
]

def add_default_ids(config):
    """Add default IDs for optional components based on the cover ID."""
    cover_id = config[CONF_ID]
    base_id = cover_id.id
    
    if CONF_CURRENT_EVENT in config and CONF_ID not in config[CONF_CURRENT_EVENT]:
        config[CONF_CURRENT_EVENT][CONF_ID] = cv.declare_id(text_sensor.TextSensor)(f"{base_id}_current_event_sensor")
    
    if CONF_NEXT_EVENT in config and CONF_ID not in config[CONF_NEXT_EVENT]:
        config[CONF_NEXT_EVENT][CONF_ID] = cv.declare_id(text_sensor.TextSensor)(f"{base_id}_next_event_sensor")
    
    if CONF_ID not in config[CONF_MODE_SELECT]:
        config[CONF_MODE_SELECT][CONF_ID] = cv.declare_id(ScheduleEventModeSelect)(f"{base_id}_mode_select")
    
    if CONF_ID not in config[CONF_UPDATE_BUTTON]:
        config[CONF_UPDATE_BUTTON][CONF_ID] = cv.declare_id(UpdateScheduleButton)(f"{base_id}_update_button")
    
    return config

CONFIG_SCHEMA = cover.cover_schema(
    ScheduleCover,
).extend({
    cv.Required(CONF_HA_SCHEDULE_ENTITY_ID): cv.string,
    cv.Optional(CONF_MAX_SCHEDULE_SIZE, default=21): cv.int_,
    cv.Optional(CONF_SCHEDULED_DATA_ITEMS): cv.ensure_list(DATA_SENSOR_SCHEMA_EVENT_BASED),
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
    cv.Required(CONF_MODE_SELECT): cv.maybe_simple_value(
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

# Apply default ID generation
CONFIG_SCHEMA = cv.All(CONFIG_SCHEMA, add_default_ids)

async def to_code(config):
    # Create the cover component (which extends EventBasedSchedulable)
    var = await cover.new_cover(config)
    await cg.register_component(var, config)
    
    # Set up base Schedule properties
    cg.add(var.set_schedule_entity_id(config[CONF_HA_SCHEDULE_ENTITY_ID]))
    cg.add(var.set_max_schedule_entries(config[CONF_MAX_SCHEDULE_SIZE]))
    
    # Calculate and create array preference for schedule times
    # ScheduleCover is event-based (stores EVENT times only, not ON/OFF pairs)
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
    
    # Create mode_select (required)
    mode_select_var = await select.new_select(config[CONF_MODE_SELECT], options=SCHEDULE_COVER_MODE_OPTIONS)
    await cg.register_component(mode_select_var, config[CONF_MODE_SELECT])
    cg.add(mode_select_var.set_schedule(var))
    cg.add(var.set_mode_select(mode_select_var))
    
    # Handle time component
    time_var = await cg.get_variable(config[CONF_TIME_ID])
    cg.add(var.set_time(time_var))
    
    # Set update on reconnect flag
    if config[CONF_UPDATE_ON_RECONNECT]:
        cg.add(var.set_update_on_reconnect(True))
    
    # Handle scheduled data items (data sensors)
    if CONF_SCHEDULED_DATA_ITEMS in config:
        for sensor_config in config[CONF_SCHEDULED_DATA_ITEMS]:
            label = sensor_config[CONF_ITEM_LABEL]
            item_type = ITEM_TYPES[sensor_config[CONF_ITEM_TYPE]]
            
            # Calculate max entries for data sensor
            max_entries = config[CONF_MAX_SCHEDULE_SIZE]
            item_type_bytes = ITEM_TYPE_BYTES[item_type]
            sensor_array_size = max_entries * item_type_bytes
            sensor_array_pref = cg.RawExpression(f'new esphome::schedule::ArrayPreference<{sensor_array_size}>()')
            
            sens = cg.new_Pvariable(sensor_config[CONF_ID])
            await sensor.register_sensor(sens, sensor_config)
            
            cg.add(sens.set_label(label))
            cg.add(sens.set_item_type(item_type))
            cg.add(sens.set_max_schedule_data_entries(max_entries))
            cg.add(sens.set_array_preference(sensor_array_pref))
            
            # Add data item to schedule component
            cg.add(var.add_data_item(label, item_type))
            # Register sensor with schedule component
            cg.add(var.register_data_sensor(sens))
```

#### Step 5: YAML Configuration Example

```yaml
cover:
  - platform: schedule
    id: blinds
    name: "Blinds Schedule"
    ha_schedule_entity_id: "schedule.blinds"
    max_schedule_entries: 10
    
    schedule_update_button:
      name: "Update Blinds Schedule"
    
    mode_selector:
      name: "Blinds Mode"
    
    scheduled_data_items:
      - id: blinds_position
        label: "position"
        item_type: float
```

---

## Testing Your Implementation

### 1. Compilation Test
```bash
esphome compile your-config.yaml
```

### 2. Upload and Monitor
```bash
esphome run your-config.yaml
```

### 3. Check Logs
Look for:
- Component setup messages
- Schedule retrieval from HA
- NVS save/load operations
- State machine transitions

### 4. Test Scenarios

#### For State-Based:
- [ ] Schedule loads on boot
- [ ] Manual Off mode works
- [ ] Manual On mode works
- [ ] Auto mode follows schedule
- [ ] Early Off returns to Auto on next event
- [ ] Boost On returns to Auto on next ON event
- [ ] Data sensors update correctly
- [ ] OFF behavior works (NAN, LAST_ON_VALUE, OFF_VALUE)
- [ ] Manual behavior works

#### For Event-Based:
- [ ] Schedule loads on boot
- [ ] Disabled mode prevents events
- [ ] Enabled mode triggers events
- [ ] Data sensors update correctly
- [ ] Events trigger at correct times

### 5. Storage Verification
Check NVS partition size:
```
Storage used = (max_entries × multiplier × 2) + 4
State-based multiplier = 2
Event-based multiplier = 1
```

---

## Common Patterns

### Accessing Data Sensors in C++
```cpp
for (auto *sensor : this->data_sensors_) {
  if (sensor->get_label() == "temperature") {
    float temp = sensor->state;
    // Use temperature value
    break;
  }
}
```

### Error Handling in apply_scheduled_state
```cpp
void apply_scheduled_state(bool on) override {
  if (on) {
    float value = NAN;
    for (auto *sensor : this->data_sensors_) {
      if (sensor->get_label() == "target") {
        value = sensor->state;
        break;
      }
    }
    
    if (std::isnan(value)) {
      ESP_LOGW(TAG, "Target value not found, using default");
      value = 50.0f;
    }
    
    // Apply value to component
  }
}
```

### Custom Storage Type
If you need a custom storage format, override these methods:
```cpp
ScheduleStorageType get_storage_type() const override {
  return STORAGE_TYPE_CUSTOM;  // Define your own enum
}

size_t get_storage_multiplier() const override {
  return 3;  // Example: 3 values per entry
}

void parse_schedule_entry(const JsonObjectConst &entry, 
                          std::vector<uint16_t> &work_buffer,
                          uint16_t day_offset) override {
  // Custom parsing logic
}
```

---

## Troubleshooting

### Compilation Errors
- Verify all includes are correct
- Check namespace declarations
- Ensure method signatures match base class

### Runtime Errors
- Check logs for state machine errors
- Verify HA schedule format matches expectations
- Ensure time component is configured

### Storage Issues
- Verify array size calculation
- Check NVS partition size
- Ensure preference hash is unique

---

## Next Steps

1. Review [ARCHITECTURE.md](ARCHITECTURE.md) for system design
2. Study existing platforms (switch, button) for reference
3. Test thoroughly with various schedule configurations
4. Document platform-specific behaviors
5. Submit PR with your new platform!
