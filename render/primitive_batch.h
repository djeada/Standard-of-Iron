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

  QVector4D model_col0{1.0F, 0.0F, 0.0F, 0.0F};
  QVector4D model_col1{0.0F, 1.0F, 0.0F, 0.0F};
  QVector4D model_col2{0.0F, 0.0F, 1.0F, 0.0F};

  QVector4D color_alpha{1.0F, 1.0F, 1.0F, 1.0F};

  void set_transform(const QMatrix4x4 &m) {

    model_col0 = QVector4D(m(0, 0), m(1, 0), m(2, 0), m(0, 3));

    model_col1 = QVector4D(m(0, 1), m(1, 1), m(2, 1), m(1, 3));

    model_col2 = QVector4D(m(0, 2), m(1, 2), m(2, 2), m(2, 3));
  }

  void set_color(const QVector3D &color, float alpha = 1.0F) {
    color_alpha = QVector4D(color.x(), color.y(), color.z(), alpha);
  }
};

static_assert(sizeof(PrimitiveInstanceGpu) == 64,
              "PrimitiveInstanceGpu must be 64 bytes for GPU alignment");

struct PrimitiveBatchParams {
  QMatrix4x4 view_proj;
  QVector3D light_direction{0.35F, 0.8F, 0.45F};
  float ambient_strength{0.3F};
};

enum class PrimitiveType : uint8_t { Sphere = 0, Cylinder = 1, Cone = 2 };

struct PrimitiveBatchCmd {
  PrimitiveType type{PrimitiveType::Sphere};
  std::vector<PrimitiveInstanceGpu> instances;
  PrimitiveBatchParams params;

  [[nodiscard]] auto instance_count() const -> std::size_t {
    return instances.size();
  }
  [[nodiscard]] auto instance_data() const -> const PrimitiveInstanceGpu * {
    return instances.empty() ? nullptr : instances.data();
  }
};

class PrimitiveBatcher {
public:
  PrimitiveBatcher();
  ~PrimitiveBatcher();

  void add_sphere(const QMatrix4x4 &transform, const QVector3D &color,
                  float alpha = 1.0F);
  void add_cylinder(const QMatrix4x4 &transform, const QVector3D &color,
                    float alpha = 1.0F);
  void add_cone(const QMatrix4x4 &transform, const QVector3D &color,
                float alpha = 1.0F);

  [[nodiscard]] auto sphere_count() const -> std::size_t {
    return m_spheres.size();
  }
  [[nodiscard]] auto cylinder_count() const -> std::size_t {
    return m_cylinders.size();
  }
  [[nodiscard]] auto cone_count() const -> std::size_t { return m_cones.size(); }
  [[nodiscard]] auto total_count() const -> std::size_t {
    return m_spheres.size() + m_cylinders.size() + m_cones.size();
  }

  [[nodiscard]] auto
  sphere_data() const -> const std::vector<PrimitiveInstanceGpu> & {
    return m_spheres;
  }
  [[nodiscard]] auto
  cylinder_data() const -> const std::vector<PrimitiveInstanceGpu> & {
    return m_cylinders;
  }
  [[nodiscard]] auto
  cone_data() const -> const std::vector<PrimitiveInstanceGpu> & {
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
  uint32_t spheres_submitted{0};
  uint32_t cylinders_submitted{0};
  uint32_t cones_submitted{0};
  uint32_t batches_rendered{0};
  uint32_t draw_calls_saved{0};

  void reset() {
    spheres_submitted = 0;
    cylinders_submitted = 0;
    cones_submitted = 0;
    batches_rendered = 0;
    draw_calls_saved = 0;
  }
};

auto get_primitive_batch_stats() -> const PrimitiveBatchStats &;
void reset_primitive_batch_stats();

} // namespace Render::GL
