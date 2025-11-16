#pragma once

#include "draw_queue.h"
#include "gl/primitives.h"
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
                    Texture *tex = nullptr, float alpha = 1.0F, int materialId = 0) = 0;
  virtual void cylinder(const QVector3D &start, const QVector3D &end,
                        float radius, const QVector3D &color,
                        float alpha = 1.0F) = 0;
  virtual void selectionRing(const QMatrix4x4 &model, float alphaInner,
                             float alphaOuter, const QVector3D &color) = 0;
  virtual void grid(const QMatrix4x4 &model, const QVector3D &color,
                    float cellSize, float thickness, float extent) = 0;
  virtual void selectionSmoke(const QMatrix4x4 &model, const QVector3D &color,
                              float baseAlpha = 0.15F) = 0;
};

namespace detail {
inline auto decomposeUnitCylinder(const QMatrix4x4 &model, QVector3D &start,
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

  void setShader(Shader *shader) { m_shader = shader; }

  void mesh(Mesh *mesh, const QMatrix4x4 &model, const QVector3D &color,
            Texture *tex = nullptr, float alpha = 1.0F, int materialId = 0) override {
    if ((m_queue == nullptr) || (mesh == nullptr)) {
      return;
    }

    if (mesh == getUnitCylinder() && (tex == nullptr) &&
        (m_shader == nullptr)) {
      QVector3D start;
      QVector3D end;
      float radius = 0.0F;
      if (detail::decomposeUnitCylinder(model, start, end, radius)) {
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
    cmd.materialId = materialId;
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
  void selectionRing(const QMatrix4x4 &model, float alphaInner,
                     float alphaOuter, const QVector3D &color) override {
    if (m_queue == nullptr) {
      return;
    }
    SelectionRingCmd cmd;
    cmd.model = model;
    cmd.alphaInner = alphaInner;
    cmd.alphaOuter = alphaOuter;
    cmd.color = color;
    m_queue->submit(cmd);
  }
  void grid(const QMatrix4x4 &model, const QVector3D &color, float cellSize,
            float thickness, float extent) override {
    if (m_queue == nullptr) {
      return;
    }
    GridCmd cmd;
    cmd.model = model;
    cmd.color = color;
    cmd.cellSize = cellSize;
    cmd.thickness = thickness;
    cmd.extent = extent;
    m_queue->submit(cmd);
  }
  void selectionSmoke(const QMatrix4x4 &model, const QVector3D &color,
                      float baseAlpha = 0.15F) override {
    if (m_queue == nullptr) {
      return;
    }
    SelectionSmokeCmd cmd;
    cmd.model = model;
    cmd.color = color;
    cmd.baseAlpha = baseAlpha;
    m_queue->submit(cmd);
  }

private:
  DrawQueue *m_queue = nullptr;
  Shader *m_shader = nullptr;
};

} // namespace Render::GL
