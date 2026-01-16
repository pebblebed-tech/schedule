#include "state_based_schedulable.h"
#include "schedule_state_mode_select.h"

namespace esphome {
namespace schedule {

static const char *const TAG = "schedule.state_based";

//==============================================================================
// MODE MANAGEMENT
//==============================================================================

void StateBasedSchedulable::set_mode_select(ScheduleStateModeSelect *mode_select) {
  this->mode_select_ = mode_select;
  if (mode_select != nullptr) {
    mode_select->set_on_value_callback([this](const std::string &value) {
      this->on_mode_changed(value);
    });
  }
}

void StateBasedSchedulable::on_mode_changed(const std::string &mode) {
  ESP_LOGI(TAG, "Schedule mode changed to: %s", mode.c_str());
  
  // Update the current mode enum based on the selected mode string
  if (mode == "Manual Off") {
    this->current_mode_ = SCHEDULE_MODE_MANUAL_OFF;
  } else if (mode == "Early Off") {
    this->current_mode_ = SCHEDULE_MODE_EARLY_OFF;
  } else if (mode == "Auto") {
    this->current_mode_ = SCHEDULE_MODE_AUTO;
  } else if (mode == "Manual On") {
    this->current_mode_ = SCHEDULE_MODE_MANUAL_ON;
  } else if (mode == "Boost On") {
    this->current_mode_ = SCHEDULE_MODE_BOOST_ON;
  } else {
    ESP_LOGW(TAG, "Unknown mode: %s, defaulting to Manual Off", mode.c_str());
    this->current_mode_ = SCHEDULE_MODE_MANUAL_OFF;
  }
  
  ESP_LOGD(TAG, "Current mode enum set to: %d", this->current_mode_);
}

// Set the mode select option programmatically using enum
void StateBasedSchedulable::set_mode_option(ScheduleMode mode) {
  if (this->mode_select_ != nullptr) {
    std::string option;
    switch (mode) {
      case SCHEDULE_MODE_MANUAL_OFF:
        option = "Manual Off";
        break;
      case SCHEDULE_MODE_EARLY_OFF:
        option = "Early Off";
        break;
      case SCHEDULE_MODE_AUTO:
        option = "Auto";
        break;
      case SCHEDULE_MODE_MANUAL_ON:
        option = "Manual On";
        break;
      case SCHEDULE_MODE_BOOST_ON:
        option = "Boost On";
        break;
      default:
        option = "Manual Off";
        ESP_LOGW(TAG, "Unknown mode enum: %d, defaulting to Manual Off", mode);
        break;
    }
    this->mode_select_->publish_state(option);
    this->current_mode_ = mode;
    ESP_LOGD(TAG, "Mode set to: %s (enum: %d)", option.c_str(), mode);
  }
}

//==============================================================================
// STATE MACHINE HELPER METHODS
//==============================================================================

int StateBasedSchedulable::mode_to_state_(ScheduleMode mode, bool event_on) {
  switch(mode) {
    case SCHEDULE_MODE_MANUAL_OFF:
      return STATE_MAN_OFF;
    case SCHEDULE_MODE_MANUAL_ON:
      return STATE_MAN_ON;
    case SCHEDULE_MODE_EARLY_OFF:
      return STATE_EARLY_OFF;
    case SCHEDULE_MODE_BOOST_ON:
      return STATE_BOOST_ON;
    case SCHEDULE_MODE_AUTO:
      return event_on ? STATE_AUTO_ON : STATE_AUTO_OFF;
    default:
      ESP_LOGW(TAG, "Unknown schedule mode: %d", mode);
      return STATE_SCHEDULE_INVALID;
  }
}

// Check if temporary mode (early-off or boost) should reset to auto
bool StateBasedSchedulable::should_reset_to_auto_(int state, bool event_on) {
  // Early-off mode resets to auto on any schedule event (either ON or OFF)
  if (state == STATE_EARLY_OFF) {
    return true;
  }
  // Boost mode resets to auto on any schedule event (either ON or OFF)
  if (state == STATE_BOOST_ON) {
    return true;
  }
  return false;
}

// Get the state after resetting from temporary mode to auto
int StateBasedSchedulable::get_state_after_mode_reset_(bool event_on) {
  return event_on ? STATE_AUTO_ON : STATE_AUTO_OFF;
}

//==============================================================================
// OVERRIDDEN BASE CLASS METHODS
//==============================================================================

void StateBasedSchedulable::advance_to_next_event_() {
  // Call base class implementation to handle common event advancement
  Schedule::advance_to_next_event_();
  
  // Update event switch state based on current event
  this->event_switch_state_ = (this->current_event_raw_ & SWITCH_STATE_BIT) != 0;
}

void StateBasedSchedulable::check_and_advance_events_() {
  // Get current time in minutes
  auto now_time = this->time_->now();
  uint16_t current_time_minutes = this->time_to_minutes_(now_time);
  
  // Check if we should advance to next event
  if (!this->should_advance_to_next_event_(current_time_minutes)) {
    return;
  }
  
  // Advance to next event (updates event_switch_state_)
  this->advance_to_next_event_();
  
  // Handle mode transitions for temporary modes
  if (this->should_reset_to_auto_(this->current_state_, this->event_switch_state_)) {
    // Reset from temporary mode (early-off or boost) back to auto
    this->current_mode_ = SCHEDULE_MODE_AUTO;
    this->current_state_ = this->get_state_after_mode_reset_(this->event_switch_state_);
    this->set_mode_option(this->current_mode_);
    ESP_LOGD(TAG, "Temporary mode expired, reset to AUTO mode, state=%d", this->current_state_);
  }
  // Update state for auto mode based on new event
  else if (this->current_mode_ == SCHEDULE_MODE_AUTO) {
    this->current_state_ = this->event_switch_state_ ? STATE_AUTO_ON : STATE_AUTO_OFF;
  }
}

void StateBasedSchedulable::initialize_schedule_operation_() {
  // Call base class implementation for common initialization
  Schedule::initialize_schedule_operation_();
  
  // Determine if current event is an "on" or "off" event
  bool in_event = (this->current_event_raw_ & SWITCH_STATE_BIT) != 0;
  this->event_switch_state_ = in_event;
  
  // Initialize last_on_value_ for each data sensor by searching backwards for the most recent ON event
  this->initialize_sensor_last_on_values_(this->current_event_index_);
  
  // Set initial state based on current event and mode
  this->current_state_ = this->mode_to_state_(this->current_mode_, this->event_switch_state_);
  
  // Reset processed_state_ to force state change detection on next loop iteration
  this->processed_state_ = STATE_TIME_INVALID;
  
  ESP_LOGD(TAG, "State-based initialization complete, state: %d", this->current_state_);
}

// Initialize last_on_value_ for each data sensor by finding the most recent ON event
void StateBasedSchedulable::initialize_sensor_last_on_values_(int16_t current_event_index) {
    ESP_LOGV(TAG, "Initializing sensor last_on_value_ from schedule history");
    
    // Search backwards from current event to find the most recent ON event
    // Start from current event and work backwards
    int16_t search_index = current_event_index;
    
    // If current event is an ON event, use it
    if ((this->schedule_times_in_minutes_[search_index] & SWITCH_STATE_BIT) != 0) {
        ESP_LOGV(TAG, "Current event is ON, using it for last_on_value_ initialization");
        uint16_t data_index = search_index / 2;
        for (auto *sensor : this->data_sensors_) {
            float value = sensor->get_sensor_value(data_index);
            sensor->set_last_on_value(value);
            ESP_LOGV(TAG, "Sensor '%s' last_on_value_ initialized to %.2f from current ON event at index %d",
                     sensor->get_label().c_str(), value, search_index);
        }
        return;
    }
    
    // Current event is OFF, search backwards for previous ON event
    search_index--;
    
    // Search backwards through this week's schedule
    while (search_index >= 0) {
        uint16_t event_raw = this->schedule_times_in_minutes_[search_index];
        
        // Check if this is an ON event
        if ((event_raw & SWITCH_STATE_BIT) != 0) {
            ESP_LOGV(TAG, "Found previous ON event at index %d", search_index);
            uint16_t data_index = search_index / 2;
            for (auto *sensor : this->data_sensors_) {
                float value = sensor->get_sensor_value(data_index);
                sensor->set_last_on_value(value);
                ESP_LOGV(TAG, "Sensor '%s' last_on_value_ initialized to %.2f from ON event at index %d",
                         sensor->get_label().c_str(), value, search_index);
            }
            return;
        }
        search_index--;
    }
    
    // No ON event found in current week, search from end of schedule backwards (previous week rollback)
    ESP_LOGV(TAG, "No ON event found in current week, searching from end of schedule");
    
    // Find the last valid entry in the schedule
    int16_t last_index = -1;
    for (size_t i = 0; i < this->schedule_times_in_minutes_.size(); i++) {
        if (this->schedule_times_in_minutes_[i] == 0xFFFF) {
            last_index = i - 1;
            break;
        }
    }
    
    if (last_index < 0) {
        ESP_LOGW(TAG, "Could not find end of schedule, cannot initialize last_on_value_");
        return;
    }
    
    // Search backwards from end of schedule
    search_index = last_index;
    while (search_index >= 0) {
        uint16_t event_raw = this->schedule_times_in_minutes_[search_index];
        
        // Check if this is an ON event
        if ((event_raw & SWITCH_STATE_BIT) != 0) {
            ESP_LOGV(TAG, "Found previous week's ON event at index %d", search_index);
            uint16_t data_index = search_index / 2;
            for (auto *sensor : this->data_sensors_) {
                float value = sensor->get_sensor_value(data_index);
                sensor->set_last_on_value(value);
                ESP_LOGV(TAG, "Sensor '%s' last_on_value_ initialized to %.2f from previous week ON event at index %d",
                         sensor->get_label().c_str(), value, search_index);
            }
            return;
        }
        search_index--;
    }
    
    // No ON event found in entire schedule
    ESP_LOGW(TAG, "No ON event found in entire schedule, last_on_value_ remains NaN");
}

// Create event string for state-based (ON/OFF format)
std::string StateBasedSchedulable::create_event_string_(uint16_t event_raw) {
	uint16_t event_time = event_raw & TIME_MASK;
	bool event_state = (event_raw & SWITCH_STATE_BIT) != 0;
	char buffer[32];
	snprintf(buffer, sizeof(buffer), "%s at %s", event_state ? "ON" : "OFF", this->format_event_time_(event_time).c_str());
	return std::string(buffer);
}

//==============================================================================
// STATE MACHINE METHODS
//==============================================================================

// Handle state transitions and perform associated actions
void StateBasedSchedulable::handle_state_change_() {
  // Only process if state actually changed
  if (this->current_state_ == this->processed_state_) {
    return;
  }
  
  this->processed_state_ = this->current_state_;
  ESP_LOGV(TAG, "Schedule state changed to: %d", this->current_state_);
  
  // Perform actions based on new state
  switch(this->current_state_) {
    case STATE_TIME_INVALID:
    case STATE_SCHEDULE_INVALID:
      // Invalid states - turn off and display error
      this->apply_scheduled_state(false);
      this->update_switch_indicator(false);
      this->display_current_next_events_(
        this->current_state_ == STATE_TIME_INVALID ? "Time Invalid" : "Schedule Invalid",
        this->current_state_ == STATE_TIME_INVALID ? "Time Invalid" : "Schedule Invalid"
      );
      break;
      
    case STATE_SCHEDULE_EMPTY:
      // Schedule valid but empty - turn off and inform
      this->apply_scheduled_state(false);
      this->update_switch_indicator(false);
      this->display_current_next_events_("Schedule Empty", "Schedule Empty");
      break;
      
    case STATE_INIT:
      // Initialization state - turn off and display message
      this->apply_scheduled_state(false);
      this->update_switch_indicator(false);
      this->display_current_next_events_("Initializing", "Initializing");
      break;
      
    case STATE_MAN_OFF:
      // Manual off mode - force off
      this->apply_scheduled_state(false);
      this->update_switch_indicator(false);
      this->display_current_next_events_("Manual Off", "");
      this->set_data_sensors_(this->current_event_index_, false, true);
      break;
      
    case STATE_MAN_ON:
      // Manual on mode - force on
      this->apply_scheduled_state(true);
      this->update_switch_indicator(true);
      this->display_current_next_events_("Manual On", "");
      this->set_data_sensors_(this->current_event_index_, true, true);
      break;
      
    case STATE_EARLY_OFF:
      // Early-off mode - turn off until next schedule event
      this->apply_scheduled_state(false);
      this->update_switch_indicator(false);
      this->display_current_next_events_("Early Off", this->create_event_string_(this->next_event_raw_));
      this->set_data_sensors_(this->current_event_index_, false, false);
      break;
      
    case STATE_BOOST_ON:
      // Boost mode - turn on until next schedule event
      this->apply_scheduled_state(true);
      this->update_switch_indicator(true);
      this->display_current_next_events_("Boost On", this->create_event_string_(this->next_event_raw_));
      this->set_data_sensors_(this->current_event_index_, true, false);
      break;
      
    case STATE_AUTO_ON:
      // Auto mode - schedule indicates ON
      this->apply_scheduled_state(true);
      this->update_switch_indicator(true);
      this->display_current_next_events_(
        this->create_event_string_(this->current_event_raw_), 
        this->create_event_string_(this->next_event_raw_)
      );
      this->set_data_sensors_(this->current_event_index_, true, false);
      break;
      
    case STATE_AUTO_OFF:
      // Auto mode - schedule indicates OFF
      this->apply_scheduled_state(false);
      this->update_switch_indicator(false);
      this->display_current_next_events_(
        this->create_event_string_(this->current_event_raw_), 
        this->create_event_string_(this->next_event_raw_)
      );
      this->set_data_sensors_(this->current_event_index_, false, false);
      break;
      
    default:
      ESP_LOGW(TAG, "Unknown schedule state in handle_state_change_: %d", this->current_state_);
      this->current_state_ = STATE_SCHEDULE_INVALID;
      break;
  }
}

//==============================================================================
// LOGGING
//==============================================================================

void StateBasedSchedulable::log_schedule_data() {
  // Use base class implementation - it's already in state-based format (ON/OFF pairs)
  Schedule::log_schedule_data();
}

} // namespace schedule
} // namespace esphome
