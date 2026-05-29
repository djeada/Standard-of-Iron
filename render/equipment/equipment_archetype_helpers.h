#pragma once

#include <QVector3D>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>

#include "../static_attachment_spec.h"
#include "generated_equipment.h"

namespace Render::GL {

inline auto quantize_equipment_value(float value) -> int {
  return std::lround(value * 1000.0F);
}

template <typename Tag, typename Key>
struct GeneratedArchetypeCacheEntry {
  Key key;
  RenderArchetype archetype;
};

template <typename Tag, typename Key, std::size_t Count>
auto cached_generated_archetype(const Key& key,
                                const std::string& debug_name,
                                const std::array<GeneratedEquipmentPrimitive, Count>&
                                    primitives) -> const RenderArchetype& {
  static std::deque<GeneratedArchetypeCacheEntry<Tag, Key>> cache;
  for (const auto& entry : cache) {
    if (entry.key == key) {
      return entry.archetype;
    }
  }
  cache.push_back({key, build_generated_equipment_archetype(debug_name, primitives)});
  return cache.back().archetype;
}

template <std::size_t Count>
auto fill_role_colors(const std::array<QVector3D, Count>& colors,
                      QVector3D* out,
                      std::size_t max) -> std::uint32_t {
  if (max < Count) {
    return 0U;
  }
  for (std::size_t i = 0; i < Count; ++i) {
    out[i] = colors[i];
  }
  return static_cast<std::uint32_t>(Count);
}

inline void fill_sequential_role_remap(Render::Creature::StaticAttachmentSpec& spec,
                                       std::uint8_t base_role_byte,
                                       std::uint8_t count) {
  for (std::uint8_t i = 0; i < count; ++i) {
    spec.palette_role_remap[i] = static_cast<std::uint8_t>(base_role_byte + i);
  }
}

} // namespace Render::GL
