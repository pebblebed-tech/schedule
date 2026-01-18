#pragma once

#include "schedule.h"

namespace esphome {
namespace schedule {

// Forward declaration
class ScheduleEventModeSelect;

// Event-based schedule states (complete state machine for Button platform)
enum EventBasedScheduleState {
  // Error/Invalid states
  STATE_EVENT_TIME_INVALID = 0,      // RTC time not synchronized
  STATE_EVENT_SCHEDULE_INVALID = 1,  // Schedule data is invalid or not available
  STATE_EVENT_SCHEDULE_EMPTY = 2,    // Schedule is valid but has no events
  
  // Initialization state
  STATE_EVENT_INIT = 3,              // Initializing schedule operation
  
  // Operational states
  STATE_EVENT_DISABLED = 4,          // Mode select set to disabled
  STATE_EVENT_READY = 5              // Ready to process events (enabled)
};

/**
 * EventBasedSchedulable - For components that only need event triggers
 * 
 * Storage format: [EVENT_TIME] singles (no OFF times)
 * - Each entry uses 1 x uint16_t = 2 bytes
 * - EVENT_TIME: bit 14 = 1, bits 0-13 = time in minutes
 * - Terminator: 2 x uint16_t (0xFFFF, 0xFFFF) for consistency
 * 
 * **~50% storage savings compared to state-based!**
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
 *       max_schedule_entries: 50  # Needs only 104 bytes (50 * 1 * 2 + 4)
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
  
  //============================================================================
  // MODE MANAGEMENT (event-based - simplified Disabled/Enabled only)
  //============================================================================
  
  /** Set the mode select component (simplified - only Disabled/Enabled) */
  void set_mode_select(ScheduleEventModeSelect *mode_select) {
    this->mode_select_ = mode_select;
  }
  
  // Logging - event-based format (single events, not ON/OFF pairs)
  void log_schedule_data() override;
  
  /** Event-based components use simplified loop without state machine 
   * Only checks for event times and triggers apply_scheduled_state(true)
   */
  void loop() override {
    uint32_t now = millis();
    
    // Periodic logging every 60 seconds
    if (now - this->last_state_log_time_ >= 60000) {
      this->last_state_log_time_ = now;
      this->log_state_flags_();
      ESP_LOGV("schedule", "Event-based loop state: %d", this->current_state_);
    }
    
    // Run every second
    if (now - this->last_time_check_ >= 1000) {
      this->last_time_check_ = now;
      
      // Check prerequisites (returns error code)
      auto prereq_error = this->check_prerequisites_();
      
      // Handle prerequisite errors with state transitions
      if (prereq_error == PREREQ_TIME_INVALID) {
        if (this->current_state_ != STATE_EVENT_TIME_INVALID) {
          ESP_LOGW("schedule", "Time is not valid, schedule operations paused");
          this->current_state_ = STATE_EVENT_TIME_INVALID;
        }
        this->display_current_next_events_("Time Invalid", "Time Invalid");
        return;
      }
      
      if (prereq_error == PREREQ_SCHEDULE_INVALID) {
        if (this->current_state_ != STATE_EVENT_SCHEDULE_INVALID) {
          ESP_LOGW("schedule", "Schedule is not valid");
          this->current_state_ = STATE_EVENT_SCHEDULE_INVALID;
        }
        this->display_current_next_events_("Schedule Invalid", "Schedule Invalid");
        return;
      }
      
      if (prereq_error == PREREQ_SCHEDULE_EMPTY) {
        if (this->current_state_ != STATE_EVENT_SCHEDULE_EMPTY) {
          ESP_LOGI("schedule", "Schedule is empty, no events to process");
          this->current_state_ = STATE_EVENT_SCHEDULE_EMPTY;
        }
        this->display_current_next_events_("Schedule Empty", "Schedule Empty");
        return;
      }
      
      // Prerequisites OK - transition from error states to INIT if needed
      if ((this->current_state_ == STATE_EVENT_TIME_INVALID || 
           this->current_state_ == STATE_EVENT_SCHEDULE_INVALID || 
           this->current_state_ == STATE_EVENT_SCHEDULE_EMPTY) && 
          prereq_error == PREREQ_OK) {
        this->current_state_ = STATE_EVENT_INIT;
        ESP_LOGV("schedule", "Prerequisites met, transitioning to INIT state");
      }
      
      // Handle initialization
      if (this->current_state_ == STATE_EVENT_INIT) {
        // Call base class initialization to find current/next events
        this->initialize_schedule_operation_();
        
        // Transition to event-ready state and mark that we need initial UI update
        this->current_state_ = STATE_EVENT_READY;
        this->needs_initial_ui_update_ = true;
        return;
      }
      
      // Check if mode select disabled the schedule
      if (this->current_state_ == STATE_EVENT_DISABLED) {
        this->display_current_next_events_("Disabled", "Disabled");
        return;
      }
      
      // Normal operation - process events
      if (this->current_state_ == STATE_EVENT_READY) {
        // Force UI update on first loop after initialization
        if (this->needs_initial_ui_update_) {
          this->update_event_based_ui_();
          this->needs_initial_ui_update_ = false;
        }
        
        // Check if we should trigger the event and update UI if events changed
        int old_index = this->current_event_index_;
        this->check_and_advance_events_();
        if (old_index != this->current_event_index_) {
          this->update_event_based_ui_();
        }
      }
    }
  }
  
 protected:
  //============================================================================
  // VIRTUAL METHOD OVERRIDES FROM BASE CLASS
  //============================================================================
  
  void advance_to_next_event_() override;
  void check_and_advance_events_() override;
  void initialize_schedule_operation_() override;
  
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
  
  //============================================================================
  // EVENT-BASED HELPER METHODS
  //============================================================================
  
  /** Update event-based UI (sensors and displays) */
  void update_event_based_ui_();
  
  /** Force reinitialization after schedule update */
  void force_reinitialize() {
    ESP_LOGD("schedule.event_based", "Forcing reinitialization");
    this->current_state_ = STATE_EVENT_INIT;
    this->needs_initial_ui_update_ = true;
  }
  
  /** Update mode select options when schedule empty state changes */
  void on_schedule_empty_changed(bool is_empty) override;
  
 private:
  //============================================================================
  // EVENT-BASED MEMBER VARIABLES
  //============================================================================
  
  ScheduleEventModeSelect *mode_select_{nullptr};
  
  // Event-based state machine
  int current_state_{STATE_EVENT_INIT};
  
  // Flag to ensure UI is updated after initialization
  bool needs_initial_ui_update_{false};
};

} // namespace schedule
} // namespace esphome
