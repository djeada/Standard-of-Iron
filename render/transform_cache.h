#pragma once

#include <QMatrix4x4>
#include <cstdint>
#include <unordered_map>

namespace Render {

template <typename KeyType = std::uint64_t> class TransformCache {
public:
  struct CachedTransform {
    QMatrix4x4 transform;
    std::uint32_t last_update_frame{0};
    bool dirty{true};
  };

  void mark_dirty(KeyType key) {
    auto it = m_cache.find(key);
    if (it != m_cache.end()) {
      it->second.dirty = true;
    }
  }

  void mark_all_dirty() {
    for (auto &entry : m_cache) {
      entry.second.dirty = true;
    }
  }

  const QMatrix4x4 *get(KeyType key, std::uint32_t current_frame) const {
    auto it = m_cache.find(key);
    if (it == m_cache.end() || it->second.dirty) {
      return nullptr;
    }

    if (current_frame - it->second.last_update_frame > m_max_frame_age) {
      return nullptr;
    }

    return &it->second.transform;
  }

  void set(KeyType key, const QMatrix4x4 &transform,
           std::uint32_t current_frame) {
    auto &entry = m_cache[key];
    entry.transform = transform;
    entry.last_update_frame = current_frame;
    entry.dirty = false;
  }

  void remove(KeyType key) { m_cache.erase(key); }

  void clear() { m_cache.clear(); }

  struct Stats {
    std::size_t total_entries{0};
    std::size_t dirty_entries{0};
    std::size_t valid_entries{0};
  };

  Stats get_stats() const {
    Stats stats;
    stats.total_entries = m_cache.size();
    for (const auto &entry : m_cache) {
      if (entry.second.dirty) {
        ++stats.dirty_entries;
      } else {
        ++stats.valid_entries;
      }
    }
    return stats;
  }

  void set_max_frame_age(std::uint32_t frames) { m_max_frame_age = frames; }

  void cleanup(std::uint32_t current_frame) {
    auto it = m_cache.begin();
    while (it != m_cache.end()) {
      if (current_frame - it->second.last_update_frame > m_max_frame_age * 2) {
        it = m_cache.erase(it);
      } else {
        ++it;
      }
    }
  }

private:
  std::unordered_map<KeyType, CachedTransform> m_cache;
  std::uint32_t m_max_frame_age{300};
};

} // namespace Render
