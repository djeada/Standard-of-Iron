#pragma once
#include <QMatrix4x4>
#include <QVector3D>
namespace Render::GL { class Mesh; class Texture; class Renderer; }
namespace Render::GL {
struct Submitter {
    Renderer* renderer{};
    void meshColored(Mesh* mesh, const QMatrix4x4& model, const QVector3D& color, Texture* tex=nullptr);
};
}
#pragma once

#include <QMatrix4x4>
#include <QVector3D>
#include "draw_queue.h"

namespace Render::GL { class Mesh; class Texture; class Renderer; }

namespace Render {

class Submitter {
public:
    explicit Submitter(DrawQueue* queue) : m_queue(queue) {}

    void mesh(Render::GL::Mesh* mesh, const QMatrix4x4& model, const QVector3D& color,
              Render::GL::Texture* tex = nullptr) {
        if (!m_queue) return;
        DrawItem it; it.mesh = mesh; it.texture = tex; it.model = model; it.color = color;
        m_queue->add(it);
    }

private:
    DrawQueue* m_queue = nullptr;
};

} // namespace Render
