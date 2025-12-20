#include "schedule.h"
#include "esphome/components/api/api_server.h"
#include "esphome/components/api/homeassistant_service.h"
#include "esphome/core/application.h"
#include "esphome/core/automation.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include "esphome/components/json/json_util.h"
#include "esphome/core/helpers.h"

#include <functional>
// This is needed due to a bug in the logic in HomeAsitant Service Call Action with JSON responses
// Define this to enable JSON response handling for HomeAssistant actions
#ifndef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
#define USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
#endif

namespace esphome {
namespace schedule {
// forward declaration of Schedule

static const char *const TAG = "schedule";
static const char *const TAG_DATA_SENSOR = "schedule.data_sensor";

class MySuccessWithJsonTrigger : public Trigger<JsonObjectConst> {
 public:
  explicit MySuccessWithJsonTrigger(Schedule *parent) : parent_(parent) {}
  
  void trigger(const JsonObjectConst &response)  {
    // Inspect the JSON. 
    ESP_LOGI(TAG, "Received JSON response from Home Assistant action");
    JsonObjectConst next_hour = response["response"]["schedule.test_sche"]["monday"][0];
              
    ESP_LOGD("mon", "Monday: %s", std::string(next_hour["from"]).c_str());
    parent_->process_schedule_(response);
  }
  
