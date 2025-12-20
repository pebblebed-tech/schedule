#pragma once
#include "esphome.h"
#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include "esphome/components/api/homeassistant_service.h"
#include "esphome/components/json/json_util.h"
#include "esphome/components/sensor/sensor.h"
#include <vector>
#include <string>
#include <cstring>

namespace esphome {
namespace schedule {

// Convert macro into a compile-time constant
constexpr std::size_t SCHEDULE_MAX_SIZE = static_cast<std::size_t>(SCHEDULE_COMPONENT_MAX_SIZE);

// DataSensor class
class DataSensor : public sensor::Sensor {
 public:
  DataSensor() = default;

  // Setters
  void set_label(const std::string &label) { this->label_ = label; }
  void set_item_type(uint16_t item_type) { this->item_type_ = item_type; }
  void set_max_schedule_size(uint16_t size);

  // Getters
  const std::string &get_label() const { return label_; }
  uint16_t get_item_type() const { return item_type_; }
  uint16_t get_max_schedule_size() const { return max_schedule_size_; }
  std::vector<uint8_t>& get_data_vector() { return data_vector_; }
  const std::vector<uint8_t>& get_data_vector() const { return data_vector_; }

  // Update the sensor value
  void publish_value(float value) { this->publish_state(value); }
  
  // Clear the data vector - set all bytes to 0
  void clear_data_vector() { 
    std::fill(this->data_vector_.begin(), this->data_vector_.end(), 0);
  }
  
  // Add value from string representation
  void add_schedule_data_to_sensor(const std::string &value_str, size_t index);

  // Get value from data vector at index, convert to float and publish
  void get_and_publish_sensor_value(size_t index);
// Helper function to get bytes for each type
  uint16_t get_bytes_for_type(uint16_t type) const;
 protected:
  

  std::string label_;
  uint16_t item_type_{0};
  uint16_t max_schedule_size_{0};
  std::vector<uint8_t> data_vector_;
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
   /// Trigger a schedule.get_schedule request.
  void request_schedule();
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

 private:
  uint16_t  timeToMinutes_(const char* time_str);
  bool isValidTime_(const JsonVariantConst &time_obj) const;

 

  size_t schedule_max_size_{0};

    // Fixed-size POD structure for persistence (trivially copyable)
  struct PrefBlob {
    uint16_t count;
    uint16_t values[SCHEDULE_MAX_SIZE * 2];
  } __attribute__((packed));
  
  std::vector<uint16_t> schedule_times_in_minutes_; // Use std::vector for runtime sizing
  std::vector<uint16_t> factory_reset_values_= {0xFFFF,0xFFFF}; // Set the MSB to denote end of schedule
  std::string ha_schedule_entity_id_;
  std::vector<DataItem> data_items_;
  std::vector<DataSensor *> data_sensors_;
protected:
  ESPPreferenceObject schedule_pref_;

    // Action object that sends the HA service call.
  esphome::api::HomeAssistantServiceCallAction<> *ha_get_schedule_action_{nullptr};


};

} // namespace schedule
} // namespace esphome
