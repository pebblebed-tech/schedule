#pragma once

#include "esphome/components/select/select.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/core/log.h"

namespace esphome {
namespace schedule {

// Forward declaration
class Schedule;

class ScheduleStateModeSelect : public select::Select, public Component {
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
      // No saved preference, default to "Manual Off"
      this->publish_state("Manual Off");
    } else {
      const auto &options = this->traits.get_options();
      if (index < options.size()) {
        this->publish_state(options[index]);
      } else {
        this->publish_state("Manual Off");
      }
    }
    // Notify via callback with the published state
    if (this->on_value_callback_ && this->has_state()) {
      this->on_value_callback_(this->current_option());
    }
  }
  

  // Set manual-only mode (schedule is empty)
  void set_manual_only_mode(bool manual_only) {
    this->schedule_is_empty_ = manual_only;
    
    // Don't dynamically change traits - keep all 5 options available
    // Just switch to appropriate state based on schedule status
    
    std::string current = this->has_state() ? this->current_option() : "";
    std::string new_state;
    
    if (manual_only) {
      // Schedule is empty - if in schedule-dependent mode, switch to Manual Off
      if (current == "Auto" || current == "Early Off" || current == "Boost On" || current.empty()) {
        new_state = "Manual Off";
      } else {
        // Keep Manual Off or Manual On
        new_state = current;
      }
    } else {
      // Schedule has entries - keep current selection
      new_state = current.empty() ? "Manual Off" : current;
    }
    
    // Publish the state
    this->publish_state(new_state);
    
    if (this->on_value_callback_) {
      this->on_value_callback_(new_state);
    }
  }
  
 
 protected:
  void control(const std::string &value) override {
    // Check if user is trying to select a schedule-dependent mode when schedule is empty
    if (this->schedule_is_empty_ && (value == "Auto" || value == "Early Off" || value == "Boost On")) {
      ESP_LOGW("schedule.mode_select", "Cannot select '%s': schedule is empty", value.c_str());
   
      // Publish the rejected value first, then the correct value
      // This forces HA to see a state change
      this->publish_state(value);

      
      // Send notification to Home Assistant
      if (this->schedule_ != nullptr) {
        this->schedule_->send_notification(
          "Cannot select " + value + " mode",
          "Schedule is empty - only Manual Off and Manual On are available"
        );
      }
      
      // Trigger callback with current state to ensure internal consistency
      if (this->on_value_callback_) {
        this->on_value_callback_("Manual Off");
      }
      // this->publish_state(current);
      this->publish_state("Manual Off");
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
    
    // Notify via callback
    if (this->on_value_callback_) {
      this->on_value_callback_(value);
    }
  }

  Schedule *schedule_{nullptr};
  ESPPreferenceObject pref_;
  std::function<void(const std::string&)> on_value_callback_;
  bool schedule_is_empty_{true};
};;

}  // namespace schedule
}  // namespace esphome
