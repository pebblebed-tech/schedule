# Quick Reference Guide

## Requirements

### Hardware
- **RTC Module Required**: DS1307, DS3231, or compatible I2C RTC
- Enables schedule operation during WiFi/HA outages
- Maintains accurate time across reboots

### Time Configuration
```yaml
time:
  - platform: ds1307
    id: rtc_time
    update_interval: never  # Sync handled by HA time
  - platform: homeassistant
    on_time_sync:
      then:
        ds1307.write_time:  # Update RTC when HA time syncs
```

---

## YAML Configuration Examples

### Switch Platform (State-Based)

```yaml
switch:
  - platform: schedule
    id: heating_schedule
    name: "Heating Schedule"
    ha_schedule_entity_id: "schedule.heating"
    max_schedule_entries: 21
    
    # Required components (auto-generated IDs)
    schedule_update_button:
      name: "Update Heating Schedule"
    
    mode_selector:
      name: "Heating Mode"
    
    # Optional components
    schedule_switch_indicator:
      name: "Heating Active"
    
    current_event:
      name: "Current Heating Event"
    
    next_event:
      name: "Next Heating Event"
    
    # Schedule variables
    scheduled_data_items:
      - id: heating_temp
        label: "temperature"
        item_type: float
        off_behavior: OFF_VALUE
        off_value: 15.0
        manual_behavior: MANUAL_VALUE
        manual_value: 20.0
```

### Button Platform (Event-Based)

```yaml
button:
  - platform: schedule
    id: blinds_button
    name: "Blinds Schedule"
    ha_schedule_entity_id: "schedule.blinds"
    max_schedule_entries: 10
    
    # Required components
    schedule_update_button:
      name: "Update Blinds Schedule"
    
    mode_selector:
      name: "Blinds Mode"
    
    # Optional components
    current_event:
      name: "Current Blinds Event"
    
    next_event:
      name: "Next Blinds Event"
    
    # Schedule variables
    scheduled_data_items:
      - id: blinds_position
        label: "position"
        item_type: float
```

## Component Auto-ID Generation

When you don't specify IDs for optional components, they are auto-generated:

```yaml
switch:
  - platform: schedule
    id: my_schedule
    # Auto-generated IDs:
    # - my_schedule_update_button
    # - my_schedule_mode_select
    # - my_schedule_indicator (if schedule_switch_indicator provided)
    # - my_schedule_current_event_sensor (if current_event provided)
    # - my_schedule_next_event_sensor (if next_event provided)
```

## Configuration Options

### Common Options (Both Platforms)

| Option | Type | Required | Default | Description |
|--------|------|----------|---------|-------------|
| `ha_schedule_entity_id` | string | Yes | - | Home Assistant schedule entity ID |
| `max_schedule_entries` | int | No | 21 | Maximum schedule entries |
| `schedule_update_button` | config | Yes | - | Button to trigger schedule update |
| `mode_selector` | config | Yes | - | Mode selection dropdown |
| `current_event` | config | No | - | Text sensor showing current event |
| `next_event` | config | No | - | Text sensor showing next event |
| `scheduled_data_items` | list | No | - | Schedule variables (temp, position, etc.) |
| `update_schedule_from_ha_on_reconnect` | bool | No | false | Auto-update on HA reconnect |

### Switch-Specific Options

| Option | Type | Required | Default | Description |
|--------|------|----------|---------|-------------|
| `schedule_switch_indicator` | config | No | - | Binary sensor showing schedule state |

### Data Item Options

**Note:** While `id` is auto-generated if not specified, you **must provide an explicit `id`** if you need to access the datasensor values in lambdas or C++ code (e.g., using `SCHEDULE_GET_DATA` macro).

| Option | Type | Required | Default | Description |
|--------|------|----------|---------|-------------|
| `id` | ID | No* | auto | Datasensor ID (*required if accessing values in code) |
| `label` | string | Yes | - | Data field name in HA schedule |
| `item_type` | enum | Yes | - | `uint8_t`, `uint16_t`, `int32_t`, `float` |

