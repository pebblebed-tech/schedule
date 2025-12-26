#pragma once
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include <vector>
#include <string>

namespace esphome {
namespace dynamic_preference {

template<size_t N>
class DynamicPreference : public Component {
 public:
  // Struct wrapper to make array size part of the type
  struct BufferType {
    uint8_t data[N];
  };
  
  void setup() override {
    ESP_LOGI("dynamic_preference", "DynamicPreference<%zu> setup", N);
  }
  
  float get_setup_priority() const override { return setup_priority::DATA; }
  
  void dump_config() override {
    ESP_LOGCONFIG("dynamic_preference", "Dynamic Preference Component:");
    ESP_LOGCONFIG("dynamic_preference", "  Buffer Size: %zu bytes", N);
    ESP_LOGCONFIG("dynamic_preference", "  Hash: 0x%08X", hash_);
  }
  
  void set_hash(uint32_t hash) { hash_ = hash; }
  uint32_t get_hash() const { return hash_; }
  
  void create_preference() {
    ESP_LOGI("dynamic_preference", "Creating preference with hash=0x%08X, size=%zu bytes", hash_, N);
    pref_ = global_preferences->make_preference<BufferType>(hash_, true);
    ESP_LOGI("dynamic_preference", "Preference created successfully");
  }
  
  bool save(const uint8_t *data, size_t length) {
    if (length != N) {
      ESP_LOGW("dynamic_preference", "Save size mismatch: expected %zu bytes, got %zu bytes", N, length);
      return false;
    }
    
    for (size_t i = 0; i < N; ++i) {
      buffer_.data[i] = data[i];
    }
    
    const bool ok = pref_.save(&buffer_);
    
    if (ok) {
      ESP_LOGI("dynamic_preference", "Saved %zu bytes successfully", N);
    } else {
      ESP_LOGW("dynamic_preference", "Save failed");
    }
    
    return ok;
  }
  
  bool load(uint8_t *data, size_t length) {
    if (length != N) {
      ESP_LOGW("dynamic_preference", "Load size mismatch: expected %zu bytes, got %zu bytes", N, length);
      return false;
    }
    
    const bool ok = pref_.load(&buffer_);
    
    if (ok) {
      for (size_t i = 0; i < N; ++i) {
        data[i] = buffer_.data[i];
      }
      ESP_LOGI("dynamic_preference", "Loaded %zu bytes successfully", N);
    } else {
      ESP_LOGI("dynamic_preference", "No stored data found");
    }
    
    return ok;
  }
  
  size_t get_size() const { return N; }
  
 protected:
  ESPPreferenceObject pref_;
  BufferType buffer_;
  uint32_t hash_{0};
};

} // namespace dynamic_preference
} // namespace esphome