#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "render/equipment/generated_equipment.h"

namespace Render::GL {

struct SpecPoint {
  float x{0.0F};
  float y{0.0F};
  float z{0.0F};
};

struct GeneratedPrimitiveSpec {
  enum class Shape : std::uint8_t {

    Sphere,

    Ellipsoid,

    Cylinder,

    Cone,

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

void append_specs(std::vector<GeneratedEquipmentPrimitive>& primitives,
                  std::span<const GeneratedPrimitiveSpec> specs);

} // namespace Render::GL
