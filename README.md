# ESPHome Schedule Component

A comprehensive scheduling component for ESPHome that integrates seamlessly with Home Assistant schedules. Control switches, buttons, and other devices based on time-based schedules with support for schedule variables (temperature, position, etc.) and multiple operating modes.
When paired with a RTC such as DS1307 this allows the device to opperate independantly from Home Assistant.

## Features

- ✅ **Home Assistant Integration** - Automatic sync with HA schedule helpers
- ✅ **Persistent Storage** - Schedules saved to NVS flash, survive reboots
- ✅ **Two Scheduled  Types** - State-based (Switch) and Event-based (Button)
- ✅ **Schedule Variables** - User defined addtional data such as Temperature, position, defined in Home Assitant schedule entry
- ✅ **Multiple Modes** - Manual, Auto, Boost, Early-off modes for switches
- ✅ **Error Notifications** - Automatic HA notifications for schedule errors


## Requirements

### Hardware Requirements

- **Real-Time Clock (RTC) Module** - Required for offline operation and network outages
  - Supported modules: DS3231, DS1307, or compatible I2C RTC
  - The RTC maintains accurate timekeeping when WiFi or Home Assistant is unavailable
  - Schedule operations will continue based on RTC time during outages
  - Example configuration:

### Software Requirements

- **ESPHome** 2025.11.1 or later
- **Home Assistant** with schedule helper integration 2025.11.3 or later
- **API connection** to Home Assistant (for schedule sync)

---
## Technical Description

For detailed technical information about the how to create other schedule components, component architecture, state machines, and internal workings, see:
- **[Technical Architecture](docs/README.md)** - Component design and implementation details

---

## Quick Start

### 1. Install the Component

Copy the `custom_components/schedule` folder to your ESPHome configuration directory

### 2. Configure a Time Source

```yaml
time:
  - platform: ds1307 # Or DS3231 It works as per the ds1307
    id: rtc_time
    update_interval: never
  - platform: homeassistant

    on_time_sync:
      then:
        # Update the RTC when the synchronization is successful
        ds1307.write_time:
```
### 3. Set up the Home Assitant API

```yaml
# Enable Home Assistant API
api:
   # password: !secret api_password
  encryption: 
    key: !secret api_key
  homeassistant_services: True
  ```

### 4. Create a Switch Schedule

```yaml
switch:
  - platform: schedule
    id: heating_schedule
    name: "Heating"
    ha_schedule_entity_id: "schedule.heating"
    
    schedule_update_button:
      name: "Update Heating Schedule"
    
    mode_selector:
      name: "Heating Mode"
```

### 5. Create a Home Assistant Schedule

In Home Assistant, create a schedule helper with the same entity ID:

**Configuration → Helpers → Add Helper → Schedule**
- Name: `Heating`
- Entity ID: `schedule.heating`

Add time slots:
- Monday-Friday: 06:00-08:00 and 17:00-22:00
- Saturday-Sunday: 08:00-23:00

## Platform Documentation

