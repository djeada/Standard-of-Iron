#include "primitive_batch.h"

namespace Render::GL {

static PrimitiveBatchStats s_batchStats;

PrimitiveBatcher::PrimitiveBatcher() {

  m_spheres.reserve(1024);
  m_cylinders.reserve(2048);
  m_cones.reserve(512);
}

PrimitiveBatcher::~PrimitiveBatcher() = default;

void PrimitiveBatcher::addSphere(const QMatrix4x4 &transform,
                                 const QVector3D &color, float alpha) {
  PrimitiveInstanceGpu inst;
  inst.setTransform(transform);
  inst.setColor(color, alpha);
  m_spheres.push_back(inst);
  ++s_batchStats.spheresSubmitted;
}

void PrimitiveBatcher::addCylinder(const QMatrix4x4 &transform,
                                   const QVector3D &color, float alpha) {
  PrimitiveInstanceGpu inst;
  inst.setTransform(transform);
  inst.setColor(color, alpha);
  m_cylinders.push_back(inst);
  ++s_batchStats.cylindersSubmitted;
}

void PrimitiveBatcher::addCone(const QMatrix4x4 &transform,
                               const QVector3D &color, float alpha) {
  PrimitiveInstanceGpu inst;
  inst.setTransform(transform);
  inst.setColor(color, alpha);
  m_cones.push_back(inst);
  ++s_batchStats.conesSubmitted;
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

auto getPrimitiveBatchStats() -> const PrimitiveBatchStats & {
  return s_batchStats;
}

void resetPrimitiveBatchStats() { s_batchStats.reset(); }

} // namespace Render::GL
