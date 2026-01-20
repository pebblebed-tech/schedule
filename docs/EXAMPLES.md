# Schedule Component Examples

This document contains detailed configuration examples for both Switch and Button platforms of the ESPHome Schedule component.

## Table of Contents

- [Switch Platform Examples](#switch-platform-examples)
  - [Simple Example: Basic Heating Control](#simple-example-basic-heating-control)
  - [Complex Example: Thermostat with Temperature Control](#complex-example-thermostat-with-temperature-control)
  - [Complex Example: Multi-Zone Heating](#complex-example-multi-zone-heating)
- [Button Platform Examples](#button-platform-examples)
  - [Example: Automated Cat Feeder](#example-automated-cat-feeder)
  - [Complex Example: Position-Controlled Blinds](#complex-example-position-controlled-blinds)
  - [Complex Example: Multi-Action Schedule](#complex-example-multi-action-schedule)

---

## Switch Platform Examples

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
    
    on_turn_on:
      - switch.turn_on: heater_relay
    
    on_turn_off:
      - switch.turn_off: heater_relay

  - platform: gpio
    id: heater_relay
    pin: GPIO5
    name: "Heater Relay"
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
        name: "Schedule Target Temperature"
        off_behavior: OFF_VALUE
        off_value: 15.0
        manual_behavior: MANUAL_VALUE
        manual_value: 20.0
    
    on_turn_on:
      - lambda: |-
          // Schedule switched ON - set thermostat to HEAT mode with scheduled temperature
          auto target = SCHEDULE_GET_DATA(heating_schedule, "temperature");
          if (!isnan(target)) {
            auto call = id(room_thermostat).make_call();
            call.set_mode(climate::CLIMATE_MODE_HEAT);
            call.set_target_temperature(target);
            call.perform();
            ESP_LOGI("heating", "Schedule ON - heating to %.1f°C", target);
          }
    
    on_turn_off:
      - lambda: |-
          // Schedule switched OFF - turn off thermostat
          auto call = id(room_thermostat).make_call();
          call.set_mode(climate::CLIMATE_MODE_OFF);
          call.perform();
          ESP_LOGI("heating", "Schedule OFF - thermostat off");

sensor:
  - platform: homeassistant
    id: current_temp
    entity_id: sensor.living_room_temperature

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
      - switch.turn_on: heater_relay
    
    idle_action:
      - switch.turn_off: heater_relay
    
    visual:
      min_temperature: 10°C
      max_temperature: 28°C
      temperature_step: 0.5°C

switch:
  - platform: gpio
    id: heater_relay
    pin: GPIO5
    internal: true
```

### Complex Example: Multi-Zone Heating

```yaml
sensor:
  - platform: homeassistant
    id: living_room_temp
    entity_id: sensor.living_room_temperature
    
  - platform: homeassistant
    id: bedroom_temp
    entity_id: sensor.bedroom_temperature

climate:
  # Zone 1: Living Room
  - platform: thermostat
    id: living_room_thermostat
    name: "Living Room Thermostat"
    sensor: living_room_temp
    
    default_preset: Home
    on_boot_restore_from: default_preset
    
    min_heating_off_time: 300s
    min_heating_run_time: 300s
    min_idle_time: 30s
    
    heat_action:
      - switch.turn_on: living_room_valve
    
    idle_action:
      - switch.turn_off: living_room_valve
    
    visual:
      min_temperature: 10°C
      max_temperature: 28°C
      temperature_step: 0.5°C

  # Zone 2: Bedroom
  - platform: thermostat
    id: bedroom_thermostat
    name: "Bedroom Thermostat"
    sensor: bedroom_temp
    
    default_preset: Home
    on_boot_restore_from: default_preset
    
    min_heating_off_time: 300s
    min_heating_run_time: 300s
    min_idle_time: 30s
    
    heat_action:
      - switch.turn_on: bedroom_valve
    
    idle_action:
      - switch.turn_off: bedroom_valve
    
    visual:
      min_temperature: 10°C
      max_temperature: 28°C
      temperature_step: 0.5°C

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
      - id: living_room_target_temp
        label: "temperature"
        item_type: float
        name: "Living Room Target Temperature"
        off_behavior: OFF_VALUE
        off_value: 15.0
        manual_behavior: MANUAL_VALUE
        manual_value: 21.0
    
    on_turn_on:
      - lambda: |-
          auto target = SCHEDULE_GET_DATA(living_room_schedule, "temperature");
          if (!isnan(target)) {
            auto call = id(living_room_thermostat).make_call();
            call.set_mode(climate::CLIMATE_MODE_HEAT);
            call.set_target_temperature(target);
            call.perform();
            ESP_LOGI("zone", "Living room schedule ON - heating to %.1f°C", target);
          }
    
    on_turn_off:
      - lambda: |-
          auto call = id(living_room_thermostat).make_call();
          call.set_mode(climate::CLIMATE_MODE_OFF);
          call.perform();
          ESP_LOGI("zone", "Living room schedule OFF");

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
      - id: bedroom_target_temp
        label: "temperature"
        item_type: float
        name: "Bedroom Target Temperature"
        off_behavior: OFF_VALUE
        off_value: 15.0
        manual_behavior: MANUAL_VALUE
        manual_value: 18.0
    
    on_turn_on:
      - lambda: |-
          auto target = SCHEDULE_GET_DATA(bedroom_schedule, "temperature");
          if (!isnan(target)) {
            auto call = id(bedroom_thermostat).make_call();
            call.set_mode(climate::CLIMATE_MODE_HEAT);
            call.set_target_temperature(target);
            call.perform();
            ESP_LOGI("zone", "Bedroom schedule ON - heating to %.1f°C", target);
          }
    
    on_turn_off:
      - lambda: |-
          auto call = id(bedroom_thermostat).make_call();
          call.set_mode(climate::CLIMATE_MODE_OFF);
          call.perform();
          ESP_LOGI("zone", "Bedroom schedule OFF");

  # Zone valve relays
  - platform: gpio
    id: living_room_valve
    pin: GPIO5
    internal: true
    
  - platform: gpio
    id: bedroom_valve
    pin: GPIO6
    internal: true
```

---

## Button Platform Examples

### Example: Automated Cat Feeder

This example demonstrates a scheduled cat feeder that dispenses food at specific times with configurable portion sizes.

```yaml
button:
  - platform: schedule
    id: cat_feeder_schedule
    name: "Cat Feeder Schedule"
    ha_schedule_entity_id: "schedule.cat_feeding"
    max_schedule_entries: 10
    
    schedule_update_button:
      name: "Update Feeding Schedule"
    
    mode_selector:
      name: "Feeder Mode"
    
    current_event_sensor:
      name: "Current Feeding Event"
    
    next_event_sensor:
      name: "Next Feeding Event"
    
    scheduled_data_items:
      - id: portion_size
        label: "portion"
        item_type: uint8_t  # Portion size in grams (0-255)
    
    on_press:
      - lambda: |-
          // Get portion size from schedule (default to 50g if not specified)
          float portion = SCHEDULE_GET_DATA(cat_feeder_schedule, "portion");
          uint8_t grams = isnan(portion) ? 50 : (uint8_t)portion;
          
          ESP_LOGI("cat_feeder", "Dispensing %d grams of food", grams);
          
          // Calculate motor run time (assuming 10g per second)
          uint32_t run_time_ms = grams * 100;
          
          // Run feeder motor
          id(feeder_motor).turn_on();
      
      - delay: !lambda |-
          float portion = SCHEDULE_GET_DATA(cat_feeder_schedule, "portion");
          uint8_t grams = isnan(portion) ? 50 : (uint8_t)portion;
          return grams * 100;  // milliseconds
      
      - switch.turn_off: feeder_motor
      
      - logger.log: "Feeding complete"
      
      # Optional: Send notification to Home Assistant
      - homeassistant.service:
          service: notify.mobile_app
          data:
            title: "Cat Feeder"
            message: !lambda |-
              float portion = SCHEDULE_GET_DATA(cat_feeder_schedule, "portion");
              uint8_t grams = isnan(portion) ? 50 : (uint8_t)portion;
              char msg[64];
              snprintf(msg, sizeof(msg), "Fed cat %dg at scheduled time", grams);
              return std::string(msg);

# Feeder motor control
switch:
  - platform: gpio
    id: feeder_motor
    pin: GPIO14
    name: "Feeder Motor"
    internal: true  # Hide from HA UI

# Optional: Manual feed button
button:
  - platform: template
    name: "Manual Feed (50g)"
    on_press:
      - switch.turn_on: feeder_motor
      - delay: 5s  # 50g = 5 seconds
      - switch.turn_off: feeder_motor
      - logger.log: "Manual feeding complete"
```

**Home Assistant Schedule Configuration:**

```yaml
schedule:
  cat_feeding:
    name: "Cat Feeding Times"
    monday:
      - from: "07:00:00"
        to: "07:00:01"
        data:
          portion: 60  # 60 grams in the morning
      - from: "18:00:00"
        to: "18:00:01"
        data:
          portion: 80  # 80 grams in the evening
    tuesday:
      - from: "07:00:00"
        to: "07:00:01"
        data:
          portion: 60
      - from: "18:00:00"
        to: "18:00:01"
        data:
          portion: 80
    # ... continue for remaining days
```

**Features:**
- Automatic feeding at scheduled times
- Variable portion sizes per feeding
- Manual feed button for unscheduled feeding
- Mobile notifications when feeding occurs
- Can be disabled via mode selector (useful when on vacation)
- Portion size stored in schedule data (60-80g typical cat portions)

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
          float position = SCHEDULE_GET_DATA(blinds_schedule, "position");
          
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

## See Also

- [Main README](../README.md) - Component overview and quick start
- [Technical Architecture](README.md) - Component design details
- [Quick Reference](QUICK_REFERENCE.md) - Configuration reference
