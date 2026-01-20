#include "event_based_schedulable.h"
#include "schedule_event_mode_select.h"
#include "esphome/core/log.h"

namespace esphome {
namespace schedule {

static const char *const TAG = "schedule.event_based";

//==============================================================================
// VIRTUAL METHOD OVERRIDES FROM BASE CLASS
//==============================================================================

void EventBasedSchedulable::on_schedule_empty_changed(bool is_empty) {
  if (this->mode_select_ != nullptr) {
    this->mode_select_->set_disabled_only_mode(is_empty);
    if (is_empty) {
      ESP_LOGI(TAG, "Schedule empty - restricting to disabled mode only");
    } else {
      ESP_LOGI(TAG, "Schedule populated - both modes available");
    }
  }
}

void EventBasedSchedulable::advance_to_next_event_() {
  // Call base class implementation to handle common event advancement
  Schedule::advance_to_next_event_();
  
  // Event-based has no additional state to track (no ON/OFF)
  // Just trigger the event when it occurs
}

void EventBasedSchedulable::check_and_advance_events_() {
  // Get current time in minutes
  auto now_time = this->time_->now();
  uint16_t current_time_minutes = this->time_to_minutes_(now_time);
  
  // Check if we should advance to next event
  if (!this->should_advance_to_next_event_(current_time_minutes)) {
    return;
  }
  
  // Advance to next event
  this->advance_to_next_event_();
  
  // Trigger the scheduled event (no state checking needed)
  // The platform-specific apply_scheduled_state() will handle the action
}

void EventBasedSchedulable::initialize_schedule_operation_() {
  // Call base class implementation to find current/next events
  Schedule::initialize_schedule_operation_();
  
  // Event-based doesn't need to initialize ON/OFF state or search for last ON values
  // Just ready to trigger events as they occur
  
  ESP_LOGD(TAG, "Event-based initialization complete, state: %d", this->current_state_);
}

//==============================================================================
// EVENT-BASED HELPER METHODS
//==============================================================================

void EventBasedSchedulable::update_event_based_ui_() {
  // For event-based, we show current event as "EVENT" and next event time
  std::string current_text = "EVENT at " + this->format_event_time_(this->current_event_raw_ & TIME_MASK);
  std::string next_text = "EVENT at " + this->format_event_time_(this->next_event_raw_ & TIME_MASK);
  
  this->display_current_next_events_(current_text, next_text);
  
  // Update data sensors with current event index
  this->set_data_sensors_(this->current_event_index_, true, false);
}

//==============================================================================
// LOGGING
//==============================================================================

void EventBasedSchedulable::log_schedule_data() {
  ESP_LOGI(TAG, "Event-Based Schedule Data:");
  ESP_LOGI(TAG, "Max Entries: %u", this->schedule_max_entries_);

  uint16_t *schedule_data = this->schedule_times_in_minutes_.data();
  size_t index = 0;
  uint16_t entry_count = 0;

  // Process entries until terminator (0xFFFF) or end of data
  while (index < this->schedule_times_in_minutes_.size() && schedule_data[index] != 0xFFFF) {
    uint16_t event_time = schedule_data[index];
    
    // Extract event time (mask off state bit)
    uint16_t time_minutes = event_time & TIME_MASK;
    uint16_t day = time_minutes / 1440;
    uint16_t hour = (time_minutes % 1440) / 60;
    uint16_t minute = time_minutes % 60;

    const char *day_names[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    ESP_LOGI(TAG, "  Entry %u: EVENT at %s:%02u:%02u (raw: 0x%04X)", 
             entry_count, day_names[day], hour, minute, event_time);

    index++;
    entry_count++;
  }

  ESP_LOGI(TAG, "Total Entries: %u", entry_count);
}

} // namespace schedule
} // namespace esphome
