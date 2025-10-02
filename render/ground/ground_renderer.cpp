#include "ground_renderer.h"
#include "../scene_renderer.h"
#include "../gl/resources.h"

namespace Render { namespace GL {

void GroundRenderer::recomputeModel() {
    m_model.setToIdentity();
    if (m_width > 0 && m_height > 0) {
        float scaleX = float(m_width) * m_tileSize * 0.5f;
        float scaleZ = float(m_height) * m_tileSize * 0.5f;
        m_model.scale(scaleX, 1.0f, scaleZ);
    } else {
        m_model.scale(m_extent, 1.0f, m_extent);
    }
}

void GroundRenderer::submit(Renderer& renderer, ResourceManager& resources) {
    if (auto* plane = resources.ground()) {
        renderer.queueMeshColored(plane, m_model, m_color, resources.white());
    }
}

} }
