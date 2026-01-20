# ESPHome Schedule Component - Configuration Examples

This folder contains complete, ready-to-use YAML configuration examples for the ESPHome Schedule component. Each example demonstrates different use cases and features.

## Prerequisites

Before using these examples, make sure you have:

1. **ESPHome** installed (2025.11.1 or later)
2. **Home Assistant** with schedule helper integration (2025.11.3 or later)
3. **ESP32 device** (these examples use ESP32, but can be adapted for other boards)
4. **RTC Module** (DS1307 or DS3231 recommended for offline operation)
5. **Schedule component** copied to `custom_components/schedule` directory

## Setup Instructions

### 1. Create secrets.yaml

Create a `secrets.yaml` file in your ESPHome directory with the following:

```yaml
# WiFi credentials
wifi_ssid: "YourWiFiSSID"
wifi_password: "YourWiFiPassword"

# API encryption key (generate with: esphome config.yaml)
api_key: "your-32-character-api-key-here"

# OTA password
ota_password: "your-ota-password"

# Fallback AP password
ap_password: "fallback-ap-password"
```

### 2. Adjust Hardware Configuration

Update GPIO pins in the examples to match your hardware:
- I2C pins for RTC (default: SDA=GPIO4, SCL=GPIO5)
- Relay/output pins


### 3. Create Home Assistant Schedules

For each example, create the corresponding schedule helper in Home Assistant.

## Switch Platform Examples

### [switch-basic-heating.yaml](switch-basic-heating.yaml)

**Simple heating control with schedule**

- **Use Case:** Basic on/off heating control
- **Features:**
  - Schedule switch controlling heater relay
  - Visual indicator
  - 5 operating modes (Manual Off, Early Off, Auto, Manual On, Boost On)
- **Hardware:** Single relay on GPIO5
- **HA Schedule:** `schedule.heating`

### [switch-thermostat-control.yaml](switch-thermostat-control.yaml)

**Thermostat with scheduled temperature setpoints**

- **Use Case:** Smart heating with temperature control
- **Features:**
  - Schedule controls thermostat temperature
  - Temperature data from schedule entries
  - Different temperatures for ON/OFF/Manual modes
  - Event tracking (current/next)
- **Hardware:** Single relay on GPIO5
- **HA Schedule:** `schedule.heating` (with `temperature` data field)
- **HA Sensor:** `sensor.living_room_temperature`

**Home Assistant Schedule Example:**

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
    # ... repeat for other days
```

### [switch-multizone-heating.yaml](switch-multizone-heating.yaml)

**Multi-zone heating with independent schedules**

- **Use Case:** Control multiple heating zones independently
- **Features:**
  - Two independent schedule switches
  - Separate thermostats for each zone
  - Independent temperature setpoints
  - Zone-specific operating modes
- **Hardware:** Two relays (GPIO5, GPIO6)
- **HA Schedules:** 
  - `schedule.living_room` (with `temperature` data)
  - `schedule.bedroom` (with `temperature` data)
- **HA Sensors:**
  - `sensor.living_room_temperature`
  - `sensor.bedroom_temperature`

## Button Platform Examples

### [button-cat-feeder.yaml](button-cat-feeder.yaml)

**Automated pet feeder with portion control**

- **Use Case:** Scheduled pet feeding with configurable portions
- **Features:**
  - Event-based scheduling (specific times)
  - Variable portion sizes from schedule data
  - Manual feed button
  - Home Assistant notifications
  - Timed motor control
- **Hardware:** Motor on GPIO14
- **HA Schedule:** `schedule.cat_feeding` (with `portion` data field)

**Home Assistant Schedule Example:**

```yaml
schedule:
  cat_feeding:
    name: "Cat Feeding Times"
    monday:
      - from: "07:00:00"
        to: "07:00:01"
        data:
          portion: 60  # grams
      - from: "18:00:00"
        to: "18:00:01"
        data:
          portion: 80  # grams
    # ... repeat for other days
```

### [button-blinds-control.yaml](button-blinds-control.yaml)

**Position-controlled automated blinds**

- **Use Case:** Scheduled blind positioning (open/close/partial)
- **Features:**
  - Event-based scheduling
  - Position control from schedule data (0-100%)
  - Fallback to full open if no position data
  - Event tracking
- **Hardware:** Motor control (implement in script)
- **HA Schedule:** `schedule.blinds` (with `position` data field)

**Home Assistant Schedule Example:**

```yaml
schedule:
  blinds:
    name: "Blinds Schedule"
    monday:
      - from: "07:00:00"
        to: "07:00:01"
        data:
          position: 100  # fully open
      - from: "20:00:00"
        to: "20:00:01"
        data:
          position: 0  # fully closed
    # ... repeat for other days
```

### [button-daily-routines.yaml](button-daily-routines.yaml)

**Morning and evening automation routines**

- **Use Case:** Complex multi-action routines at scheduled times
- **Features:**
  - Two separate routine buttons
  - Morning routine with brightness control
  - Evening routine with multiple actions
  - Integration with lights, covers, and TTS
- **Hardware:** 
  - RGB light (GPIO25/26/27)
  - Outdoor lights relay (GPIO14)
  - Blinds motor
- **HA Schedules:**
  - `schedule.morning_routine` (with `brightness` data)
  - `schedule.evening_routine`

## Customization Tips

### Adjusting Schedule Capacity

Modify `max_schedule_entries` based on your needs:

```yaml
switch:
  - platform: schedule
    max_schedule_entries: 21  # Increase or decrease as needed
```

### Adding More Data Fields

Add additional scheduled data items:

```yaml
scheduled_data_items:
  - id: temperature
    label: "temperature"
    item_type: float
  - id: humidity
    label: "humidity"
    item_type: uint8_t
```

## Troubleshooting

### Schedule Not Syncing

1. Check Home Assistant API connection
2. Verify schedule entity ID matches exactly
3. Press the "Update Schedule" button manually
4. Check ESPHome logs for errors

### RTC Not Found

1. Check I2C wiring (SDA/SCL)
2. Verify RTC module power (3.3V or 5V)
3. Check I2C scan results in logs
4. Try different I2C pins if needed

### Data Sensors Showing NaN

1. Verify `label` matches HA schedule data field exactly
2. Check schedule has data for the entry
3. Ensure `id` is set for data sensors if using in lambdas
4. Check `off_behavior` and `manual_behavior` settings

## Related Documentation

- **[Main README](../README.md)** - Component overview and features
- **[EXAMPLES.md](../docs/EXAMPLES.md)** - Code snippets and explanations
- **[QUICK_REFERENCE.md](../docs/QUICK_REFERENCE.md)** - Configuration reference
- **[ARCHITECTURE.md](../docs/ARCHITECTURE.md)** - Technical details

## Support

For issues, questions, or contributions:
- **Repository:** [pebblebed-tech/schedule](https://github.com/pebblebed-tech/schedule)
- **Documentation:** `docs/` folder

---

**Note:** These examples assume you have the schedule component installed in `custom_components/schedule`. Adjust the `external_components` path if your installation differs.
