#pragma once
#include "esphome.h"
#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include "esphome/components/api/homeassistant_service.h"
#include "esphome/components/json/json_util.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/button/button.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/select/select.h"
#include "esphome/components/time/real_time_clock.h"
#include <vector>
#include <string>
#include <cstring>
#include <map>
#include "array_preference.h"
#include "data_sensor.h"

namespace esphome {
namespace schedule {

// Schedule mode enum matching SCHEDULE_MODE_OPTIONS in Python
enum ScheduleMode {
  SCHEDULE_MODE_MANUAL_OFF = 0,
  SCHEDULE_MODE_EARLY_OFF = 1,
  SCHEDULE_MODE_AUTO = 2,
  SCHEDULE_MODE_MANUAL_ON = 3,
  SCHEDULE_MODE_BOOST_ON = 4
};

// Finite state machine states for loop control
enum ScheduleState {
  STATE_NOT_VALID = 0,
  STATE_INIT = 1,
  STATE_INIT_OK = 2,
  STATE_MAN_OFF = 3,
  STATE_MAN_ON = 4,
  STATE_RUN_EARLY_OFF = 5,
  STATE_RUN_BOOST = 6,
  STATE_RUN_ON = 7,
  STATE_RUN_OFF = 8
};

// Forward declarations
class Schedule;
class ScheduleModeSelect;

// UpdateScheduleButton class - button to trigger schedule update
class UpdateScheduleButton : public button::Button, public Component {
 public:
  void set_schedule(Schedule *schedule) { this->schedule_ = schedule; }
  
 protected:
  void press_action() override;
  Schedule *schedule_{nullptr};
};

// ScheduleSwitchIndicator class - binary sensor to indicate schedule state
class ScheduleSwitchIndicator : public binary_sensor::BinarySensor, public Component {
 public:
  void set_schedule(Schedule *schedule) { this->schedule_ = schedule; }
  
  // Method to publish state from Schedule component
  void publish_switch_state(bool state) { this->publish_state(state); }
  
 protected:
  Schedule *schedule_{nullptr};
};

// ScheduleSwitch class - switch controlled by schedule
class ScheduleSwitch : public switch_::Switch, public Component {
 public:
  void set_schedule(Schedule *schedule) { this->schedule_ = schedule; }
  
  // Store current sensor values before triggering
  void set_sensor_value(const std::string &label, float value) {
    this->sensor_values_[label] = value;
  }
  
  // Get sensor value by label (returns undefined if not found)
  float get_sensor_value(const std::string &label) const {
    auto it = this->sensor_values_.find(label);
    return (it != this->sensor_values_.end()) ? it->second : NAN;
  }
  
  // Check if sensor value exists
  bool has_sensor_value(const std::string &label) const {
    return this->sensor_values_.find(label) != this->sensor_values_.end();
  }
  
 protected:
  void write_state(bool state) override;
  Schedule *schedule_{nullptr};
  std::map<std::string, float> sensor_values_;
};

// Schedule class
class Schedule : public EntityBase, public Component  {
 public:
  struct DataItem {
    std::string label;
    uint16_t value;
    uint16_t size;
    };
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return esphome::setup_priority::LATE; }
  void set_schedule_entity_id(const std::string &ha_schedule_entity_id);
  void set_max_schedule_size(size_t size);
  void set_max_schedule_entries(size_t entries);
  void set_update_schedule_on_reconnect(bool update) { this->update_on_reconnect_ = update; }
   /// Trigger a schedule.get_schedule request.
  void request_schedule();
  void create_schedule_preference();
  void load_schedule_from_pref_();
  void save_schedule_to_pref_();
  void setup_schedule_retrieval_service_();
  void request_pref_hash() {
    ESP_LOGI("*****************", "Preference Hash: %u", this->get_preference_hash());
  }
  void process_schedule_(const JsonObjectConst &response); 
   // Log schedule data for debugging
  void log_schedule_data();
   
  void add_data_item(const std::string &label, uint16_t value);
  const std::vector<DataItem>& get_data_items() const {
        return data_items_;
    }
  void print_data_items();
  void register_data_sensor(DataSensor *sensor) {
    this->data_sensors_.push_back(sensor);
  }
  
  void sched_add_pref(ArrayPreferenceBase *array_pref);
  
  // Set the switch indicator binary sensor
  void set_switch_indicator(ScheduleSwitchIndicator *indicator) {
    this->switch_indicator_ = indicator;
  }
  
  void set_schedule_switch(ScheduleSwitch *schedule_switch) { 
    this->schedule_switch_ = schedule_switch; 
  }
  
  void set_mode_select(ScheduleModeSelect *mode_select);
    // Set the mode select option programmatically using enum
  void set_mode_option(ScheduleMode mode);
  
  // Called when mode select changes from Home Assistant
  void on_mode_changed(const std::string &mode);

