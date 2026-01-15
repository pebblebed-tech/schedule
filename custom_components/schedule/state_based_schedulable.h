#pragma once

#include "schedule.h"

namespace esphome {
namespace schedule {

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
  
  // No need to override parse_schedule_entry() - base implementation is state-based
  // No need to override get_storage_multiplier() - base implementation returns 2
};

} // namespace schedule
} // namespace esphome
