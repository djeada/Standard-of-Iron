#pragma once

#include <array>
#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "audio_system.h"
#include "cue_ids.h"

namespace Game::Audio {

struct CueBinding {
  std::vector<std::string> resource_ids;
  AudioCategory category{AudioCategory::SFX};
  float volume{AudioConstants::DEFAULT_VOLUME};
  int priority{AudioConstants::DEFAULT_PRIORITY};
  int cooldown_ms{0};
  bool loop{false};
};

class CueRegistry {
public:
  static auto instance() -> CueRegistry&;

  void bind(const std::string& cue_id, CueBinding binding);
  void clear();

  auto play(const std::string& cue_id,
            float volume_scale = AudioConstants::DEFAULT_VOLUME) -> bool;

  [[nodiscard]] auto is_bound(const std::string& cue_id) const -> bool;

  [[nodiscard]] auto silent_cues() const -> std::vector<std::string>;

private:
  CueRegistry() = default;

  CueRegistry(const CueRegistry&) = delete;
  auto operator=(const CueRegistry&) -> CueRegistry& = delete;

  auto choose_resource_locked(const std::string& cue_id,
                              const CueBinding& binding) -> std::string;

  mutable std::mutex m_mutex;
  std::unordered_map<std::string, CueBinding> m_bindings;
  std::unordered_map<std::string, std::chrono::steady_clock::time_point> m_last_played;
  std::unordered_map<std::string, std::string> m_last_resource;
  std::unordered_map<std::string, unsigned> m_silent_requests;
};

auto play_cue(const std::string& cue_id,
              float volume_scale = AudioConstants::DEFAULT_VOLUME) -> bool;

} // namespace Game::Audio
