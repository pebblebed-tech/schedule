#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/sensor/sensor.h"
#include "array_preference.h"
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>

namespace esphome {
namespace schedule {

// Forward declaration
class Schedule;

// Enum for data sensor off behavior
enum DataSensorOffBehavior {
  DATA_SENSOR_OFF_BEHAVIOR_NAN = 0,
  DATA_SENSOR_OFF_BEHAVIOR_LAST_ON_VALUE = 1,
  DATA_SENSOR_OFF_BEHAVIOR_OFF_VALUE = 2
};

// Enum for data sensor manual mode behavior
enum DataSensorManualBehavior {
  DATA_SENSOR_MANUAL_BEHAVIOR_NAN = 0,
  DATA_SENSOR_MANUAL_BEHAVIOR_LAST_ON_VALUE = 1,
  DATA_SENSOR_MANUAL_BEHAVIOR_MANUAL_VALUE = 2
};

// DataSensor class
class DataSensor : public sensor::Sensor {
 public:
  DataSensor() = default;

  // Setup method to handle initialization and preference loading
  void setup();
  // Dump configuration for debugging
  void dump_config();

  // Setters
  void set_label(const std::string &label) { this->label_ = label; }
  void set_item_type(uint16_t item_type) { this->item_type_ = item_type; }
  void set_max_schedule_data_entries(uint16_t size);
  void set_parent_schedule(Schedule *parent) { this->parent_schedule_ = parent; }
  void set_array_preference(ArrayPreferenceBase *array_pref) { this->array_pref_ = array_pref; }
  void set_manual_value(float value) { this->manual_value_ = value; }
  void set_manual_behavior(DataSensorManualBehavior behavior) { this->manual_behavior_ = behavior; }
  void set_off_behavior(DataSensorOffBehavior behavior) { this->off_behavior_ = behavior; }
  void set_off_value(float value) { this->off_value_ = value; }

  // Getters
  const std::string &get_label() const { return label_; }
  uint16_t get_item_type() const { return item_type_; }
  uint16_t get_max_schedule_data_entries() const { return max_schedule_data_entries_; }
  uint16_t get_bytes_for_type(uint16_t type) const;
  float get_manual_value() const { return manual_value_; }
  DataSensorManualBehavior get_manual_behavior() const { return manual_behavior_; }
  DataSensorOffBehavior get_off_behavior() const { return off_behavior_; }
  float get_off_value() const { return off_value_; }
  float get_last_on_value() const { return last_on_value_; }
  void set_last_on_value(float value) { this->last_on_value_ = value; }
  
  // Access to data vector
  std::vector<uint8_t>& get_data_vector() { return data_vector_; }
  const std::vector<uint8_t>& get_data_vector() const { return data_vector_; }
  uint16_t get_data_vector_size() const { return this->data_vector_.size(); }
  
  // Get value from data vector at index, convert to float and publish
  void get_and_publish_sensor_value(size_t index);
  
  // Get value from data vector at index without publishing
  float get_sensor_value(size_t index);
  
  // Update the sensor value 
  void publish_value(float value); 
  
  // Apply behavior modes
  void apply_off_behavior(const char* context);  // Apply off behavior and publish value
  void apply_manual_behavior();  // Apply manual behavior and publish value
  void apply_state(int16_t event_index, bool switch_state, bool manual_override);  // Apply appropriate state based on mode
  
  // Add value from string representation
  void add_schedule_data_to_sensor(const std::string &value_str, size_t index);
  
  // Clear the local data vector - set all bytes to 0
  void clear_data_vector() { 
    std::fill(this->data_vector_.begin(), this->data_vector_.end(), 0);
  }
  
  // Log sensor data for debugging
  void log_data_sensor(std::string prefix); 
  
  // Preference management
  uint32_t get_preference_hash() const;
  void save_data_to_pref();  // Save from data_vector_ to array_pref_

 protected:
  void create_preference_();
  void load_data_from_pref_();  // Load from array_pref_ to data_vector_
  const char* get_off_behavior_string() const;  // Helper to convert off_behavior enum to string
  const char* get_manual_behavior_string() const;  // Helper to convert manual_behavior enum to string
  float manual_value_{0.0f};
  DataSensorManualBehavior manual_behavior_{DATA_SENSOR_MANUAL_BEHAVIOR_NAN};
  DataSensorOffBehavior off_behavior_{DATA_SENSOR_OFF_BEHAVIOR_NAN};
  float off_value_{0.0f};
  float last_on_value_{NAN};  // Store last ON value

  std::string label_;
  uint16_t item_type_{0};
  size_t total_bytes_{0};
  uint16_t max_schedule_data_entries_{0};
  std::vector<uint8_t> data_vector_;  // Local working storage
  ArrayPreferenceBase *array_pref_{nullptr};  // Persistent storage
  Schedule *parent_schedule_{nullptr};
};

}  // namespace schedule
}  // namespace esphome
