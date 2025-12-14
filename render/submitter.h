#pragma once

#include "draw_queue.h"
#include "gl/primitives.h"
#include "primitive_batch.h"
#include <QMatrix4x4>
#include <QVector3D>

namespace Render::GL {
class Mesh;
class Texture;
} // namespace Render::GL

namespace Render::GL {

class ISubmitter {
public:
  virtual ~ISubmitter() = default;
  virtual void mesh(Mesh *mesh, const QMatrix4x4 &model, const QVector3D &color,
                    Texture *tex = nullptr, float alpha = 1.0F,
                    int material_id = 0) = 0;
  virtual void cylinder(const QVector3D &start, const QVector3D &end,
                        float radius, const QVector3D &color,
                        float alpha = 1.0F) = 0;
  virtual void selection_ring(const QMatrix4x4 &model, float alpha_inner,
                              float alpha_outer, const QVector3D &color) = 0;
  virtual void grid(const QMatrix4x4 &model, const QVector3D &color,
                    float cell_size, float thickness, float extent) = 0;
  virtual void selection_smoke(const QMatrix4x4 &model, const QVector3D &color,
                               float base_alpha = 0.15F) = 0;
  virtual void healing_beam(const QVector3D &start, const QVector3D &end,
                            const QVector3D &color, float progress,
                            float beam_width, float intensity, float time) = 0;
  virtual void healer_aura(const QVector3D &position, const QVector3D &color,
                           float radius, float intensity, float time) = 0;
  virtual void combat_dust(const QVector3D &position, const QVector3D &color,
                           float radius, float intensity, float time) = 0;
};

namespace detail {
inline auto decompose_unit_cylinder(const QMatrix4x4 &model, QVector3D &start,
                                    QVector3D &end, float &radius) -> bool {
  start = model.map(QVector3D(0.0F, -0.5F, 0.0F));
  end = model.map(QVector3D(0.0F, 0.5F, 0.0F));
  QVector3D const sx = model.mapVector(QVector3D(1.0F, 0.0F, 0.0F));
  QVector3D const sz = model.mapVector(QVector3D(0.0F, 0.0F, 1.0F));
  radius = 0.5F * (sx.length() + sz.length());
  return radius > 0.0F;
}
} // namespace detail

class QueueSubmitter : public ISubmitter {
public:
  explicit QueueSubmitter(DrawQueue *queue) : m_queue(queue) {}

  [[nodiscard]] Shader *shader() const { return m_shader; }
  void set_shader(Shader *shader) { m_shader = shader; }

  void mesh(Mesh *mesh, const QMatrix4x4 &model, const QVector3D &color,
            Texture *tex = nullptr, float alpha = 1.0F,
            int material_id = 0) override {
    if ((m_queue == nullptr) || (mesh == nullptr)) {
      return;
    }

    if (mesh == get_unit_cylinder() && (tex == nullptr) &&
        (m_shader == nullptr)) {
      QVector3D start;
      QVector3D end;
      float radius = 0.0F;
      if (detail::decompose_unit_cylinder(model, start, end, radius)) {
        CylinderCmd cyl;
        cyl.start = start;
        cyl.end = end;
        cyl.radius = radius;
        cyl.color = color;
        cyl.alpha = alpha;
        m_queue->submit(cyl);
        return;
      }
    }
    MeshCmd cmd;
    cmd.mesh = mesh;
    cmd.texture = tex;
    cmd.model = model;
    cmd.color = color;
    cmd.alpha = alpha;
    cmd.material_id = material_id;
    cmd.shader = m_shader;
    m_queue->submit(cmd);
  }
  void cylinder(const QVector3D &start, const QVector3D &end, float radius,
                const QVector3D &color, float alpha = 1.0F) override {
    if (m_queue == nullptr) {
      return;
    }
    CylinderCmd cmd;
    cmd.start = start;
    cmd.end = end;
    cmd.radius = radius;
    cmd.color = color;
    cmd.alpha = alpha;
    m_queue->submit(cmd);
  }
  void selection_ring(const QMatrix4x4 &model, float alpha_inner,
                      float alpha_outer, const QVector3D &color) override {
    if (m_queue == nullptr) {
      return;
    }
    SelectionRingCmd cmd;
    cmd.model = model;
    cmd.alpha_inner = alpha_inner;
    cmd.alpha_outer = alpha_outer;
    cmd.color = color;
    m_queue->submit(cmd);
  }
  void grid(const QMatrix4x4 &model, const QVector3D &color, float cell_size,
            float thickness, float extent) override {
    if (m_queue == nullptr) {
      return;
    }
    GridCmd cmd;
    cmd.model = model;
    cmd.color = color;
    cmd.cell_size = cell_size;
    cmd.thickness = thickness;
    cmd.extent = extent;
    m_queue->submit(cmd);
  }
  void selection_smoke(const QMatrix4x4 &model, const QVector3D &color,
                       float base_alpha = 0.15F) override {
    if (m_queue == nullptr) {
      return;
    }
    SelectionSmokeCmd cmd;
    cmd.model = model;
    cmd.color = color;
    cmd.base_alpha = base_alpha;
    m_queue->submit(cmd);
  }
  void healing_beam(const QVector3D &start, const QVector3D &end,
                    const QVector3D &color, float progress, float beam_width,
                    float intensity, float time) override {
    if (m_queue == nullptr) {
      return;
    }
    HealingBeamCmd cmd;
    cmd.start_pos = start;
    cmd.end_pos = end;
    cmd.color = color;
    cmd.progress = progress;
    cmd.beam_width = beam_width;
    cmd.intensity = intensity;
    cmd.time = time;
    m_queue->submit(cmd);
  }
  void healer_aura(const QVector3D &position, const QVector3D &color,
                   float radius, float intensity, float time) override {
    if (m_queue == nullptr) {
      return;
    }
    HealerAuraCmd cmd;
    cmd.position = position;
    cmd.color = color;
    cmd.radius = radius;
    cmd.intensity = intensity;
    cmd.time = time;
    m_queue->submit(cmd);
  }
  void combat_dust(const QVector3D &position, const QVector3D &color,
                   float radius, float intensity, float time) override {
    if (m_queue == nullptr) {
      return;
    }
    CombatDustCmd cmd;
    cmd.position = position;
    cmd.color = color;
    cmd.radius = radius;
    cmd.intensity = intensity;
    cmd.time = time;
    m_queue->submit(cmd);
  }

private:
  DrawQueue *m_queue = nullptr;
  Shader *m_shader = nullptr;
};

class BatchingSubmitter : public ISubmitter {
public:
  explicit BatchingSubmitter(ISubmitter *fallback,
                             PrimitiveBatcher *batcher = nullptr)
      : m_fallback(fallback), m_batcher(batcher) {}

