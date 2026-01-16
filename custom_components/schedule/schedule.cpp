#include "esphome.h"
#include "schedule.h"
#include "data_sensor.h"

#include <functional>

namespace esphome {
namespace schedule {

// Logging tag
static const char *TAG = "schedule";



// TODO: Add handling for empty schedule on update from HA in respect to a valid schedule empty, Mode select and switch state
// TODO: auto generate indicator, mode select, current & next event and update button if not provided in yaml
// TODO: Ensure schedule_retrieval_service_ is only setup once
// Test cases
// TODO: Check multiple data sensors with different item types

// TODO: Check that select defaults to manual off on first run and saves to preferences
// TODO: check that sensor options like unit_of_measurement, device_class, state_class etc can be set from yaml
// TODO: Check logging is correct and useful
// Cleanup and docs
// TODO: Run clang-format on these files
// TODO: Add Doxygen comments to all methods and classes
// TODO: Add comments to python so the user knows what each config option does

// This is needed due to a bug in the logic in Home Assistant Service Call Action with JSON responses
// Define this to enable JSON response handling for HomeAssistant actions
#ifndef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
#define USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
#endif

// Constants for schedule time encoding are now defined in schedule.h

//==============================================================================
// HELPER CLASSES FOR HOME ASSISTANT INTEGRATION
//==============================================================================

class MySuccessWithJsonTrigger : public Trigger<JsonObjectConst> {
 public:
  explicit MySuccessWithJsonTrigger(Schedule *parent) : parent_(parent) {}
  
  void trigger(const JsonObjectConst &response)  {
    ESP_LOGI(TAG, "Received JSON response from Home Assistant action");
    parent_->process_schedule_(response);
  }
  
