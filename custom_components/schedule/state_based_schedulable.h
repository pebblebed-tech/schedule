#pragma once

#include "schedule.h"

namespace esphome {
namespace schedule {

// Forward declaration
class ScheduleStateModeSelect;

// Schedule mode enum for state-based components
enum ScheduleMode {
  SCHEDULE_MODE_MANUAL_OFF = 0,
  SCHEDULE_MODE_EARLY_OFF = 1,
  SCHEDULE_MODE_AUTO = 2,
  SCHEDULE_MODE_MANUAL_ON = 3,
  SCHEDULE_MODE_BOOST_ON = 4
};

/**
 * StateBasedSchedulable - For components that maintain continuous state
 * 
 * This is essentially an alias/marker for the default Schedule behavior.
 * The Schedule base class already implements state-based storage by default.
 * 
 * Storage format: [ON_TIME, OFF_TIME] pairs
 * - Each entry uses 2 x uint16_t = 4 bytes
 * - ON_TIME:  bit 14 = 1, bits 0-13 = time in minutes
 * - OFF_TIME: bit 14 = 0, bits 0-13 = time in minutes
 * 
 * Examples: Switch, Climate, Light, Fan
 * 
 * Usage in YAML:
 *   switch:
 *     - platform: schedule
 *       max_schedule_entries: 50  # Needs 204 bytes (50 * 2 * 2 + 4)
 */
class StateBasedSchedulable : public Schedule {
 public:
  StateBasedSchedulable() = default;
  
  /** Returns STORAGE_TYPE_STATE_BASED (default) */
  ScheduleStorageType get_storage_type() const override {
    return STORAGE_TYPE_STATE_BASED;
  }
  
  /** State-based components use the full state machine with modes */
  void loop() override {
    uint32_t now = millis();
    
    // Periodic logging every 60 seconds
    if (now - this->last_state_log_time_ >= 60000) {
      this->last_state_log_time_ = now;
      this->log_state_flags_();
      ESP_LOGV("schedule", "Schedule loop state: %d", this->current_state_);
      ESP_LOGV("schedule", "Current mode: %d", this->current_mode_);
    }

    // Normal operation - run every second
    if (now - this->last_time_check_ >= 1000) {
      this->last_time_check_ = now;
      
      // Check all prerequisites (time, connection, schedule validity)
      if (!this->check_prerequisites_()) {
        // Handle state change for error states if needed
        this->handle_state_change_();
        return;  // Prerequisites not met, state already set by check_prerequisites_()
      }
      
      // Handle initialization state
      if (this->current_state_ == STATE_INIT) {
        this->initialize_schedule_operation_();
        ESP_LOGI("schedule", "Normal operation, mode = %d State = %d", this->current_mode_, this->current_state_);
        return;  // Exit to allow next iteration to handle normal operation
      }
      
      // Skip if still in error states
      if (this->current_state_ == STATE_TIME_INVALID ||
          this->current_state_ == STATE_SCHEDULE_INVALID ||
          this->current_state_ == STATE_SCHEDULE_EMPTY) {
        return;
      }
      
      // Update current state based on mode and event state
      this->current_state_ = this->mode_to_state_(this->current_mode_, this->event_switch_state_);
      
      // Handle state changes - this will only process if state differs from processed_state_
      this->handle_state_change_();
      
      // Check and advance schedule events if time has reached next event
      this->check_and_advance_events_();
    }
  }
  
  //============================================================================
  // MODE MANAGEMENT (state-based only)
  //============================================================================
  void set_mode_select(ScheduleStateModeSelect *mode_select);
  void set_mode_option(ScheduleMode mode);
  void on_mode_changed(const std::string &mode);
  
  // Logging - state-based format (ON/OFF pairs)
  void log_schedule_data() override;
  
  // No need to override parse_schedule_entry() - base implementation is state-based
  // No need to override get_storage_multiplier() - base implementation returns 2
  
 protected:
  //============================================================================
  // VIRTUAL METHOD OVERRIDES FROM BASE CLASS
  //============================================================================
  void advance_to_next_event_() override;
  void check_and_advance_events_() override;
  void initialize_schedule_operation_() override;
  
  //============================================================================
  // STATE MACHINE METHODS (state-based only)
  //============================================================================
  void handle_state_change_();  // Process state transitions and update outputs
  
  //============================================================================
  // STATE MACHINE HELPER METHODS (state-based only)
  //============================================================================
  ScheduleState mode_to_state_(ScheduleMode mode, bool event_on);
  bool should_reset_to_auto_(ScheduleState state, bool event_on);
  ScheduleState get_state_after_mode_reset_(bool event_on);
  
  //============================================================================
  // STATE-SPECIFIC MEMBER VARIABLES
  //============================================================================
  ScheduleStateModeSelect *mode_select_{nullptr};
  ScheduleMode current_mode_{SCHEDULE_MODE_MANUAL_OFF};
  bool event_switch_state_{false};  // Current ON/OFF state from schedule
};

} // namespace schedule
} // namespace esphome
