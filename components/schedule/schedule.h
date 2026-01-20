#pragma once
#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include "esphome/core/automation.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/components/api/api_server.h"
#include "esphome/components/api/user_services.h"
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

// Macro to safely get data sensor value from schedule by label
// Usage: float temp = SCHEDULE_GET_DATA(testschedule, "temp");
// Returns NAN if sensor not found, logs warning
#define SCHEDULE_GET_DATA(schedule_id, label) \
  ({ \
    float _result = NAN; \
    auto *_sensor = id(schedule_id).get_data_sensor(label); \
    if (_sensor != nullptr) { \
      _result = _sensor->state; \
    } else { \
      ESP_LOGW("schedule", "Data sensor with label '%s' not found in schedule '%s'", label, #schedule_id); \
    } \
    _result; \
  })

namespace esphome {

// Forward declarations for API types
namespace api {
template <typename... Ts> class HomeAssistantServiceCallAction;
}

namespace schedule {

// Constants for schedule event encoding
static constexpr uint16_t SWITCH_STATE_BIT = 0x4000;  // Bit 14: switch state
static constexpr uint16_t TIME_MASK = 0x3FFF;         // Bits 0-13: time in minutes

// Enum for storage type - determines how schedule data is stored
enum ScheduleStorageType {
  STORAGE_TYPE_STATE_BASED = 0,  // Stores [ON_TIME, OFF_TIME] pairs (default)
  STORAGE_TYPE_EVENT_BASED = 1   // Stores [EVENT_TIME] singles only
};

// Forward declarations
class Schedule;
class ScheduleSwitch;

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

// Schedule class - base component (doesn't inherit from EntityBase to avoid diamond problem)
class Schedule : public Component  {
 public:
  struct DataItem {
    std::string label;
    uint16_t value;
    uint16_t size;
  };

  //============================================================================
  // VIRTUAL METHODS FOR STORAGE TYPE EXTENSIBILITY
  //============================================================================
  
  /** Get the storage type for this schedule component
   * Default is STATE_BASED for backward compatibility
   * Override in derived classes for different storage types
   */
  virtual ScheduleStorageType get_storage_type() const { 
    return STORAGE_TYPE_STATE_BASED; 
  }
  
  /** Get storage multiplier for array size calculation 
   * State-based: 2 (ON + OFF per entry)
   * Event-based: 1 (EVENT only per entry)
   */
  virtual size_t get_storage_multiplier() const {
    return (get_storage_type() == STORAGE_TYPE_STATE_BASED) ? 2 : 1;
  }
  
  /** Parse a single schedule entry from Home Assistant JSON
   * Default implementation: state-based (extracts "from" and "to")
   * Override for event-based (extracts only "from")
   */
  virtual void parse_schedule_entry(const JsonObjectConst &entry, 
                                    std::vector<uint16_t> &work_buffer,
                                    uint16_t day_offset);

  //============================================================================
  // COMPONENT LIFECYCLE METHODS
  //============================================================================
  void setup() override;
  float get_setup_priority() const override { return esphome::setup_priority::LATE; }
  
  void set_max_schedule_entries(size_t entries);
  void set_max_schedule_size(size_t size);
  void set_update_schedule_on_reconnect(bool update) { this->update_on_reconnect_ = update; }
  size_t get_max_schedule_entries() const { return this->schedule_max_entries_; }
  
  //============================================================================
  // INTERNAL IDENTIFICATION (for preferences - set by platform implementation)
  //============================================================================
  void sync_from_entity(const std::string &object_id, const std::string &name) {
    this->object_id_ = object_id;
    this->name_ = name;
  }
  const std::string& get_object_id() const { return this->object_id_; }
  uint32_t get_object_id_hash() const { return fnv1_hash(this->object_id_); }
  uint32_t get_preference_hash() const { return this->get_object_id_hash(); }
  
  //============================================================================
  // CONFIGURATION AND SETUP METHODS
  //============================================================================
  void set_schedule_entity_id(const std::string &ha_schedule_entity_id);
  void set_switch_indicator(ScheduleSwitchIndicator *indicator) {
    this->switch_indicator_ = indicator;
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
  
  // Send notification to Home Assistant
  void send_notification(const std::string &message, const std::string &title) {
    this->send_ha_notification_(message, title);
  }
  
  //============================================================================
  // PLATFORM-SPECIFIC METHODS (to be implemented by derived classes)
  //============================================================================
 protected:
  /** Update the schedule state machine - call this from platform's loop().
   * 
   * This performs all schedule logic: checks time validity, HA connection,
   * processes events, and calls apply_scheduled_state() when state changes.
   * Platform implementations should call this from their loop() method.
   */
  
  /** Dump base schedule configuration - call from platform's dump_config().
   * 
   * This logs the common schedule configuration. Platform implementations
   * should call this from their dump_config() and add platform-specific info.
   */
  void dump_config_base();
  
  /** Apply the scheduled on/off state to the platform-specific component.
   * 
   * This method must be implemented by each platform (ScheduleSwitch, ScheduleClimate, etc.)
   * to apply the scheduled state in a platform-appropriate way:
   * - Switch: just turn on/off
   * - Climate: turn on/off + set temperature from data sensors
   * - Cover: open/close + set position from data sensors
   * - Lock: lock/unlock + set timeout from data sensors
   * 
   * Data sensor values are accessible via this->data_sensors_
   * 
   * @param on true if schedule indicates ON state, false for OFF state
   */
  virtual void apply_scheduled_state(bool on) = 0;
  
  // Schedule configuration and data (protected for derived class access)
  size_t schedule_max_entries_{0};
  std::vector<uint16_t> schedule_times_in_minutes_;
  
  // Data sensors are protected so platform implementations can access sensor values
  std::vector<DataSensor*> data_sensors_;
  
 public:
  
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
    ESP_LOGI("schedule", "Preference Hash: %u", this->get_preference_hash());
  }
  
  //============================================================================
  // DATA MANAGEMENT
  //============================================================================
  void add_data_item(const std::string &label, uint16_t value);
  const std::vector<DataItem>& get_data_items() const {
    return data_items_;
  }
  void print_data_items();
  virtual void log_schedule_data();  // Virtual - derived classes log their own format
  void register_data_sensor(DataSensor *sensor) {
    this->data_sensors_.push_back(sensor);
  }
  
  // Get data sensor by label
  DataSensor* get_data_sensor(const std::string &label) {
    for (auto *sensor : this->data_sensors_) {
      if (sensor->get_label() == label) {
        return sensor;
      }
    }
    return nullptr;
  }
  
  /** Force reinitialization after schedule update 
   * Virtual method - derived classes implement their own state reset logic
   */
  virtual void force_reinitialize() = 0;
  
  /** Handle schedule empty state change - update mode select options accordingly
   * Virtual method - state-based schedulable overrides to update mode options
   */
  virtual void on_schedule_empty_changed(bool is_empty) {}

  //============================================================================
  // TEST AND DEBUG METHODS
  //============================================================================
  void test_create_preference();
  void test_save_preference();
  void test_load_preference();
  
 protected:
  //============================================================================
  // TIME AND FORMATTING UTILITIES (used by derived classes)
  //============================================================================
  uint16_t timeToMinutes_(const char* time_str);

 protected:
  //============================================================================
  // MAIN LOOP HELPER METHODS
  //============================================================================
  // Prerequisite check return values
  enum PrerequisiteError {
    PREREQ_OK = 0,
    PREREQ_TIME_INVALID = 1,
    PREREQ_SCHEDULE_INVALID = 2,
    PREREQ_SCHEDULE_EMPTY = 3
  };
  
  PrerequisiteError check_prerequisites_();
  bool should_advance_to_next_event_(uint16_t current_time_minutes);
  virtual void advance_to_next_event_();
  virtual void check_and_advance_events_();
  
  //============================================================================
  // CONFIGURATION AND SETUP HELPERS
  //============================================================================
  void check_rtc_time_valid_();
  void check_ha_connection_();
  void log_state_flags_();
  
  //============================================================================
  // EVENT MANAGEMENT AND SCHEDULING
  //============================================================================
  virtual void initialize_schedule_operation_();

 private:
  int16_t find_current_event_(uint16_t current_time_minutes);
  
  //============================================================================
  // TIME AND FORMATTING UTILITIES
  //============================================================================
  bool isValidTime_(const JsonVariantConst &time_obj) const;
  uint16_t get_current_week_minutes_();
  
 protected:
  //============================================================================
  // FORMATTING (Protected for derived classes)
  //============================================================================
  std::string format_event_time_(uint16_t time_minutes);
  
  //============================================================================
  // UI UPDATE HELPERS (Protected for StateBasedSchedulable)
  //============================================================================
  void display_current_next_events_(std::string current_text, std::string next_text);
  void set_data_sensors_(int16_t current_index, bool state, bool manual_override);
  
 private:
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
  
  // Entity identification (since Schedule doesn't inherit from EntityBase)
  std::string object_id_;
  std::string name_;
  
  // Preference and configuration
  ArrayPreferenceBase *sched_array_pref_{nullptr};
  size_t schedule_max_size_{0};
  std::string ha_schedule_entity_id_;
  
  // Schedule data (private core data)
  std::vector<uint16_t> factory_reset_values_ = {0xFFFF, 0xFFFF};
  std::vector<DataItem> data_items_;
  
  // UI components
  ScheduleSwitchIndicator *switch_indicator_{nullptr};
  text_sensor::TextSensor *current_event_sensor_{nullptr};
  text_sensor::TextSensor *next_event_sensor_{nullptr};
  
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
  
 protected:
  // Timing (protected for derived class access)
  uint32_t last_time_check_{0};
  uint32_t last_state_log_time_{0};
  time::RealTimeClock *time_{nullptr};
  
  // Time utilities (protected for derived class access)
  // Template function must be defined in header
  uint16_t time_to_minutes_(auto current_now) {
    // Calculate current time in minutes from start of week (Monday = 0)
    // ESPHome: 1=Sunday, 2=Monday, ..., 7=Saturday
    // We need: Monday=0, ..., Sunday=6
    uint8_t day_of_week = (current_now.day_of_week + 5) % 7;  // Convert to Monday=0
    uint16_t current_time_minutes = (day_of_week * 1440) + (current_now.hour * 60) + current_now.minute;
    return current_time_minutes;
  }
  
  // Event tracking (protected for state machine access)
  uint16_t current_event_raw_{0};
  uint16_t next_event_raw_{0};
  int16_t current_event_index_{-1};
  int16_t next_event_index_{-1};
  
 private:
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