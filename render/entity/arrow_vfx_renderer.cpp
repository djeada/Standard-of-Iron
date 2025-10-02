#include "arrow_vfx_renderer.h"
#include "registry.h"
#include "../scene_renderer.h"
#include "../gl/resources.h"
#include "../../game/systems/arrow_system.h"
#include <algorithm>
#include <cmath>

namespace Render::GL {

void renderArrows(Renderer* renderer, ResourceManager* resources, const Game::Systems::ArrowSystem& arrowSystem) {
    if (!renderer || !resources) return;
    auto* arrowMesh = resources->arrow();
    if (!arrowMesh) return;
    const auto& arrows = arrowSystem.arrows();
    for (const auto& arrow : arrows) {
        if (!arrow.active) continue;
        const QVector3D delta = arrow.end - arrow.start;
        const float dist = std::max(0.001f, delta.length());
        QVector3D pos = arrow.start + delta * arrow.t;
        float h = arrow.arcHeight * 4.0f * arrow.t * (1.0f - arrow.t);
        pos.setY(pos.y() + h);
        QMatrix4x4 model;
        model.translate(pos.x(), pos.y(), pos.z());
        QVector3D dir = delta.normalized();
        float yawDeg = std::atan2(dir.x(), dir.z()) * 180.0f / 3.14159265f;
        model.rotate(yawDeg, QVector3D(0,1,0));
        float vy = (arrow.end.y() - arrow.start.y()) / dist;
        float pitchDeg = -std::atan2(vy - (8.0f * arrow.arcHeight * (arrow.t - 0.5f) / dist), 1.0f) * 180.0f / 3.14159265f;
        model.rotate(pitchDeg, QVector3D(1,0,0));
        const float zScale = 0.40f;
        const float xyScale = 0.26f;
        model.translate(0.0f, 0.0f, -zScale * 0.5f);
        model.scale(xyScale, xyScale, zScale);
    renderer->queueMeshColored(arrowMesh, model, arrow.color);
    }
}

} // namespace Render::GL
