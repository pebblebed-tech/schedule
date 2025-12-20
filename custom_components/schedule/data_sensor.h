// data_sensor.h - Create this file in custom_components/schedule/
#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include <string>

namespace esphome {
namespace schedule {

class DataSensor : public sensor::Sensor, public Component {
 public:
  // Custom constructor that takes label and item type
  DataSensor(const std::string &label, uint16_t item_type) 
      : label_(label), item_type_(item_type) {
    // Set the sensor name based on the label
    this->set_name(label.c_str());
  }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return esphome::setup_priority::DATA; }

  // Getters
  const std::string &get_label() const { return label_; }
  uint16_t get_item_type() const { return item_type_; }

  // Update the sensor value based on the item type
  void publish_value(float value);

 protected:
  std::string label_;
  uint16_t item_type_;
};

}  // namespace schedule
}  // namespace esphome