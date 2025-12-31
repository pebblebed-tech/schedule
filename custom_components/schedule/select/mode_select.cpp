#include "mode_select.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace schedule {

static const char *const TAG = "schedule.select";

void ScheduleModeSelect::setup() {
  std::string value;
  this->pref_ = global_preferences->make_preference<uint8_t>(this->get_object_id_hash());
  
  uint8_t index = 0;
  if (!this->pref_.load(&index)) {
    // Preference doesn't exist, set default to "Manual Off" (index 0)
    value = "Manual Off";
    ESP_LOGD(TAG, "State not found in preferences, defaulting to '%s'", value.c_str());
  } else {
    // Load the saved state
    if (index < this->traits.get_options().size()) {
      value = this->traits.get_options()[index];
      ESP_LOGD(TAG, "Restored state from preferences: '%s'", value.c_str());
    } else {
      // Invalid index, use default
      value = "Manual Off";
      ESP_LOGW(TAG, "Invalid index %d in preferences, defaulting to '%s'", index, value.c_str());
    }
  }
  
  this->publish_state(value);
}

void ScheduleModeSelect::dump_config() {
  LOG_SELECT("", "Schedule Mode Select", this);
}

void ScheduleModeSelect::control(const std::string &value) {
  // Publish the new state
  this->publish_state(value);
  
  // Save to preferences
  const auto &options = this->traits.get_options();
  auto it = std::find(options.begin(), options.end(), value);
  if (it != options.end()) {
    uint8_t index = std::distance(options.begin(), it);
    this->pref_.save(&index);
    ESP_LOGD(TAG, "Saved mode '%s' (index %d) to preferences", value.c_str(), index);
  }
  
  // Log the state change
  ESP_LOGI(TAG, "Mode select changed to: %s", value.c_str());
  
  // Notify the schedule component about the mode change
  if (this->parent_ != nullptr) {
    this->parent_->on_mode_changed(value);
  }
}

}  // namespace schedule
}  // namespace esphome
