#pragma once

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace schedule {
class ArrayPreferenceBase : public Component {
 public:
  virtual void create_preference(uint32_t key) = 0;
  virtual void load() = 0;
  virtual void save() = 0;
  virtual uint8_t *data() = 0;
  virtual size_t size() const = 0;
  virtual bool is_valid() const = 0;

  void setup() override {}
  void loop() override {}
};

template<size_t N>
class ArrayPreference : public ArrayPreferenceBase {
 public:
  ArrayPreference() { memset(data_, 0, N); }

  void create_preference(uint32_t key) override {
    pref_ = global_preferences->make_preference<uint8_t[N]>(key);
  }

  void load() override {
    uint8_t buf[N];
    valid_ = pref_.load(&buf);
    if (valid_) {
        memcpy(data_, buf, N);
    } else {
        ESP_LOGW("ArrayPreference", "Failed to load preference");
    }
  }

  void save() override {
    pref_.save(&data_);
    global_preferences->sync();
  }

  uint8_t *data() override { return data_; }
  size_t size() const override { return N; }
  bool is_valid() const override { return valid_; }

 private:
  uint8_t data_[N];
  ESPPreferenceObject pref_;
  bool valid_ = false;
};
}   // namespace schedule
}  // namespace esphome
