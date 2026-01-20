# Schedule Component - Testing Checklist

This document contains a comprehensive list of tests to perform before releasing the schedule component.

## Test Environment Setup

- [ ] ESPHome device with RTC (DS1307 or similar)
- [ ] Home Assistant instance with API connection
- [ ] At least one schedule entity configured in Home Assistant
- [ ] Test schedules configured for both state-based and event-based platforms

---

## 1. Installation & Configuration Tests

### 1.1 State-Based Platform (Switch)
- [ ] Component compiles without errors
- [ ] Component compiles without warnings
- [ ] YAML validation passes
- [ ] Device boots successfully
- [ ] All required entities appear in Home Assistant:
  - [ ] Switch entity
  - [ ] Mode select entity
  - [ ] Update button
  - [ ] Schedule indicator (if configured)
  - [ ] Current event sensor (if configured)
  - [ ] Next event sensor (if configured)
  - [ ] Data sensors (if configured)

### 1.2 Event-Based Platform (Button)
- [ ] Component compiles without errors
- [ ] Component compiles without warnings
- [ ] YAML validation passes
- [ ] Device boots successfully
- [ ] All required entities appear in Home Assistant:
  - [ ] Button entity
  - [ ] Mode select entity
  - [ ] Update button
  - [ ] Current event sensor (if configured)
  - [ ] Next event sensor (if configured)
  - [ ] Data sensors (if configured)

---

## 2. Schedule Retrieval & Processing Tests

### 2.1 Initial Schedule Load
- [ ] Device loads schedule from NVS on boot (if saved)
- [ ] Device requests schedule from HA on first boot
- [ ] Device requests schedule from HA when `update_schedule_from_ha_on_reconnect: true`
- [ ] Device does NOT request schedule when `update_schedule_from_ha_on_reconnect: false`

### 2.2 Schedule Update Button
- [ ] Pressing update button triggers schedule retrieval
- [ ] Schedule data is correctly parsed from HA response
- [ ] Schedule is saved to NVS after successful update
- [ ] Current/next event sensors update after schedule update
- [ ] Log shows "Processing complete" message
- [ ] Log shows correct number of entries processed

### 2.3 Schedule Validation
- [ ] Valid schedule with entries is accepted
- [ ] Valid empty schedule is accepted
- [ ] Schedule with invalid time format is rejected with error
- [ ] Schedule with missing 'from' field is rejected with error
- [ ] Schedule with missing 'to' field is rejected with error
- [ ] Schedule with missing day is rejected with error
- [ ] Schedule exceeding max_schedule_entries is truncated with warning
- [ ] Error notifications are sent to Home Assistant on validation failure

### 2.4 Entity ID Changes
- [ ] Changing ha_schedule_entity_id in YAML invalidates old schedule
- [ ] Device requests new schedule after entity ID change
- [ ] Old schedule data is not used after entity ID change
- [ ] Entity ID hash is saved and compared correctly

---

## 3. Time & RTC Tests

### 3.1 RTC Synchronization
- [ ] Device reads time from RTC on boot
- [ ] Device syncs RTC from Home Assistant time component
- [ ] Schedule operations pause when RTC time is invalid
- [ ] Schedule operations resume when RTC time becomes valid
- [ ] Log shows "Device time is now valid" message when time syncs

### 3.2 Time-Based Event Triggering
- [ ] Current event is correctly identified at boot
- [ ] Next event is correctly identified at boot
- [ ] Events trigger at the correct time (within 1 second)
- [ ] Events trigger correctly across day boundaries (23:59 → 00:00)
- [ ] Events trigger correctly across week boundaries (Sunday → Monday)
- [ ] Schedule rolls over correctly at end of week

---

## 4. State-Based Platform Tests (Switch)

### 4.1 Mode Selection
- [ ] Mode defaults to "Manual Off" on first boot
- [ ] Mode selection is saved to preferences
- [ ] Mode selection persists across reboots
- [ ] All 5 modes are available: Manual Off, Early Off, Auto, Manual On, Boost On
- [ ] Changing mode updates switch state appropriately

### 4.2 Auto Mode Operation
- [ ] Switch turns ON at scheduled ON time
- [ ] Switch turns OFF at scheduled OFF time
- [ ] Switch indicator binary sensor reflects switch state
- [ ] Current event sensor shows correct ON/OFF state
- [ ] Next event sensor shows correct upcoming event

### 4.3 Manual Override Modes
- [ ] **Manual Off**: Switch stays OFF regardless of schedule
- [ ] **Manual On**: Switch stays ON regardless of schedule
- [ ] Switch state persists in manual modes across scheduled events

### 4.4 Temporary Override Modes
- [ ] **Early Off**: Switch turns OFF immediately
- [ ] **Early Off**: Mode returns to Auto at next scheduled event
- [ ] **Boost On**: Switch turns ON immediately
- [ ] **Boost On**: Mode returns to Auto at next scheduled ON event
- [ ] Temporary modes reset correctly

