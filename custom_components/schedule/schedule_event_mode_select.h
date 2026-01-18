#pragma once

#include "esphome/components/select/select.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/core/log.h"

namespace esphome {
namespace schedule {

// Forward declaration
class Schedule;

// ScheduleEventModeSelect - Simplified mode select for event-based components
// Only supports Disabled/Enabled (no state-based modes like Manual On/Off)
class ScheduleEventModeSelect : public select::Select, public Component {
 public:
  void set_on_value_callback(std::function<void(const std::string&)> &&callback) {
    this->on_value_callback_ = callback;
  }
  
  void set_schedule(Schedule *schedule) {
    this->schedule_ = schedule;
  }

  void setup() override {
    this->pref_ = global_preferences->make_preference<uint8_t>(this->get_object_id_hash());
    uint8_t index;
    if (!this->pref_.load(&index)) {
      // No saved preference, default to "Enabled"
      this->publish_state("Enabled");
    } else {
      const auto &options = this->traits.get_options();
      if (index < options.size()) {
        this->publish_state(options[index]);
      } else {
        this->publish_state("Enabled");
      }
    }
    // Notify via callback with the published state
    if (this->on_value_callback_ && this->has_state()) {
      this->on_value_callback_(this->current_option());
    }
  }
  
  // Set disabled-only mode (schedule is empty)
  void set_disabled_only_mode(bool disabled_only) {
    if (disabled_only) {
      // Only allow disabled mode
      this->traits.set_options({"Disabled"});
    } else {
      // Allow both modes
      this->traits.set_options({"Disabled", "Enabled"});
    }
    
    // If current mode is not in new options, switch to Disabled
    const auto &options = this->traits.get_options();
    std::string current = this->current_option();
    bool found = false;
    for (const auto &opt : options) {
      if (opt == current) {
        found = true;
        break;
      }
    }
    
    if (!found) {
      this->publish_state("Disabled");
      if (this->on_value_callback_) {
        this->on_value_callback_("Disabled");
      }
    } else {
      // Re-publish current state to notify HA of trait changes
      this->publish_state(current);
    }
  }
  
 protected:
  void control(const std::string &value) override {
    // Save the selected option index
    const auto &options = this->traits.get_options();
    for (uint8_t i = 0; i < options.size(); i++) {
      if (options[i] == value) {
        this->pref_.save(&i);
        break;
      }
    }
    
    this->publish_state(value);
    
    // Trigger callback
    if (this->on_value_callback_) {
      this->on_value_callback_(value);
    }
  }

 private:
  Schedule *schedule_{nullptr};
  ESPPreferenceObject pref_;
  std::function<void(const std::string&)> on_value_callback_;
};

} // namespace schedule
} // namespace esphome
