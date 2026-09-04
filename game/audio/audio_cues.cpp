#include "audio_cues.h"

#include <QDebug>

#include <algorithm>
#include <cstddef>
#include <deque>
#include <iterator>
#include <mutex>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "cue_trace.h"

namespace Game::Audio {

namespace {

auto cue_rng() -> std::mt19937& {
  thread_local std::mt19937 rng(std::random_device{}());
  return rng;
}

void trace_cue_drop(const std::string& cue_id,
                    CueOutcome outcome,
                    const std::string& source) {
  CueTrace::instance().record(cue_id, {}, outcome, source);
}

} // namespace

auto CueRegistry::instance() -> CueRegistry& {
  static CueRegistry registry;
  return registry;
}

void CueRegistry::bind(const std::string& cue_id, CueBinding binding) {
  if (cue_id.empty()) {
    return;
  }

  std::lock_guard<std::mutex> const lock(m_mutex);
  m_bindings[cue_id] = std::move(binding);
  m_recent_resources.erase(cue_id);
}

void CueRegistry::clear() {
  std::lock_guard<std::mutex> const lock(m_mutex);
  m_bindings.clear();
  m_last_played.clear();
  m_recent_resources.clear();
  m_resource_dispatched_at.clear();
  m_silent_requests.clear();
}

void CueRegistry::reset_cooldowns() {
  std::lock_guard<std::mutex> const lock(m_mutex);
  m_last_played.clear();
  m_resource_dispatched_at.clear();
}

auto CueRegistry::is_bound(const std::string& cue_id) const -> bool {
  std::lock_guard<std::mutex> const lock(m_mutex);
  auto it = m_bindings.find(cue_id);
  return it != m_bindings.end() && !it->second.resource_ids.empty();
}

auto CueRegistry::silent_cues() const -> std::vector<std::string> {
  std::lock_guard<std::mutex> const lock(m_mutex);

  std::vector<std::string> cue_ids;
  cue_ids.reserve(m_silent_requests.size());
  for (const auto& [cue_id, count] : m_silent_requests) {
    cue_ids.push_back(cue_id);
  }
  std::sort(cue_ids.begin(), cue_ids.end());
  return cue_ids;
}

auto CueRegistry::is_variant_free_locked(const std::string& resource_id) const -> bool {
  auto& audio = AudioSystem::get_instance();
  if (!audio.is_resource_ready(resource_id)) {
    return false;
  }

  const auto dispatched = m_resource_dispatched_at.find(resource_id);
  if (dispatched == m_resource_dispatched_at.end()) {
    return true;
  }

  const int cooldown_ms = audio.resource_cooldown_ms(resource_id);
  if (cooldown_ms <= 0) {
    return true;
  }
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - dispatched->second)
                           .count();
  return elapsed >= cooldown_ms;
}

auto CueRegistry::last_variant(const std::string& cue_id) const -> std::string {
  std::lock_guard<std::mutex> const lock(m_mutex);

  auto it = m_recent_resources.find(cue_id);
  if (it == m_recent_resources.end() || it->second.empty()) {
    return {};
  }
  return it->second.back();
}

auto CueRegistry::shuffle_history_size(std::size_t pool_size) -> std::size_t {
  if (pool_size < 2U) {
    return 0U;
  }
  return std::max<std::size_t>(1U, pool_size / 2U);
}

