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
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif
#include <vector>
#include <string>
#include <cstring>
#include "array_preference.h"
#include "data_sensor.h"

namespace esphome {
namespace schedule {

// Forward declaration
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
  
 protected:
  void write_state(bool state) override;
  Schedule *schedule_{nullptr};
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
  
  void set_mode_select(ScheduleModeSelect *mode_select) { 
    this->mode_select_ = mode_select; 
  }
  
  void set_current_event_sensor(text_sensor::TextSensor *sensor) {
    this->current_event_sensor_ = sensor;
  }
  
  void set_next_event_sensor(text_sensor::TextSensor *sensor) {
    this->next_event_sensor_ = sensor;
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
      this->schedule_switch_->publish_state(state);
    }
  }
  
  // Set the mode select option programmatically
  void set_mode_option(const std::string &option);
  
  // Called when mode select changes from Home Assistant
  void on_mode_changed(const std::string &mode);

  // Test methods for debugging preference storage
  void test_create_preference();
  void test_save_preference();
  void test_load_preference();

 private:
  uint16_t  timeToMinutes_(const char* time_str);
  bool isValidTime_(const JsonVariantConst &time_obj) const;
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
  bool ha_connected_{false};
  uint32_t last_connection_check_{0};
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