### 4.5 Empty Schedule Behavior
- [ ] When schedule is empty, only "Manual Off" and "Manual On" modes are available
- [ ] Auto modes (Auto, Early Off, Boost On) are not available when schedule is empty
- [ ] Mode automatically switches to "Manual Off" if in Auto mode when schedule becomes empty
- [ ] All modes become available again when schedule is populated

---

## 5. Event-Based Platform Tests (Button)

### 5.1 Mode Selection
- [ ] Mode defaults to "Enabled" on first boot
- [ ] Mode selection is saved to preferences
- [ ] Mode selection persists across reboots
- [ ] Both modes are available: Disabled, Enabled

### 5.2 Event Triggering
- [ ] Button press action triggers at scheduled event time
- [ ] `on_press` automations execute correctly
- [ ] Events trigger correctly in Enabled mode
- [ ] Events do NOT trigger in Disabled mode

### 5.3 Empty Schedule Behavior
- [ ] When schedule is empty, only "Disabled" mode is available
- [ ] "Enabled" mode is not available when schedule is empty
- [ ] Mode automatically switches to "Disabled" if in Enabled mode when schedule becomes empty
- [ ] Both modes become available again when schedule is populated

---

## 6. Data Sensor Tests

### 6.1 Data Sensor Setup
- [ ] Data sensors are created with correct IDs
- [ ] Data sensors show correct labels
- [ ] Data sensors use correct item_type (uint8_t, uint16_t, int32_t, float)
- [ ] Data sensors have correct units and device classes (if configured)

### 6.2 State-Based Data Sensor Behavior
- [ ] **OFF_VALUE**: Sensor shows configured off_value when schedule is OFF
- [ ] **LAST_ON_VALUE**: Sensor shows last ON value when schedule is OFF
- [ ] **NAN**: Sensor shows NaN when schedule is OFF
- [ ] **MANUAL_VALUE**: Sensor shows configured manual_value in Manual On mode
- [ ] **LAST_ON_VALUE**: Sensor shows last ON value in Manual On mode
- [ ] **NAN**: Sensor shows NaN in Manual On mode

### 6.3 Event-Based Data Sensor Behavior
- [ ] Data sensor updates to current event value when event triggers
- [ ] Data sensor values persist between events
- [ ] SCHEDULE_GET_DATA macro retrieves correct values in automations

### 6.4 Data Type Validation
- [ ] uint8_t values are parsed correctly (0-255)
- [ ] uint16_t values are parsed correctly (0-65535)
- [ ] int32_t values are parsed correctly (negative and positive)
- [ ] float values are parsed correctly (decimals)
- [ ] Invalid data types in HA schedule are rejected with error
- [ ] Out-of-range values are rejected with error

### 6.5 Data Persistence
- [ ] Data sensor values are saved to NVS
- [ ] Data sensor values persist across reboots
- [ ] Data sensor values are restored from NVS on boot

---

## 7. Home Assistant Integration Tests

### 7.1 API Connection
- [ ] Component detects HA API connection on boot
- [ ] Component detects HA API disconnection
- [ ] Component detects HA API reconnection
- [ ] Schedule operations pause when HA disconnected (if schedule invalid)
- [ ] Schedule update requested on reconnection (if configured)

### 7.2 Service Calls
- [ ] schedule.get_schedule service call succeeds
- [ ] Service response is correctly parsed
- [ ] Service call failures are logged with errors
- [ ] Multiple concurrent service calls are handled correctly

### 7.3 Notifications
- [ ] Error notifications are sent to HA on schedule parsing failures
- [ ] Notification service is set up correctly
- [ ] Notifications include correct message and title

---

## 8. Preference (NVS) Tests

### 8.1 Schedule Data Persistence
- [ ] Schedule data is saved to NVS after update
- [ ] Schedule data is loaded from NVS on boot
- [ ] Schedule terminator (0xFFFF) is correctly written
- [ ] Schedule terminator is correctly detected on load
- [ ] Empty schedule is correctly identified (terminator at position 0)

### 8.2 Mode Preference Persistence
- [ ] Mode selection is saved to NVS
- [ ] Mode selection is loaded on boot
- [ ] Invalid mode index defaults to safe mode (Manual Off / Disabled)

### 8.3 Entity ID Hash Persistence
- [ ] Entity ID hash is saved to NVS
- [ ] Entity ID hash is loaded on boot
- [ ] Entity ID changes are detected via hash comparison

### 8.4 Data Sensor Persistence
- [ ] Data sensor values are saved to NVS
- [ ] Data sensor values are loaded on boot
- [ ] Multiple data sensors save/load correctly

### 8.5 NVS Storage Limits
- [ ] State-based storage calculation: (max_entries × 2 × 2) + 4 bytes
- [ ] Event-based storage calculation: (max_entries × 1 × 2) + 4 bytes
- [ ] Data sensor storage: max_entries × item_size bytes
- [ ] Total NVS usage is within device limits
- [ ] NVS stats show correct usage (use test button)