auto CueRegistry::choose_resource_locked(const std::string& cue_id,
                                         const CueBinding& binding) -> std::string {
  std::vector<std::string> available;
  std::vector<float> weights;
  available.reserve(binding.resource_ids.size());
  weights.reserve(binding.resource_ids.size());
  for (std::size_t index = 0; index < binding.resource_ids.size(); ++index) {
    const auto& resource_id = binding.resource_ids[index];
    if (!AudioSystem::get_instance().has_resource(resource_id)) {
      continue;
    }
    available.push_back(resource_id);
    weights.push_back(
        index < binding.weights.size() ? std::max(0.0F, binding.weights[index]) : 1.0F);
  }

  if (available.empty()) {
    return {};
  }
  if (available.size() == 1U) {
    return available.front();
  }

  const auto recent_it = m_recent_resources.find(cue_id);
  const std::size_t history = shuffle_history_size(available.size());

  const auto played_recently = [&](const std::string& resource_id) {
    if (recent_it == m_recent_resources.end()) {
      return false;
    }
    const auto& recent = recent_it->second;
    const std::size_t window = std::min(history, recent.size());
    const auto window_begin = recent.end() - static_cast<std::ptrdiff_t>(window);
    return std::find(window_begin, recent.end(), resource_id) != recent.end();
  };

  std::vector<std::size_t> candidates;
  std::vector<std::size_t> unheard;
  std::vector<std::size_t> playable;
  candidates.reserve(available.size());
  unheard.reserve(available.size());
  playable.reserve(available.size());
  for (std::size_t index = 0; index < available.size(); ++index) {
    if (weights[index] <= 0.0F) {
      continue;
    }
    playable.push_back(index);
    if (played_recently(available[index])) {
      continue;
    }
    unheard.push_back(index);
    if (is_variant_free_locked(available[index])) {
      candidates.push_back(index);
    }
  }

  if (candidates.empty()) {
    candidates = unheard;
  }
  if (candidates.empty()) {
    candidates = playable;
  }
  if (candidates.empty()) {
    return available.front();
  }

  std::vector<double> candidate_weights;
  candidate_weights.reserve(candidates.size());
  for (const std::size_t index : candidates) {
    candidate_weights.push_back(static_cast<double>(weights[index]));
  }

  std::discrete_distribution<std::size_t> dist(candidate_weights.begin(),
                                               candidate_weights.end());
  return available[candidates[dist(cue_rng())]];
}

void CueRegistry::remember_choice_locked(const std::string& cue_id,
                                         const std::string& resource_id,
                                         std::size_t pool_size) {
  const std::size_t history = shuffle_history_size(pool_size);
  if (history == 0U) {
    return;
  }

  auto& recent = m_recent_resources[cue_id];
  recent.push_back(resource_id);
  while (recent.size() > history) {
    recent.pop_front();
  }
}

auto CueRegistry::play(const std::string& cue_id,
                       float volume_scale,
                       const std::string& source,
                       const WorldPoint* position) -> bool {
  std::string resource_id;
  CueBinding binding;

  {
    std::lock_guard<std::mutex> const lock(m_mutex);

    auto it = m_bindings.find(cue_id);
    if (it == m_bindings.end() || it->second.resource_ids.empty()) {
      trace_cue_drop(cue_id, CueOutcome::Unbound, source);
      if (++m_silent_requests[cue_id] == 1U) {
        qWarning() << "audio cue requested but bound to nothing:"
                   << QString::fromStdString(cue_id);
      }
      return false;
    }

    binding = it->second;

    const auto now = std::chrono::steady_clock::now();
    if (binding.cooldown_ms > 0) {
      auto last_it = m_last_played.find(cue_id);
      if (last_it != m_last_played.end()) {
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - last_it->second)
                .count();
        if (elapsed < binding.cooldown_ms) {
          trace_cue_drop(cue_id, CueOutcome::CueCooldown, source);
          return false;
        }
      }
    }

    resource_id = choose_resource_locked(cue_id, binding);
    if (resource_id.empty()) {
      trace_cue_drop(cue_id, CueOutcome::NoLoadedResource, source);
      if (++m_silent_requests[cue_id] == 1U) {
        qWarning() << "audio cue" << QString::fromStdString(cue_id)
                   << "has bindings but none are loaded; check its load_policy";
      }
      return false;
    }

    m_last_played[cue_id] = now;
    m_resource_dispatched_at[resource_id] = now;
    remember_choice_locked(cue_id, resource_id, binding.resource_ids.size());
  }

  AudioSystem::get_instance().play_sound(resource_id,
                                         binding.volume * volume_scale,
                                         binding.loop,
                                         binding.priority,
                                         binding.category,
                                         cue_id,
                                         source,
                                         binding.spatial ? position : nullptr);
  return true;
}

auto play_cue_from(const std::string& cue_id,
                   float volume_scale,
                   std::string source,
                   const WorldPoint* position) -> bool {
  return CueRegistry::instance().play(cue_id, volume_scale, source, position);
}

} // namespace Game::Audio
