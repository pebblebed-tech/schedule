#include "schedule.h"
#include "data_sensor.h"
#ifdef USE_SELECT
#include "select/mode_select.h"
#endif
#include "esphome/components/api/api_server.h"
#include "esphome/components/api/homeassistant_service.h"
#include "esphome/core/application.h"
#include "esphome/core/automation.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include "esphome/components/json/json_util.h"
#include "esphome/core/helpers.h"

#include <functional>



// TODO: Add error handling for service call failures
// TODO: Add HomeAssitant notify service call on schedule retrieval failure, incorrect values in schedule and oversize schedule
// TODO: Add a timer to check to and from times and set a switch and the data_sensors if time is within a scheduled period
// TODO: In the main loop check for WIFI / API connection status and adjust state machine to ensure when connetion is restored schedule is re-requested
// TODO: Add state machine to control device behaviour in device loop modes (setup, time_not_valid, no_schedule_stored, normal_connected_on,normal_connected_off, normal_disconnected_on,normal_disconnected_off, Error etc)
// TODO: Add switch to that is controlled by schedule to & from scheduled times
// TODO: Add a timer that runs every second to check if we are connected to home assistant
// TODO: Check that select defaults to manual off on first run and saves to preferences
// TODO: Ensure schedule_retrieval_service_ is only setup once
// TODO: Run clang-format on these files
// TODO: Add Doxygen comments to all methods and classes
// TODO: Add comments to python so the user knows what each config option does
// TODO: Check that if JSON has mising days if no events are scheduled for that day it is handled gracefully
// TODO: check that sensor options like unit_of_measurement, device_class, state_class etc can be set from yaml

// This is needed due to a bug in the logic in HomeAsitant Service Call Action with JSON responses
// Define this to enable JSON response handling for HomeAssistant actions
#ifndef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
#define USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
#endif

namespace esphome {
namespace schedule {
// forward declaration of Schedule

static const char *const TAG = "schedule";

class MySuccessWithJsonTrigger : public Trigger<JsonObjectConst> {
 public:
  explicit MySuccessWithJsonTrigger(Schedule *parent) : parent_(parent) {}
  