 protected:
  Schedule *parent_;
};

// Error trigger (error message string)
class MyErrorTrigger : public Trigger<std::string> {
 public:
  void trigger(const std::string &err)  {
    ESP_LOGW(TAG, "Home Assistant Get_Schedule service call failed: %s", err.c_str());
  }
};


// DataSensor implementations
void DataSensor::set_max_schedule_size(uint16_t size) {
  this->max_schedule_size_ = size;
  // Calculate bytes needed based on item_type and resize vector
  uint16_t bytes_per_item = get_bytes_for_type(this->item_type_);
  this->data_vector_.resize(this->max_schedule_size_ * bytes_per_item);
  ESP_LOGD(TAG, "Sensor %s has a size of %u ",this->get_object_id().c_str(), this->data_vector_.size());
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
  uint16_t bytes_per_item = get_bytes_for_type(this->item_type_);
  size_t actual_index = index * bytes_per_item;

  // Validate index is within bounds
  if (actual_index + bytes_per_item > this->data_vector_.size()) {
    ESP_LOGE(TAG_DATA_SENSOR, "Index %u out of bounds for sensor '%s' (max: %u)", 
             static_cast<unsigned>(index), this->get_label().c_str(), 
             static_cast<unsigned>(this->data_vector_.size() / bytes_per_item));
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

//class Schedule;

void Schedule::set_max_schedule_size(size_t size) {
    this->schedule_max_size_ = size * 2;  // Each entry has a start and end time so actual size is double   // Resize the vector once the size is known at runtime
    //this->schedule_times_in_minutes_.resize(this->schedule_max_size_); 
}

void Schedule::setup() {
    ESP_LOGI(TAG, "Setting up Schedule component...");
    load_schedule_from_pref_();
    setup_schedule_retrieval_service_();
}

void Schedule::loop() {

}

void Schedule::dump_config() {
    
    ESP_LOGCONFIG(TAG, "Schedule Entity ID: %s", ha_schedule_entity_id_.c_str());
    ESP_LOGCONFIG(TAG, "Schedule Max size: %d", schedule_max_size_);
    ESP_LOGCONFIG(TAG, "SCHEDULE_MAX_SIZE : %d",SCHEDULE_MAX_SIZE); 
    ESP_LOGCONFIG(TAG, "Comp ID ID: %s", this->get_object_id().c_str());
    ESP_LOGCONFIG(TAG, "Object ID: %s", this->get_object_id().c_str());
    ESP_LOGCONFIG(TAG, "Preference Hash: %u", this->get_preference_hash());
    ESP_LOGCONFIG(TAG, "Object Hash ID: %u", this->get_object_id_hash());
    ESP_LOGCONFIG(TAG, "name: %s", this->get_name());

}

void Schedule::set_schedule_entity_id(const std::string &ha_schedule_entity_id){
    this->ha_schedule_entity_id_ = ha_schedule_entity_id;
}

// Helper function to convert "HH:MM:SS" or "HH:MM" to minutes from start of day
uint16_t Schedule::timeToMinutes_(const char* time_str) {
        uint8_t  h = 0, m = 0, s = 0;
        if (sscanf(time_str, "%d:%d:%d", &h, &m, &s) == 3) return (h * 60) + m;
        if (sscanf(time_str, "%d:%d", &h, &m) == 2) return (h * 60) + m;
        return 0;
}
// Check from and to times are valid times eg 00:00:00 through 23:59:59
bool Schedule::isValidTime_(const JsonVariantConst &time_obj) const {
    const char* time_str = time_obj.as<const char*>();
    int h = 0, m = 0, s = 0;
    if (sscanf(time_str, "%d:%d:%d", &h, &m, &s) == 3) {
        return (h >= 0 && h < 24 && m >= 0 && m < 60 && s >= 0 && s < 60);
    }
    if (sscanf(time_str, "%d:%d", &h, &m) == 2) {
        return (h >= 0 && h < 24 && m >= 0 && m < 60);
    }
    return false;
}
// Function to load the array from flash memory
void Schedule::load_schedule_from_pref_() {
    ESP_LOGV(TAG, "Loading schedule from preferences");
    ESP_LOGD(TAG, "Preference hash: 0x%08X", this->get_preference_hash());
    uint32_t hash = 1967399030U ^ fnv1_hash(App.get_compilation_time_ref().c_str());
    this->schedule_pref_ = global_preferences->make_preference<PrefBlob>(hash);
    ESP_LOGD(TAG, "WE have preference");
    PrefBlob blob{};
    const bool ok = this->schedule_pref_.load(&blob);
    if (ok) {
    // Basic validation of loaded count
        if (blob.count <= this->schedule_max_size_) {
                this->schedule_times_in_minutes_.clear();
                this->schedule_times_in_minutes_.reserve(blob.count);
                for (size_t i = 0; i < blob.count; ++i)
                    this->schedule_times_in_minutes_.push_back(blob.values[i]);
                ESP_LOGI(TAG, "Loaded %u uint16_t values from preferences", static_cast<unsigned>(blob.count));
        } else {
            ESP_LOGW(TAG, "Stored count (%u) exceeds max size (%u); ignoring stored data", static_cast<unsigned>(blob.count), static_cast<unsigned>(this->schedule_max_size_));
                // fallback to initial values and persist them
                this->schedule_times_in_minutes_ = this->factory_reset_values_;
                if (this->schedule_times_in_minutes_.size() > this->schedule_max_size_) {
                    this->schedule_times_in_minutes_.resize(this->schedule_max_size_);
                }
                    this->save_schedule_to_pref_();
        }
        } else {
            // No stored data: use YAML initial values and persist them
            this->schedule_times_in_minutes_ = this->factory_reset_values_;
           if (this->schedule_times_in_minutes_.size() > this->schedule_max_size_) {
            this->schedule_times_in_minutes_.resize(this->schedule_max_size_);
           }
            this->save_schedule_to_pref_();
            ESP_LOGI(TAG, "No stored values; using YAML defaults and saving them");

        }
     // Debug log values
    for (size_t i = 0; i < this->schedule_times_in_minutes_.size(); ++i) {
        ESP_LOGD(TAG, "schedule_times_in_minutes_[%u] = %u", static_cast<unsigned>(i), this->schedule_times_in_minutes_[i]);
        } 
}

// Function to save the current vector array to flash memory
void Schedule::save_schedule_to_pref_() {
    ESP_LOGV(TAG, "Saving schedule");
    // Safety check to ensure we do not exceed max size
    if (schedule_times_in_minutes_.size() > schedule_max_size_) {
        schedule_times_in_minutes_.resize(schedule_max_size_);
        ESP_LOGW(TAG, "Input schedule size exceeds max size. Truncating to max size of %d entries.", schedule_max_size_);
    }
    PrefBlob blob{};
    const size_t n = std::min(this->schedule_times_in_minutes_.size(), this->schedule_max_size_);
    blob.count = static_cast<uint16_t>(n);
    for (size_t i = 0; i < n; ++i)
        blob.values[i] = this->schedule_times_in_minutes_[i];
    // zero remainder (not strictly required, but keeps data deterministic)
    for (size_t i = n; i < this->schedule_max_size_; ++i)
        blob.values[i] = 0;

    const bool ok = this->schedule_pref_.save(&blob);
    if (ok) {
        ESP_LOGI(TAG, "Saved %u uint16_t values to preferences", static_cast<unsigned>(n));
    } else {
        ESP_LOGW(TAG, "Failed to save values to preferences");
    }
}



// Method to setup the automation and action to retrieve the schedule from Home Assistant
void Schedule::setup_schedule_retrieval_service_() {
     if (this->ha_schedule_entity_id_.empty()) {
        ESP_LOGE(TAG, "Cannot trigger retrieval: schedule_entity_id is empty.");
        return;
    }
    #ifdef USE_API
        if (esphome::api::global_api_server == nullptr) {
            ESP_LOGW(TAG, "APIServer not available");
        return;
        }
        ESP_LOGI(TAG, "C++ component triggering schedule.get_schedule for %s...", ha_schedule_entity_id_.c_str());
        // Get the global API server instance (required for communication)
        api::APIServer *api_server = api::global_api_server;

        this->ha_get_schedule_action_ = new api::HomeAssistantServiceCallAction<>(api_server, false);
        
        this->ha_get_schedule_action_->set_service("schedule.get_schedule");
        this->ha_get_schedule_action_->init_data(1);
        this->ha_get_schedule_action_->add_data("entity_id", this->ha_schedule_entity_id_);
        this->ha_get_schedule_action_->init_data_template(0);
        this->ha_get_schedule_action_->init_variables(0);
        this->ha_get_schedule_action_->set_wants_status();
        this->ha_get_schedule_action_->set_wants_response();


    #ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
// Ensure these are visible here: JsonObjectConst and std::string are used below.
// Small Action that forwards JsonObjectConst payloads to your listener object.
    class JsonListenerAction : public esphome::Action<JsonObjectConst> {
    public:
        explicit JsonListenerAction(MySuccessWithJsonTrigger *listener) : listener_(listener) {}
    protected:
        void play(const JsonObjectConst &response) override {
            if (this->listener_)
                this->listener_->trigger(response);
            }
    private:
        MySuccessWithJsonTrigger *listener_;
    };

// Small Action that forwards std::string payloads to your error listener object.
    class StringListenerAction : public esphome::Action<std::string> {
    public:
        explicit StringListenerAction(MyErrorTrigger *listener) : listener_(listener) {}
    protected:
        void play(const std::string &s) override {
            if (this->listener_)
                this->listener_->trigger(s);
            }
    private:
        MyErrorTrigger *listener_;
    };

// File-scope containers to own the automations/actions so they remain alive.
// (Prefer storing these as members on your schedule object instead if possible.)
    static std::vector<std::unique_ptr<esphome::Automation<JsonObjectConst>>> ha_json_automations;
    static std::vector<std::unique_ptr<esphome::Action<JsonObjectConst>>> ha_json_actions;
    static std::vector<std::unique_ptr<esphome::Automation<std::string>>> ha_str_automations;
    static std::vector<std::unique_ptr<esphome::Action<std::string>>> ha_str_actions;

    // Create listener objects
    auto *success_listener = new MySuccessWithJsonTrigger(this);
    auto *error_listener = new MyErrorTrigger();

    // Wire success JSON trigger -> Automation -> Action -> MySuccessWithJsonTrigger::trigger(...)
    auto *json_trigger = this->ha_get_schedule_action_->get_success_trigger_with_response();
    if (json_trigger != nullptr) {
        auto json_automation = std::make_unique<esphome::Automation<JsonObjectConst>>(json_trigger);
        auto json_action = std::make_unique<JsonListenerAction>(success_listener);
        json_automation->add_action(json_action.get());
        ha_json_automations.emplace_back(std::move(json_automation));
        ha_json_actions.emplace_back(std::move(json_action));
    }

    // Wire error trigger (string) -> Automation -> Action -> MyErrorTrigger::trigger(...)
    auto *err_trigger = this->ha_get_schedule_action_->get_error_trigger();
    if (err_trigger != nullptr) {
        auto str_automation = std::make_unique<esphome::Automation<std::string>>(err_trigger);
        auto str_action = std::make_unique<StringListenerAction>(error_listener);
        str_automation->add_action(str_action.get());
        ha_str_automations.emplace_back(std::move(str_automation));
        ha_str_actions.emplace_back(std::move(str_action));
    }
    #endif  // USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON

   #else
        ESP_LOGW(TAG, "API not enabled in build");
    #endif 
}

void Schedule::request_schedule() {
    #ifdef USE_API
        if (this->ha_get_schedule_action_ == nullptr) {
        ESP_LOGW(TAG, "Schedule action not ready");
        return;
        }
        // Play action with no template args
        this->ha_get_schedule_action_->play();
    #else
        ESP_LOGW(TAG, " API not enabled");
    #endif
}



void Schedule::process_schedule_(const ArduinoJson::JsonObjectConst &response) {
    std::vector<uint16_t> work_buffer_;
    ESP_LOGI(TAG, "Processing received schedule data into integer array for %s...", this->ha_schedule_entity_id_.c_str());
    
    // Safetycheck that the expected entity is present in the response
    if (!response["response"][this->ha_schedule_entity_id_.c_str()].is<JsonObjectConst>()) {
        ESP_LOGW(TAG, "Expected entity '%s' not found in response", this->ha_schedule_entity_id_.c_str());
        return;
    }
    JsonObjectConst schedule = response["response"][this->ha_schedule_entity_id_.c_str()];
    work_buffer_.clear();
    
    // Create work buffers for each data sensor
    std::vector<std::vector<std::string>> data_work_buffers;
    for (size_t i = 0; i < this->data_sensors_.size(); ++i) {
        data_work_buffers.emplace_back(std::vector<std::string>());
    }
    
    // Iterate over each day of the week
    const char* days[] = {"monday", "tuesday", "wednesday", "thursday", "friday", "saturday", "sunday"};
    uint16_t day_offset_minutes = 0;
    
    for (int i = 0; i < 7; ++i) {
        if (!schedule[days[i]].is<JsonArrayConst>()) {
            ESP_LOGE(TAG, "Day '%s' not found in schedule; aborting schedule processing.", days[i]);
            ESP_LOGE(TAG, "Schedule data is corrupted or incomplete. Please verify the schedule configuration.");
            return;
        }
        JsonArrayConst day_array = schedule[days[i]].as<JsonArrayConst>();
        
        for (JsonObjectConst entry : day_array) {
            // Validate entry has "from" and "to" fields
            if (!entry["from"].is<const char*>() || !entry["to"].is<const char*>()) {
                ESP_LOGE(TAG, "Invalid or missing 'from'/'to' fields in %s; aborting schedule processing.", days[i]);
                ESP_LOGE(TAG, "Schedule data is corrupted or incomplete. Please verify the schedule configuration.");
                return;
            }
            
            if (!(this->isValidTime_(entry["from"]) && this->isValidTime_(entry["to"]))) {
                ESP_LOGE(TAG, "Invalid time range in %s: from='%s', to='%s'; aborting schedule processing.",
                        days[i],
                        entry["from"].as<const char*>(),
                        entry["to"].as<const char*>());

                ESP_LOGE(TAG, "Schedule data is corrupted or incomplete. Please verify the schedule configuration.");
                return;
            }
            
            uint16_t from = this->timeToMinutes_(entry["from"]);
            uint16_t to = this->timeToMinutes_(entry["to"]);
            ESP_LOGV(TAG, "%s: from='%s' (%u min), to='%s' (%u min)", 
                     days[i], 
                     entry["from"].as<const char*>(), from,
                     entry["to"].as<const char*>(), to);
            work_buffer_.push_back(from + day_offset_minutes);
            work_buffer_.push_back(to + day_offset_minutes);
            
            // Check if entry has "data" field
            if (!entry["data"].is<JsonObjectConst>()) {
                ESP_LOGE(TAG, "Missing 'data' field in %s entry; aborting schedule processing.", days[i]);
                ESP_LOGE(TAG, "Schedule data is corrupted or incomplete. Please verify the schedule configuration.");
                return;
            }
            
            JsonObjectConst data = entry["data"].as<JsonObjectConst>();
            
            // Process each data item for this entry
            for (size_t sensor_idx = 0; sensor_idx < this->data_sensors_.size(); ++sensor_idx) {
                DataSensor *sensor = this->data_sensors_[sensor_idx];
                const std::string &label = sensor->get_label();
                
                // Check if the data field exists
                if (!data[label.c_str()].is<JsonVariantConst>()) {
                    ESP_LOGE(TAG, "Missing data field '%s' in %s entry; aborting schedule processing.", 
                             label.c_str(), days[i]);
                    ESP_LOGE(TAG, "Schedule data is corrupted or incomplete. Please verify the schedule configuration.");
                    return;
                }
                
                JsonVariantConst data_value = data[label.c_str()];
                
                // Validate data type matches sensor item_type
                uint16_t item_type = sensor->get_item_type();
                std::string value_str;
                
                switch (item_type) {
                    case 0:  // uint8_t
                    case 1:  // uint16_t
                    case 2: {  // int32_t
                        if (!data_value.is<int>() && !data_value.is<long>()) {
                            ESP_LOGE(TAG, "Data field '%s' in %s is not an integer type; aborting schedule processing.", 
                                     label.c_str(), days[i]);
                            ESP_LOGE(TAG, "Expected integer for sensor '%s' with item_type %u", label.c_str(), item_type);
                            return;
                        }
                        value_str = std::to_string(data_value.as<long>());
                        break;
                    }
                    case 3: {  // float
                        if (!data_value.is<float>() && !data_value.is<double>() && !data_value.is<int>()) {
                            ESP_LOGE(TAG, "Data field '%s' in %s is not a numeric type; aborting schedule processing.", 
                                     label.c_str(), days[i]);
                            ESP_LOGE(TAG, "Expected numeric for sensor '%s' with item_type %u", label.c_str(), item_type);
                            return;
                        }
                        value_str = std::to_string(data_value.as<float>());
                        break;
                    }
                    default:
                        ESP_LOGE(TAG, "Unknown item_type %u for sensor '%s'; aborting schedule processing.", 
                                 item_type, label.c_str());
                        return;
                }
                
                // Add to work buffer for this sensor
                data_work_buffers[sensor_idx].push_back(value_str);
            }
        }
        
        // Offset for the next day: each day is 1440 minutes (24 hours)
        day_offset_minutes += 1440;
    }
    
    // Append terminating values with MSB set
    work_buffer_.push_back(0xFFFF);
    work_buffer_.push_back(0xFFFF);
    
    // Check size against max size
    if (work_buffer_.size() > this->schedule_max_size_) {
        ESP_LOGW(TAG, "Received schedule (%u entries) exceeds max size (%u); truncating.", 
                 static_cast<unsigned>(work_buffer_.size()), static_cast<unsigned>(this->schedule_max_size_));
        work_buffer_.resize(this->schedule_max_size_);
        
        // Truncate data work buffers to match
        size_t max_entries = this->schedule_max_size_ / 2;
        for (auto &buffer : data_work_buffers) {
            if (buffer.size() > max_entries) {
                buffer.resize(max_entries);
            }
        }
    }
    
    // Store the processed schedule times
    this->schedule_times_in_minutes_ = work_buffer_;
    ESP_LOGI(TAG, "Processed %u schedule entries.", static_cast<unsigned>(this->schedule_times_in_minutes_.size()/2));
    
    // Populate each data sensor with its work buffer
    for (size_t sensor_idx = 0; sensor_idx < this->data_sensors_.size(); ++sensor_idx) {
        DataSensor *sensor = this->data_sensors_[sensor_idx];
        sensor->clear_data_vector();
        
        for (size_t entry_idx = 0; entry_idx < data_work_buffers[sensor_idx].size(); ++entry_idx) {
            sensor->add_schedule_data_to_sensor(data_work_buffers[sensor_idx][entry_idx], entry_idx);
        }
        
        ESP_LOGI(TAG, "Populated sensor '%s' with %u entries", 
                 sensor->get_label().c_str(), static_cast<unsigned>(data_work_buffers[sensor_idx].size()));
    }
    
    // Persist the new schedule to flash    
    save_schedule_to_pref_();
}
void Schedule::add_data_item(const std::string &label, uint16_t value) {
    uint16_t size;
    switch (value) {
        case 0:  // uint8_t
            size = 1*SCHEDULE_MAX_SIZE;
            break;
        case 1:  // uint16_t
            size = 2*SCHEDULE_MAX_SIZE;
            break;

        case 2:  // int32_t
            size = 4*SCHEDULE_MAX_SIZE;
            break;
        case 3:  // float
            size = 4*SCHEDULE_MAX_SIZE;
            break;
        default:
            size = 0;
            break;
    }
    data_items_.emplace_back(DataItem{label, value, size});
    }
void Schedule::print_data_items() {
        for (const auto& item : data_items_) {
            ESP_LOGI(TAG, "Data Item - Label: %s, Value: %u, Size: %u", item.label.c_str(), item.value, item.size);
        }
    }
void Schedule::log_schedule_data() {
    ESP_LOGV(TAG, "=== Schedule Data Dump ===");
    ESP_LOGV(TAG, "Schedule times count: %u", static_cast<unsigned>(this->schedule_times_in_minutes_.size()));
    
    // Log schedule times (pairs of from/to)
    for (size_t i = 0; i < this->schedule_times_in_minutes_.size(); i += 2) {
        if (i + 1 < this->schedule_times_in_minutes_.size()) {
            uint16_t from = this->schedule_times_in_minutes_[i];
            uint16_t to = this->schedule_times_in_minutes_[i + 1];
            
            // Check for terminator
            if (from == 0xFFFF && to == 0xFFFF) {
                ESP_LOGV(TAG, "Entry %u: TERMINATOR (0xFFFF, 0xFFFF)", static_cast<unsigned>(i / 2));
                break;
            }
            
            // Convert minutes to day and time
            uint16_t from_day = from / 1440;
            uint16_t from_minutes = from % 1440;
            uint16_t from_hours = from_minutes / 60;
            uint16_t from_mins = from_minutes % 60;
            
            uint16_t to_day = to / 1440;
            uint16_t to_minutes = to % 1440;
            uint16_t to_hours = to_minutes / 60;
            uint16_t to_mins = to_minutes % 60;
            
            const char* day_names[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
            ESP_LOGV(TAG, "Entry %u: From=%s %02u:%02u (%u) To=%s %02u:%02u (%u)", 
                     static_cast<unsigned>(i / 2),
                     day_names[from_day], from_hours, from_mins, from,
                     day_names[to_day], to_hours, to_mins, to);
        }
    }
    
    // Log data sensor contents
    ESP_LOGV(TAG, "=== Data Sensors ===");
    ESP_LOGV(TAG, "Number of data sensors: %u", static_cast<unsigned>(this->data_sensors_.size()));
    
    for (size_t sensor_idx = 0; sensor_idx < this->data_sensors_.size(); ++sensor_idx) {
        DataSensor *sensor = this->data_sensors_[sensor_idx];
        ESP_LOGV(TAG, "Sensor %u: Label='%s', Type=%u, Vector Size=%u bytes", 
                 static_cast<unsigned>(sensor_idx),
                 sensor->get_label().c_str(),
                 sensor->get_item_type(),
                 static_cast<unsigned>(sensor->get_data_vector().size()));
        
        uint16_t bytes_per_item = sensor->get_bytes_for_type(sensor->get_item_type());
        size_t num_entries = sensor->get_data_vector().size() / bytes_per_item;
        
        // Log each value in the sensor's data vector
        for (size_t entry_idx = 0; entry_idx < num_entries; ++entry_idx) {
            size_t actual_index = entry_idx * bytes_per_item;
            
            switch (sensor->get_item_type()) {
                case 0: {  // uint8_t
                    uint8_t value = sensor->get_data_vector()[actual_index];
                    ESP_LOGV(TAG, "  Entry %u: %u (uint8_t)", static_cast<unsigned>(entry_idx), value);
                    break;
                }
                case 1: {  // uint16_t
                    uint16_t value;
                    std::memcpy(&value, &sensor->get_data_vector()[actual_index], sizeof(value));
                    ESP_LOGV(TAG, "  Entry %u: %u (uint16_t)", static_cast<unsigned>(entry_idx), value);
                    break;
                }
                case 2: {  // int32_t
                    int32_t value;
                    std::memcpy(&value, &sensor->get_data_vector()[actual_index], sizeof(value));
                    ESP_LOGV(TAG, "  Entry %u: %d (int32_t)", static_cast<unsigned>(entry_idx), value);
                    break;
                }
                case 3: {  // float
                    float value;
                    std::memcpy(&value, &sensor->get_data_vector()[actual_index], sizeof(value));
                    ESP_LOGV(TAG, "  Entry %u: %.2f (float)", static_cast<unsigned>(entry_idx), value);
                    break;
                }
            }
            
            // Stop if we hit all zeros (uninitialized data)
            bool all_zeros = true;
            for (size_t b = 0; b < bytes_per_item; ++b) {
                if (sensor->get_data_vector()[actual_index + b] != 0) {
                    all_zeros = false;
                    break;
                }
            }
            if (all_zeros && entry_idx > 0) {
                ESP_LOGV(TAG, "  (remaining entries are zeros)");
                break;
            }
        }
    }
    
    ESP_LOGV(TAG, "=== End Schedule Data Dump ===");
}
} // namespace schedule
} // namespace esphome
