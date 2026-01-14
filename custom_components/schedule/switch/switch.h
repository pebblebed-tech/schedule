#pragma once
#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "../schedule.h"
#include <map>
#include <string>

namespace esphome {
namespace schedule {

// ScheduleSwitch class - switch that contains schedule logic
class ScheduleSwitch : public switch_::Switch, public Schedule {
 public:
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
  
  void setup() override {
    // Sync entity info from EntityBase (via Switch) to Schedule base class
    // EntityBase::get_object_id() returns const char*, EntityBase::get_name() returns StringRef
    this->sync_from_entity(this->EntityBase::get_object_id(), this->EntityBase::get_name().c_str());
    // Call Schedule's setup
    Schedule::setup();
  }
  
  void loop() override;
  void dump_config() override;
  
 protected:
  // Platform-specific implementation of scheduled state application
  void apply_scheduled_state(bool on) override {
    // Update sensor values before publishing state (for automations)
    for (auto *sensor : this->data_sensors_) {
      float current_value = sensor->state;
      this->set_sensor_value(sensor->get_label(), current_value);
    }
    // Publish the switch state
    this->publish_state(on);
  }
  
  void write_state(bool state) override;
  std::map<std::string, float> sensor_values_;
};

} // namespace schedule
} // namespace esphome
