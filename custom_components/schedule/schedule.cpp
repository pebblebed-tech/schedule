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


namespace esphome {
namespace schedule {
// forward declaration of Schedule

static const char *const TAG = "schedule";
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
                ESP_LOGW(TAG, "Stored count %u exceeds MAX_VALUES %u; ignoring stored blob", static_cast<unsigned>(blob.count), this->schedule_max_size_);
                // fallback to initial values and persist them
                this->schedule_times_in_minutes_ = this->factory_reset_values_;
                if (this->schedule_times_in_minutes_.size() > this->schedule_max_size_) this->schedule_times_in_minutes_.resize(this->schedule_max_size_);
                    this->save_schedule_to_pref_();
             }
        } else {
            // No stored data: use YAML initial values and persist them
            this->schedule_times_in_minutes_ = this->factory_reset_values_;
            if (this->schedule_times_in_minutes_.size() > this->schedule_max_size_) this->schedule_times_in_minutes_.resize(this->schedule_max_size_);
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
        ESP_LOGE(TAG, "Target schedule entity %s not found in the response.", this->ha_schedule_entity_id_.c_str());
        return;
    }
    JsonObjectConst schedule = response["response"][this->ha_schedule_entity_id_.c_str()];
    work_buffer_.clear();
    // Iterate over each day of the week
    const char* days[] = {"monday", "tuesday", "wednesday", "thursday", "friday", "saturday", "sunday"};
    uint16_t  day_offset_minutes = 0;
    for (int i = 0; i < 7; ++i) {
        const char* current_day = days[i];
        if (schedule[current_day].is<JsonArrayConst>()) {
            JsonArrayConst day_ranges = schedule[current_day].as<JsonArrayConst>();
            for (JsonObjectConst range : day_ranges) {
                //check that to and from exists 
                
                // ESP_LOGD(TAG, "Processing range in %s: from='%s', to='%s'", current_day, range["from"].as<const char*>(), range["to"].as<const char*>());
                if (!(range["from"] && range["to"])) {
                    ESP_LOGW(TAG, "Skipping invalid range in %s: missing 'from' or 'to'", current_day);
                    continue;
                }   
                if (!isValidTime_(range["from"]) || !isValidTime_(range["to"])) {
                    ESP_LOGW(TAG, "Skipping invalid time format in %s: from='%s', to='%s'", current_day, range["from"].as<const char*>(), range["to"].as<const char*>());
                    continue;
                }
                const char* start_time_str = range["from"];
                uint16_t  start_time_mins = timeToMinutes_(start_time_str) + day_offset_minutes;
                // Set bit 14 to indicate a "On" time
                start_time_mins |= 0x4000;
                work_buffer_.push_back(start_time_mins);
                const char* end_time_str = range["to"];
                uint16_t  end_time_mins = timeToMinutes_(end_time_str) + day_offset_minutes;
                // Clear bit 14 to indicate an "Off" time just to be sure
                end_time_mins &= ~0x4000;
                work_buffer_.push_back(end_time_mins);
            }
        }
        day_offset_minutes += 1440; 
    } 
    // Append terminating values with MSB set
    work_buffer_.push_back(0xFFFF);
    work_buffer_.push_back(0xFFFF);
    // check size against max size
    if (work_buffer_.size() > this->schedule_max_size_) {
        ESP_LOGW(TAG, "Received schedule size %u exceeds max size %u; truncating", static_cast<unsigned>(work_buffer_.size()/2), static_cast<unsigned>(this->schedule_max_size_/2));
        work_buffer_.resize(this->schedule_max_size_);
        // send notifiication to home assistant

    }
    this->schedule_times_in_minutes_ = work_buffer_;
    ESP_LOGI(TAG, "Processed %u schedule entries.", static_cast<unsigned>(this->schedule_times_in_minutes_.size()/2));
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

} // namespace schedule
} // namespace esphome
