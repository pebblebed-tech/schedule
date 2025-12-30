#include "data_sensor.h"
#include "schedule.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace schedule {

static const char *const TAG_DATA_SENSOR = "schedule.data_sensor";

void DataSensor::setup() {
  ESP_LOGI(TAG_DATA_SENSOR, "Setting up DataSensor '%s'...", this->get_label().c_str());
  
  // Ensure max_schedule_entries_ is set before setup
  if (this->max_schedule_data_entries_ == 0) {
    ESP_LOGE(TAG_DATA_SENSOR, "max_schedule_entries not set for sensor '%s'", this->get_label().c_str());
    return;
  }
  
  // Ensure array_pref_ is set
  if (this->array_pref_ == nullptr) {
    ESP_LOGE(TAG_DATA_SENSOR, "array_pref not set for sensor '%s'", this->get_label().c_str());
    return;
  }
  
  // Calculate bytes needed and resize local vector
  this->total_bytes_ = this->max_schedule_data_entries_ * this->get_bytes_for_type(this->item_type_);
  this->data_vector_.resize(total_bytes_);
  std::fill(this->data_vector_.begin(), this->data_vector_.end(), 0);
  
  // Create preference and load data from persistent storage into local vector
  this->create_preference_();
  this->load_data_from_pref_();
  
  ESP_LOGI(TAG_DATA_SENSOR, "DataSensor '%s' setup complete: %u bytes local storage, %u bytes persistent storage",
           this->get_label().c_str(), 
           static_cast<unsigned>(this->data_vector_.size()),
           static_cast<unsigned>(this->array_pref_->size()));
}

void DataSensor::dump_config() {
  size_t array_size = this->array_pref_ ? this->array_pref_->size() : 0;
  ESP_LOGCONFIG(TAG_DATA_SENSOR, "DataSensor '%s': label='%s', item_type=%u, max_schedule_entries=%u, data_vector_size=%u bytes, array_pref_size=%u bytes", 
           this->get_object_id().c_str(),
           this->get_label().c_str(),
           this->item_type_,
           static_cast<unsigned>(this->max_schedule_data_entries_),
           static_cast<unsigned>(this->data_vector_.size()),
           static_cast<unsigned>(array_size));
}

void DataSensor::set_max_schedule_data_entries(uint16_t size) {
  this->max_schedule_data_entries_ = size;
  ESP_LOGD(TAG_DATA_SENSOR, "Sensor %s set to %u entries", this->get_object_id().c_str(), this->max_schedule_data_entries_);
}

uint16_t DataSensor::get_bytes_for_type(uint16_t type) const {
  switch (type) {
    case 0:  // uint8_t
      return 1;
    case 1:  // uint16_t
      return 2;
    case 2:  // int32_t
      return 4;
    case 3:  // float
      return 4;
    default:
      return 1;
  }
}

