#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "render/equipment/generated_equipment.h"

namespace Render::GL {

// A constexpr-friendly point. QVector3D has no constexpr constructor, so
// authored primitive tables use this and convert when they are expanded.
struct SpecPoint {
  float x{0.0F};
  float y{0.0F};
  float z{0.0F};
};

// One authored primitive of a piece of generated equipment, in the same terms
// as the generated_* helpers.
//
// Helmets and crests are assemblies of a few dozen of these. Written as calls
// they are a wall of float literals; written as a table the shape of the piece
// is legible and a number can be nudged without hunting through builder code.
struct GeneratedPrimitiveSpec {
  enum class Shape : std::uint8_t {
    // `center` and `radius`.
    Sphere,
    // `center` and `extent` as the three radii.
    Ellipsoid,
    // `from` to `to`, with `radius`.
    Cylinder,
    // `from` (base) to `to` (tip), with `radius` at the base.
    Cone,
    // `center` and `extent` as the half-sizes.
    Box,
  };

  Shape shape{Shape::Sphere};
  SpecPoint center;
  SpecPoint to;
  SpecPoint extent;
  float radius{0.0F};
  std::uint8_t palette_slot{0U};
  float alpha{1.0F};
  int material_id{0};
};

// Expands an authored table onto the end of a primitive list.
void append_specs(std::vector<GeneratedEquipmentPrimitive>& primitives,
                  std::span<const GeneratedPrimitiveSpec> specs);

} // namespace Render::GL
