#pragma once

// Stage 8a — Declarative rig DSL types.
//
// The goal is to let a rig be expressed as *data*: a list of parts that
// reference named anchor points and a palette slot, instead of ~2000 lines
// of hand-rolled matrix math. At render time a `RigInterpreter` walks the
// def, resolves each anchor through a pluggable `AnchorResolver`, and emits
// DrawPartCmds via `ISubmitter::part()`.
//
// Design constraints:
//   * POD-only. A `RigDef` can be a `constexpr inline` header-only constant
//     so asset authors don't pay any per-frame allocation cost.
//   * No virtuals, no QObject, no Qt dependency in this header (ISubmitter
//     is needed by the interpreter, not the def). This keeps rig definitions
//     dumpable to a plain C++ TU and trivially loadable from data at a
//     later stage.
//   * Anchor points are *named*, not positioned. Positions come from the
//     resolver, which knows the current bone frames / pose. The same
//     RigDef therefore works across poses, LODs, and even across species
//     (e.g. a saddle rig resolves different anchors on horse vs elephant).
//   * Colour is either a literal or a palette slot index. The palette lives
//     on the resolver (so tinting / faction colours happen without touching
//     the def).

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace Render::RigDSL {

enum class PartKind : std::uint8_t {
  Cylinder, // Two anchors: axis endpoints. Radius = size.x.
  Cone,     // Two anchors: base (anchor_a) + apex (anchor_b). Radius = size.x.
  Sphere,   // One anchor: anchor_a = centre. Radius = size.x.
  Box,      // Two anchors: diagonal corners. size.xyz used as half-extents.
};

// Named palette slots. The resolver maps these to concrete QVector3D colours
// per unit (so faction tint lives on the resolver, not duplicated in defs).
enum class PaletteSlot : std::uint8_t {
  Literal = 0,  // Use literal_color directly.
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

// Compact literal colour (8-bit per channel). Converted to QVector3D by the
// interpreter. Keeping this 4 bytes instead of 12 lets a RigDef fit several
// hundred parts in one cache line-friendly array.
struct PackedColor {
  std::uint8_t r{255};
  std::uint8_t g{255};
  std::uint8_t b{255};
  std::uint8_t _pad{255};
};

// Anchor names are interned as small tokens. The resolver maps
// AnchorId -> QVector3D world-local position. AnchorId is an opaque handle;
// the interpreter never inspects it.
using AnchorId = std::uint16_t;
inline constexpr AnchorId kInvalidAnchor = 0xFFFFU;

// Scalar IDs let rigs name dynamic values (e.g. "helmet_r", "head_r * 1.06")
// that vary per render call. They multiply size_x/y/z at interpret time.
// kInvalidScalar => size fields are absolute.
using ScalarId = std::uint16_t;
inline constexpr ScalarId kInvalidScalar = 0xFFFFU;

struct PartDef {
  PartKind kind{PartKind::Cylinder};
  std::uint8_t material_id{0};
  std::uint8_t lod_mask{0xFFU};  // Bitmask: which LODs include this part.
  PaletteSlot color_slot{PaletteSlot::Literal};
  PackedColor literal_color{};
  AnchorId anchor_a{kInvalidAnchor};
  AnchorId anchor_b{kInvalidAnchor}; // Unused for Sphere. (kept aligned for constexpr-friendly literal initialisers)
  ScalarId scale_id{kInvalidScalar}; // Resolver-supplied size multiplier.
  float size_x{1.0F}; // Radius for cylinder/cone/sphere, half-extent-x for box.
  float size_y{1.0F}; // Box half-extent-y (unused otherwise).
  float size_z{1.0F}; // Box half-extent-z (unused otherwise).
  float alpha{1.0F};
  float color_scale{1.0F}; // Multiplier applied to the palette colour.
};

// A RigDef is just a span of parts plus a debug name. The contract is that
// RigInterpreter::render() iterates parts in order and emits one part() call
// per PartDef that passes the LOD mask.
struct RigDef {
  std::string_view name;
  const PartDef *parts{nullptr};
  std::size_t part_count{0};

  [[nodiscard]] constexpr auto size() const noexcept -> std::size_t {
    return part_count;
  }
  [[nodiscard]] constexpr auto operator[](std::size_t i) const noexcept
      -> const PartDef & {
    return parts[i];
  }
};

// Convenience constexpr factory so a rig can be declared as:
//   constexpr PartDef kWatchtowerParts[] = { ... };
//   constexpr RigDef kWatchtowerRig = make_rig("watchtower", kWatchtowerParts);
template <std::size_t N>
constexpr auto make_rig(std::string_view name,
                        const PartDef (&arr)[N]) noexcept -> RigDef {
  return RigDef{name, arr, N};
}

} // namespace Render::RigDSL
