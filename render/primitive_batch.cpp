#include "primitive_batch.h"

namespace Render::GL {

static PrimitiveBatchStats s_batchStats;

PrimitiveBatcher::PrimitiveBatcher() {

  m_spheres.reserve(1024);
  m_cylinders.reserve(2048);
  m_cones.reserve(512);
}

PrimitiveBatcher::~PrimitiveBatcher() = default;

void PrimitiveBatcher::add_sphere(const QMatrix4x4 &transform,
                                  const QVector3D &color, float alpha) {
  PrimitiveInstanceGpu inst;
  inst.set_transform(transform);
  inst.set_color(color, alpha);
  m_spheres.push_back(inst);
  ++s_batchStats.spheres_submitted;
}

void PrimitiveBatcher::add_cylinder(const QMatrix4x4 &transform,
                                    const QVector3D &color, float alpha) {
  PrimitiveInstanceGpu inst;
  inst.set_transform(transform);
  inst.set_color(color, alpha);
  m_cylinders.push_back(inst);
  ++s_batchStats.cylinders_submitted;
}

void PrimitiveBatcher::add_cone(const QMatrix4x4 &transform,
                                const QVector3D &color, float alpha) {
  PrimitiveInstanceGpu inst;
  inst.set_transform(transform);
  inst.set_color(color, alpha);
  m_cones.push_back(inst);
  ++s_batchStats.cones_submitted;
}

void PrimitiveBatcher::clear() {
  m_spheres.clear();
  m_cylinders.clear();
  m_cones.clear();
}

void PrimitiveBatcher::reserve(std::size_t spheres, std::size_t cylinders,
                               std::size_t cones) {
  m_spheres.reserve(spheres);
  m_cylinders.reserve(cylinders);
  m_cones.reserve(cones);
}

auto get_primitive_batch_stats() -> const PrimitiveBatchStats & {
  return s_batchStats;
}

void reset_primitive_batch_stats() { s_batchStats.reset(); }

} 
