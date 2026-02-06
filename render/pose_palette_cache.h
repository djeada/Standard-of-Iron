#pragma once

#include "gl/humanoid/humanoid_types.h"
#include "template_cache.h"
#include <array>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Render::GL {

struct PosePaletteKey {
  AnimState state{AnimState::Idle};
  std::uint8_t frame{0};
  bool is_moving{false};

  bool operator==(const PosePaletteKey &o) const {
    return state == o.state && frame == o.frame && is_moving == o.is_moving;
  }
};

struct PosePaletteKeyHash {
  std::size_t operator()(const PosePaletteKey &k) const noexcept {
    std::size_t h = static_cast<std::size_t>(k.state);
    h ^= static_cast<std::size_t>(k.frame) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= static_cast<std::size_t>(k.is_moving) + 0x9e3779b9 + (h << 6) +
         (h >> 2);
    return h;
  }
};

struct PosePaletteEntry {
  HumanoidPose pose;
  float time{0.0F};
};

class PosePaletteCache {
public:
  static auto instance() noexcept -> PosePaletteCache & {
    static PosePaletteCache inst;
    return inst;
  }

  void generate();

  auto get(const PosePaletteKey &key) const -> const PosePaletteEntry *;

  void clear();

  [[nodiscard]] auto size() const -> std::size_t;

  [[nodiscard]] auto is_generated() const -> bool { return m_generated; }

private:
  PosePaletteCache() = default;

  std::unordered_map<PosePaletteKey, PosePaletteEntry, PosePaletteKeyHash>
      m_palette;
  mutable std::mutex m_mutex;
  bool m_generated{false};
};

} // namespace Render::GL
