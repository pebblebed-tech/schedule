#pragma once
#include "esphome/core/component.h"
#include "esphome/components/button/button.h"
#include "../event_based_schedulable.h"

namespace esphome {
namespace schedule {

// ScheduleButton class - button that triggers on scheduled events
class ScheduleButton : public button::Button, public EventBasedSchedulable {
 public:
  void setup() override {
    // Sync entity info from EntityBase (via Button) to Schedule base class
    this->sync_from_entity(this->get_object_id(), this->get_name().c_str());
    // Call EventBasedSchedulable's setup
    EventBasedSchedulable::setup();
  }
  
  void dump_config() override;
  
 protected:
  // Platform-specific implementation of scheduled state application
  // For buttons, "on" means press the button
  void apply_scheduled_state(bool on) override {
    if (on) {
      // Trigger button press when schedule event occurs
      this->press();
    }
    // Note: For event-based components, "on" is always true at the event time
    // There is no "off" state for buttons
  }
  
  void press_action() override {
    // User pressed the button manually - no action needed
    // The button press is already published by Button class
  }
};

} // namespace schedule
} // namespace esphome
