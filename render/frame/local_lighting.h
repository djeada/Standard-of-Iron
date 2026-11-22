#pragma once

#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace Render {

inline constexpr std::size_t k_max_local_lights = 16;

struct LocalLight {
  QVector3D position;
  QVector3D color{1.0F, 0.72F, 0.42F};
  float radius = 6.0F;
  float intensity = 1.0F;
  bool casts_shadow = false;
};

[[nodiscard]] inline auto select_local_lights(const std::vector<LocalLight>& lights,
                                              const QVector3D& camera_position)
    -> std::array<LocalLight, k_max_local_lights> {

  struct Ranked {
    LocalLight light;
    float score = -1.0F;
  };

  std::array<Ranked, k_max_local_lights> ranked{};
  std::size_t filled = 0;

  for (const auto& source : lights) {
    LocalLight light = source;
    light.radius = std::max(light.radius, 0.01F);
    light.intensity = std::max(light.intensity, 0.0F);

    const float distance_squared =
        std::max((light.position - camera_position).lengthSquared(), 1.0F);
    const float score =
        light.intensity * light.radius * light.radius / distance_squared;

    if (filled == ranked.size() && score <= ranked[filled - 1].score) {
      continue;
    }

    std::size_t slot = filled;
    while (slot > 0 && score > ranked[slot - 1].score) {
      --slot;
    }
    if (slot >= ranked.size()) {
      continue;
    }

    const std::size_t last = std::min(filled, ranked.size() - 1);
    for (std::size_t i = last; i > slot; --i) {
      ranked[i] = ranked[i - 1];
    }
    ranked[slot] = Ranked{light, score};
    filled = std::min(filled + 1, ranked.size());
  }

  std::array<LocalLight, k_max_local_lights> result{};
  for (std::size_t i = 0; i < ranked.size(); ++i) {
    if (i < filled) {
      result[i] = ranked[i].light;
      continue;
    }
    result[i].radius = 0.0F;
    result[i].intensity = 0.0F;
  }
  return result;
}

struct LocalLightingBlock {
  static constexpr std::size_t k_vec4_floats = 4;
  static constexpr std::size_t k_float_count =
      (k_max_local_lights * k_vec4_floats * 2) + k_vec4_floats;

  using Packed = std::array<float, k_float_count>;

  static constexpr std::size_t k_position_radius_offset = 0;
  static constexpr std::size_t k_color_intensity_offset =
      k_max_local_lights * k_vec4_floats;
  static constexpr std::size_t k_meta_offset = k_max_local_lights * k_vec4_floats * 2;
};

[[nodiscard]] inline auto
pack_local_lights_std140(const std::array<LocalLight, k_max_local_lights>& lights)
    -> LocalLightingBlock::Packed {
  LocalLightingBlock::Packed packed{};
  std::size_t active_count = 0;

  for (const auto& light : lights) {
    if (light.intensity <= 0.0F || light.radius <= 0.0F) {
      continue;
    }
    const std::size_t slot = active_count * LocalLightingBlock::k_vec4_floats;
    const std::size_t position_offset =
        LocalLightingBlock::k_position_radius_offset + slot;
    packed[position_offset + 0] = light.position.x();
    packed[position_offset + 1] = light.position.y();
    packed[position_offset + 2] = light.position.z();
    packed[position_offset + 3] = light.radius;

    const std::size_t color_offset =
        LocalLightingBlock::k_color_intensity_offset + slot;
    packed[color_offset + 0] = light.color.x();
    packed[color_offset + 1] = light.color.y();
    packed[color_offset + 2] = light.color.z();
    packed[color_offset + 3] = light.intensity;
    ++active_count;
  }

  packed[LocalLightingBlock::k_meta_offset] = static_cast<float>(active_count);
  return packed;
}