void DataSensor::add_schedule_data_to_sensor(const std::string &value_str, size_t index) {
  if (value_str.empty()) {
    ESP_LOGE(TAG_DATA_SENSOR, "Empty string cannot be converted to value for sensor '%s'", this->get_label().c_str());
    return;
  }

  // Calculate bytes per item and actual vector index
  uint16_t bytes_per_item = get_bytes_for_type(this->item_type_);
  size_t actual_index = index * bytes_per_item;

  // Validate index is within bounds
  if (actual_index + bytes_per_item > this->data_vector_.size()) {
    ESP_LOGE(TAG_DATA_SENSOR, "Index %u out of bounds for sensor '%s' (max: %u)", 
             static_cast<unsigned>(index), this->get_label().c_str(), 
             static_cast<unsigned>(this->data_vector_.size() / bytes_per_item));
    return;
  }

  switch (this->item_type_) {
    case 0: {  // uint8_t
      char *endptr;
      unsigned long temp = strtoul(value_str.c_str(), &endptr, 10);
      if (*endptr != '\0' || endptr == value_str.c_str()) {
        ESP_LOGE(TAG_DATA_SENSOR, "Invalid argument: cannot convert '%s' to numeric value for sensor '%s'", 
                 value_str.c_str(), this->get_label().c_str());
        return;
      }
      if (temp > 255) {
        ESP_LOGE(TAG_DATA_SENSOR, "Value '%s' out of range for uint8_t (0-255) in sensor '%s'", value_str.c_str(), this->get_label().c_str());
        return;
      }
      uint8_t value = static_cast<uint8_t>(temp);
      this->data_vector_[actual_index] = value;
      break;
    }
    case 1: {  // uint16_t
      char *endptr;
      unsigned long temp = strtoul(value_str.c_str(), &endptr, 10);
      if (*endptr != '\0' || endptr == value_str.c_str()) {
        ESP_LOGE(TAG_DATA_SENSOR, "Invalid argument: cannot convert '%s' to numeric value for sensor '%s'", 
                 value_str.c_str(), this->get_label().c_str());
        return;
      }
      if (temp > 65535) {
        ESP_LOGE(TAG_DATA_SENSOR, "Value '%s' out of range for uint16_t (0-65535) in sensor '%s'", value_str.c_str(), this->get_label().c_str());
        return;
      }
      uint16_t value = static_cast<uint16_t>(temp);
      uint8_t bytes[2];
      std::memcpy(bytes, &value, sizeof(value));
      std::memcpy(&this->data_vector_[actual_index], bytes, 2);
      break;
    }
    case 2: {  // int32_t
      char *endptr;
      long temp = strtol(value_str.c_str(), &endptr, 10);
      if (*endptr != '\0' || endptr == value_str.c_str()) {
        ESP_LOGE(TAG_DATA_SENSOR, "Invalid argument: cannot convert '%s' to numeric value for sensor '%s'", 
                 value_str.c_str(), this->get_label().c_str());
        return;
      }
      if (temp < INT32_MIN || temp > INT32_MAX) {
        ESP_LOGE(TAG_DATA_SENSOR, "Value '%s' out of range for int32_t (%d to %d) in sensor '%s'", 
                 value_str.c_str(), INT32_MIN, INT32_MAX, this->get_label().c_str());
        return;
      }
      int32_t value = static_cast<int32_t>(temp);
      uint8_t bytes[4];
      std::memcpy(bytes, &value, sizeof(value));
      std::memcpy(&this->data_vector_[actual_index], bytes, 4);
      break;
    }
    case 3: {  // float
      char *endptr;
      float value = strtof(value_str.c_str(), &endptr);
      if (*endptr != '\0' || endptr == value_str.c_str()) {
        ESP_LOGE(TAG_DATA_SENSOR, "Invalid argument: cannot convert '%s' to numeric value for sensor '%s'", 
                 value_str.c_str(), this->get_label().c_str());
        return;
      }
      if (!std::isfinite(value)) {
        ESP_LOGE(TAG_DATA_SENSOR, "Value '%s' is not a valid finite float in sensor '%s'", value_str.c_str(), this->get_label().c_str());
        return;
      }
      uint8_t bytes[4];
      std::memcpy(bytes, &value, sizeof(value));
      std::memcpy(&this->data_vector_[actual_index], bytes, 4);
      break;
    }
    default:
      ESP_LOGE(TAG_DATA_SENSOR, "Unknown item_type: %u for sensor '%s'", this->item_type_, this->get_label().c_str());
      return;
  }
}

void DataSensor::get_and_publish_sensor_value(size_t index) {
  // Calculate bytes per item and actual vector index
  size_t actual_index = index * this->get_bytes_for_type(this->item_type_);

  // Validate index is within bounds
  if (actual_index + this->get_bytes_for_type(this->item_type_) > this->data_vector_.size()) {
    ESP_LOGE(TAG_DATA_SENSOR, "Index %u out of bounds for sensor '%s' (max: %u)", 
             static_cast<unsigned>(index), this->get_label().c_str(), 
             static_cast<unsigned>(this->data_vector_.size() / this->get_bytes_for_type(this->item_type_)));
    return;
  }

  float value_as_float = 0.0f;

  switch (this->item_type_) {
    case 0: {  // uint8_t
      uint8_t value = this->data_vector_[actual_index];
      value_as_float = static_cast<float>(value);
      break;
    }
    case 1: {  // uint16_t
      uint16_t value;
      std::memcpy(&value, &this->data_vector_[actual_index], sizeof(value));
      value_as_float = static_cast<float>(value);
      break;
    }
    case 2: {  // int32_t
      int32_t value;
      std::memcpy(&value, &this->data_vector_[actual_index], sizeof(value));
      value_as_float = static_cast<float>(value);
      break;
    }
    case 3: {  // float
      std::memcpy(&value_as_float, &this->data_vector_[actual_index], sizeof(value_as_float));
      break;
    }
    default:
      ESP_LOGE(TAG_DATA_SENSOR, "Unknown item_type: %u for sensor '%s'", this->item_type_, this->get_label().c_str());
      return;
  }

  // Publish the value as a sensor state
  this->publish_state(value_as_float);
  ESP_LOGD(TAG_DATA_SENSOR, "Published value %.2f from index %u for sensor '%s'", 
           value_as_float, static_cast<unsigned>(index), this->get_label().c_str());
}

