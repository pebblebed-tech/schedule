#pragma once

#include "esphome/components/select/select.h"
#include "esphome/core/preferences.h"
#include "../schedule.h"

namespace esphome {
namespace schedule {

class ScheduleModeSelect : public select::Select, public Parented<Schedule> {
 public:
  ScheduleModeSelect() = default;

  void setup();
  void dump_config();

 protected:
  void control(const std::string &value) override;
  
  ESPPreferenceObject pref_;
};

}  // namespace schedule
}  // namespace esphome