[[nodiscard]] inline auto
active_local_lights(const std::array<LocalLight, k_max_local_lights>& lights)
    -> std::vector<LocalLight> {
  std::vector<LocalLight> active;
  active.reserve(k_max_local_lights);
  for (const auto& light : lights) {
    if (light.intensity <= 0.0F || light.radius <= 0.0F) {
      continue;
    }
    active.push_back(light);
  }
  return active;
}

[[nodiscard]] inline auto
local_light_mask_for_bounds(const std::vector<LocalLight>& active,
                            const QVector3D& center,
                            float bounding_radius) -> unsigned int {
  unsigned int mask = 0U;
  const std::size_t count = std::min(active.size(), k_max_local_lights);
  for (std::size_t i = 0; i < count; ++i) {
    const float reach = active[i].radius + bounding_radius;
    if ((active[i].position - center).lengthSquared() <= reach * reach) {
      mask |= (1U << i);
    }
  }
  return mask;
}

inline constexpr float k_local_light_fade_seconds = 0.35F;
inline constexpr float k_local_light_match_distance = 0.75F;

class LocalLightFader {
public:
  [[nodiscard]] auto
  update(const std::vector<LocalLight>& candidates,
         const QVector3D& camera_position,
         float elapsed_seconds) -> std::array<LocalLight, k_max_local_lights> {
    const float delta = m_has_previous_time
                            ? std::clamp(elapsed_seconds - m_previous_time, 0.0F, 0.25F)
                            : 0.0F;
    m_previous_time = elapsed_seconds;
    m_has_previous_time = true;

    const auto selected = select_local_lights(candidates, camera_position);
    std::array<bool, k_max_local_lights> claimed{};

    for (auto& slot : m_slots) {
      if (!slot.occupied) {
        continue;
      }
      slot.target = 0.0F;
      for (std::size_t i = 0; i < selected.size(); ++i) {
        if (claimed[i] || selected[i].radius <= 0.0F || selected[i].intensity <= 0.0F) {
          continue;
        }
        if ((selected[i].position - slot.light.position).length() >
            k_local_light_match_distance) {
          continue;
        }
        claimed[i] = true;
        slot.light = selected[i];
        slot.target = 1.0F;
        break;
      }
    }

    for (std::size_t i = 0; i < selected.size(); ++i) {
      if (claimed[i] || selected[i].radius <= 0.0F || selected[i].intensity <= 0.0F) {
        continue;
      }
      for (auto& slot : m_slots) {
        if (slot.occupied) {
          continue;
        }
        slot.occupied = true;
        slot.light = selected[i];
        slot.weight = 0.0F;
        slot.target = 1.0F;
        claimed[i] = true;
        break;
      }
    }

    const float step = delta <= 0.0F ? 1.0F : delta / k_local_light_fade_seconds;
    std::array<LocalLight, k_max_local_lights> result{};
    std::size_t emitted = 0;
    for (auto& slot : m_slots) {
      if (!slot.occupied) {
        continue;
      }
      slot.weight = std::clamp(
          slot.weight + std::clamp(slot.target - slot.weight, -step, step), 0.0F, 1.0F);
      if (slot.weight <= 0.0F && slot.target <= 0.0F) {
        slot = Slot{};
        continue;
      }
      result[emitted] = slot.light;
      result[emitted].intensity = slot.light.intensity * slot.weight;
      ++emitted;
    }
    for (std::size_t i = emitted; i < result.size(); ++i) {
      result[i].radius = 0.0F;
      result[i].intensity = 0.0F;
    }
    return result;
  }

  void reset() noexcept {
    m_slots = {};
    m_previous_time = 0.0F;
    m_has_previous_time = false;
  }

private:
  struct Slot {
    LocalLight light;
    float weight = 0.0F;
    float target = 0.0F;
    bool occupied = false;
  };

  std::array<Slot, k_max_local_lights> m_slots{};
  float m_previous_time = 0.0F;
  bool m_has_previous_time = false;
};

} // namespace Render