 protected:
  Schedule *parent_;
};

class MyErrorTrigger : public Trigger<std::string> {
 public:
  void trigger(const std::string &err)  {
    ESP_LOGW(TAG, "Home Assistant Get_Schedule service call failed: %s", err.c_str());
  }
};

//==============================================================================
// UI COMPONENT IMPLEMENTATIONS
//==============================================================================

void UpdateScheduleButton::press_action() {
  if (this->schedule_ != nullptr) {
    ESP_LOGI(TAG, "Update button pressed, requesting schedule update...");
    this->schedule_->request_schedule();
  } else {
    ESP_LOGW(TAG, "Update button pressed but schedule is not set");
  }
}

//==============================================================================
// COMPONENT LIFECYCLE METHODS
//==============================================================================

void Schedule::set_max_schedule_entries(size_t entries) {
    this->schedule_max_entries_ = entries; 
    this->set_max_schedule_size(entries);  //This will set the size of schedule_times_in_minutes_ vector in bytes
}

void Schedule::set_max_schedule_size(size_t size) {
    // Use virtual method to get multiplier based on storage type
    // State-based: 2 (ON + OFF per entry)
    // Event-based: 1 (EVENT only per entry)
    size_t multiplier = this->get_storage_multiplier();
    this->schedule_max_size_ = (size * multiplier) + 2;  // entries * multiplier + 2 uint16_t terminator
    this->schedule_times_in_minutes_.resize(this->schedule_max_size_); 
}

void Schedule::set_schedule_entity_id(const std::string &ha_schedule_entity_id){
    this->ha_schedule_entity_id_ = ha_schedule_entity_id;
}

void Schedule::setup() {
    ESP_LOGI(TAG, "Setting up Schedule component...");
    
    // Check if device time is valid
    this->check_rtc_time_valid_();
    
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
    
    // Load stored entity ID and check if it changed
    this->load_entity_id_from_pref_();
    uint32_t current_hash = fnv1_hash(this->ha_schedule_entity_id_);
    this->entity_id_changed_ = (this->stored_entity_id_hash_ != current_hash);
    if (this->entity_id_changed_) {
        ESP_LOGI(TAG, "Schedule entity ID changed (hash: 0x%08X -> 0x%08X)",
                 this->stored_entity_id_hash_, current_hash);
    }

    this->setup_schedule_retrieval_service_();
    this->setup_notification_service_();
        // Check initial Home Assistant API connection status
    this->ha_connected_ = api::global_api_server->is_connected();
    ESP_LOGI(TAG, "Initial Home Assistant API connection status: %s", this->ha_connected_ ? "connected" : "disconnected");
    this->last_connection_check_ = millis();
   // If get schedule on reconnect is set and we are connected request schedule or if schedule is not valid or entity ID changed
    if ((this->ha_connected_ && this->update_on_reconnect_) || 
        (this->ha_connected_ && this->schedule_valid_ == false) ||
        (this->ha_connected_ && this->entity_id_changed_)) {
        ESP_LOGD(TAG, "Requesting update schedule from Home Assistant...");
        if (this->entity_id_changed_) {
            // Invalidate old schedule since it's for a different entity
            this->schedule_valid_ = false;
            this->schedule_empty_ = true;
            this->save_entity_id_to_pref_();
            // Clear the flag after handling the change
            this->entity_id_changed_ = false;
        }
        this->request_schedule();
    } else if (this->entity_id_changed_) {
        ESP_LOGW(TAG, "Entity ID changed but Home Assistant not connected - using old schedule until connected");
    }
    
    if (this->current_event_sensor_ != nullptr) {
        this->current_event_sensor_->publish_state("Initializing...");
    }
    if (this->next_event_sensor_ != nullptr) {
        this->next_event_sensor_->publish_state("Initializing...");
    }
}

void Schedule::dump_config_base() {
    ESP_LOGCONFIG(TAG, "Schedule (Base) Configuration:");
    ESP_LOGCONFIG(TAG, "Schedule Entity ID: %s", ha_schedule_entity_id_.c_str());
    ESP_LOGCONFIG(TAG, "Max number of entries the schedule can hold: %d", schedule_max_entries_);
    ESP_LOGCONFIG(TAG, "Schedule Max size in bytes: %d", schedule_max_size_);
    ESP_LOGCONFIG(TAG, "Object ID: %s", this->get_object_id().c_str());
    ESP_LOGCONFIG(TAG, "Preference Hash: %u", this->get_preference_hash());
    ESP_LOGCONFIG(TAG, "Object Hash ID: %u", this->get_object_id_hash());
    ESP_LOGCONFIG(TAG, "name: %s", this->name_.c_str());
    ESP_LOGCONFIG(TAG, "Home Assistant connected: %s", this->ha_connected_ ? "Yes" : "No");
    ESP_LOGCONFIG(TAG, "RTC Time valid: %s", this->rtc_time_valid_ ? "Yes" : "No");
    ESP_LOGCONFIG(TAG, "Schedule valid: %s", this->schedule_valid_ ? "Yes" : "No");
    ESP_LOGCONFIG(TAG, "Schedule empty: %s", this->schedule_empty_ ? "Yes" : "No");
    ESP_LOGCONFIG(TAG, "Registered Data Sensors:");
    for (auto *sensor : this->data_sensors_) {
        sensor->dump_config();
    }   
}

//==============================================================================
// STATE MACHINE HELPER METHODS
//==============================================================================

Schedule::PrerequisiteError Schedule::check_prerequisites_() {
    // Check time validity
    if (!this->rtc_time_valid_) {
        this->check_rtc_time_valid_();
        if (!this->rtc_time_valid_) {
            return PREREQ_TIME_INVALID;
        }
    }
    
    // Check Home Assistant API connection periodically
    uint32_t now = millis();
    if (!this->ha_connected_) {
        uint32_t check_interval = this->ha_connected_once_ ? 60000 : 5000;
        if (now - this->last_connection_check_ >= check_interval) {
            this->last_connection_check_ = now;
            this->check_ha_connection_();
            
            // If we just reconnected, request schedule update if configured or needed
            if ((this->ha_connected_ && this->update_on_reconnect_) || 
                (this->ha_connected_ && this->schedule_valid_ == false) ||
                (this->ha_connected_ && this->entity_id_changed_)) {
                ESP_LOGI(TAG, "Reconnected to Home Assistant, requesting schedule update...");
                if (this->entity_id_changed_) {
                    ESP_LOGI(TAG, "Entity ID changed (hash: 0x%08X -> 0x%08X), invalidating old schedule",
                             this->stored_entity_id_hash_, fnv1_hash(this->ha_schedule_entity_id_));
                    // Invalidate old schedule since it's for a different entity
                    this->schedule_valid_ = false;
                    this->schedule_empty_ = true;
                    this->save_entity_id_to_pref_();
                    // Clear the flag after handling the change
                    this->entity_id_changed_ = false;
                }
                this->request_schedule();
            }
        }
        
        // If not connected and schedule is not valid, return error
        if (!this->schedule_valid_ && !this->ha_connected_) {
            return PREREQ_SCHEDULE_INVALID;
        }
    }
    
    // Check schedule validity
    if (!this->schedule_valid_) {
        return PREREQ_SCHEDULE_INVALID;
    }
    
    // Check if schedule is empty
    if (this->schedule_empty_) {
        return PREREQ_SCHEDULE_EMPTY;
    }
    
    return PREREQ_OK;
}

void Schedule::check_rtc_time_valid_() {
    if (this->time_ != nullptr) {
        auto now = this->time_->now();
        if (!now.is_valid()) {
            if (this->rtc_time_valid_) {
                // Time was valid but now is not (shouldn't normally happen)
                ESP_LOGW(TAG, "Device time is no longer valid!");
                this->rtc_time_valid_ = false;
            }
            // Only log on first check during setup
            else if (this->last_time_check_ == 0) {
                ESP_LOGW(TAG, "Device time is not yet synchronized. Schedule functions will not work until time is valid.");
            }
        } else {
            if (!this->rtc_time_valid_) {
                // Time just became valid
                ESP_LOGI(TAG, "Device time is now valid: %04d-%02d-%02d %02d:%02d:%02d",
                         now.year, now.month, now.day_of_month,
                         now.hour, now.minute, now.second);
                this->rtc_time_valid_ = true;
            }
            // Only log verbose on first check during setup
            else if (this->last_time_check_ == 0) {
                ESP_LOGV(TAG, "Device time is valid: %04d-%02d-%02d %02d:%02d:%02d",
                         now.year, now.month, now.day_of_month,
                         now.hour, now.minute, now.second);
            }
        }
    } else {
        // Only log warning once during setup
        if (this->last_time_check_ == 0) {
            ESP_LOGW(TAG, "No time component configured. Time-based schedule functions will not work.");
        }
    }
}

void Schedule::check_ha_connection_() {
    bool connected = api::global_api_server->is_connected();
    
    // If connection state changed
    if (this->ha_connected_ != connected) {
      if (connected) {
        // Just reconnected
        this->ha_connected_ = true;
        this->ha_connected_once_ = true;  // Mark that we've connected at least once
        ESP_LOGI(TAG, "Home Assistant API reconnected");
      } else {
        // Disconnected
        this->ha_connected_ = false;
        ESP_LOGI(TAG, "Home Assistant API disconnected");
      }
    }
}

void Schedule::log_state_flags_() {
    ESP_LOGV(TAG, "State flags: HA=%s, RTC=%s, Valid=%s, Empty=%s",
             this->ha_connected_ ? "Y" : "N",
             this->rtc_time_valid_ ? "Y" : "N",
             this->schedule_valid_ ? "Y" : "N",
             this->schedule_empty_ ? "Y" : "N");
}

//==============================================================================
// EVENT MANAGEMENT AND SCHEDULING
//==============================================================================

void Schedule::initialize_schedule_operation_() {
    ESP_LOGI(TAG, "Initializing schedule operation...");
    
    if (this->time_ == nullptr) {
        ESP_LOGW(TAG, "Cannot initialize schedule operation: no time component");
        return;
    }
    
    // Get current time
    auto now = this->time_->now();
    if (!now.is_valid()) {
        ESP_LOGW(TAG, "Cannot initialize schedule operation: invalid time");
        return;
    }
    // Calculate current time in minutes from start of week
    uint16_t current_time_minutes = this->time_to_minutes_(now);
    
    ESP_LOGD(TAG, "Current time: Day %u, %02d:%02d (week minute: %u)", 
             now.day_of_week, now.hour, now.minute, current_time_minutes);
    
    // Find current active event
    current_event_index_ = this->find_current_event_(current_time_minutes);
    // we should not get here with an invalid index as schedule is valid
    if (current_event_index_ < 0) {
        ESP_LOGW(TAG, "No current event found, schedule is empty");
        // Set flags accordingly
        this->schedule_empty_ = true;
        return;
    }

    // Now set up the current and next event details
    this->current_event_raw_ = this->schedule_times_in_minutes_[current_event_index_];
    uint16_t current_event_time = current_event_raw_ & TIME_MASK;
    
    // Check if current time is less than current event time
    // This means all events are in the future and we wrapped to the last event
    bool all_events_in_future = (current_time_minutes < current_event_time);
    
    if (all_events_in_future) {
        // We're before the first event of the week, so next event is the first event
        ESP_LOGD(TAG, "All events in future, next event is first event of new week");
        this->next_event_raw_ = this->schedule_times_in_minutes_[0];
        this->next_event_index_ = 0;
    } else {
        // Normal case: get the next event after current
        this->next_event_raw_ = this->schedule_times_in_minutes_[current_event_index_ + 1];
        this->next_event_index_ = current_event_index_ + 1;
        
        if (this->next_event_raw_ == 0xFFFF) {
            // End of schedule reached, roll over to start of schedule
            ESP_LOGI(TAG, "End of schedule reached, rolling over to start of schedule");
            this->next_event_raw_ = this->schedule_times_in_minutes_[0];
            this->next_event_index_ = 0;
        }
    }
    
	ESP_LOGV(TAG,"current_event_raw_: 0x%04X, next_event_raw_: 0x%04X current_event_index: %d, next_event_index: %d", this->current_event_raw_, this->next_event_raw_, current_event_index_, this->next_event_index_);
    uint16_t next_event_time = this->next_event_raw_ & TIME_MASK;

    // Determine if current event is an "on" or "off" event
    bool in_event = (this->current_event_raw_ & SWITCH_STATE_BIT) != 0;
    
    ESP_LOGV(TAG, "Current event index: %d, time: %s, state: %s", 
             current_event_index_, this->format_event_time_(current_event_time).c_str(), in_event ? "ON" : "OFF");
    
    ESP_LOGD(TAG, "Schedule operation initialized");
}

int16_t Schedule::find_current_event_(uint16_t current_time_minutes) {
    const uint16_t TIME_MASK = 0x3FFF;  // Mask to extract time (bits 0-13)
    
    int16_t current_index = -1;  // No event yet
    
    for (size_t i = 0; i < this->schedule_times_in_minutes_.size(); i++) {
        uint16_t entry_raw = this->schedule_times_in_minutes_[i];
        
        // Check for terminator (0xFFFF)
        if (entry_raw == 0xFFFF) {
            break;
        }
        
        // Extract time value (mask off top 2 bits)
        uint16_t entry_time = entry_raw & TIME_MASK;
        
        // If this entry's time has passed, it becomes the current event
        if (entry_time <= current_time_minutes) {
            current_index = static_cast<int16_t>(i);			
        } else {
            // Once we hit a future entry, stop searching
            break;
        }
    }
   
    // If no event has occurred yet this week, wrap around to the last event from previous week
    if (current_index == -1) {
        // Find the last valid entry in the schedule (before the 0xFFFF terminator)
        for (size_t i = 0; i < this->schedule_times_in_minutes_.size(); i++) {
            if (this->schedule_times_in_minutes_[i] == 0xFFFF) {
                // Found terminator, the previous entry is the last event
                if (i > 0) {
                    current_index = static_cast<int16_t>(i - 1);
                }
                break;
            }
        }
    }
    
    return current_index;
}

bool Schedule::should_advance_to_next_event_(uint16_t current_time_minutes) {
    uint16_t next_event_time = this->next_event_raw_ & TIME_MASK;
    uint16_t current_event_time = this->current_event_raw_ & TIME_MASK;
    
    // Check if schedule wrapped around (next event is earlier in week than current)
    bool wrapped_around = (next_event_time < current_event_time);
    
    // If wrapped around, check if current time has also wrapped (is before current event)
    // This indicates we've crossed into the new week
    bool time_has_wrapped = (current_time_minutes < current_event_time);
    
    // Debug logging every 60 seconds
    static uint32_t last_debug_log = 0;
    if (millis() - last_debug_log >= 60000) {
        last_debug_log = millis();
        ESP_LOGD(TAG, "Event check: current_time=%u, next_event=%u, current_event=%u, wrapped=%s, time_wrapped=%s", 
                 current_time_minutes, next_event_time, current_event_time, 
                 wrapped_around ? "Y" : "N", time_has_wrapped ? "Y" : "N");
    }
    
    // Advance to next event if:
    // 1. Current time has reached next event time AND
    // 2. Either we haven't wrapped, OR we have wrapped AND time has also wrapped
    return (current_time_minutes >= next_event_time && (!wrapped_around || time_has_wrapped));
}

void Schedule::advance_to_next_event_() {
    // Current event becomes the next event
    this->current_event_raw_ = this->next_event_raw_;
    this->current_event_index_ = this->next_event_index_;
    
    // Check if we've reached the end of the schedule
    if (this->schedule_times_in_minutes_[this->current_event_index_ + 1] == 0xFFFF) {
        // End of schedule reached, roll over to start
        ESP_LOGI(TAG, "End of schedule reached, rolling over to start of schedule");
        this->next_event_raw_ = this->schedule_times_in_minutes_[0];
        this->next_event_index_ = 0;
    } else {
        // Get the next event after current
        this->next_event_raw_ = this->schedule_times_in_minutes_[this->current_event_index_ + 1];
        this->next_event_index_ = this->current_event_index_ + 1;
    }
}

void Schedule::check_and_advance_events_() {
    // Get current time in minutes
    auto now_time = this->time_->now();
    uint16_t current_time_minutes = this->time_to_minutes_(now_time);
    
    // Check if we should advance to next event
    if (!this->should_advance_to_next_event_(current_time_minutes)) {
        return;
    }
    
    // Advance to next event
    this->advance_to_next_event_();
}

//==============================================================================
// UI UPDATE METHODS
//==============================================================================

void Schedule::display_current_next_events_(std::string current_text, std::string next_text) {	
	if (this->current_event_sensor_ != nullptr) {
		// Check if sensor is already displaying the correct text
		if(this->current_event_sensor_->get_state() != current_text) {
			this->current_event_sensor_->publish_state(current_text);
		}
	}
	if (this->next_event_sensor_ != nullptr) {
		// Check if sensor is already displaying the correct text
		if(this->next_event_sensor_->get_state() != next_text) {
			this->next_event_sensor_->publish_state(next_text);
		}
	}
}

void Schedule::set_data_sensors_(int16_t event_index, bool switch_state, bool manual_override) {
    for (auto *sensor : this->data_sensors_) {
        sensor->apply_state(event_index, switch_state, manual_override);
    }
}

//==============================================================================
// TIME AND FORMATTING UTILITIES
//==============================================================================

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

uint16_t Schedule::get_current_week_minutes_() {
    if (this->time_ == nullptr) {
        ESP_LOGW(TAG, "No time component configured");
        return 0;
    }
    
    auto now = this->time_->now();
    if (!now.is_valid()) {
        ESP_LOGW(TAG, "Invalid time");
        return 0;
    }
    
    // Calculate current time in minutes from start of week (Monday = 0)
    // ESPHome: 1=Sunday, 2=Monday, ..., 7=Saturday
    // We need: Monday=0, ..., Sunday=6
    uint8_t day_of_week = (now.day_of_week + 5) % 7;  // Convert to Monday=0
    uint16_t current_time_minutes = (day_of_week * 1440) + (now.hour * 60) + now.minute;
    
    return current_time_minutes;
}

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

std::string Schedule::format_event_time_(uint16_t time_minutes) {
    uint8_t day = time_minutes / 1440;  // 1440 minutes in a day
	std::string day_str;
	switch(day) {
		case 0:	day_str = "Mon"; break;
		case 1:	day_str = "Tue"; break;	
		case 2:	day_str = "Wed"; break;
		case 3:	day_str = "Thu"; break;
		case 4:	day_str = "Fri"; break;
		case 5:	day_str = "Sat"; break;	
		case 6:	day_str = "Sun"; break;
		default: day_str = "???"; break;
	}
    uint16_t minutes_in_day = time_minutes % 1440;
    uint8_t hour = minutes_in_day / 60;
    uint8_t minute = minutes_in_day % 60;
    
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%s:%02u:%02u", day_str.c_str(), hour, minute);
    return std::string(buffer);
}

//==============================================================================
// PREFERENCE MANAGEMENT
//==============================================================================

void Schedule::create_schedule_preference() {
    ESP_LOGI(TAG, "Creating schedule preference with key hash: %u", this->get_object_id_hash());
    if (!sched_array_pref_) {
        this->schedule_empty_ = true;
        this->schedule_valid_ = false;
        return;
    }
    sched_array_pref_->create_preference(this->get_object_id_hash());
    ESP_LOGV(TAG, "Schedule preference created successfully.");
}

void Schedule::load_schedule_from_pref_() {
    ESP_LOGV(TAG, "Loading schedule from preferences");
    if (!sched_array_pref_) {
        ESP_LOGW(TAG, "No schedule preference object available to load from");
        this->schedule_empty_ = true;
        this->schedule_valid_ = false;
        return;
    } 
    // Create temporary buffer to load data
    std::vector<uint16_t> temp_buffer(this->schedule_max_size_);
    // Load data into the array preference
    this->sched_array_pref_->load();
    //
    bool ok = this->sched_array_pref_->is_valid();
    //bool ok = false;
    ESP_LOGV(TAG, "Schedule preference load completed");
   if (!ok) {
       ESP_LOGW(TAG, "Schedule preference data is not valid");
       this->schedule_empty_ = true;
    }
    else {
        uint8_t *buf = this->sched_array_pref_->data();
        std::memcpy(temp_buffer.data(), buf, this->sched_array_pref_->size());
        // Check for terminator [0xFFFF, 0xFFFF] - used by both state-based and event-based
        for (size_t i = 0; i < this->schedule_max_size_; i += 2) {
            if (temp_buffer[i] == 0xFFFF && temp_buffer[i + 1] == 0xFFFF) {
                ESP_LOGI(TAG, "Found terminator at index %u; actual schedule size is %u entries", 
                         static_cast<unsigned>(i / 2), static_cast<unsigned>(i));
                // Schedule is empty if terminator is at the very first position
                this->schedule_empty_ = (i == 0);
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
        //this->schedule_empty_ = false;
        this->schedule_valid_ = true;   
        ESP_LOGI(TAG, "Loaded %u uint16_t values from preferences", 
                 static_cast<unsigned>(this->schedule_times_in_minutes_.size()));
        
    } else {
        // No stored data: use factory defaults and persist them
        this->schedule_times_in_minutes_ = this->factory_reset_values_;
        this->schedule_empty_ = true;
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
    log_state_flags_();
    for (size_t i = 0; i < this->schedule_times_in_minutes_.size(); ++i) {
		// Log index and value in hex format
        ESP_LOGV(TAG, "schedule_times_in_minutes_[%u] = 0x%04X", static_cast<unsigned>(i), this->schedule_times_in_minutes_[i]);
    } 
}

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

void Schedule::load_entity_id_from_pref_() {
    // Create a preference hash for entity ID storage
    uint32_t entity_pref_hash = fnv1_hash("entity_id") ^ this->get_object_id_hash();
    
    auto restore = global_preferences->make_preference<uint32_t>(entity_pref_hash);
    
    if (restore.load(&this->stored_entity_id_hash_)) {
        ESP_LOGV(TAG, "Loaded stored entity ID hash from preferences: 0x%08X", this->stored_entity_id_hash_);
    } else {
        this->stored_entity_id_hash_ = 0;
        ESP_LOGV(TAG, "No stored entity ID hash found in preferences");
    }
}

void Schedule::save_entity_id_to_pref_() {
    uint32_t entity_pref_hash = fnv1_hash("entity_id") ^ this->get_object_id_hash();
    uint32_t current_hash = fnv1_hash(this->ha_schedule_entity_id_);
    
    auto restore = global_preferences->make_preference<uint32_t>(entity_pref_hash);
    restore.save(&current_hash);
    this->stored_entity_id_hash_ = current_hash;
    
    ESP_LOGV(TAG, "Saved entity ID hash to preferences: 0x%08X", current_hash);
}

void Schedule::sched_add_pref(ArrayPreferenceBase *array_pref) {
  sched_array_pref_ = array_pref;
}

//==============================================================================
// HOME ASSISTANT INTEGRATION
//==============================================================================

void Schedule::setup_schedule_retrieval_service_() {
     if (this->ha_schedule_entity_id_.empty()) {
        ESP_LOGE(TAG, "Cannot trigger retrieval: schedule_entity_id is empty.");
        ha_connected_ = false;
        return;
    }
    #ifdef USE_API
        if (esphome::api::global_api_server == nullptr) {
            ESP_LOGW(TAG, "APIServer not available");
            ha_connected_ = false;
        return;
        }
        ESP_LOGI(TAG, "Setting up schedule.get_schedule service for %s...", ha_schedule_entity_id_.c_str());
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
    
    // Mark schedule as invalid at start of processing
    this->schedule_valid_ = false;
    
    // Safetycheck that the expected entity is present in the response
    if (!response["response"][this->ha_schedule_entity_id_.c_str()].is<JsonObjectConst>()) {
        ESP_LOGW(TAG, "Expected entity '%s' not found in response", this->ha_schedule_entity_id_.c_str());
        std::string msg = "Schedule retrieval failed: Entity '" + this->ha_schedule_entity_id_ + "' not found in response";
        this->send_ha_notification_(msg, "Schedule Error");
        return;
    }
    JsonObjectConst schedule = response["response"][this->ha_schedule_entity_id_.c_str()];
    
    work_buffer_.clear();
    
    // Create temporary work buffers for each data sensor to collect values during parsing
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
            std::string msg = "Schedule parsing failed: Day '" + std::string(days[i]) + "' not found. Schedule data is corrupted or incomplete.";
            this->send_ha_notification_(msg, "Schedule Error");
            return;
        }
        JsonArrayConst day_array = schedule[days[i]].as<JsonArrayConst>();
        
        for (JsonObjectConst entry : day_array) {
            // Validate entry has "from" and "to" fields
            if (!entry["from"].is<const char*>() || !entry["to"].is<const char*>()) {
                ESP_LOGE(TAG, "Invalid or missing 'from'/'to' fields in %s; aborting schedule processing.", days[i]);
                ESP_LOGE(TAG, "Schedule data is corrupted or incomplete. Please verify the schedule configuration.");
                std::string msg = "Schedule parsing failed: Invalid or missing 'from'/'to' fields in " + std::string(days[i]) + ". Please verify the schedule configuration.";
                this->send_ha_notification_(msg, "Schedule Error");
                return;
            }
            
            if (!(this->isValidTime_(entry["from"]) && this->isValidTime_(entry["to"]))) {
                ESP_LOGE(TAG, "Invalid time range in %s: from='%s', to='%s'; aborting schedule processing.",
                        days[i],
                        entry["from"].as<const char*>(),
                        entry["to"].as<const char*>());

                ESP_LOGE(TAG, "Schedule data is corrupted or incomplete. Please verify the schedule configuration.");
                std::string msg = "Schedule parsing failed: Invalid time range in " + std::string(days[i]) + 
                                  " (from='" + std::string(entry["from"].as<const char*>()) + 
                                  "', to='" + std::string(entry["to"].as<const char*>()) + "'). Please verify the schedule configuration.";
                this->send_ha_notification_(msg, "Schedule Error");
                return;
            }
            
            // EXTENSIBILITY: Call virtual method to parse entry based on storage type
            // Default (state-based): stores [ON_TIME, OFF_TIME] pairs
            // Event-based override: stores [EVENT_TIME] singles
            this->parse_schedule_entry(entry, work_buffer_, day_offset_minutes);
            
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
                    std::string msg = "Schedule parsing failed: Missing data field '" + label + "' in " + 
                                      std::string(days[i]) + " entry. Please verify the schedule configuration.";
                    this->send_ha_notification_(msg, "Schedule Error");
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
                            std::string msg = "Schedule parsing failed: Data field '" + label + "' in " + 
                                              std::string(days[i]) + " is not an integer type (expected for item_type " + 
                                              std::to_string(item_type) + ").";
                            this->send_ha_notification_(msg, "Schedule Error");
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
                            std::string msg = "Schedule parsing failed: Data field '" + label + "' in " + 
                                              std::string(days[i]) + " is not a numeric type (expected for item_type " + 
                                              std::to_string(item_type) + ").";
                            this->send_ha_notification_(msg, "Schedule Error");
                            return;
                        }
                        value_str = std::to_string(data_value.as<float>());
                        break;
                    }
                    default:
                        ESP_LOGE(TAG, "Unknown item_type %u for sensor '%s'; aborting schedule processing.", 
                                 item_type, label.c_str());
                        std::string msg = "Schedule parsing failed: Unknown item_type " + std::to_string(item_type) + 
                                          " for sensor '" + label + "'. Expected types: 0=uint8_t, 1=uint16_t, 2=int32_t, 3=float.";
                        this->send_ha_notification_(msg, "Schedule Error");
                        return;
                }
                
                // Add to work buffer for this sensor
                data_work_buffers[sensor_idx].push_back(value_str);
            }
        }
        
