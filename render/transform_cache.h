#pragma once

#include <QMatrix4x4>
#include <cstdint>
#include <unordered_map>

namespace Render {

template <typename KeyType = std::uint64_t> class TransformCache {
public:
  struct CachedTransform {
    QMatrix4x4 transform;
    std::uint32_t lastUpdateFrame{0};
    bool dirty{true};
  };

  void markDirty(KeyType key) {
    auto it = m_cache.find(key);
    if (it != m_cache.end()) {
      it->second.dirty = true;
    }
  }

  void markAllDirty() {
    for (auto &entry : m_cache) {
      entry.second.dirty = true;
    }
  }

  const QMatrix4x4 *get(KeyType key, std::uint32_t currentFrame) const {
    auto it = m_cache.find(key);
    if (it == m_cache.end() || it->second.dirty) {
      return nullptr;
    }

    if (currentFrame - it->second.lastUpdateFrame > m_maxFrameAge) {
      return nullptr;
    }

    return &it->second.transform;
  }

  void set(KeyType key, const QMatrix4x4 &transform,
           std::uint32_t currentFrame) {
    auto &entry = m_cache[key];
    entry.transform = transform;
    entry.lastUpdateFrame = currentFrame;
    entry.dirty = false;
  }

  void remove(KeyType key) { m_cache.erase(key); }

  void clear() { m_cache.clear(); }

  struct Stats {
    std::size_t totalEntries{0};
    std::size_t dirtyEntries{0};
    std::size_t validEntries{0};
  };

  Stats getStats() const {
    Stats stats;
    stats.totalEntries = m_cache.size();
    for (const auto &entry : m_cache) {
      if (entry.second.dirty) {
        ++stats.dirtyEntries;
      } else {
        ++stats.validEntries;
      }
    }
    return stats;
  }

  void setMaxFrameAge(std::uint32_t frames) { m_maxFrameAge = frames; }

  void cleanup(std::uint32_t currentFrame) {
    auto it = m_cache.begin();
    while (it != m_cache.end()) {
      if (currentFrame - it->second.lastUpdateFrame > m_maxFrameAge * 2) {
        it = m_cache.erase(it);
      } else {
        ++it;
      }
    }
  }

private:
  std::unordered_map<KeyType, CachedTransform> m_cache;
  std::uint32_t m_maxFrameAge{300};
};

} 
