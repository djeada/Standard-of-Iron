#include "render/equipment/generated_primitive_spec.h"

namespace Render::GL {

namespace {
[[nodiscard]] auto to_vector(const SpecPoint& point) noexcept -> QVector3D {
  return {point.x, point.y, point.z};
}
} // namespace

void append_specs(std::vector<GeneratedEquipmentPrimitive>& primitives,
                  std::span<const GeneratedPrimitiveSpec> specs) {
  primitives.reserve(primitives.size() + specs.size());
  for (const auto& spec : specs) {
    switch (spec.shape) {
    case GeneratedPrimitiveSpec::Shape::Sphere:
      primitives.push_back(generated_sphere(to_vector(spec.center),
                                            spec.radius,
                                            spec.palette_slot,
                                            spec.alpha,
                                            spec.material_id));
      break;
    case GeneratedPrimitiveSpec::Shape::Ellipsoid:
      primitives.push_back(generated_ellipsoid(to_vector(spec.center),
                                               to_vector(spec.extent),
                                               spec.palette_slot,
                                               spec.alpha,
                                               spec.material_id));
      break;
    case GeneratedPrimitiveSpec::Shape::Cylinder:
      primitives.push_back(generated_cylinder(to_vector(spec.center),
                                              to_vector(spec.to),
                                              spec.radius,
                                              spec.palette_slot,
                                              spec.alpha,
                                              spec.material_id));
      break;
    case GeneratedPrimitiveSpec::Shape::Cone:
      primitives.push_back(generated_cone(to_vector(spec.center),
                                          to_vector(spec.to),
                                          spec.radius,
                                          spec.palette_slot,
                                          spec.alpha,
                                          spec.material_id));
      break;
    case GeneratedPrimitiveSpec::Shape::Box:
      primitives.push_back(generated_box(to_vector(spec.center),
                                         to_vector(spec.extent),
                                         spec.palette_slot,
                                         spec.alpha,
                                         spec.material_id));
      break;
    }
  }
}

} // namespace Render::GL
