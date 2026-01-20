#include "esphome.h"
#include "schedule_button.h"
#include "../schedule.h"

namespace esphome {
namespace schedule {

static const char *const TAG = "schedule.button";

void ScheduleButton::dump_config() {
  LOG_BUTTON("", "Schedule Button", this);
  // Call base schedule configuration
  this->dump_config_base();
}

// Note: setup() is defined inline in the header
// Note: loop() is handled by EventBasedSchedulable base class
// Note: apply_scheduled_state() and press_action() are defined inline in the header

} // namespace schedule
} // namespace esphome
