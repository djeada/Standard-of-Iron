#include "arrow_system.h"
#include "../../render/scene_renderer.h"
#include "../../render/gl/resources.h"
#include "../../render/geom/arrow.h"
#include <algorithm>

namespace Game::Systems {

ArrowSystem::ArrowSystem() {}

void ArrowSystem::spawnArrow(const QVector3D& start, const QVector3D& end, const QVector3D& color, float speed) {
    ArrowInstance a;
    a.start = start;
    a.end = end;
    a.color = color;
    a.t = 0.0f;
    a.speed = speed;
    a.active = true;
    float dist = (end - start).length();
    a.arcHeight = std::clamp(0.15f * dist, 0.2f, 1.2f);
    m_arrows.push_back(a);
}

void ArrowSystem::update(Engine::Core::World* world, float deltaTime) {
    for (auto& arrow : m_arrows) {
        if (!arrow.active) continue;
        arrow.t += deltaTime * arrow.speed / (arrow.start - arrow.end).length();
        if (arrow.t >= 1.0f) {
            arrow.t = 1.0f;
            arrow.active = false;
        }
    }
    // Remove inactive arrows
    m_arrows.erase(std::remove_if(m_arrows.begin(), m_arrows.end(), [](const ArrowInstance& a){ return !a.active; }), m_arrows.end());
}

} // namespace Game::Systems
