#pragma once
// ============================================================================
// REFERENCE IMPLEMENTATION - NOT COMPILED
// ============================================================================
// This file shows an alternative architecture approach using a separate
// SchedulableBase class. The actual implementation uses virtual methods
// in the Schedule class directly to avoid diamond inheritance issues.
// 
// This file is kept for documentation purposes only.
// ============================================================================

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include "esphome/components/time/real_time_clock.h"
#include <vector>
#include <string>

namespace esphome {
namespace schedule {

// Forward declarations
class DataSensor;
class ArrayPreferenceBase;

/**
 * SchedulableBase - Alternative architecture approach
 * 
 * This represents what a separate base class would look like if we wanted
 * to separate scheduling logic from the Schedule component. However, this
 * approach was not used in the actual implementation due to:
 * 
 * 1. Diamond inheritance problems (Component -> SchedulableBase -> Schedule)
 * 2. More complex inheritance hierarchy
 * 3. More code duplication
 * 4. Harder to extend for new platforms
 * 
 * Instead, the actual implementation uses virtual methods directly in the
 * Schedule class, which is simpler and more maintainable.
 */
class SchedulableBase : public Component {
 public:
  SchedulableBase() = default;
  virtual ~SchedulableBase() = default;

  // =========================================================================
  // VIRTUAL INTERFACE - Must be implemented by derived classes
  // =========================================================================
  
  /**
   * Get the storage type for this schedulable component.
   * 
   * @return STORAGE_TYPE_STATE_BASED for components that maintain state (Switch, Climate)
   *         STORAGE_TYPE_EVENT_BASED for event-triggered components (Button, Cover)
   */
  virtual ScheduleStorageType get_storage_type() const = 0;
  
  /**
   * Get the storage multiplier for array size calculations.
   * 
   * @return 2 for state-based (ON + OFF per entry)
   *         1 for event-based (EVENT only per entry)
   */
  virtual size_t get_storage_multiplier() const = 0;
  
  /**
   * Parse a single schedule entry from Home Assistant JSON.
   * 
   * State-based: Extracts "from" and "to" times, stores as [ON_TIME, OFF_TIME]
   * Event-based: Extracts only "from" time, stores as [EVENT_TIME]
   * 
   * @param entry The JSON entry from HA schedule
   * @param work_buffer Buffer to append parsed times to
   * @param day_offset Minutes offset for the current day (0, 1440, 2880, etc.)
   */
  virtual void parse_schedule_entry(const JsonObjectConst &entry,
                                    std::vector<uint16_t> &work_buffer,
                                    uint16_t day_offset) = 0;
  
  /**
   * Apply the scheduled ON/OFF state to the platform component.
   * 
   * This is where platform-specific logic goes:
   * - Switch: publish_state(on)
   * - Climate: set mode and temperature
   * - Cover: move to position
   * - Button: press if on=true
   * 
   * @param on true for ON/triggered state, false for OFF state
   */
  virtual void apply_scheduled_state(bool on) = 0;

  // =========================================================================
  // COMPONENT LIFECYCLE
  // =========================================================================
  
  /**
   * Setup the schedulable component.
   * Loads preferences, initializes state machine, etc.
   */
  void setup() override;
  
  /**
   * Main loop - updates state machine and checks for events.
   */
  void loop() override;
  
  /**
   * Get setup priority.
   */
  float get_setup_priority() const override {
    return esphome::setup_priority::LATE;
  }

  // =========================================================================
  // CONFIGURATION METHODS
  // =========================================================================
  
  /**
   * Set the Home Assistant schedule entity ID.
   */
  void set_schedule_entity_id(const std::string &entity_id) {
    this->ha_schedule_entity_id_ = entity_id;
  }
  
  /**
   * Set maximum schedule entries.
   */
  void set_max_schedule_entries(size_t entries);
  
  /**
   * Set time component for schedule timing.
   */
  void set_time(time::RealTimeClock *time) {
    this->time_ = time;
  }
  
  /**
   * Set whether to update schedule on HA reconnect.
   */
  void set_update_on_reconnect(bool update) {
    this->update_on_reconnect_ = update;
  }
  
  /**
   * Register a data sensor with this schedule.
   */
  void register_data_sensor(DataSensor *sensor);
  
  /**
   * Set array preference for schedule storage.
   */
  void set_array_preference(ArrayPreferenceBase *pref) {
    this->sched_array_pref_ = pref;
  }

  // =========================================================================
  // INTERNAL IDENTIFICATION
  // =========================================================================
  
  /**
   * Sync entity info from the platform component.
   * Called by platform implementations in their setup().
   */
  void sync_from_entity(const std::string &object_id, const std::string &name) {
    this->object_id_ = object_id;
    this->name_ = name;
  }
  
  const std::string& get_object_id() const { return this->object_id_; }
  uint32_t get_object_id_hash() const { return fnv1_hash(this->object_id_); }

 protected:
  // =========================================================================
  // STATE MACHINE METHODS
  // =========================================================================
  
