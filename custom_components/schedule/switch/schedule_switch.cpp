#include "esphome.h"
#include "switch.h"
#include "../schedule.h"

namespace esphome {
namespace schedule {

static const char *const TAG = "schedule.switch";

void ScheduleSwitch::write_state(bool state) {
  this->publish_state(state);
  ESP_LOGI(TAG, "Schedule switch state changed to: %s", state ? "ON" : "OFF");
  
  // Update the switch indicator (this IS the schedule)
  this->update_switch_indicator(state);
}

void ScheduleSwitch::loop() {
  // Call the schedule state machine update
  this->update_schedule_state();
}

void ScheduleSwitch::dump_config() {
  LOG_SWITCH("", "Schedule Switch", this);
  // Call base schedule configuration
  this->dump_config_base();
}

// Note: setup() is defined inline in the header

} // namespace schedule
} // namespace esphome
