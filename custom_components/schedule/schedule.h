#pragma once
#include "esphome.h"
#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include "esphome/components/api/homeassistant_service.h"
#include "esphome/components/json/json_util.h"
#include <vector>
#include <string>

namespace esphome {
namespace schedule {

// Convert macro into a compile-time constant
constexpr std::size_t SCHEDULE_MAX_SIZE = static_cast<std::size_t>(SCHEDULE_COMPONENT_MAX_SIZE);

class Schedule : public EntityBase, public Component  {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return esphome::setup_priority::LATE; }
  void set_schedule_entity_id(const std::string &ha_schedule_entity_id);
  void set_max_schedule_size(size_t size);
   /// Trigger a schedule.get_schedule request.
  void request_schedule();
   void load_schedule_from_pref_();
  void save_schedule_to_pref_();
  void setup_schedule_retrieval_service_();
  void request_pref_hash() {
    ESP_LOGI("*****************", "Preference Hash: %u", this->get_preference_hash());
  }
   void process_schedule_(const JsonObjectConst &response); 
 private:
  uint16_t  timeToMinutes_(const char* time_str);
  bool isValidTime_(const JsonVariantConst &time_obj) const;
 
 

  size_t schedule_max_size_{0};

    // Fixed-size POD structure for persistence (trivially copyable)
  struct PrefBlob {
    uint16_t count;
    uint16_t values[SCHEDULE_MAX_SIZE * 2];
  } __attribute__((packed));

  std::vector<uint16_t> schedule_times_in_minutes_; // Use std::vector for runtime sizing
  std::vector<uint16_t> factory_reset_values_= {0xFFFF,0xFFFF}; // Set the MSB to denote end of schedule
  std::string ha_schedule_entity_id_;
protected:
  ESPPreferenceObject schedule_pref_;

    // Action object that sends the HA service call.
  esphome::api::HomeAssistantServiceCallAction<> *ha_get_schedule_action_{nullptr};


};

} // namespace schedule
} // namespace esphome
