// ============================================================================
// REFERENCE IMPLEMENTATION - NOT COMPILED
// ============================================================================
// This file shows an alternative architecture approach using a separate
// SchedulableBase class. The actual implementation uses virtual methods
// in the Schedule class directly to avoid diamond inheritance issues.
// 
// This file is kept for documentation purposes only.
// ============================================================================

#include "schedulable_base.h"
#include "esphome/core/log.h"

namespace esphome {
namespace schedule {

static const char *TAG = "schedulable";

// ============================================================================
// COMPONENT LIFECYCLE
// ============================================================================

void SchedulableBase::setup() {
  ESP_LOGI(TAG, "Setting up SchedulableBase component...");
  
  // Check if time component is configured
  if (this->time_ == nullptr) {
    ESP_LOGW(TAG, "No time component configured!");
  } else {
    this->check_rtc_time_valid_();
  }
  
  // Create and load schedule preference
  this->create_schedule_preference();
  this->load_schedule_from_pref_();
  
  // Check Home Assistant connection
  this->check_ha_connection_();
  
  // Request schedule if needed
  if (this->ha_connected_ && !this->schedule_valid_) {
    ESP_LOGI(TAG, "Schedule invalid, requesting from Home Assistant...");
    this->request_schedule();
  }
  
  ESP_LOGI(TAG, "SchedulableBase setup complete");
}

void SchedulableBase::loop() {
  // Check prerequisites
  auto prereq_result = this->check_prerequisites_();
  
  if (prereq_result != PREREQ_OK) {
    // Prerequisites not met, can't run schedule
    return;
  }
  
  // Check and advance events if needed
  this->check_and_advance_events_();
  
  // Platform-specific state application is handled by derived classes
}

void SchedulableBase::set_max_schedule_entries(size_t entries) {
  this->schedule_max_entries_ = entries;
  
  // Calculate total size based on storage type
  size_t multiplier = this->get_storage_multiplier();
  this->schedule_max_size_ = (entries * multiplier) + 2;  // +2 for terminator
  
  // Resize storage vector
  this->schedule_times_in_minutes_.resize(this->schedule_max_size_);
}

void SchedulableBase::register_data_sensor(DataSensor *sensor) {
  this->data_sensors_.push_back(sensor);
}

// ============================================================================
// STATE MACHINE METHODS
// ============================================================================

SchedulableBase::PrerequisiteError SchedulableBase::check_prerequisites_() {
  // Check time validity
  this->check_rtc_time_valid_();
  if (!this->rtc_time_valid_) {
    return PREREQ_TIME_INVALID;
  }
  
  // Check Home Assistant connection
  this->check_ha_connection_();
  
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

void SchedulableBase::initialize_schedule_operation_() {
  ESP_LOGI(TAG, "Initializing schedule operation...");
  
  if (this->time_ == nullptr) {
    ESP_LOGW(TAG, "Cannot initialize: no time component");
    return;
  }
  
  // Get current time
  auto now = this->time_->now();
  if (!now.is_valid()) {
    ESP_LOGW(TAG, "Cannot initialize: invalid time");
    return;
  }
  
  // Calculate current time in minutes from start of week
  uint16_t current_time_minutes = this->get_current_week_minutes_();
  
  // Find current event
  // (Implementation would go here - similar to actual Schedule class)
  
  ESP_LOGD(TAG, "Schedule operation initialized");
}

bool SchedulableBase::should_advance_to_next_event_(uint16_t current_time_minutes) {
  const uint16_t TIME_MASK = 0x3FFF;
  uint16_t next_event_time = this->next_event_raw_ & TIME_MASK;
  uint16_t current_event_time = this->current_event_raw_ & TIME_MASK;
  
  // Check if schedule wrapped around
  bool wrapped_around = (next_event_time < current_event_time);
  bool time_has_wrapped = (current_time_minutes < current_event_time);
  
  // Advance if current time >= next event time
  return (current_time_minutes >= next_event_time && (!wrapped_around || time_has_wrapped));
}

void SchedulableBase::advance_to_next_event_() {
  // Move to next event
  this->current_event_raw_ = this->next_event_raw_;
  this->current_event_index_ = this->next_event_index_;
  
  // Check if we've reached the end
  if (this->schedule_times_in_minutes_[this->current_event_index_ + 1] == 0xFFFF) {
    // Roll over to start
    this->next_event_raw_ = this->schedule_times_in_minutes_[0];
    this->next_event_index_ = 0;
  } else {
    // Get next event
    this->next_event_raw_ = this->schedule_times_in_minutes_[this->current_event_index_ + 1];
    this->next_event_index_ = this->current_event_index_ + 1;
  }
}

void SchedulableBase::check_and_advance_events_() {
  // Get current time
  uint16_t current_time_minutes = this->get_current_week_minutes_();
  
  // Check if we should advance
  if (this->should_advance_to_next_event_(current_time_minutes)) {
    this->advance_to_next_event_();
    
    // Determine if this is an ON or OFF event
    const uint16_t SWITCH_STATE_BIT = 0x4000;
    bool on = (this->current_event_raw_ & SWITCH_STATE_BIT) != 0;
    
    // Apply the scheduled state (platform-specific)
    this->apply_scheduled_state(on);
  }
}

// ============================================================================
// HELPER METHODS
// ============================================================================

void SchedulableBase::check_rtc_time_valid_() {
  if (this->time_ != nullptr) {
    auto now = this->time_->now();
    this->rtc_time_valid_ = now.is_valid();
  } else {
    this->rtc_time_valid_ = false;
  }
}

void SchedulableBase::check_ha_connection_() {
  // This would check the API server connection
  // Implementation depends on API server availability
  this->ha_connected_ = false;  // Placeholder
}

uint16_t SchedulableBase::time_to_minutes_(const char* time_str) {
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

uint16_t SchedulableBase::get_current_week_minutes_() {
  if (this->time_ == nullptr) {
    return 0;
  }
  
  auto now = this->time_->now();
  if (!now.is_valid()) {
    return 0;
  }
  
  // Convert ESPHome day (1=Sunday) to Monday=0
  uint8_t day_of_week = (now.day_of_week + 5) % 7;
  uint16_t current_time_minutes = (day_of_week * 1440) + (now.hour * 60) + now.minute;
  
  return current_time_minutes;
}

std::string SchedulableBase::format_event_time_(uint16_t time_minutes) {
  uint8_t day = time_minutes / 1440;
  const char* day_names[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
  
  uint16_t minutes_in_day = time_minutes % 1440;
  uint8_t hour = minutes_in_day / 60;
  uint8_t minute = minutes_in_day % 60;
  
  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%s:%02u:%02u", day_names[day], hour, minute);
  return std::string(buffer);
}

// ============================================================================
// HOME ASSISTANT INTEGRATION
// ============================================================================

void SchedulableBase::request_schedule() {
  ESP_LOGI(TAG, "Requesting schedule from Home Assistant...");
  // This would trigger the HA service call
  // Implementation depends on API server setup
}

void SchedulableBase::process_schedule_(const JsonObjectConst &response) {
  ESP_LOGI(TAG, "Processing schedule data...");
  
  // Mark as invalid during processing
  this->schedule_valid_ = false;
  
  // Check if entity exists in response
  if (!response["response"][this->ha_schedule_entity_id_.c_str()].is<JsonObjectConst>()) {
    ESP_LOGW(TAG, "Entity '%s' not found in response", this->ha_schedule_entity_id_.c_str());
    this->send_ha_notification_("Schedule entity not found", "Schedule Error");
    return;
  }
  
  JsonObjectConst schedule = response["response"][this->ha_schedule_entity_id_.c_str()];
  
  std::vector<uint16_t> work_buffer;
  work_buffer.clear();
  
  // Process each day
  const char* days[] = {"monday", "tuesday", "wednesday", "thursday", "friday", "saturday", "sunday"};
  uint16_t day_offset_minutes = 0;
  
  for (int i = 0; i < 7; ++i) {
    if (!schedule[days[i]].is<JsonArrayConst>()) {
      ESP_LOGE(TAG, "Day '%s' not found", days[i]);
      this->send_ha_notification_("Schedule day missing", "Schedule Error");
      return;
    }
    
    JsonArrayConst day_array = schedule[days[i]].as<JsonArrayConst>();
    
    for (JsonObjectConst entry : day_array) {
      // Use virtual method to parse entry (state-based vs event-based)
      this->parse_schedule_entry(entry, work_buffer, day_offset_minutes);
    }
    
    day_offset_minutes += 1440;  // Next day
  }
  
  // Add terminator
  work_buffer.push_back(0xFFFF);
  work_buffer.push_back(0xFFFF);
  
  // Check if empty
  this->schedule_empty_ = (work_buffer.size() == 2);
  
  // Check size
  if (work_buffer.size() > this->schedule_max_size_) {
    ESP_LOGW(TAG, "Schedule too large, truncating");
    work_buffer.resize(this->schedule_max_size_);
    this->send_ha_notification_("Schedule truncated (too large)", "Schedule Warning");
  }
  
  // Pad to max size
  while (work_buffer.size() < this->schedule_max_size_) {
    work_buffer.push_back(0);
  }
  
  // Store the schedule
  this->schedule_times_in_minutes_ = work_buffer;
  this->schedule_valid_ = true;
  
  // Save to preferences
  this->save_schedule_to_pref_();
  
  ESP_LOGI(TAG, "Schedule processed successfully");
}

void SchedulableBase::send_ha_notification_(const std::string &message, const std::string &title) {
  ESP_LOGI(TAG, "Notification: [%s] %s", title.c_str(), message.c_str());
  // This would send a notification to Home Assistant
  // Implementation depends on API server setup
}

// ============================================================================
// PREFERENCE MANAGEMENT
// ============================================================================

void SchedulableBase::create_schedule_preference() {
  if (!this->sched_array_pref_) {
    ESP_LOGW(TAG, "No array preference configured");
    this->schedule_valid_ = false;
    this->schedule_empty_ = true;
    return;
  }
  
  this->sched_array_pref_->create_preference(this->get_object_id_hash());
  ESP_LOGV(TAG, "Schedule preference created");
}

void SchedulableBase::load_schedule_from_pref_() {
  if (!this->sched_array_pref_) {
    ESP_LOGW(TAG, "No array preference to load from");
    this->schedule_valid_ = false;
    this->schedule_empty_ = true;
    return;
  }
  
  // Load data
  this->sched_array_pref_->load();
  
  if (!this->sched_array_pref_->is_valid()) {
    ESP_LOGW(TAG, "No valid preference data");
    this->schedule_valid_ = false;
    this->schedule_empty_ = true;
    return;
  }
  
  // Copy data to schedule vector
  uint8_t *buf = this->sched_array_pref_->data();
  std::memcpy(this->schedule_times_in_minutes_.data(), buf, this->sched_array_pref_->size());
  
  // Check for empty schedule (terminator at start)
  this->schedule_empty_ = (this->schedule_times_in_minutes_[0] == 0xFFFF);
  this->schedule_valid_ = true;
  
  ESP_LOGI(TAG, "Schedule loaded from preferences");
}

void SchedulableBase::save_schedule_to_pref_() {
  if (!this->sched_array_pref_) {
    ESP_LOGW(TAG, "No array preference to save to");
    return;
  }
  
  // Copy data to preference
  uint8_t *buf = this->sched_array_pref_->data();
  std::memcpy(buf, this->schedule_times_in_minutes_.data(), this->sched_array_pref_->size());
  
  // Save to NVS
  this->sched_array_pref_->save();
  
  ESP_LOGV(TAG, "Schedule saved to preferences");
}

// ============================================================================
// DERIVED CLASS IMPLEMENTATIONS
// ============================================================================

void StateBasedSchedulable::parse_schedule_entry(const JsonObjectConst &entry,
                                                  std::vector<uint16_t> &work_buffer,
                                                  uint16_t day_offset) {
  // State-based: Extract both "from" and "to" times
  const char* from_str = entry["from"].as<const char*>();
  const char* to_str = entry["to"].as<const char*>();
  
  uint16_t from_minutes = this->time_to_minutes_(from_str) + day_offset;
  uint16_t to_minutes = this->time_to_minutes_(to_str) + day_offset;
  
  const uint16_t SWITCH_STATE_BIT = 0x4000;
  
  // Store ON time
  work_buffer.push_back(from_minutes | SWITCH_STATE_BIT);
  
  // Store OFF time
  work_buffer.push_back(to_minutes & ~SWITCH_STATE_BIT);
}

void StateBasedSchedulable::set_mode(ScheduleMode mode) {
  this->current_mode_ = mode;
  ESP_LOGI(TAG, "Mode changed to: %d", mode);
}

void EventBasedSchedulable::parse_schedule_entry(const JsonObjectConst &entry,
                                                  std::vector<uint16_t> &work_buffer,
                                                  uint16_t day_offset) {
  // Event-based: Extract only "from" time (50% storage savings!)
  const char* from_str = entry["from"].as<const char*>();
  
  uint16_t event_minutes = this->time_to_minutes_(from_str) + day_offset;
  
  const uint16_t SWITCH_STATE_BIT = 0x4000;
  
  // Store event time (always with state bit set)
  work_buffer.push_back(event_minutes | SWITCH_STATE_BIT);
}

void EventBasedSchedulable::set_enabled(bool enabled) {
  this->enabled_ = enabled;
  ESP_LOGI(TAG, "Event-based schedule %s", enabled ? "enabled" : "disabled");
}

} // namespace schedule
} // namespace esphome