  [[nodiscard]] ISubmitter *fallback_submitter() const { return m_fallback; }

  void set_batcher(PrimitiveBatcher *batcher) { m_batcher = batcher; }
  void set_enabled(bool enabled) { m_enabled = enabled; }

  void mesh(Mesh *mesh, const QMatrix4x4 &model, const QVector3D &color,
            Texture *tex = nullptr, float alpha = 1.0F,
            int material_id = 0) override {

    if (m_enabled && m_batcher != nullptr && tex == nullptr) {
      if (mesh == get_unit_sphere()) {
        m_batcher->add_sphere(model, color, alpha);
        return;
      }
      if (mesh == get_unit_cylinder()) {
        m_batcher->add_cylinder(model, color, alpha);
        return;
      }
      if (mesh == get_unit_cone()) {
        m_batcher->add_cone(model, color, alpha);
        return;
      }
    }

    if (m_fallback != nullptr) {
      m_fallback->mesh(mesh, model, color, tex, alpha, material_id);
    }
  }

  void cylinder(const QVector3D &start, const QVector3D &end, float radius,
                const QVector3D &color, float alpha = 1.0F) override {

    if (m_fallback != nullptr) {
      m_fallback->cylinder(start, end, radius, color, alpha);
    }
  }

  void selection_ring(const QMatrix4x4 &model, float alpha_inner,
                      float alpha_outer, const QVector3D &color) override {
    if (m_fallback != nullptr) {
      m_fallback->selection_ring(model, alpha_inner, alpha_outer, color);
    }
  }

  void grid(const QMatrix4x4 &model, const QVector3D &color, float cell_size,
            float thickness, float extent) override {
    if (m_fallback != nullptr) {
      m_fallback->grid(model, color, cell_size, thickness, extent);
    }
  }

  void selection_smoke(const QMatrix4x4 &model, const QVector3D &color,
                       float base_alpha = 0.15F) override {
    if (m_fallback != nullptr) {
      m_fallback->selection_smoke(model, color, base_alpha);
    }
  }

  void healing_beam(const QVector3D &start, const QVector3D &end,
                    const QVector3D &color, float progress, float beam_width,
                    float intensity, float time) override {
    if (m_fallback != nullptr) {
      m_fallback->healing_beam(start, end, color, progress, beam_width,
                               intensity, time);
    }
  }

  void healer_aura(const QVector3D &position, const QVector3D &color,
                   float radius, float intensity, float time) override {
    if (m_fallback != nullptr) {
      m_fallback->healer_aura(position, color, radius, intensity, time);
    }
  }

  void combat_dust(const QVector3D &position, const QVector3D &color,
                   float radius, float intensity, float time) override {
    if (m_fallback != nullptr) {
      m_fallback->combat_dust(position, color, radius, intensity, time);
    }
  }

private:
  ISubmitter *m_fallback = nullptr;
  PrimitiveBatcher *m_batcher = nullptr;
  bool m_enabled = true;
};

} // namespace Render::GL
