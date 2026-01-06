#pragma once

#include "esphome/components/select/select.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/core/log.h"

namespace esphome {
namespace schedule {

// Forward declaration
class Schedule;

class ScheduleModeSelect : public select::Select, public Component {
 public:
  void set_on_value_callback(std::function<void(const std::string&)> &&callback) {
    this->on_value_callback_ = callback;
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
    
    // Notify via callback
    if (this->on_value_callback_) {
      this->on_value_callback_(value);
    }
  }

  ESPPreferenceObject pref_;
  std::function<void(const std::string&)> on_value_callback_;
};

}  // namespace schedule
}  // namespace esphome
