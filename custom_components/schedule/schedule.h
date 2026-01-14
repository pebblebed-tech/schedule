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
  // Error/Invalid states (use when something is wrong)
  STATE_TIME_INVALID = 0,      // RTC time not synchronized
  STATE_SCHEDULE_INVALID = 1,  // Schedule data is invalid or not available
  STATE_SCHEDULE_EMPTY = 2,    // Schedule is valid but has no events
  
  // Initialization states
  STATE_INIT = 3,              // Initializing schedule operation
  
  // Manual override states
  STATE_MAN_OFF = 4,           // Manual override: forced off
  STATE_MAN_ON = 5,            // Manual override: forced on
  
  // Temporary mode states
  STATE_EARLY_OFF = 6,         // Early-off mode active (temporary override)
  STATE_BOOST_ON = 7,          // Boost mode active (temporary override)
  
  // Auto mode states
  STATE_AUTO_ON = 8,           // Auto mode: schedule indicates ON
  STATE_AUTO_OFF = 9           // Auto mode: schedule indicates OFF
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

  //============================================================================
  // COMPONENT LIFECYCLE METHODS
  //============================================================================
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return esphome::setup_priority::LATE; }
  
  void set_max_schedule_entries(size_t entries);
  void set_max_schedule_size(size_t size);
  void set_update_schedule_on_reconnect(bool update) { this->update_on_reconnect_ = update; }
  
  //============================================================================
  // CONFIGURATION AND SETUP METHODS
  //============================================================================
  void set_schedule_entity_id(const std::string &ha_schedule_entity_id);
  void set_mode_select(ScheduleModeSelect *mode_select);
  void set_switch_indicator(ScheduleSwitchIndicator *indicator) {
    this->switch_indicator_ = indicator;
  }
  void set_schedule_switch(ScheduleSwitch *schedule_switch) { 
    this->schedule_switch_ = schedule_switch; 
  }
  void set_current_event_sensor(text_sensor::TextSensor *sensor) {
    this->current_event_sensor_ = sensor;
  }
  void set_next_event_sensor(text_sensor::TextSensor *sensor) {
    this->next_event_sensor_ = sensor;
  }
  void set_time(time::RealTimeClock *time) {
    this->time_ = time;
  }
  
  //============================================================================
  // UI UPDATE METHODS
  //============================================================================
  void update_switch_indicator(bool state) {
    if (this->switch_indicator_ != nullptr) {
      this->switch_indicator_->publish_switch_state(state);
    }
  }
  void set_schedule_switch_state(bool state) {
    if (this->schedule_switch_ != nullptr) {
      // Update sensor values in the switch before changing state
      this->update_switch_sensor_values_();
      this->schedule_switch_->publish_state(state);
    }
  }
  
  //============================================================================
  // HOME ASSISTANT INTEGRATION
  //============================================================================
  void request_schedule();
  void process_schedule_(const JsonObjectConst &response);
  void setup_schedule_retrieval_service_();
  
  //============================================================================
  // PREFERENCE MANAGEMENT
  //============================================================================
  void create_schedule_preference();
  void load_schedule_from_pref_();
  void save_schedule_to_pref_();
  void sched_add_pref(ArrayPreferenceBase *array_pref);
  void request_pref_hash() {
    ESP_LOGI("*****************", "Preference Hash: %u", this->get_preference_hash());
  }
  
  //============================================================================
  // MODE SELECTION AND CONTROL
  //============================================================================
  void set_mode_option(ScheduleMode mode);
  void on_mode_changed(const std::string &mode);
  
  //============================================================================
  // DATA MANAGEMENT
  //============================================================================
  void add_data_item(const std::string &label, uint16_t value);
  const std::vector<DataItem>& get_data_items() const {
    return data_items_;
  }
  void print_data_items();
  void log_schedule_data();
  void register_data_sensor(DataSensor *sensor) {
    this->data_sensors_.push_back(sensor);
  }

  //============================================================================
  // TEST AND DEBUG METHODS
  //============================================================================
  void test_create_preference();
  void test_save_preference();
  void test_load_preference();

 private:
  //============================================================================
  // STATE MACHINE HELPER METHODS
  //============================================================================
  ScheduleState mode_to_state_(ScheduleMode mode, bool event_on);
  bool should_reset_to_auto_(ScheduleState state, bool event_on);
  ScheduleState get_state_after_mode_reset_(bool event_on);
  void handle_state_change_();
  
  //============================================================================
  // MAIN LOOP HELPER METHODS
  //============================================================================
  bool check_prerequisites_();
  bool should_advance_to_next_event_(uint16_t current_time_minutes);
  void advance_to_next_event_();
  void check_and_advance_events_();
  
  //============================================================================
  // CONFIGURATION AND SETUP HELPERS
  //============================================================================
  void check_rtc_time_valid_();
  void check_ha_connection_();
  void log_state_flags_();
  
  //============================================================================
  // EVENT MANAGEMENT AND SCHEDULING
  //============================================================================
  void initialize_schedule_operation_();
  void initialize_sensor_last_on_values_(int16_t current_event_index);
  int16_t find_current_event_(uint16_t current_time_minutes);
  
  //============================================================================
  // TIME AND FORMATTING UTILITIES
  //============================================================================
  uint16_t timeToMinutes_(const char* time_str);
  bool isValidTime_(const JsonVariantConst &time_obj) const;
  uint16_t time_to_minutes_(auto current_now);
  std::string format_event_time_(uint16_t time_minutes);
  std::string create_event_string_(uint16_t event_raw);
  uint16_t get_current_week_minutes_();
  
  //============================================================================
  // UI UPDATE HELPERS
  //============================================================================
  void display_current_next_events_(std::string current_text, std::string next_text);
  void set_data_sensors_(int16_t current_index, bool state, bool manual_override);
  void update_switch_sensor_values_();
  
  //============================================================================
  // PREFERENCE MANAGEMENT HELPERS
  //============================================================================
  void load_entity_id_from_pref_();
  void save_entity_id_to_pref_();
  
  //============================================================================
  // HOME ASSISTANT INTEGRATION HELPERS
  //============================================================================
  void setup_notification_service_();
  void send_ha_notification_(const std::string &message, const std::string &title);
  
  //============================================================================
  // MEMBER VARIABLES
  //============================================================================
  
  // Preference and configuration
  ArrayPreferenceBase *sched_array_pref_{nullptr};
  size_t schedule_max_size_{0};
  size_t schedule_max_entries_{0};
  std::string ha_schedule_entity_id_;
  
  // Schedule data
  std::vector<uint16_t> schedule_times_in_minutes_;
  std::vector<uint16_t> factory_reset_values_ = {0xFFFF, 0xFFFF};
  std::vector<DataItem> data_items_;
  std::vector<DataSensor *> data_sensors_;
  
  // UI components
  ScheduleSwitchIndicator *switch_indicator_{nullptr};
  ScheduleSwitch *schedule_switch_{nullptr};
  ScheduleModeSelect *mode_select_{nullptr};
  text_sensor::TextSensor *current_event_sensor_{nullptr};
  text_sensor::TextSensor *next_event_sensor_{nullptr};
  
  // State machine
  ScheduleMode current_mode_{SCHEDULE_MODE_MANUAL_OFF};
  ScheduleState current_state_{STATE_INIT};
  ScheduleState processed_state_{STATE_INIT};
  
  // Status flags
  bool ha_connected_{false};
  bool ha_connected_once_{false};
  bool rtc_time_valid_{false};
  bool schedule_valid_{false};
  bool schedule_empty_{true};
  bool update_on_reconnect_{false};
  bool entity_id_changed_{false};
  
  // Timing
  uint32_t last_connection_check_{0};
  uint32_t last_time_check_{0};
  uint32_t last_state_log_time_{0};
  time::RealTimeClock *time_{nullptr};
  
  // Event tracking
  uint16_t current_event_raw_{0};
  uint16_t next_event_raw_{0};
  int16_t current_event_index_{-1};
  int16_t next_event_index_{-1};
  bool event_switch_state_{false};
  
  // Entity ID tracking
  uint32_t stored_entity_id_hash_{0};

protected:
  // Home Assistant service actions
  esphome::api::HomeAssistantServiceCallAction<> *ha_get_schedule_action_{nullptr};
  esphome::api::HomeAssistantServiceCallAction<> *ha_notify_action_{nullptr};

  // Containers to own the automations/actions so they remain alive
  std::vector<std::unique_ptr<esphome::Automation<JsonObjectConst>>> ha_json_automations_;
  std::vector<std::unique_ptr<esphome::Action<JsonObjectConst>>> ha_json_actions_;
  std::vector<std::unique_ptr<esphome::Automation<std::string>>> ha_str_automations_;
  std::vector<std::unique_ptr<esphome::Action<std::string>>> ha_str_actions_;
};


} // namespace schedule
} // namespace esphome