        // Offset for the next day: each day is 1440 minutes (24 hours)
        day_offset_minutes += 1440;
    }
    
    // Append terminating values [0xFFFF, 0xFFFF] - used by both storage types
    work_buffer_.push_back(0xFFFF);
    work_buffer_.push_back(0xFFFF);
    
    // Check if schedule is empty (only contains terminator)
    bool is_empty = (work_buffer_.size() == this->get_storage_multiplier());
    
    // Check size against max size
    if (work_buffer_.size() > this->schedule_max_size_) {
        ESP_LOGW(TAG, "Received schedule (%u entries) exceeds max size (%u); truncating.", 
                 static_cast<unsigned>(work_buffer_.size()), static_cast<unsigned>(this->schedule_max_size_));
        std::string msg = "Schedule too large: Received " + std::to_string(work_buffer_.size()) + 
                          " entries but max size is " + std::to_string(this->schedule_max_size_) + 
                          ". Schedule has been truncated. Consider reducing schedule complexity or increasing max_schedule_size.";
        this->send_ha_notification_(msg, "Schedule Warning");
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
    // Mark schedule as valid after successful processing
    this->schedule_valid_ = true;
    // Set schedule_empty based on whether we found any schedule entries
    this->schedule_empty_ = is_empty;
    
    if (is_empty) {
        ESP_LOGI(TAG, "Schedule is empty (no time entries found).");
    }
    log_state_flags_();
}

