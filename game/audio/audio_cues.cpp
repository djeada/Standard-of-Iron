#include "audio_cues.h"

#include <QDebug>

#include <algorithm>
#include <mutex>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace Game::Audio {

namespace {

auto cue_rng() -> std::mt19937& {
  thread_local std::mt19937 rng(std::random_device{}());
  return rng;
}

void trace_cue_drop(const std::string& cue_id, const char* reason) {
  static const bool trace = !qEnvironmentVariableIsEmpty("SOI_AUDIO_TRACE");
  if (!trace) {
    return;
  }

  qInfo().noquote() << QStringLiteral("audio cue %1: dropped:%2")
                           .arg(QString::fromStdString(cue_id),
                                QString::fromLatin1(reason));
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
  m_last_resource.erase(cue_id);
}

void CueRegistry::clear() {
  std::lock_guard<std::mutex> const lock(m_mutex);
  m_bindings.clear();
  m_last_played.clear();
  m_last_resource.clear();
  m_silent_requests.clear();
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

auto CueRegistry::choose_resource_locked(const std::string& cue_id,
                                         const CueBinding& binding) -> std::string {
  std::vector<std::string> available;
  available.reserve(binding.resource_ids.size());
  for (const auto& resource_id : binding.resource_ids) {
    if (AudioSystem::get_instance().has_resource(resource_id)) {
      available.push_back(resource_id);
    }
  }

  if (available.empty()) {
    return {};
  }
  if (available.size() == 1U) {
    return available.front();
  }

  const auto last_it = m_last_resource.find(cue_id);
  const std::string last_resource =
      (last_it == m_last_resource.end()) ? std::string{} : last_it->second;

  std::uniform_int_distribution<size_t> dist(0, available.size() - 1U);
  const size_t start = dist(cue_rng());
  for (size_t offset = 0; offset < available.size(); ++offset) {
    const std::string& candidate = available[(start + offset) % available.size()];
    if (candidate != last_resource) {
      return candidate;
    }
  }
  return available.front();
}

auto CueRegistry::play(const std::string& cue_id, float volume_scale) -> bool {
  std::string resource_id;
  CueBinding binding;

  {
    std::lock_guard<std::mutex> const lock(m_mutex);

    auto it = m_bindings.find(cue_id);
    if (it == m_bindings.end() || it->second.resource_ids.empty()) {
      trace_cue_drop(cue_id, "unbound");
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
          trace_cue_drop(cue_id, "cue_cooldown");
          return false;
        }
      }
    }

    resource_id = choose_resource_locked(cue_id, binding);
    if (resource_id.empty()) {
      trace_cue_drop(cue_id, "no_loaded_resource");
      if (++m_silent_requests[cue_id] == 1U) {
        qWarning() << "audio cue" << QString::fromStdString(cue_id)
                   << "has bindings but none are loaded; check its load_policy";
      }
      return false;
    }

    m_last_played[cue_id] = now;
    m_last_resource[cue_id] = resource_id;
  }

  AudioSystem::get_instance().play_sound(resource_id,
                                         binding.volume * volume_scale,
                                         binding.loop,
                                         binding.priority,
                                         binding.category,
                                         cue_id);
  return true;
}

auto play_cue(const std::string& cue_id, float volume_scale) -> bool {
  return CueRegistry::instance().play(cue_id, volume_scale);
}

} // namespace Game::Audio
