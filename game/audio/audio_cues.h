#pragma once

#include <array>
#include <chrono>
#include <deque>
#include <mutex>
#include <source_location>
#include <string>
#include <unordered_map>
#include <vector>

#include "audio_system.h"
#include "cue_ids.h"
#include "cue_trace.h"
#include "spatial.h"

namespace Game::Audio {

struct CueBinding {
  std::vector<std::string> resource_ids;
  std::vector<float> weights;
  AudioCategory category{AudioCategory::SFX};
  float volume{AudioConstants::DEFAULT_VOLUME};
  int priority{AudioConstants::DEFAULT_PRIORITY};
  int cooldown_ms{0};
  bool loop{false};
  bool spatial{false};
};

class CueRegistry {
public:
  static auto instance() -> CueRegistry&;

  void bind(const std::string& cue_id, CueBinding binding);
  void clear();
  void reset_cooldowns();

  auto play(const std::string& cue_id,
            float volume_scale = AudioConstants::DEFAULT_VOLUME,
            const std::string& source = {},
            const WorldPoint* position = nullptr) -> bool;

  [[nodiscard]] auto is_bound(const std::string& cue_id) const -> bool;

  [[nodiscard]] auto silent_cues() const -> std::vector<std::string>;

  [[nodiscard]] auto last_variant(const std::string& cue_id) const -> std::string;

  [[nodiscard]] static auto shuffle_history_size(std::size_t pool_size) -> std::size_t;

private:
  CueRegistry() = default;

  CueRegistry(const CueRegistry&) = delete;
  auto operator=(const CueRegistry&) -> CueRegistry& = delete;

  auto choose_resource_locked(const std::string& cue_id,
                              const CueBinding& binding) -> std::string;
  void remember_choice_locked(const std::string& cue_id,
                              const std::string& resource_id,
                              std::size_t pool_size);
  [[nodiscard]] auto
  is_variant_free_locked(const std::string& resource_id) const -> bool;

  mutable std::mutex m_mutex;
  std::unordered_map<std::string, CueBinding> m_bindings;
  std::unordered_map<std::string, std::chrono::steady_clock::time_point> m_last_played;
  std::unordered_map<std::string, std::deque<std::string>> m_recent_resources;
  std::unordered_map<std::string, std::chrono::steady_clock::time_point>
      m_resource_dispatched_at;
  std::unordered_map<std::string, unsigned> m_silent_requests;
};

auto play_cue_from(const std::string& cue_id,
                   float volume_scale,
                   std::string source,
                   const WorldPoint* position = nullptr) -> bool;

inline auto play_cue(
    const std::string& cue_id,
    float volume_scale = AudioConstants::DEFAULT_VOLUME,
    const std::source_location& location = std::source_location::current()) -> bool {
  return play_cue_from(cue_id, volume_scale, cue_source_of(location));
}

inline auto play_cue_at(
    const std::string& cue_id,
    const WorldPoint& position,
    float volume_scale = AudioConstants::DEFAULT_VOLUME,
    const std::source_location& location = std::source_location::current()) -> bool {
  return play_cue_from(cue_id, volume_scale, cue_source_of(location), &position);
}

} // namespace Game::Audio
