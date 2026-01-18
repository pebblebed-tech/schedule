#pragma once

#include "schedule.h"

namespace esphome {
namespace schedule {

// Forward declaration
class ScheduleStateModeSelect;

// State-based schedule states (complete state machine for Switch platform)
enum StateBasedScheduleState {
  // Error/Invalid states
  STATE_TIME_INVALID = 0,      // RTC time not synchronized
  STATE_SCHEDULE_INVALID = 1,  // Schedule data is invalid or not available
  STATE_SCHEDULE_EMPTY = 2,    // Schedule is valid but has no events
  
  // Initialization state
  STATE_INIT = 3,              // Initializing schedule operation
  
  // Manual override states
  STATE_MAN_OFF = 4,           // Manual override: forced off
  STATE_MAN_ON = 5,            // Manual override: forced on
  
  // Temporary mode states
  STATE_EARLY_OFF = 6,         // Early-off mode active (temporary override)
  STATE_BOOST_ON = 7,          // Boost mode active (temporary override)
  
  // Auto mode states
  STATE_AUTO_ON = 8,           // Auto mode: schedule indicates ON
  STATE_AUTO_OFF = 9           // Auto mode: schedule indicates OFF
};

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
      
      // Check all prerequisites (returns error code)
      auto prereq_error = this->check_prerequisites_();
      
      // Handle prerequisite errors with state transitions
      if (prereq_error != PREREQ_OK) {
        // Map prerequisite errors to states
        if (prereq_error == PREREQ_TIME_INVALID) {
          if (this->current_state_ != STATE_TIME_INVALID) {
            ESP_LOGW("schedule", "Time is not valid, schedule operations paused");
            this->current_state_ = STATE_TIME_INVALID;
          }
        } else if (prereq_error == PREREQ_SCHEDULE_INVALID) {
          if (this->current_state_ != STATE_SCHEDULE_INVALID) {
            ESP_LOGW("schedule", "Schedule is not valid and Home Assistant not connected");
            this->current_state_ = STATE_SCHEDULE_INVALID;
          }
        } else if (prereq_error == PREREQ_SCHEDULE_EMPTY) {
          if (this->current_state_ != STATE_SCHEDULE_EMPTY) {
            ESP_LOGI("schedule", "Schedule is empty, no events to process");
            this->current_state_ = STATE_SCHEDULE_EMPTY;
          }
        }
        // Handle state change for error states
        this->handle_state_change_();
        return;
      }
      
      // Prerequisites OK - transition from error states to INIT if needed
      if ((this->current_state_ == STATE_TIME_INVALID || 
           this->current_state_ == STATE_SCHEDULE_INVALID || 
           this->current_state_ == STATE_SCHEDULE_EMPTY) && 
          prereq_error == PREREQ_OK) {
        this->current_state_ = STATE_INIT;
        ESP_LOGV("schedule", "Prerequisites met, transitioning to INIT state");
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
  void initialize_sensor_last_on_values_(int16_t current_event_index);
  std::string create_event_string_(uint16_t event_raw);
  int mode_to_state_(ScheduleMode mode, bool event_on);
  bool should_reset_to_auto_(int state, bool event_on);
  int get_state_after_mode_reset_(bool event_on);
  
  /** Force reinitialization after schedule update */
  void force_reinitialize() {
    ESP_LOGD("schedule.state_based", "Forcing reinitialization");
    this->current_state_ = STATE_INIT;
    this->processed_state_ = STATE_INIT;
  }
  
  /** Update mode select options when schedule empty state changes */
  void on_schedule_empty_changed(bool is_empty) override;
  
  //============================================================================
  // STATE-SPECIFIC MEMBER VARIABLES
  //============================================================================
  ScheduleStateModeSelect *mode_select_{nullptr};
  ScheduleMode current_mode_{SCHEDULE_MODE_MANUAL_OFF};
  bool event_switch_state_{false};  // Current ON/OFF state from schedule
  
  // State machine
  int current_state_{STATE_INIT};
  int processed_state_{STATE_INIT};
};

} // namespace schedule
} // namespace esphome