- **[Switch Platform](#switch-platform)** - State-based scheduling for switches, climate, lights
- **[Button Platform](#button-platform)** - Event-based scheduling for buttons, covers, locks
- **[Home Assistant Schedules](#home-assistant-schedules)** - How to configure HA schedules with additional data

## Storage Comparison

### State-Based (Switch)
- Stores ON/OFF time pairs
- Supports 5 modes (Manual Off/On, Auto, Early Off, Boost On)
- **Storage:** (entries × 4) + 4 bytes
- **Example:** 21 entries = 88 bytes

### Event-Based (Button)
- Stores event times only
- Supports 2 modes (Disabled, Enabled)
- **Storage:** (entries × 2) + 4 bytes (**50% savings!**)
- **Example:** 21 entries = 46 bytes

---

## Switch Platform

The **Switch Platform** provides **state-based scheduling** where each schedule entry defines an ON time and OFF time. Perfect for controlling heating, lighting, or any device that needs to maintain a state over time.

### Basic Configuration

```yaml
switch:
  - platform: schedule
    id: heating_schedule
    name: "Heating Schedule"
    ha_schedule_entity_id: "schedule.heating"
    
    schedule_update_button:
      name: "Update Heating Schedule"
    
    mode_selector:
      name: "Heating Mode"
```

### Configuration Variables

#### Required Variables

- **`id`** (**Required**, ID): Component identifier for use in lambdas
- **`name`** (**Required**, string): Display name for the switch
- **`ha_schedule_entity_id`** (**Required**, string): Entity ID of Home Assistant schedule (e.g., `schedule.heating`)
- **`schedule_update_button`** (**Required**, button config): Button to trigger schedule update from Home Assistant
  - **`name`** (string): Button display name
- **`mode_selector`** (**Required**, select config): Mode selection dropdown
  - **`name`** (string): Selector display name

#### Optional Variables

- **`max_schedule_entries`** (*Optional*, int): Maximum number of schedule entries. Default: `21`
- **`indicator`** (*Optional*, switch config): Visual indicator that mirrors schedule state
  - **`name`** (string): Indicator display name
  - Auto-generates ID: `{switch_id}_indicator`
- **`current_event_sensor`** (*Optional*, sensor config): Shows current schedule entry index
  - **`name`** (string): Sensor display name
  - Auto-generates ID: `{switch_id}_current_event`
- **`next_event_sensor`** (*Optional*, sensor config): Shows next schedule entry index
  - **`name`** (string): Sensor display name
  - Auto-generates ID: `{switch_id}_next_event`
- **`scheduled_data_items`** (*Optional*, list): Custom data fields for schedule entries
  - See [Schedule Data Items](#schedule-data-items) below

### Schedule Data Items

Add custom data to each schedule entry (temperature, brightness, etc.):

```yaml
scheduled_data_items:
  - id: target_temp
    label: "temperature"
    item_type: float
    off_behavior: OFF_VALUE
    off_value: 15.0
    manual_behavior: MANUAL_VALUE
    manual_value: 20.0
```

#### Data Item Configuration

- **`id`** (**Required**, ID): Component identifier
- **`label`** (**Required**, string): Field name in Home Assistant schedule data
- **`item_type`** (**Required**, enum): Data type
  - `uint8_t` - 0 to 255
  - `uint16_t` - 0 to 65,535
  - `int32_t` - -2,147,483,648 to 2,147,483,647
  - `float` - Floating point number
- **`off_behavior`** (*Optional*, enum): Behavior when schedule is OFF. Default: `NAN`
  - `NAN` - Sensor shows NaN
  - `LAST_ON_VALUE` - Keep last ON value
  - `OFF_VALUE` - Use specific `off_value`
- **`off_value`** (*Optional*, number): Value to use when `off_behavior` is `OFF_VALUE`
- **`manual_behavior`** (*Optional*, enum): Behavior in manual mode. Default: `NAN`
  - `NAN` - Sensor shows NaN
  - `LAST_ON_VALUE` - Keep last ON value
  - `MANUAL_VALUE` - Use specific `manual_value`
- **`manual_value`** (*Optional*, number): Value to use when `manual_behavior` is `MANUAL_VALUE`

### Operating Modes

The switch platform supports 5 operating modes via the mode selector:

| Mode | Behavior |
|------|----------|
| **Manual Off** | Force switch OFF, ignore schedule completely |
| **Early Off** | Turn OFF immediately, return to Auto at next schedule event |
| **Auto** | Follow schedule automatically (default) |
| **Manual On** | Force switch ON, ignore schedule completely |
| **Boost On** | Turn ON immediately, return to Auto at next ON event |

**Note:** When the schedule is empty (no entries configured in Home Assistant), only **Manual Off** and **Manual On** modes are available. Auto-related modes (Auto, Early Off, Boost On) are automatically hidden until a valid schedule is configured. If the current mode is Auto when the schedule becomes empty, it automatically switches to Manual Off.

### Lambda Methods

Access schedule components in lambda expressions:

```cpp
// Check if schedule is currently ON
if (id(heating_schedule).state) {
  // Schedule is ON
}

// Get current mode
auto mode = id(heating_schedule_mode_select).state;

// Get data sensor value using SCHEDULE_GET_DATA macro
float temp = SCHEDULE_GET_DATA(id(heating_schedule), "temperature");

// Check if valid value
if (!isnan(temp)) {
  // Use temp value
}

// Change mode programmatically
auto call = id(heating_schedule_mode_select).make_call();
call.set_option("Auto");
call.perform();

// Trigger schedule update
id(heating_schedule_update_button).press();
```


### SCHEDULE_GET_DATA Macro

The `SCHEDULE_GET_DATA` macro provides a convenient way to retrieve data sensor values from a schedule component. It handles null pointer checking and logging automatically.

**Usage:**
```cpp
float value = SCHEDULE_GET_DATA(schedule_id, "label");
```

**Parameters:**
- `schedule_id` - The ID of your schedule component (without `id()` wrapper)
- `label` - The label string defined in `scheduled_data_items` configuration

**Returns:**
- The current sensor value as a float
- `NaN` if the sensor is not found or hasn't been populated

**Example:**
```cpp
// In your YAML configuration:
// scheduled_data_items:
//   - label: "temperature"
//     item_type: float

// In lambda code:
float temp = SCHEDULE_GET_DATA(heating_schedule, "temperature");

if (!isnan(temp)) {
  ESP_LOGI("heating", "Target temperature: %.1f�C", temp);
  id(thermostat).target_temperature = temp;
} else {
  ESP_LOGW("heating", "No temperature data available");
}
```

**Equivalent Long-Form Code:**

```cpp
// Instead of:
float temp = SCHEDULE_GET_DATA(heating_schedule, "temperature");

// You can write:
float temp = NAN;
auto *sensor = id(heating_schedule).get_data_sensor("temperature");
if (sensor != nullptr) {
  temp = sensor->state;
} else {
  ESP_LOGW("schedule", "Data sensor with label 'temperature' not found in schedule 'heating_schedule'");
}
```

**Advantages of Using the Macro:**
- Less boilerplate code
- Automatic null pointer safety
- Consistent error logging
- Cleaner, more readable lambdas
- Reduces copy-paste errors

### Simple Example: Basic Heating Control

```yaml
switch:
  - platform: schedule
    id: heating_schedule
    name: "Heating Schedule"
    ha_schedule_entity_id: "schedule.heating"
    
    schedule_update_button:
      name: "Update Heating Schedule"
    
    mode_selector:
      name: "Heating Mode"
    
    indicator:
      name: "Heating Indicator"

  - platform: gpio
    id: heater_relay
    pin: GPIO5
    name: "Heater Relay"

# Link schedule to physical relay
on_...:
  - lambda: |-
      if (id(heating_schedule).state) {
        id(heater_relay).turn_on();
      } else {
        id(heater_relay).turn_off();
      }
```

### Complex Example: Thermostat with Temperature Control

```yaml
switch:
  - platform: schedule
    id: heating_schedule
    name: "Heating Schedule"
    ha_schedule_entity_id: "schedule.heating"
    max_schedule_entries: 21
    
    schedule_update_button:
      name: "Update Heating Schedule"
    
    mode_selector:
      name: "Heating Mode"
    
    current_event_sensor:
      name: "Current Heating Event"
    
    next_event_sensor:
      name: "Next Heating Event"
    
    scheduled_data_items:
      - id: heating_temp
        label: "temperature"
        item_type: float
        off_behavior: OFF_VALUE
        off_value: 15.0
        manual_behavior: MANUAL_VALUE
        manual_value: 20.0

sensor:
  - platform: homeassistant
    id: current_temp
    entity_id: sensor.living_room_temperature
    
  - platform: template
    id: target_temp_display
    name: "Target Temperature"
    lambda: 'return SCHEDULE_GET_DATA(id(heating_schedule), "temperature");'
    unit_of_measurement: "°C"
    accuracy_decimals: 1

switch:
  - platform: gpio
    id: heater_relay
    pin: GPIO5
    internal: true

climate:
  - platform: thermostat
    id: room_thermostat
    name: "Living Room Thermostat"
    sensor: current_temp
    
    default_preset: Home
    on_boot_restore_from: default_preset
    
    min_heating_off_time: 300s
    min_heating_run_time: 300s
    min_idle_time: 30s
    
    heat_action:
      - lambda: |-
          // Only heat if schedule is ON
          if (id(heating_schedule).state) {
            id(heater_relay).turn_on();
            
            // Apply scheduled temperature if available
            auto target = SCHEDULE_GET_DATA(id(heating_schedule), "temperature");
            if (!isnan(target)) {
              id(room_thermostat).target_temperature = target;
              ESP_LOGI("climate", "Setting target temp to %.1f°C", target);
            }
          } else {
            id(heater_relay).turn_off();
          }
    
    idle_action:
      - switch.turn_off: heater_relay
    
    visual:
      min_temperature: 10°C
      max_temperature: 28°C
      temperature_step: 0.5°C

# Update thermostat when schedule state changes
interval:
  - interval: 30s
    then:
      - lambda: |-
          if (!id(heating_schedule).state) {
            // Turn off heating if schedule is OFF
            id(heater_relay).turn_off();
          }
```

### Complex Example: Multi-Zone Heating

```yaml
switch:
  # Zone 1: Living Room
  - platform: schedule
    id: living_room_schedule
    name: "Living Room Schedule"
    ha_schedule_entity_id: "schedule.living_room"
    
    schedule_update_button:
      name: "Update Living Room Schedule"
    
    mode_selector:
      name: "Living Room Mode"
    
    scheduled_data_items:
      - id: living_room_temp
        label: "temperature"
        item_type: float
        off_behavior: OFF_VALUE
        off_value: 15.0
        manual_behavior: MANUAL_VALUE
        manual_value: 21.0

  # Zone 2: Bedroom
  - platform: schedule
    id: bedroom_schedule
    name: "Bedroom Schedule"
    ha_schedule_entity_id: "schedule.bedroom"
    
    schedule_update_button:
      name: "Update Bedroom Schedule"
    
    mode_selector:
      name: "Bedroom Mode"
    
    scheduled_data_items:
      - id: bedroom_temp
        label: "temperature"
        item_type: float
        off_behavior: OFF_VALUE
        off_value: 15.0
        manual_behavior: MANUAL_VALUE
        manual_value: 18.0

  # Zone relays
  - platform: gpio
    id: living_room_valve
    pin: GPIO5
    name: "Living Room Valve"
    
  - platform: gpio
    id: bedroom_valve
    pin: GPIO6
    name: "Bedroom Valve"

# Control valves based on schedules
interval:
  - interval: 30s
    then:
      - lambda: |-
          // Living room control
          if (id(living_room_schedule).state) {
            id(living_room_valve).turn_on();
            float target = SCHEDULE_GET_DATA(id(living_room_schedule), "temperature");
            if (!isnan(target)) {
              ESP_LOGI("zone", "Living room target: %.1f°C", target);
            }
          } else {
            id(living_room_valve).turn_off();
          }
          
          // Bedroom control
          if (id(bedroom_schedule).state) {
            id(bedroom_valve).turn_on();
            float target = SCHEDULE_GET_DATA(id(bedroom_schedule), "temperature");
            if (!isnan(target)) {
              ESP_LOGI("zone", "Bedroom target: %.1f°C", target);
            }
          } else {
            id(bedroom_valve).turn_off();
          }
```

---

## Button Platform

The **Button Platform** provides **event-based scheduling** where each schedule entry defines a single event time. Perfect for triggering actions like opening blinds, unlocking doors, or sending notifications.

### Basic Configuration

```yaml
button:
  - platform: schedule
    id: blinds_schedule
    name: "Blinds Schedule"
    ha_schedule_entity_id: "schedule.blinds"
    
    schedule_update_button:
      name: "Update Blinds Schedule"
    
    mode_selector:
      name: "Blinds Mode"
```

### Configuration Variables

#### Required Variables

- **`id`** (**Required**, ID): Component identifier for use in lambdas
- **`name`** (**Required**, string): Display name for the button
- **`ha_schedule_entity_id`** (**Required**, string): Entity ID of Home Assistant schedule (e.g., `schedule.blinds`)
- **`schedule_update_button`** (**Required**, button config): Button to trigger schedule update from Home Assistant
  - **`name`** (string): Button display name
- **`mode_selector`** (**Required**, select config): Mode selection dropdown
  - **`name`** (string): Selector display name

#### Optional Variables

- **`max_schedule_entries`** (*Optional*, int): Maximum number of schedule entries. Default: `21`
- **`current_event_sensor`** (*Optional*, sensor config): Shows current schedule entry index
  - **`name`** (string): Sensor display name
  - Auto-generates ID: `{button_id}_current_event`
- **`next_event_sensor`** (*Optional*, sensor config): Shows next schedule entry index
  - **`name`** (string): Sensor display name
  - Auto-generates ID: `{button_id}_next_event`
- **`scheduled_data_items`** (*Optional*, list): Custom data fields for schedule entries
  - See [Schedule Data Items](#button-schedule-data-items) below

### Button Schedule Data Items

Add custom data to each schedule event (position, brightness, etc.):

```yaml
scheduled_data_items:
  - id: blinds_position
    label: "position"
    item_type: float
```

#### Data Item Configuration

- **`id`** (**Required**, ID): Component identifier
- **`label`** (**Required**, string): Field name in Home Assistant schedule data
- **`item_type`** (**Required**, enum): Data type
  - `uint8_t` - 0 to 255
  - `uint16_t` - 0 to 65,535
  - `int32_t` - -2,147,483,648 to 2,147,483,647
  - `float` - Floating point number

**Note:** Event-based schedules do NOT support `off_behavior` or `manual_behavior` since events are instantaneous.

### Operating Modes

The button platform supports 2 operating modes:

| Mode | Behavior |
|------|----------|
| **Disabled** | Don't trigger schedule events |
| **Enabled** | Trigger events according to schedule (default) |

**Note:** When the schedule is empty (no entries configured in Home Assistant), only the **Disabled** mode is available. The **Enabled** mode is automatically hidden until a valid schedule is configured. If the current mode is Enabled when the schedule becomes empty, it automatically switches to Disabled.

### Lambda Methods

Access schedule components in lambda expressions:

```cpp
// Check current mode
auto mode = id(blinds_schedule_mode_select).state;

// Get data sensor value using SCHEDULE_GET_DATA macro
float position = SCHEDULE_GET_DATA(id(blinds_schedule), "position");

// Check if valid value
if (!isnan(position)) {
  // Use position value
}

// Change mode programmatically
auto call = id(blinds_schedule_mode_select).make_call();
call.set_option("Enabled");
call.perform();

// Trigger schedule update
id(blinds_schedule_update_button).press();

// Trigger button press manually
id(blinds_schedule).press();
```

### Simple Example: Timed Blinds Opening

```yaml
button:
  - platform: schedule
    id: blinds_schedule
    name: "Blinds Schedule"
    ha_schedule_entity_id: "schedule.blinds"
    
    schedule_update_button:
      name: "Update Blinds Schedule"
    
    mode_selector:
      name: "Blinds Mode"
    
    on_press:
      - logger.log: "Opening blinds..."
      - cover.open: living_room_blinds

cover:
  - platform: template
    id: living_room_blinds
    name: "Living Room Blinds"
    
    open_action:
      - logger.log: "Blinds opening"
    
    close_action:
      - logger.log: "Blinds closing"
```

### Complex Example: Position-Controlled Blinds

```yaml
button:
  - platform: schedule
    id: blinds_schedule
    name: "Blinds Schedule"
    ha_schedule_entity_id: "schedule.blinds"
    max_schedule_entries: 10
    
    schedule_update_button:
      name: "Update Blinds Schedule"
    
    mode_selector:
      name: "Blinds Mode"
    
    current_event_sensor:
      name: "Current Blinds Event"
    
    next_event_sensor:
      name: "Next Blinds Event"
    
    scheduled_data_items:
      - id: blinds_position
        label: "position"
        item_type: float
    
    on_press:
      - lambda: |-
          float position = SCHEDULE_GET_DATA(id(blinds_schedule), "position");
          
          if (!isnan(position)) {
            ESP_LOGI("blinds", "Setting position to %.0f%%", position);
            
            auto call = id(living_room_blinds).make_call();
            call.set_position(position / 100.0);
            call.perform();
          } else {
            ESP_LOGW("blinds", "No position data, opening fully");
            id(living_room_blinds).open();
          }

cover:
  - platform: template
    id: living_room_blinds
    name: "Living Room Blinds"
    has_position: true
    
    open_action:
      - logger.log: "Opening blinds to 100%"
      - script.execute: move_blinds_script
    
    close_action:
      - logger.log: "Closing blinds to 0%"
      - script.execute: move_blinds_script
    
    position_action:
      - logger.log: "Setting custom position"
      - script.execute: move_blinds_script
    
    stop_action:
      - logger.log: "Stopping blinds"

script:
  - id: move_blinds_script
    then:
      - logger.log: "Moving blinds to target position"
```

### Complex Example: Multi-Action Schedule

```yaml
button:
  # Morning routine
  - platform: schedule
    id: morning_routine
    name: "Morning Routine"
    ha_schedule_entity_id: "schedule.morning_routine"
    
    schedule_update_button:
      name: "Update Morning Routine"
    
    mode_selector:
      name: "Morning Routine Mode"
    
    scheduled_data_items:
      - id: routine_brightness
        label: "brightness"
        item_type: uint8_t
    
    on_press:
      - logger.log: "Starting morning routine"
      - cover.open: all_blinds
      - delay: 30s
      - lambda: |-
          uint8_t brightness = id(routine_brightness).state;
          if (!isnan(brightness)) {
            auto call = id(living_room_light).turn_on();
            call.set_brightness(brightness / 100.0);
            call.perform();
          }
      - homeassistant.service:
          service: tts.google_say
          data:
            entity_id: media_player.living_room
            message: "Good morning! Time to wake up."

  # Evening routine
  - platform: schedule
    id: evening_routine
    name: "Evening Routine"
    ha_schedule_entity_id: "schedule.evening_routine"
    
    schedule_update_button:
      name: "Update Evening Routine"
    
    mode_selector:
      name: "Evening Routine Mode"
    
    on_press:
      - logger.log: "Starting evening routine"
      - cover.close: all_blinds
      - delay: 30s
      - light.turn_on:
          id: living_room_light
          brightness: 50%
      - switch.turn_on: outdoor_lights

light:
  - platform: rgb
    id: living_room_light
    name: "Living Room Light"
    red: output_red
    green: output_green
    blue: output_blue

cover:
  - platform: template
    id: all_blinds
    name: "All Blinds"
    open_action:
      - logger.log: "Opening all blinds"
    close_action:
      - logger.log: "Closing all blinds"
```

---

## Home Assistant Schedules

The Schedule component syncs with Home Assistant schedule helpers. This section explains how to create and configure HA schedules with additional data fields.

### Creating a Schedule Helper

#### Via Home Assistant UI

1. **Configuration → Helpers → Add Helper → Schedule**
2. Enter details:
   - **Name:** `Heating` (creates entity `schedule.heating`)
   - **Icon:** `mdi:radiator`
3. Click **Create**

#### Via configuration.yaml

```yaml
schedule:
  heating:
    name: Heating
    icon: mdi:radiator
    monday:
      - from: "06:00:00"
        to: "08:00:00"
      - from: "17:00:00"
        to: "22:00:00"
    tuesday:
      - from: "06:00:00"
        to: "08:00:00"
      - from: "17:00:00"
        to: "22:00:00"
    # ... continue for other days
```

### Adding Schedule Entries

Each schedule entry has:
- **from:** Start time (HH:MM:SS)
- **to:** End time (HH:MM:SS) - *Only for state-based*
- **data:** Custom data fields (optional)

#### State-Based Schedule (Switch)

```yaml
schedule:
  heating:
    name: Heating Schedule
    monday:
      - from: "06:00:00"
        to: "08:00:00"
        data:
          temperature: 21.0
      - from: "17:00:00"
        to: "22:00:00"
        data:
          temperature: 20.5
    tuesday:
      - from: "06:00:00"
        to: "08:00:00"
        data:
          temperature: 21.0
      - from: "17:00:00"
        to: "22:00:00"
        data:
          temperature: 20.5
    # ... continue for remaining days
```

#### Event-Based Schedule (Button)

```yaml
schedule:
  blinds:
    name: Blinds Schedule
    monday:
      - from: "07:00:00"
        data:
          position: 100.0
      - from: "20:00:00"
        data:
          position: 0.0
    tuesday:
      - from: "07:00:00"
        data:
          position: 100.0
      - from: "20:00:00"
        data:
          position: 0.0
    # ... continue for remaining days
```

**Note:** For event-based schedules, only the `from` time is used. The `to` field is ignored.

### Additional Data Fields

Additional data fields allow you to store custom values with each schedule entry. These are accessed via data sensors in ESPHome.

#### Data Field Types

Home Assistant supports:
- **Numbers:** Integer or float values
- **Strings:** Text values (not currently supported by ESPHome component)
- **Booleans:** true/false (not currently supported by ESPHome component)

#### Matching Data Fields

The `label` in ESPHome **MUST** match the field name in HA schedule data:

**ESPHome:**
```yaml
scheduled_data_items:
  - id: heating_temp
    label: "temperature"  # This must match HA
    item_type: float
```

**Home Assistant:**
```yaml
schedule:
  heating:
    monday:
      - from: "06:00:00"
        to: "08:00:00"
        data:
          temperature: 21.0  # Field name matches "temperature"
```

#### Multiple Data Fields

You can have multiple data fields per entry:

**ESPHome:**
```yaml
scheduled_data_items:
  - id: target_temp
    label: "temperature"
    item_type: float
  - id: target_humidity
    label: "humidity"
    item_type: uint8_t
  - id: fan_speed
    label: "fan_speed"
    item_type: uint8_t
```

**Home Assistant:**
```yaml
schedule:
  climate_control:
    monday:
      - from: "06:00:00"
        to: "22:00:00"
        data:
          temperature: 21.5
          humidity: 50
          fan_speed: 2
```

### Data Sensor Behavior

Data sensors show different values depending on schedule state and configuration:

#### State-Based (Switch)

| Schedule State | Mode | Data Sensor Value |
|---------------|------|-------------------|
| ON | Auto | Current schedule data value |
| OFF | Auto | Depends on `off_behavior` |
| - | Manual Off/On | Depends on `manual_behavior` |

**off_behavior Options:**
- `NAN` - Sensor shows `NaN`
- `LAST_ON_VALUE` - Keeps last ON value
- `OFF_VALUE` - Shows configured `off_value`

**manual_behavior Options:**
- `NAN` - Sensor shows `NaN`
- `LAST_ON_VALUE` - Keeps last ON value
- `MANUAL_VALUE` - Shows configured `manual_value`

#### Event-Based (Button)

Data sensors always show the value from the next scheduled event. If no upcoming event has data, sensor shows `NaN`.

### Complete Example

**Home Assistant schedule helper:**

```yaml
schedule:
  multi_zone_heating:
    name: Multi-Zone Heating
    icon: mdi:radiator
    monday:
      - from: "06:00:00"
        to: "08:00:00"
        data:
          living_room_temp: 21.0
          bedroom_temp: 18.0
          bathroom_temp: 22.0
      - from: "17:00:00"
        to: "22:00:00"
        data:
          living_room_temp: 20.5
          bedroom_temp: 19.0
          bathroom_temp: 21.0
    tuesday:
      - from: "06:00:00"
        to: "08:00:00"
        data:
          living_room_temp: 21.0
          bedroom_temp: 18.0
          bathroom_temp: 22.0
      - from: "17:00:00"
        to: "22:00:00"
        data:
          living_room_temp: 20.5
          bedroom_temp: 19.0
          bathroom_temp: 21.0
    # ... continue for remaining days
```

**ESPHome configuration:**

```yaml
switch:
  - platform: schedule
    id: heating_schedule
    name: "Heating Schedule"
    ha_schedule_entity_id: "schedule.multi_zone_heating"
    
    schedule_update_button:
      name: "Update Heating Schedule"
    
    mode_selector:
      name: "Heating Mode"
    
    scheduled_data_items:
      - id: living_room_target
        label: "living_room_temp"
        item_type: float
        off_behavior: OFF_VALUE
        off_value: 15.0
        
      - id: bedroom_target
        label: "bedroom_temp"
        item_type: float
        off_behavior: OFF_VALUE
        off_value: 15.0
        
      - id: bathroom_target
        label: "bathroom_temp"
        item_type: float
        off_behavior: OFF_VALUE
        off_value: 16.0

sensor:
  - platform: template
    name: "Living Room Target"
    lambda: 'return id(living_room_target).state;'
    unit_of_measurement: "°C"
    accuracy_decimals: 1
    
  - platform: template
    name: "Bedroom Target"
    lambda: 'return id(bedroom_target).state;'
    unit_of_measurement: "°C"
    accuracy_decimals: 1
    
  - platform: template
    name: "Bathroom Target"
    lambda: 'return id(bathroom_target).state;'
    unit_of_measurement: "°C"
    accuracy_decimals: 1
```

### Editing Schedules in Home Assistant

#### Using Schedule Card

Add to your Lovelace dashboard:

```yaml
type: schedule-card
entity: schedule.heating
```

Features:
- Visual time slot editor
- Drag to resize time slots
- Click to add/remove slots
- Copy slots between days

#### Adding Data Fields in UI

1. Click on a time slot in the schedule card
2. Click **"Add data field"**
3. Enter field name (must match ESPHome `label`)
4. Enter value
5. Click **Save**

#### Programmatic Updates

Use Home Assistant automations to modify schedules:

```yaml
automation:
  - alias: "Update heating schedule for winter"
    trigger:
      - platform: state
        entity_id: input_select.season
        to: "Winter"
    action:
      - service: schedule.add
        target:
          entity_id: schedule.heating
        data:
          day: monday
          from: "06:00:00"
          to: "08:00:00"
          data:
            temperature: 22.0
```

### Troubleshooting

#### Schedule Not Syncing

**Symptoms:** ESPHome doesn't receive schedule updates

**Checks:**
1. Verify `ha_schedule_entity_id` matches exactly
2. Check ESPHome logs for API errors
3. Ensure Home Assistant API connection is working
4. Press the update button manually to force sync

#### Data Fields Showing NaN

**Symptoms:** Data sensors always show NaN

**Checks:**
1. Verify `label` in ESPHome matches field name in HA exactly
2. Check `off_behavior` and `manual_behavior` settings
3. Ensure schedule entries actually have the data field
4. Check ESPHome logs for parsing errors

#### Wrong Data Type

**Symptoms:** Error notification about incorrect data type

**Checks:**
1. Verify `item_type` matches data type in HA
2. Use `float` for decimal numbers
3. Use `int32_t` for whole numbers (positive or negative)
4. Use `uint8_t` or `uint16_t` for small positive integers

#### Schedule Entry Limit

**Symptoms:** Error notification about oversized schedule

**Solution:**
- Reduce number of schedule entries in HA
- Increase `max_schedule_entries` in ESPHome (default: 21)
- Consider splitting into multiple schedules

### Best Practices

1. **Consistent Naming:** Use clear, descriptive names for schedules and data fields
2. **Field Name Matching:** Always ensure ESPHome `label` matches HA field name exactly
3. **Data Types:** Use appropriate data types to save storage
4. **Entry Limits:** Keep entries under limit (default 21) or adjust `max_schedule_entries`
5. **Manual Updates:** Press update button after modifying HA schedule
6. **Validation:** Check ESPHome logs and HA notifications for errors

---

## Advanced Topics

### Storage Efficiency

Choose the right platform for storage efficiency:

**State-Based (Switch):**
- Each entry: 4 bytes (2 bytes ON time + 2 bytes OFF time)
- Overhead: 4 bytes (entry count + terminator)
- **Example:** 21 entries = (21 × 4) + 4 = **88 bytes**

**Event-Based (Button):**
- Each entry: 2 bytes (event time only)
- Overhead: 4 bytes (entry count + terminator)
- **Example:** 21 entries = (21 × 2) + 4 = **46 bytes** (**47% savings!**)

### Performance Characteristics

- **Setup Time:** ~100-200ms per component
- **Loop Cycle:** ~20ms (state machine check)
- **NVS Writes:** Only on schedule updates (not on every state change)
- **Memory Usage:** ~200 bytes + schedule data + data sensors

### Error Notifications

The component automatically sends Home Assistant persistent notifications for:

1. **Schedule Retrieval Failure** - HA entity not found
2. **Parsing Errors** - Invalid JSON or structure
3. **Missing Fields** - Required data missing
4. **Invalid Values** - Out of range or wrong type
5. **Oversized Schedule** - Too many entries
6. **Data Sensor Errors** - Missing or incorrect data type

Notifications include:
- ESPHome device name
- Schedule entity ID
- Specific error details
- Timestamp

### Backup and Restore

Schedules are stored in NVS flash and persist across:
- Reboots
- Power cycles
- Firmware updates (if NVS partition preserved)

**To backup:**
- Schedules sync from Home Assistant (no manual backup needed)

**To restore:**
- Press update button after restoring HA
- Or wait for automatic sync on next update

---

## Support and Resources

- **Full Documentation:** `docs/` folder
- **Architecture Details:** `docs/ARCHITECTURE.md`
- **Developer Guide:** `docs/ADDING_NEW_PLATFORMS.md`
- **Quick Reference:** `docs/QUICK_REFERENCE.md`

**Component Version:** 1.0.0  
**ESPHome Version:** 2024.6.0+  
**Home Assistant Version:** 2024.6.0+

---

**Repository:** [pebblebed-tech/schedule](https://github.com/pebblebed-tech/schedule)