  void trigger(const JsonObjectConst &response)  {
    // Inspect the JSON response
    ESP_LOGI(TAG, "Received JSON response from Home Assistant action");
    
    // Process the schedule - the process_schedule_ method will extract the correct entity
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

// UpdateScheduleButton implementation
void UpdateScheduleButton::press_action() {
  if (this->schedule_ != nullptr) {
    ESP_LOGI(TAG, "Update button pressed, requesting schedule update...");
    this->schedule_->request_schedule();
  } else {
    ESP_LOGW(TAG, "Update button pressed but schedule is not set");
  }
}

// ScheduleSwitch implementation
void ScheduleSwitch::write_state(bool state) {
  // Publish the new state
  this->publish_state(state);
  
  // Log the state change
  ESP_LOGI(TAG, "Schedule switch state changed to: %s", state ? "ON" : "OFF");
  
  // Update the indicator to follow the switch state
  if (this->schedule_ != nullptr) {
    this->schedule_->update_switch_indicator(state);
  }
}

// Schedule implementations

//class Schedule;
// Setter for max schedule entries EG number of schedule time pairs
void Schedule::set_max_schedule_entries(size_t entries) {
    this->schedule_max_entries_ = entries; 
    this->set_max_schedule_size(entries);  //This will set the size of schedule_times_in_minutes_ vector in bytes
}
// Setter for max schedule size in bytes adjusted for time pairs and 16bit values EG size in bytes = (entries * 4) + 4
void Schedule::set_max_schedule_size(size_t size) {
    this->schedule_max_size_ = (size * 4)+4;  // Each entry has a start and end time so actual size is double, plus 2 for terminator all in 16bit so *2
    this->schedule_times_in_minutes_.resize(this->schedule_max_size_); 
}

void Schedule::setup() {
    ESP_LOGI(TAG, "Setting up Schedule component...");
    
    // Set parent reference and call setup on each data sensor
    // The data sensors hold the attribute data that are supplied by the service call
    for (auto *sensor : this->data_sensors_) {
        sensor->set_parent_schedule(this);
        sensor->setup();
    }
    // Create schedule preference object
    this->create_schedule_preference();
    // Now load from preference;
    this->load_schedule_from_pref_();

    this->setup_schedule_retrieval_service_();
    this->setup_notification_service_();
}

void Schedule::loop() {

}

void Schedule::dump_config() {
    
    ESP_LOGCONFIG(TAG, "Schedule Entity ID: %s", ha_schedule_entity_id_.c_str());
    ESP_LOGCONFIG(TAG, "Max number of entries the schedule can hold: %d", schedule_max_entries_);
    ESP_LOGCONFIG(TAG, "Schedule Max size in bytes: %d", schedule_max_size_);
    ESP_LOGCONFIG(TAG, "Object ID: %s", this->get_object_id().c_str());
    ESP_LOGCONFIG(TAG, "Preference Hash: %u", this->get_preference_hash());
    ESP_LOGCONFIG(TAG, "Object Hash ID: %u", this->get_object_id_hash());
    ESP_LOGCONFIG(TAG, "name: %s", this->get_name());
    ESP_LOGCONFIG(TAG, "Registered Data Sensors:");
    for (auto *sensor : this->data_sensors_) {
        sensor->dump_config();
    }   

}

void Schedule::set_schedule_entity_id(const std::string &ha_schedule_entity_id){
    this->ha_schedule_entity_id_ = ha_schedule_entity_id;
}

// Helper function to convert "HH:MM:SS" or "HH:MM" to minutes from start of day
uint16_t Schedule::timeToMinutes_(const char* time_str) {
    int h = 0, m = 0, s = 0;
    if (sscanf(time_str, "%d:%d:%d", &h, &m, &s) == 3) {
        return (h * 60) + m;
    }
    if (sscanf(time_str, "%d:%d", &h, &m) == 2) {
        return (h * 60) + m;
    }
    ESP_LOGE(TAG, "Failed to parse time string '%s'", time_str);
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
// Create the schedule preference object
void Schedule::create_schedule_preference() {
    if (!sched_array_pref_) return;
        sched_array_pref_->create_preference(this->get_object_id_hash());
}

// Function to load the array from NVS memory
void Schedule::load_schedule_from_pref_() {
    ESP_LOGV(TAG, "Loading schedule from preferences");
    if (!sched_array_pref_) {
        ESP_LOGW(TAG, "No schedule preference object available to load from");
        return;
    } 
    // Create temporary buffer to load data
    std::vector<uint16_t> temp_buffer(this->schedule_max_size_);
    // Load data into the array preference
    this->sched_array_pref_->load();
    //
    bool ok = this->sched_array_pref_->is_valid();
    //bool ok = false;
 
   if (!ok) {
       ESP_LOGW(TAG, "Schedule preference data is not valid");
    }
    else {
        uint8_t *buf = this->sched_array_pref_->data();
        std::memcpy(temp_buffer.data(), buf, this->sched_array_pref_->size());
        // Check for terminator (0xFFFF, 0xFFFF) to determine actual number of entries
        for (size_t i = 0; i < this->schedule_max_size_; i += 2) {
            if (temp_buffer[i] == 0xFFFF && temp_buffer[i + 1] == 0xFFFF) {
                ESP_LOGI(TAG, "Found terminator at index %u; actual schedule size is %u entries", 
                         static_cast<unsigned>(i / 2), static_cast<unsigned>(i));
                ok=true;
                break;
            }
            if (i == this->schedule_max_size_ - 2) {
                ESP_LOGW(TAG, "No terminator found");
                ok=false;
            }
        }
    }
        
    if (ok) {
        // Copy all data from temp_buffer into schedule_times_in_minutes_
        this->schedule_times_in_minutes_.clear();
        this->schedule_times_in_minutes_.reserve(this->schedule_max_size_);
        
        // Copy ALL values - don't stop at terminator during load
        for (size_t i = 0; i < this->schedule_max_size_; ++i) {
            this->schedule_times_in_minutes_.push_back(temp_buffer[i]);
        }
        
        ESP_LOGI(TAG, "Loaded %u uint16_t values from preferences", 
                 static_cast<unsigned>(this->schedule_times_in_minutes_.size()));
    } else {
        // No stored data: use YAML initial values and persist them
        this->schedule_times_in_minutes_ = this->factory_reset_values_;
        if (this->schedule_times_in_minutes_.size() > this->schedule_max_size_) {
            this->schedule_times_in_minutes_.resize(this->schedule_max_size_);
        }
       // Fill remaining entries with zeros if needed
       while (this->schedule_times_in_minutes_.size() < this->schedule_max_size_) {
           this->schedule_times_in_minutes_.push_back(0);
        }
        // save defaults to preferences
        uint8_t *buf = sched_array_pref_->data();
        std::memcpy(buf, this->schedule_times_in_minutes_.data(), sched_array_pref_->size());
        sched_array_pref_->save();
        ESP_LOGI(TAG, "No stored values; using factory defaults and saving them");
    }
    // Debug log values
    for (size_t i = 0; i < this->schedule_times_in_minutes_.size(); ++i) {
        ESP_LOGD(TAG, "schedule_times_in_minutes_[%u] = %u", static_cast<unsigned>(i), this->schedule_times_in_minutes_[i]);
    } 
}

// Function to save the current schedule to NVS memory
void Schedule::save_schedule_to_pref_() {
    ESP_LOGV(TAG, "Saving schedule");
    
    // Safety check to ensure we do not exceed max size
    if (schedule_times_in_minutes_.size() > schedule_max_size_) {
        schedule_times_in_minutes_.resize(schedule_max_size_);
        ESP_LOGW(TAG, "Input schedule size exceeds max size. Truncating to max size of %zu entries.", schedule_max_size_);
    }
    uint8_t *buf = sched_array_pref_->data();
    std::memcpy(buf, this->schedule_times_in_minutes_.data(), this->sched_array_pref_->size());
    this->sched_array_pref_->save();
    ESP_LOGV(TAG, "Schedule times saved to preferences using %u bytes.", this->sched_array_pref_->size());
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

// Small Action that forwards JsonObjectConst payloads to the listener object.
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

// Small Action that forwards std::string payloads to the error listener object.
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

    // Create listener objects
    auto *success_listener = new MySuccessWithJsonTrigger(this);
    auto *error_listener = new MyErrorTrigger();

    // Wire success JSON trigger -> Automation -> Action -> MySuccessWithJsonTrigger::trigger(...)
    auto *json_trigger = this->ha_get_schedule_action_->get_success_trigger_with_response();
    if (json_trigger != nullptr) {
        auto json_automation = std::make_unique<esphome::Automation<JsonObjectConst>>(json_trigger);
        auto json_action = std::make_unique<JsonListenerAction>(success_listener);
        json_automation->add_action(json_action.get());
        this->ha_json_automations_.emplace_back(std::move(json_automation));
        this->ha_json_actions_.emplace_back(std::move(json_action));
    }

    // Wire error trigger (string) -> Automation -> Action -> MyErrorTrigger::trigger(...)
    auto *err_trigger = this->ha_get_schedule_action_->get_error_trigger();
    if (err_trigger != nullptr) {
        auto str_automation = std::make_unique<esphome::Automation<std::string>>(err_trigger);
        auto str_action = std::make_unique<StringListenerAction>(error_listener);
        str_automation->add_action(str_action.get());
        this->ha_str_automations_.emplace_back(std::move(str_automation));
        this->ha_str_actions_.emplace_back(std::move(str_action));
    }
    #endif  // USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON

   #else
        ESP_LOGW(TAG, "API not enabled in build");
    #endif 
}
// Method to setup the automation and action to send notifications to Home Assistant
void Schedule::setup_notification_service_() {
    #ifdef USE_API
        if (esphome::api::global_api_server == nullptr) {
            ESP_LOGW(TAG, "APIServer not available for notification setup");
            return;
        }
        
        ESP_LOGD(TAG, "Setting up Home Assistant notification service...");
        
        // Get the global API server instance
        api::APIServer *api_server = api::global_api_server;
        
        // Create a HomeAssistant service call action for notify.persistent_notification
        this->ha_notify_action_ = new api::HomeAssistantServiceCallAction<>(api_server, false);
        
        this->ha_notify_action_->set_service("notify.persistent_notification");
        this->ha_notify_action_->init_data(2);
        this->ha_notify_action_->init_data_template(0);
        this->ha_notify_action_->init_variables(0);
        
        ESP_LOGD(TAG, "Notification service setup complete");
        
    #else
        ESP_LOGW(TAG, "API not enabled - cannot setup notification service");
    #endif
}
// Method that sends notifications to Home Assistant
void Schedule::send_ha_notification_(const std::string &message, const std::string &title) {
    #ifdef USE_API
        if (this->ha_notify_action_ == nullptr) {
            ESP_LOGW(TAG, "Notification action not ready");
            return;
        }
        
        ESP_LOGI(TAG, "Sending notification to Home Assistant: %s", message.c_str());
        
        // Re-initialize data for this call
        this->ha_notify_action_->init_data(2);
        this->ha_notify_action_->add_data("message", message);
        this->ha_notify_action_->add_data("title", title);
        
        // Execute the notification
        this->ha_notify_action_->play();
        
    #else
        ESP_LOGW(TAG, "API not enabled - cannot send notification");
    #endif
}
// Method to request the schedule from Home Assistant
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

// Method to process the schedule data received from Home Assistant
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
    // fill the remainder with zeros to maintain constant size
    while (work_buffer_.size() < this->schedule_max_size_) {
        work_buffer_.push_back(0);
    }   
    // All data validated and processed successfully
    ESP_LOGD(TAG, "Processed schedule with %u entries successfully.", static_cast<unsigned>(work_buffer_.size()) / 2);
    // Store the processed schedule times in schedule runtime buffer
    this->schedule_times_in_minutes_ = work_buffer_;
    // Populate each data sensor with its runtime buffer
    for (size_t sensor_idx = 0; sensor_idx < this->data_sensors_.size(); ++sensor_idx) {
        DataSensor *sensor = this->data_sensors_[sensor_idx];
        sensor->clear_data_vector();
        
        for (size_t entry_idx = 0; entry_idx < data_work_buffers[sensor_idx].size(); ++entry_idx) {
            sensor->add_schedule_data_to_sensor(data_work_buffers[sensor_idx][entry_idx], entry_idx);
        }
        
        // Save sensor data from runtime vector to preferences
        sensor->save_data_to_pref();
        
        ESP_LOGI(TAG, "Populated sensor '%s' with %u entries", 
                 sensor->get_label().c_str(), static_cast<unsigned>(data_work_buffers[sensor_idx].size()));
    }

    
    ESP_LOGI(TAG, "Schedule processing complete.");
    // Persist the new schedule to flash    
    save_schedule_to_pref_();
}
// Method to add a data item to the schedule's data items list
void Schedule::add_data_item(const std::string &label, uint16_t value) {
    // Let calculate size in bytes based on type
    uint16_t size;
    switch (value) {
        case 0:  // uint8_t
            size = 1*this->schedule_max_entries_;
            break;
        case 1:  // uint16_t
            size = 2*this->schedule_max_entries_;
            break;
        case 2:  // int32_t
            size = 4*this->schedule_max_entries_;
            break;
        case 3:  // float
            size = 4*this->schedule_max_entries_;
            break;
        default:
            size = 0;
            break;
    }
    // Add to schedule data items list
    data_items_.emplace_back(DataItem{label, value, size});
}
// Log all data items for debugging
void Schedule::print_data_items() {
        for (const auto& item : data_items_) {
            ESP_LOGD(TAG, "Data Item - Label: %s, Value: %u, Size: %u", item.label.c_str(), item.value, item.size);
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
                 static_cast<unsigned>(sensor->get_data_vector_size()));
        
        uint16_t bytes_per_item = sensor->get_bytes_for_type(sensor->get_item_type());
        size_t num_entries = sensor->get_data_vector_size() / bytes_per_item;
        
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
    
 /*    ESP_LOGV(TAG, "=== End Schedule Data Dump ===");
    this->send_ha_notification_(
        "Test notification from Schedule component after logging schedule data.",
        "Schedule Warning"
    ); */
}

// Method to set the schedule's array preference object
void Schedule::sched_add_pref(ArrayPreferenceBase *array_pref) {
  sched_array_pref_ = array_pref;

}

// Callback when mode select changes from Home Assistant
void Schedule::on_mode_changed(const std::string &mode) {
  ESP_LOGI(TAG, "Schedule mode changed by Home Assistant to: %s", mode.c_str());
  
  // Add your custom logic here to respond to mode changes
  // For example, update schedule behavior based on the selected mode
  if (mode == "Manual Off") {
    // Handle manual off mode
  } else if (mode == "Early Off") {
    // Handle early off mode
  } else if (mode == "Auto") {
    // Handle auto mode
  } else if (mode == "Manual On") {
    // Handle manual on mode
  } else if (mode == "Boost On") {
    // Handle boost mode
  }
}

// Set the mode select option programmatically
void Schedule::set_mode_option(const std::string &option) {
  if (this->mode_select_ != nullptr) {
    this->mode_select_->publish_state(option);
  }
}


// --------------------------------------------------
// TEST 1: Create preference
// --------------------------------------------------
void Schedule::test_create_preference() {
  if (!sched_array_pref_) return;


  sched_array_pref_->create_preference(this->get_object_id_hash());

  ESP_LOGI(TAG, "test_create_preference: key=0x%08X", this->get_object_id_hash());
}

// --------------------------------------------------
// TEST 2: Save preference (write bytes 0..99)
// --------------------------------------------------
void Schedule::test_save_preference() {
  if (!sched_array_pref_) return;

  uint8_t *buf = sched_array_pref_->data();
  size_t limit = schedule_max_size_ < 100 ? schedule_max_size_ : 100;

  for (size_t i = 0; i < limit; i++) {
    buf[i] = static_cast<uint8_t>(i);
  }

  sched_array_pref_->save();
  ESP_LOGI(TAG, "test_save_preference: wrote %u bytes", (unsigned)limit);
}

// --------------------------------------------------
// TEST 3: Load preference and log bytes
// --------------------------------------------------
void Schedule::test_load_preference() {
  if (!sched_array_pref_) return;

  sched_array_pref_->load();
  uint8_t *buf = sched_array_pref_->data();

  ESP_LOGI(TAG, "test_load_preference: bytes 0..9:");
  for (size_t i = 0; i < 10 && i < this->schedule_max_size_; i++) {
    ESP_LOGI(TAG, "  [%u] = %u", (unsigned)i, buf[i]);
  }

  ESP_LOGI(TAG, "test_load_preference: bytes 90..99:");
  for (size_t i = 90; i < 100 && i < this->schedule_max_size_; i++) {
    ESP_LOGI(TAG, "  [%u] = %u", (unsigned)i, buf[i]);
  }
}



} // namespace schedule
} // namespace esphome