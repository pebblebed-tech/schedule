#pragma once

#include "schedule.h"

namespace esphome {
namespace schedule {

/**
 * EventBasedSchedulable - For components that only need event triggers
 * 
 * Storage format: [EVENT_TIME] singles (no OFF times)
 * - Each entry uses 1 x uint16_t = 2 bytes
 * - EVENT_TIME: bit 14 = 1, bits 0-13 = time in minutes
 * 
 * **50% storage savings compared to state-based!**
 * 
 * Examples: Cover, Lock, Button, Script
 * These components only care about when an event triggers, not maintaining
 * continuous ON/OFF state. For example:
 * - Cover: only needs to know WHEN to move to a position
 * - Lock: only needs to know WHEN to lock/unlock
 * 
 * Usage in YAML:
 *   cover:
 *     - platform: schedule
 *       max_schedule_entries: 50  # Needs only 102 bytes (50 * 1 * 2 + 2)
 */
class EventBasedSchedulable : public Schedule {
 public:
  EventBasedSchedulable() = default;
  
  /** Returns STORAGE_TYPE_EVENT_BASED */
  ScheduleStorageType get_storage_type() const override {
    return STORAGE_TYPE_EVENT_BASED;
  }
  
  /** Get storage multiplier for array size calculation */
  size_t get_storage_multiplier() const override {
    return 1;  // EVENT only per entry (50% savings!)
  }
  
  /** Event-based components use simplified loop without state machine 
   * Only checks for event times and triggers apply_scheduled_state(true)
   */
  void loop() override {
    uint32_t now = millis();
    
    // Run every second
    if (now - this->last_time_check_ >= 1000) {
      this->last_time_check_ = now;
      
      // Check prerequisites (time, connection, schedule validity)
      if (!this->check_prerequisites_()) {
        return;
      }
      
      // Handle initialization
      if (this->current_state_ == STATE_INIT) {
        this->initialize_schedule_operation_();
        return;
      }
      
      // Skip if in error states
      if (this->current_state_ == STATE_TIME_INVALID ||
          this->current_state_ == STATE_SCHEDULE_INVALID ||
          this->current_state_ == STATE_SCHEDULE_EMPTY) {
        return;
      }
      
      // For event-based: just check if we should trigger the event
      // No state machine - either enabled or disabled
      this->check_and_advance_events_();
    }
  }
  
 protected:
  /** Parse schedule entry for event-based storage
   * 
   * Extracts only "from" time as [EVENT] single:
   * - from time: bits 0-13 = minutes, bit 14 = 1 (trigger event)
   * - "to" time is ignored/not stored
   */
  void parse_schedule_entry(const JsonObjectConst &entry, 
                           std::vector<uint16_t> &work_buffer,
                           uint16_t day_offset) override {
    // Event-based: Extract only "from" time (ignore "to")
    uint16_t event_time = this->timeToMinutes_(entry["from"]) + 0x4000;  // Set bit 14
    
    // Add only the event time (no OFF time)
    work_buffer.push_back(event_time + day_offset);
    
    // NOTE: "to" time from HA schedule is ignored
    // The component only cares about when the event triggers
  }

  /** Log schedule data in event-based format (single events, not ON/OFF pairs) */
  void log_schedule_data() override {
    ESP_LOGI("schedule", "Event-Based Schedule Data:");
    ESP_LOGI("schedule", "Current Week Minute: %u, Max Entries: %u", this->time_, this->schedule_max_entries_);

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
      ESP_LOGI("schedule", "  Entry %u: EVENT at %s:%02u:%02u (raw: 0x%04X)", 
               entry_count, day_names[day], hour, minute, event_time);

      index++;
      entry_count++;
    }

    ESP_LOGI("schedule", "Total Entries: %u", entry_count);
  }
};

} // namespace schedule
} // namespace esphome