uint32_t DataSensor::get_preference_hash() const {
  // Create a unique hash based on parent schedule's object ID and this sensor's label
  if (this->parent_schedule_ != nullptr) {
    uint32_t parent_hash = this->parent_schedule_->get_object_id_hash();
    uint32_t label_hash = fnv1_hash(this->label_);
    return parent_hash ^ label_hash;
  }
  // Fallback to just label hash if no parent
  return fnv1_hash(this->label_);
}

void DataSensor::create_preference_() {
  if (this->array_pref_ == nullptr) {
    ESP_LOGE(TAG_DATA_SENSOR, "array_pref is null for sensor '%s'", this->get_label().c_str());
    return;
  }
  
  uint32_t hash = this->get_preference_hash();
  this->array_pref_->create_preference(hash);
  ESP_LOGD(TAG_DATA_SENSOR, "Created preference for sensor '%s' with hash 0x%08X", 
           this->get_label().c_str(), hash);
}

void DataSensor::load_data_from_pref_() {
  if (this->array_pref_ == nullptr) {
    ESP_LOGE(TAG_DATA_SENSOR, "array_pref is null for sensor '%s'", this->get_label().c_str());
    return;
  }
  
  // Load data from persistent storage into array_pref
  this->array_pref_->load();
  
  if (this->array_pref_->is_valid()) {
    // Copy from array_pref to local data_vector_
    uint8_t *pref_data = this->array_pref_->data();
    size_t size = std::min(this->data_vector_.size(), this->array_pref_->size());
    std::memcpy(this->data_vector_.data(), pref_data, size);
    ESP_LOGI(TAG_DATA_SENSOR, "Loaded %u bytes from preferences into local vector for sensor '%s'", 
             static_cast<unsigned>(size), this->get_label().c_str());
  } else {
    ESP_LOGI(TAG_DATA_SENSOR, "No stored values for sensor '%s'; using defaults (zeros)", 
             this->get_label().c_str());
  }
}

void DataSensor::save_data_to_pref() {
  if (this->array_pref_ == nullptr) {
    ESP_LOGE(TAG_DATA_SENSOR, "array_pref is null for sensor '%s'", this->get_label().c_str());
    return;
  }
  
  // Copy from local data_vector_ to array_pref
  uint8_t *pref_data = this->array_pref_->data();
  size_t size = std::min(this->data_vector_.size(), this->array_pref_->size());
  std::memcpy(pref_data, this->data_vector_.data(), size);
  
  // Save to persistent storage
  this->array_pref_->save();
  ESP_LOGI(TAG_DATA_SENSOR, "Saved %u bytes from local vector to preferences for sensor '%s'", 
           static_cast<unsigned>(size), this->get_label().c_str());
}

void DataSensor::log_data_sensor(std::string prefix) {
  ESP_LOGI(TAG_DATA_SENSOR, "Function %s DataSensor '%s' data vector contents:", prefix.c_str(), this->get_label().c_str());
  for (size_t i = 0; i < this->data_vector_.size(); ++i) {
    ESP_LOGI(TAG_DATA_SENSOR, "Index %u: 0x%02X", static_cast<unsigned>(i), this->data_vector_[i]);
  }
}

}  // namespace schedule
}  // namespace esphome