void Schedule::parse_schedule_entry(const JsonObjectConst &entry, 
                                    std::vector<uint16_t> &work_buffer,
                                    uint16_t day_offset) {
    // State-based: Extract both "from" (ON) and "to" (OFF) times
    uint16_t from = this->timeToMinutes_(entry["from"]) + 0x4000;  // Set bit 14 for ON
    uint16_t to = this->timeToMinutes_(entry["to"]);                // Bit 14 clear for OFF
    
    work_buffer.push_back(from + day_offset);
    work_buffer.push_back(to + day_offset);
}

//==============================================================================
// DATA MANAGEMENT
//==============================================================================

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

void Schedule::print_data_items() {
        for (const auto& item : data_items_) {
            ESP_LOGD(TAG, "Data Item - Label: %s, Value: %u, Size: %u", item.label.c_str(), item.value, item.size);
        }
    }

void Schedule::log_schedule_data() {
    // Default implementation for state-based format (ON/OFF pairs)
    // EventBasedSchedulable will override with single-event format
    ESP_LOGV(TAG, "=== Schedule Data Dump (State-Based Format) ===");
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
            
            // Convert minutes to day and time need to mask off msb and bit 14
            from = from & TIME_MASK;
            uint16_t from_day = from / 1440;
            uint16_t from_minutes = from % 1440;
            uint16_t from_hours = from_minutes / 60;
            uint16_t from_mins = from_minutes % 60;

            // Repeat for "to" time
            to = to & TIME_MASK;
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
}

//==============================================================================
// TEST AND DEBUG METHODS
//==============================================================================

void Schedule::test_create_preference() {
  if (!sched_array_pref_) return;

  sched_array_pref_->create_preference(this->get_object_id_hash());
  ESP_LOGI(TAG, "test_create_preference: key=0x%08X", this->get_object_id_hash());
}

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