---

## 9. Lambda & Automation Tests

### 9.1 SCHEDULE_GET_DATA Macro
- [ ] Macro retrieves correct data sensor value by label
- [ ] Macro returns NaN when sensor not found
- [ ] Macro works in switch on_turn_on automation
- [ ] Macro works in switch on_turn_off automation
- [ ] Macro works in button on_press automation
- [ ] Macro handles nullptr gracefully

### 9.2 Direct Access Methods
- [ ] `id(schedule_id).current_option()` returns correct mode
- [ ] `id(data_sensor_id).state` returns correct value
- [ ] Switch state can be read in lambdas
- [ ] Button state can be checked in lambdas

---

## 10. Edge Case & Error Handling Tests

### 10.1 Schedule Edge Cases
- [ ] Schedule with single entry works correctly
- [ ] Schedule with maximum entries works correctly
- [ ] Schedule with events at 00:00 works correctly
- [ ] Schedule with events at 23:59 works correctly
- [ ] Schedule spanning entire week works correctly
- [ ] Schedule with no events on some days works correctly

### 10.2 Time Edge Cases
- [ ] Schedule works correctly during DST transitions (if applicable)
- [ ] Schedule works correctly on leap day (Feb 29)
- [ ] Schedule works correctly across year boundaries

### 10.3 Error Recovery
- [ ] Component recovers from invalid schedule data
- [ ] Component recovers from HA API disconnection
- [ ] Component recovers from RTC failure
- [ ] Component recovers from NVS read failure
- [ ] Component continues operating with factory defaults if NVS is corrupted

### 10.4 Memory & Performance
- [ ] No memory leaks during normal operation
- [ ] No memory leaks during schedule updates
- [ ] Component loop() executes within 1 second
- [ ] Multiple schedule components on same device work correctly
- [ ] Device remains responsive during schedule operations

---

## 11. Logging Tests

### 11.1 Log Levels
- [ ] DEBUG logs show detailed state machine transitions
- [ ] INFO logs show important events (mode changes, schedule updates)
- [ ] WARNING logs show potential issues (invalid time, disconnections)
- [ ] ERROR logs show critical failures (parsing errors, service call failures)
- [ ] VERBOSE logs show detailed internal state (when enabled)

### 11.2 Log Best Practices
- [ ] Log messages are short and concise
- [ ] No component name redundancy in messages (TAG handles it)
- [ ] No unnecessary punctuation (periods, exclamation marks)
- [ ] ESP_LOGCONFIG messages are combined (not multiple separate calls)
- [ ] No logging in tight loops

---

## 12. Documentation Tests

- [ ] README.md is complete and accurate
- [ ] QUICK_REFERENCE.md examples work as documented
- [ ] ARCHITECTURE.md diagrams match implementation
- [ ] IMPLEMENTATION_SUMMARY.md is up to date
- [ ] Code comments are clear and helpful
- [ ] All TODOs in code are addressed or documented

---

## 13. Upgrade & Migration Tests

### 13.1 Firmware Updates
- [ ] Schedule data persists across firmware updates
- [ ] Mode selection persists across firmware updates
- [ ] Data sensor values persist across firmware updates
- [ ] Entity ID hash persists across firmware updates

### 13.2 Configuration Changes
- [ ] Adding new data sensors works without losing schedule
- [ ] Removing data sensors works without corruption
- [ ] Changing max_schedule_entries works correctly
- [ ] Changing ha_schedule_entity_id triggers re-fetch

---

## 14. Multi-Device & Scale Tests

- [ ] Multiple schedule components on same device work independently
- [ ] Multiple devices with same schedule entity ID work correctly
- [ ] Schedule updates don't interfere between components
- [ ] NVS namespace isolation prevents conflicts

---

## Release Checklist

Before releasing:
- [ ] All critical tests pass
- [ ] All known bugs are fixed
- [ ] Documentation is complete
- [ ] Example in documentation are tested to work
- [ ] Examples are tested and working
- [ ] Code is formatted (clang-format)
- [ ] Logging conforms to ESPHome best practices
- [ ] No compiler warnings
- [ ] Version number is updated
- [ ] CHANGELOG.md is updated
- [ ] GitHub release notes are prepared

---

## Test Results Template

```markdown
## Test Results - [Date]

**Tester:** [Name]
**ESPHome Version:** [Version]
**Device:** [ESP32/ESP8266 variant]
**Test Duration:** [Hours]

### Summary
- Total Tests: XX
- Passed: XX
- Failed: XX
- Skipped: XX

### Failed Tests
1. [Test Name] - [Reason] - [Priority: High/Medium/Low]
2. ...

### Notes
- [Any additional observations]
- [Performance metrics]
- [Issues found]
```