**State-Based Only:**
| Option | Type | Required | Default | Description |
|--------|------|----------|---------|-------------|
| `off_behavior` | enum | No | NAN | `NAN`, `LAST_ON_VALUE`, `OFF_VALUE` |
| `off_value` | float | No | 0.0 | Value when schedule is OFF |
| `manual_behavior` | enum | No | NAN | `NAN`, `LAST_ON_VALUE`, `MANUAL_VALUE` |
| `manual_value` | float | No | 0.0 | Value in manual mode |

## Mode Options

### Switch/State-Based Modes
- **Manual Off** - Force OFF, ignore schedule
- **Early Off** - Turn OFF now, return to Auto at next event
- **Auto** - Follow schedule automatically
- **Manual On** - Force ON, ignore schedule
- **Boost On** - Turn ON now, return to Auto at next ON event

### Button/Event-Based Modes
- **Disabled** - Don't trigger events
- **Enabled** - Trigger events according to schedule

## Storage Calculations

### State-Based (ON/OFF pairs)
```
Storage bytes = (max_entries × 2 × 2) + 4
Example: 21 entries = (21 × 2 × 2) + 4 = 88 bytes
```

### Event-Based (Event times only)
```
Storage bytes = (max_entries × 1 × 2) + 4
Example: 21 entries = (21 × 1 × 2) + 4 = 46 bytes (47% savings!)
```

## Common Tasks

### Force Schedule Update
Press the update button or call:
```yaml
button.press: my_schedule_update_button
```

### Change Mode Programmatically
```yaml
select.set:
  id: my_schedule_mode_select
  option: "Auto"
```

### Access Data Sensor Values

**Recommended - Using Macro:**
```yaml
lambda: |-
  float temp = SCHEDULE_GET_DATA(heating_schedule, "temperature");
  if (!isnan(temp)) {
    ESP_LOGI("heating", "Target: %.1f�C", temp);
  }
```

**Alternative - Direct Access:**
```yaml
lambda: |-
  return SCHEDULE_GET_DATA(heating_schedule, "temperature");
```

> **Note:** The macro handles null pointer checking and logs warnings if the sensor doesn't exist.


## Home Assistant Schedule Format

Your HA schedule entity should have this structure:

```yaml
schedule.heating:
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
  # ... other days
```

## Troubleshooting

### Schedule not loading?
- Check HA connection status
- Verify `ha_schedule_entity_id` matches your HA entity
- Check logs for parsing errors
- Ensure schedule has valid time ranges

### Storage full?
- Reduce `max_schedule_entries`
- Use event-based for 50% savings (if applicable)
- Remove unused data items

### Data sensors showing NaN?
- Check `off_behavior` and `manual_behavior` settings
- Ensure data field names match HA schedule
- Verify `item_type` is correct

### Mode not persisting?
- Mode is saved to NVS automatically
- Check mode_selector has unique ID
- Verify component is registered

## Performance Notes

- Schedule updates are throttled to prevent flooding
- NVS writes only occur on schedule changes
- State machine runs every loop() for accuracy
- Connection checks are periodic (not every loop)

## Best Practices

✅ **DO:**
- Use event-based for components that don't need continuous state
- Set appropriate `max_schedule_entries` for your needs
- Use descriptive names for clarity in HA
- Test with simple schedules first

❌ **DON'T:**
- Set `max_schedule_entries` unnecessarily high
- Use state-based when event-based would work
- Forget to configure the time component
- Modify schedule data structures directly

## Quick Comparison

| Feature | State-Based | Event-Based |
|---------|-------------|-------------|
| Storage | 4 bytes/entry | 2 bytes/entry |
| Modes | 5 (Manual Off/On, Auto, Early Off, Boost) | 2 (Disabled, Enabled) |
| Use Case | Continuous state (heater ON/OFF) | One-time actions (open blinds) |
| Examples | Switch, Climate, Light | Button, Cover, Lock |
| Data Behaviors | OFF & Manual behaviors | No special behaviors |

---

For detailed development information, see [ADDING_NEW_PLATFORMS.md](ADDING_NEW_PLATFORMS.md)

For architecture details, see [ARCHITECTURE.md](ARCHITECTURE.md)



