#pragma once

#include "draw_queue.h"
#include "gl/primitives.h"
#include <QMatrix4x4>
#include <QVector3D>

namespace Render::GL {
class Mesh;
class Texture;
} 

namespace Render::GL {

class ISubmitter {
public:
  virtual ~ISubmitter() = default;
  virtual void mesh(Mesh *mesh, const QMatrix4x4 &model, const QVector3D &color,
                    Texture *tex = nullptr, float alpha = 1.0f) = 0;
  virtual void cylinder(const QVector3D &start, const QVector3D &end,
                        float radius, const QVector3D &color,
                        float alpha = 1.0f) = 0;
  virtual void selectionRing(const QMatrix4x4 &model, float alphaInner,
                             float alphaOuter, const QVector3D &color) = 0;
  virtual void grid(const QMatrix4x4 &model, const QVector3D &color,
                    float cellSize, float thickness, float extent) = 0;
  virtual void selectionSmoke(const QMatrix4x4 &model, const QVector3D &color,
                              float baseAlpha = 0.15f) = 0;
};

namespace detail {
inline bool decomposeUnitCylinder(const QMatrix4x4 &model, QVector3D &start,
                                  QVector3D &end, float &radius) {
  start = model.map(QVector3D(0.0f, -0.5f, 0.0f));
  end = model.map(QVector3D(0.0f, 0.5f, 0.0f));
  QVector3D sx = model.mapVector(QVector3D(1.0f, 0.0f, 0.0f));
  QVector3D sz = model.mapVector(QVector3D(0.0f, 0.0f, 1.0f));
  radius = 0.5f * (sx.length() + sz.length());
  return radius > 0.0f;
}
} 

class QueueSubmitter : public ISubmitter {
public:
  explicit QueueSubmitter(DrawQueue *queue) : m_queue(queue) {}

  void setShader(Shader *shader) { m_shader = shader; }

  void mesh(Mesh *mesh, const QMatrix4x4 &model, const QVector3D &color,
            Texture *tex = nullptr, float alpha = 1.0f) override {
    if (!m_queue || !mesh)
      return;

    if (mesh == getUnitCylinder() && (!tex) && (!m_shader)) {
      QVector3D start, end;
      float radius = 0.0f;
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
    cmd.shader = m_shader;
    m_queue->submit(cmd);
  }
  void cylinder(const QVector3D &start, const QVector3D &end, float radius,
                const QVector3D &color, float alpha = 1.0f) override {
    if (!m_queue)
      return;
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
    if (!m_queue)
      return;
    SelectionRingCmd cmd;
    cmd.model = model;
    cmd.alphaInner = alphaInner;
    cmd.alphaOuter = alphaOuter;
    cmd.color = color;
    m_queue->submit(cmd);
  }
  void grid(const QMatrix4x4 &model, const QVector3D &color, float cellSize,
            float thickness, float extent) override {
    if (!m_queue)
      return;
    GridCmd cmd;
    cmd.model = model;
    cmd.color = color;
    cmd.cellSize = cellSize;
    cmd.thickness = thickness;
    cmd.extent = extent;
    m_queue->submit(cmd);
  }
  void selectionSmoke(const QMatrix4x4 &model, const QVector3D &color,
                      float baseAlpha = 0.15f) override {
    if (!m_queue)
      return;
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

} 