  void set_current_event_sensor(text_sensor::TextSensor *sensor) {
    this->current_event_sensor_ = sensor;
  }
  
  void set_next_event_sensor(text_sensor::TextSensor *sensor) {
    this->next_event_sensor_ = sensor;
  }
  
  // Set the time component for time validation
  void set_time(time::RealTimeClock *time) {
    this->time_ = time;
  }
  
  // Update the switch indicator state
  void update_switch_indicator(bool state) {
    if (this->switch_indicator_ != nullptr) {
      this->switch_indicator_->publish_switch_state(state);
    }
  }
  
  // Control the schedule switch state
  void set_schedule_switch_state(bool state) {
    if (this->schedule_switch_ != nullptr) {
      // Update sensor values in the switch before changing state
      this->update_switch_sensor_values_();
      this->schedule_switch_->publish_state(state);
    }
  }
  


  // Test methods for debugging preference storage
  void test_create_preference();
  void test_save_preference();
  void test_load_preference();

 private:
  uint16_t  timeToMinutes_(const char* time_str);
  bool isValidTime_(const JsonVariantConst &time_obj) const;
  void check_rtc_time_valid_();  // Check and update RTC time validity
  void check_ha_connection_();   // Check and update Home Assistant connection state
  void log_state_flags_();       // Log verbose state of boolean flags
  void initialize_schedule_operation_();  // Initialize current and next event times
  void display_current_next_events_(std::string current_text, std::string next_text); // Update current/next event sensors
  int16_t find_current_event_(uint16_t current_time_minutes);  // Find index of current active event (on or off)
  uint16_t time_to_minutes_(auto current_now); // Helper to convert time to minutes from week start
  std::string format_event_time_(uint16_t time_minutes); // Format event time for logging
  std::string create_event_string_(uint16_t event_raw); // Create event string for sensors
  uint16_t get_current_week_minutes_(); // Get current time in minutes from week start
  void set_data_sensors_(int16_t current_index, bool state, bool manual_override);
  void update_switch_sensor_values_();  // Update sensor values in the switch before triggering
  ArrayPreferenceBase *sched_array_pref_{nullptr};
  size_t schedule_max_size_{0};
  size_t schedule_max_entries_{0};

  
  std::vector<uint16_t> schedule_times_in_minutes_; // Use std::vector for runtime sizing
  std::vector<uint16_t> factory_reset_values_= {0xFFFF,0xFFFF}; // Set the MSB to denote end of schedule
  std::string ha_schedule_entity_id_;
  std::vector<DataItem> data_items_;
  std::vector<DataSensor *> data_sensors_;
  ScheduleSwitchIndicator *switch_indicator_{nullptr};
  ScheduleSwitch *schedule_switch_{nullptr};
  ScheduleModeSelect *mode_select_{nullptr};
  text_sensor::TextSensor *current_event_sensor_{nullptr};
  text_sensor::TextSensor *next_event_sensor_{nullptr};
  ScheduleMode current_mode_{SCHEDULE_MODE_MANUAL_OFF};  // Current schedule mode from select
  ScheduleState current_state_{STATE_INIT};  // Current FSM state
  ScheduleState processed_state_{STATE_INIT};  // the last FSM state
  bool ha_connected_{false};
  bool ha_connected_once_{false};
  bool rtc_time_valid_{false};
  bool schedule_valid_{false};
  bool schedule_empty_{true};
  bool update_on_reconnect_{false};
  uint32_t last_connection_check_{0};
  uint32_t last_time_check_{0};
  uint32_t last_schedule_request_time_{0};
  uint32_t last_state_log_time_{0};
  time::RealTimeClock *time_{nullptr};
  uint16_t current_event_raw_{0};  
  uint16_t next_event_raw_{0};
  int16_t current_event_index_{-1};
  int16_t next_event_index_{-1};
  bool event_switch_state_{false};  // true=on, false=off
protected:

  // Action object that sends the HA service call.
  esphome::api::HomeAssistantServiceCallAction<> *ha_get_schedule_action_{nullptr};
  
  // Action object for sending notifications to Home Assistant
  esphome::api::HomeAssistantServiceCallAction<> *ha_notify_action_{nullptr};

  // Containers to own the automations/actions so they remain alive
  std::vector<std::unique_ptr<esphome::Automation<JsonObjectConst>>> ha_json_automations_;
  std::vector<std::unique_ptr<esphome::Action<JsonObjectConst>>> ha_json_actions_;
  std::vector<std::unique_ptr<esphome::Automation<std::string>>> ha_str_automations_;
  std::vector<std::unique_ptr<esphome::Action<std::string>>> ha_str_actions_;

 private:
  // Setup notification service
  void setup_notification_service_();
  // Send notification to Home Assistant
  void send_ha_notification_(const std::string &message, const std::string &title);

};


} // namespace schedule
} // namespace esphome