  /**
   * Check all prerequisites for schedule operation.
   * @return Error code or PREREQ_OK
   */
  enum PrerequisiteError {
    PREREQ_OK = 0,
    PREREQ_TIME_INVALID = 1,
    PREREQ_SCHEDULE_INVALID = 2,
    PREREQ_SCHEDULE_EMPTY = 3
  };
  PrerequisiteError check_prerequisites_();
  
  /**
   * Initialize schedule operation - find current/next events.
   */
  void initialize_schedule_operation_();
  
  /**
   * Check if it's time to advance to the next event.
   */
  bool should_advance_to_next_event_(uint16_t current_time_minutes);
  
  /**
   * Advance to the next scheduled event.
   */
  void advance_to_next_event_();
  
  /**
   * Check and advance events if needed.
   */
  void check_and_advance_events_();

  // =========================================================================
  // HELPER METHODS
  // =========================================================================
  
  /**
   * Check if RTC time is valid.
   */
  void check_rtc_time_valid_();
  
  /**
   * Check Home Assistant API connection.
   */
  void check_ha_connection_();
  
  /**
   * Convert time string to minutes.
   */
  uint16_t time_to_minutes_(const char* time_str);
  
  /**
   * Get current week minutes (Monday 00:00 = 0).
   */
  uint16_t get_current_week_minutes_();
  
  /**
   * Format event time for display.
   */
  std::string format_event_time_(uint16_t time_minutes);

  // =========================================================================
  // HOME ASSISTANT INTEGRATION
  // =========================================================================
  
  /**
   * Request schedule from Home Assistant.
   */
  void request_schedule();
  
  /**
   * Process schedule data from Home Assistant.
   */
  void process_schedule_(const JsonObjectConst &response);
  
  /**
   * Send notification to Home Assistant.
   */
  void send_ha_notification_(const std::string &message, const std::string &title);

  // =========================================================================
  // PREFERENCE MANAGEMENT
  // =========================================================================
  
  /**
   * Create schedule preference in NVS.
   */
  void create_schedule_preference();
  
  /**
   * Load schedule from preferences.
   */
  void load_schedule_from_pref_();
  
  /**
   * Save schedule to preferences.
   */
  void save_schedule_to_pref_();

  // =========================================================================
  // MEMBER VARIABLES
  // =========================================================================
  
  // Entity identification
  std::string object_id_;
  std::string name_;
  
  // Schedule configuration
  std::string ha_schedule_entity_id_;
  size_t schedule_max_entries_{0};
  size_t schedule_max_size_{0};
  bool update_on_reconnect_{false};
  
  // Schedule data
  std::vector<uint16_t> schedule_times_in_minutes_;
  std::vector<DataSensor*> data_sensors_;
  ArrayPreferenceBase *sched_array_pref_{nullptr};
  
  // Time component
  time::RealTimeClock *time_{nullptr};
  
  // Status flags
  bool ha_connected_{false};
  bool rtc_time_valid_{false};
  bool schedule_valid_{false};
  bool schedule_empty_{true};
  
  // Event tracking
  uint16_t current_event_raw_{0};
  uint16_t next_event_raw_{0};
  int16_t current_event_index_{-1};
  int16_t next_event_index_{-1};
  
  // Timing
  uint32_t last_time_check_{0};
  uint32_t last_connection_check_{0};
};

/**
 * StateBasedSchedulable - For components with continuous state
 * 
 * This would extend SchedulableBase for state-based components.
 * In the actual implementation, this inherits from Schedule instead.
 */
class StateBasedSchedulable : public SchedulableBase {
 public:
  ScheduleStorageType get_storage_type() const override {
    return STORAGE_TYPE_STATE_BASED;
  }
  
  size_t get_storage_multiplier() const override {
    return 2;  // ON + OFF per entry
  }
  
  void parse_schedule_entry(const JsonObjectConst &entry,
                            std::vector<uint16_t> &work_buffer,
                            uint16_t day_offset) override;
  
  // State-based specific methods
  void set_mode(ScheduleMode mode);
  ScheduleMode get_mode() const { return current_mode_; }
  
 protected:
  ScheduleMode current_mode_{SCHEDULE_MODE_MANUAL_OFF};
  StateBasedScheduleState current_state_{STATE_TIME_INVALID};
};

/**
 * EventBasedSchedulable - For event-triggered components
 * 
 * This would extend SchedulableBase for event-based components.
 * In the actual implementation, this inherits from Schedule instead.
 */
class EventBasedSchedulable : public SchedulableBase {
 public:
  ScheduleStorageType get_storage_type() const override {
    return STORAGE_TYPE_EVENT_BASED;
  }
  
  size_t get_storage_multiplier() const override {
    return 1;  // EVENT only per entry (50% savings!)
  }
  
  void parse_schedule_entry(const JsonObjectConst &entry,
                            std::vector<uint16_t> &work_buffer,
                            uint16_t day_offset) override;
  
  // Event-based specific methods
  void set_enabled(bool enabled);
  bool is_enabled() const { return enabled_; }
  
 protected:
  bool enabled_{true};
  EventBasedScheduleState current_state_{STATE_EVENT_TIME_INVALID};
};

} // namespace schedule
} // namespace esphome
