#pragma once

#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Render::GL {

class Buffer;
class Mesh;

struct PrimitiveInstanceGpu {

  QVector4D modelCol0{1.0F, 0.0F, 0.0F, 0.0F};
  QVector4D modelCol1{0.0F, 1.0F, 0.0F, 0.0F};
  QVector4D modelCol2{0.0F, 0.0F, 1.0F, 0.0F};

  QVector4D colorAlpha{1.0F, 1.0F, 1.0F, 1.0F};

  void setTransform(const QMatrix4x4 &m) {

    modelCol0 = QVector4D(m(0, 0), m(1, 0), m(2, 0), m(0, 3));

    modelCol1 = QVector4D(m(0, 1), m(1, 1), m(2, 1), m(1, 3));

    modelCol2 = QVector4D(m(0, 2), m(1, 2), m(2, 2), m(2, 3));
  }

  void setColor(const QVector3D &color, float alpha = 1.0F) {
    colorAlpha = QVector4D(color.x(), color.y(), color.z(), alpha);
  }
};

static_assert(sizeof(PrimitiveInstanceGpu) == 64,
              "PrimitiveInstanceGpu must be 64 bytes for GPU alignment");

struct PrimitiveBatchParams {
  QMatrix4x4 viewProj;
  QVector3D lightDirection{0.35F, 0.8F, 0.45F};
  float ambientStrength{0.3F};
};

enum class PrimitiveType : uint8_t { Sphere = 0, Cylinder = 1, Cone = 2 };

struct PrimitiveBatchCmd {
  PrimitiveType type{PrimitiveType::Sphere};
  std::vector<PrimitiveInstanceGpu> instances;
  PrimitiveBatchParams params;

  [[nodiscard]] auto instanceCount() const -> std::size_t {
    return instances.size();
  }
  [[nodiscard]] auto instanceData() const -> const PrimitiveInstanceGpu * {
    return instances.empty() ? nullptr : instances.data();
  }
};

class PrimitiveBatcher {
public:
  PrimitiveBatcher();
  ~PrimitiveBatcher();

  void addSphere(const QMatrix4x4 &transform, const QVector3D &color,
                 float alpha = 1.0F);
  void addCylinder(const QMatrix4x4 &transform, const QVector3D &color,
                   float alpha = 1.0F);
  void addCone(const QMatrix4x4 &transform, const QVector3D &color,
               float alpha = 1.0F);

  [[nodiscard]] auto sphereCount() const -> std::size_t {
    return m_spheres.size();
  }
  [[nodiscard]] auto cylinderCount() const -> std::size_t {
    return m_cylinders.size();
  }
  [[nodiscard]] auto coneCount() const -> std::size_t { return m_cones.size(); }
  [[nodiscard]] auto totalCount() const -> std::size_t {
    return m_spheres.size() + m_cylinders.size() + m_cones.size();
  }

  [[nodiscard]] auto
  sphereData() const -> const std::vector<PrimitiveInstanceGpu> & {
    return m_spheres;
  }
  [[nodiscard]] auto
  cylinderData() const -> const std::vector<PrimitiveInstanceGpu> & {
    return m_cylinders;
  }
  [[nodiscard]] auto
  coneData() const -> const std::vector<PrimitiveInstanceGpu> & {
    return m_cones;
  }

  void clear();

  void reserve(std::size_t spheres, std::size_t cylinders, std::size_t cones);

private:
  std::vector<PrimitiveInstanceGpu> m_spheres;
  std::vector<PrimitiveInstanceGpu> m_cylinders;
  std::vector<PrimitiveInstanceGpu> m_cones;
};

struct PrimitiveBatchStats {
  uint32_t spheresSubmitted{0};
  uint32_t cylindersSubmitted{0};
  uint32_t conesSubmitted{0};
  uint32_t batchesRendered{0};
  uint32_t drawCallsSaved{0};

  void reset() {
    spheresSubmitted = 0;
    cylindersSubmitted = 0;
    conesSubmitted = 0;
    batchesRendered = 0;
    drawCallsSaved = 0;
  }
};

auto getPrimitiveBatchStats() -> const PrimitiveBatchStats &;
void resetPrimitiveBatchStats();

} // namespace Render::GL
