#include "ground_renderer.h"
#include "../scene_renderer.h"
#include "../gl/resources.h"

namespace Render { namespace GL {

void GroundRenderer::recomputeModel() {
    m_model.setToIdentity();
    // Slightly lower ground to avoid z-fighting and occluding unit bases at y≈0
    m_model.translate(0.0f, -0.02f, 0.0f);
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
        renderer.mesh(plane, m_model, m_color, resources.white(), 1.0f);
    }
    if (m_showGrid) {
        renderer.grid(m_model, m_gridColor, m_tileSize, m_gridThickness);
    }
}

} }
