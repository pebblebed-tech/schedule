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
    if (pref_.load(&buf)) memcpy(data_, buf, N);
  }

  void save() override {
    pref_.save(&data_);
  }

  uint8_t *data() override { return data_; }
  size_t size() const override { return N; }

 private:
  uint8_t data_[N];
  ESPPreferenceObject pref_;
};
}   // namespace schedule
}  // namespace esphome
