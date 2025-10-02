#pragma once

#include <QMatrix4x4>
#include <QVector3D>
#include "draw_queue.h"

namespace Render::GL { class Mesh; class Texture; }

namespace Render::GL {

class ISubmitter {
public:
    virtual ~ISubmitter() = default;
    virtual void mesh(Mesh* mesh, const QMatrix4x4& model, const QVector3D& color,
                      Texture* tex = nullptr, float alpha = 1.0f) = 0;
    virtual void selectionRing(const QMatrix4x4& model, float alphaInner, float alphaOuter,
                               const QVector3D& color) = 0;
    // Optional grid overlay submission
    virtual void grid(const QMatrix4x4& model, const QVector3D& color,
                      float cellSize, float thickness) = 0;
};

class QueueSubmitter : public ISubmitter {
public:
    explicit QueueSubmitter(DrawQueue* queue) : m_queue(queue) {}
    void mesh(Mesh* mesh, const QMatrix4x4& model, const QVector3D& color,
              Texture* tex = nullptr, float alpha = 1.0f) override {
        if (!m_queue || !mesh) return;
        MeshCmd cmd; cmd.mesh = mesh; cmd.texture = tex; cmd.model = model; cmd.color = color; cmd.alpha = alpha;
        m_queue->submit(cmd);
    }
    void selectionRing(const QMatrix4x4& model, float alphaInner, float alphaOuter,
                       const QVector3D& color) override {
        if (!m_queue) return;
        SelectionRingCmd cmd; cmd.model = model; cmd.alphaInner = alphaInner; cmd.alphaOuter = alphaOuter; cmd.color = color;
        m_queue->submit(cmd);
    }
    void grid(const QMatrix4x4& model, const QVector3D& color,
              float cellSize, float thickness) override {
        if (!m_queue) return;
        GridCmd cmd; cmd.model = model; cmd.color = color; cmd.cellSize = cellSize; cmd.thickness = thickness;
        m_queue->submit(cmd);
    }
private:
    DrawQueue* m_queue = nullptr;
};

} // namespace Render::GL
