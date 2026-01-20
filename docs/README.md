# Schedule Component Documentation

This directory contains documentation and reference files for the ESPHome Schedule component.

## Documentation Files

### Getting Started
- **[QUICK_REFERENCE.md](QUICK_REFERENCE.md)** - Quick reference for common tasks and YAML configuration
- **[EXAMPLES.md](EXAMPLES.md)** - Detailed configuration examples for Switch and Button platforms
- **[ADDING_NEW_PLATFORMS.md](ADDING_NEW_PLATFORMS.md)** - Detailed guide for adding new state-based or event-based components

### Architecture & Design
- **[ARCHITECTURE.md](ARCHITECTURE.md)** - Complete architecture overview with class diagrams

## Actual Implementation Files

The actual implementation files are in the parent directory:

### Core Files
- `schedule.h` / `schedule.cpp` - Base schedule component with virtual methods for extensibility
- `state_based_schedulable.h` / `state_based_schedulable.cpp` - State-based scheduling (Switch, Climate, Light)
- `event_based_schedulable.h` / `event_based_schedulable.cpp` - Event-based scheduling (Button, Cover, Lock)
- `data_sensor.h` / `data_sensor.cpp` - Data sensor for schedule variables
- `array_preference.h` - Array preference template for NVS storage
- `schedule_state_mode_select.h` - Mode selection for state-based components
- `schedule_event_mode_select.h` - Mode selection for event-based components

### Platform Implementations
- `switch/` - Schedule Switch platform (state-based)
- `button/` - Schedule Button platform (event-based)

### Python Configuration
- `__init__.py` - Parent module with shared schemas and helper functions
- `switch/__init__.py` - Switch platform configuration with auto-ID generation
- `button/__init__.py` - Button platform configuration with auto-ID generation

## Key Concepts

### Storage Types

**State-Based (Default)**
- Stores [ON_TIME, OFF_TIME] pairs
- Used for: Switch, Climate, Light, Fan
- Storage: (entries × 4) + 4 bytes
- Modes: Manual Off, Early Off, Auto, Manual On, Boost On

**Event-Based**
- Stores [EVENT_TIME] singles only
- Used for: Cover, Lock, Button, Script
- Storage: (entries × 2) + 4 bytes (50% savings!)
- Modes: Disabled, Enabled

### Features

- **Home Assistant Integration**: Automatic schedule sync from HA
- **Persistent Storage**: Schedules saved to NVS flash
- **Data Sensors**: Schedule variables for temperatures, positions, etc.
- **Mode Selection**: Manual override, boost, early-off modes
- **Auto-ID Generation**: Optional components get automatic IDs
- **Error Notifications**: Sends HA notifications on parsing errors
- **Dynamic Mode Options**: Modes automatically adjust when schedule is empty
- **Optimized Logging**: ESPHome best practices for minimal flash/network usage
- **RTC Support**: Continues operation during WiFi/HA outages

### For More Information

- Start with [QUICK_REFERENCE.md](QUICK_REFERENCE.md) for YAML examples and common tasks
- Read [ADDING_NEW_SCHEDULED_COMPOENTS.md](ADDING_NEW_SCHEDULED_COMPONENTS.md) for step-by-step platform development
- Check [ARCHITECTURE.md](ARCHITECTURE.md) for system design and class relationships


---

**Repository:** [pebblebed-tech/schedule](https://github.com/pebblebed-tech/schedule)


