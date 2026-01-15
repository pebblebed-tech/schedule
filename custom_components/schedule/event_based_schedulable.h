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
};

} // namespace schedule
} // namespace esphome
