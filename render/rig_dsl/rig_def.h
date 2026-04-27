#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace Render::RigDSL {

enum class PartKind : std::uint8_t {
  Cylinder,
  Cone,
  Sphere,
  Box,
};

enum class PaletteSlot : std::uint8_t {
  Literal = 0,
  Skin,
  Cloth_Primary,
  Cloth_Secondary,
  Leather,
  Metal,
  Metal_Accent,
  Wood,
  Bone,
  Accent,
  _Count,
};

struct PackedColor {
  std::uint8_t r{255};
  std::uint8_t g{255};
  std::uint8_t b{255};
  std::uint8_t _pad{255};
};

using AnchorId = std::uint16_t;
inline constexpr AnchorId kInvalidAnchor = 0xFFFFU;

using ScalarId = std::uint16_t;
inline constexpr ScalarId kInvalidScalar = 0xFFFFU;

struct PartDef {
  PartKind kind{PartKind::Cylinder};
  std::uint8_t material_id{0};
  std::uint8_t lod_mask{0xFFU};
  PaletteSlot color_slot{PaletteSlot::Literal};
  PackedColor literal_color{};
  AnchorId anchor_a{kInvalidAnchor};
  AnchorId anchor_b{kInvalidAnchor};
  ScalarId scale_id{kInvalidScalar};
  float size_x{1.0F};
  float size_y{1.0F};
  float size_z{1.0F};
  float alpha{1.0F};
  float color_scale{1.0F};
};

struct RigDef {
  std::string_view name;
  const PartDef *parts{nullptr};
  std::size_t part_count{0};

  [[nodiscard]] constexpr auto size() const noexcept -> std::size_t {
    return part_count;
  }
  [[nodiscard]] constexpr auto
  operator[](std::size_t i) const noexcept -> const PartDef & {
    return parts[i];
  }
};

template <std::size_t N>
constexpr auto make_rig(std::string_view name,
                        const PartDef (&arr)[N]) noexcept -> RigDef {
  return RigDef{name, arr, N};
}

} // namespace Render::RigDSL
