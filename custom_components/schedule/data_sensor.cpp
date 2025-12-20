// data_sensor.cpp - Create this file in custom_components/schedule/
#include "data_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace schedule {

static const char *const TAG = "schedule.data_sensor";

void DataSensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up DataSensor '%s'...", this->label_.c_str());
}

void DataSensor::dump_config() {
  LOG_SENSOR("", "DataSensor", this);
  ESP_LOGCONFIG(TAG, "  Label: %s", this->label_.c_str());
  ESP_LOGCONFIG(TAG, "  Item Type: %u", this->item_type_);
}

void DataSensor::publish_value(float value) {
  this->publish_state(value);
}

}  // namespace schedule
}  // namespace esphome