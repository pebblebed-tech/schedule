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
    this->schedule_is_empty_ = disabled_only;
    
    // Don't dynamically change traits - keep both options available
    // Just switch to appropriate state based on schedule status
    
    std::string current = this->has_state() ? this->current_option() : "";
    std::string new_state;
    
    if (disabled_only) {
      // Schedule is empty, force to Disabled
      new_state = "Disabled";
    } else {
      // Schedule has entries
      if (current == "Disabled" || current.empty()) {
        // Auto-enable when schedule becomes available
        new_state = "Enabled";
      } else {
        // Keep current selection if valid
        new_state = current;
      }
    }
    
    // Publish the state
    this->publish_state(new_state);
    
    if (this->on_value_callback_) {
      this->on_value_callback_(new_state);
    }
  }
  
 protected:
  void control(const std::string &value) override {
    // Check if user is trying to enable when schedule is empty
    if (this->schedule_is_empty_ && value == "Enabled") {
      ESP_LOGW("schedule.mode_select", "Cannot enable: schedule is empty");
      
      // Publish the rejected value first, then the correct value
      // This forces HA to see a state change
      this->publish_state("Enabled");

      
      // Send notification to Home Assistant
      if (this->schedule_ != nullptr) {
        this->schedule_->send_notification("Cannot enable schedule mode", "Schedule is empty - no events to trigger");
      }
      
      // Trigger callback with Disabled to ensure internal consistency
      if (this->on_value_callback_) {
        this->on_value_callback_("Disabled");
      }
      this->publish_state("Disabled");
      
      return;
            
    }
    
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
  bool schedule_is_empty_{true};
};

} // namespace schedule
} // namespace esphome
