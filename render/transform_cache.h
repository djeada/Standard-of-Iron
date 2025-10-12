#pragma once

#include <QMatrix4x4>
#include <cstdint>
#include <unordered_map>

namespace Render {

// Simple transform cache for static/rarely-moving objects
// Avoids recomputing expensive matrix operations every frame
template <typename KeyType = std::uint64_t>
class TransformCache {
public:
  struct CachedTransform {
    QMatrix4x4 transform;
    std::uint32_t lastUpdateFrame{0};
    bool dirty{true};
  };

  // Mark a transform as dirty (needs recomputation)
  void markDirty(KeyType key) {
    auto it = m_cache.find(key);
    if (it != m_cache.end()) {
      it->second.dirty = true;
    }
  }

  // Mark all transforms as dirty (e.g., on camera change)
  void markAllDirty() {
    for (auto &entry : m_cache) {
      entry.second.dirty = true;
    }
  }

  // Get cached transform if valid, or nullptr if dirty/missing
  const QMatrix4x4 *get(KeyType key, std::uint32_t currentFrame) const {
    auto it = m_cache.find(key);
    if (it == m_cache.end() || it->second.dirty) {
      return nullptr;
    }
    
    // Optional: invalidate if too old (prevents stale entries)
    if (currentFrame - it->second.lastUpdateFrame > m_maxFrameAge) {
      return nullptr;
    }
    
    return &it->second.transform;
  }

  // Update or insert a transform
  void set(KeyType key, const QMatrix4x4 &transform, std::uint32_t currentFrame) {
    auto &entry = m_cache[key];
    entry.transform = transform;
    entry.lastUpdateFrame = currentFrame;
    entry.dirty = false;
  }

  // Remove a specific entry
  void remove(KeyType key) {
    m_cache.erase(key);
  }

  // Clear all cached transforms
  void clear() {
    m_cache.clear();
  }

  // Get cache statistics
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

  // Set maximum frame age before automatic invalidation
  void setMaxFrameAge(std::uint32_t frames) {
    m_maxFrameAge = frames;
  }

  // Cleanup old entries (call periodically)
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
  std::uint32_t m_maxFrameAge{300}; // ~5 seconds at 60fps
};

// Usage example:
//
// TransformCache<EntityID> cache;
//
// // Rendering loop:
// for (auto entity : entities) {
//   const QMatrix4x4 *cached = cache.get(entity.id, currentFrame);
//   if (cached) {
//     // Use cached transform
//     renderer.submit(*cached);
//   } else {
//     // Compute and cache
//     QMatrix4x4 transform = computeExpensiveTransform(entity);
//     cache.set(entity.id, transform, currentFrame);
//     renderer.submit(transform);
//   }
// }
//
// // When entity moves:
// cache.markDirty(entity.id);
//
// // Periodic cleanup (e.g., every 60 frames):
// if (currentFrame % 60 == 0) {
//   cache.cleanup(currentFrame);
// }

} // namespace Render
