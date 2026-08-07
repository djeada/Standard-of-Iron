#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cstddef>

namespace Render::GL {

inline constexpr int k_max_shadow_cascades = 4;

struct DirectionalShadowBlock {
  static constexpr std::size_t k_mat4_floats = 16;
  static constexpr std::size_t k_vec4_floats = 4;
  static constexpr std::size_t k_float_count =
      (k_mat4_floats * static_cast<std::size_t>(k_max_shadow_cascades)) +
      (k_vec4_floats * 4);

  using Packed = std::array<float, k_float_count>;

  std::array<QMatrix4x4, k_max_shadow_cascades> light_view_projection{};
  std::array<float, k_max_shadow_cascades> split_distances{};

  float enabled = 0.0F;
  float cascade_count = 0.0F;
  float shadow_map_texel_size = 0.0F;
  float pcf_radius = 0.0F;

  QVector3D camera_position;

  float depth_bias = 0.0F;
  float normal_bias = 0.0F;
  float cascade_blend = 0.0F;

  [[nodiscard]] auto packed_std140() const noexcept -> Packed {
    Packed packed{};
    std::size_t cursor = 0;

    for (const auto& matrix : light_view_projection) {
      std::copy_n(matrix.constData(), k_mat4_floats, packed.data() + cursor);
      cursor += k_mat4_floats;
    }

    for (const float split : split_distances) {
      packed[cursor++] = split;
    }

    packed[cursor++] = enabled;
    packed[cursor++] = cascade_count;
    packed[cursor++] = shadow_map_texel_size;
    packed[cursor++] = pcf_radius;

    packed[cursor++] = camera_position.x();
    packed[cursor++] = camera_position.y();
    packed[cursor++] = camera_position.z();
    packed[cursor++] = 1.0F;

    packed[cursor++] = depth_bias;
    packed[cursor++] = normal_bias;
    packed[cursor++] = cascade_blend;
    packed[cursor++] = 0.0F;

    return packed;
  }
};

static_assert(DirectionalShadowBlock::k_float_count == 80,
              "DirectionalShadows in assets/shaders/include/directional_shadows.glsl "
              "declares mat4[4] + 4 vec4");

} // namespace Render::GL
