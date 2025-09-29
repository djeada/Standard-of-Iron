#include "archer_renderer.h"
#include "registry.h"
#include "../gl/renderer.h"
#include "../gl/mesh.h"
#include "../gl/texture.h"
#include "../geom/selection_ring.h"
#include "../../game/visuals/team_colors.h"
#include "../../game/core/entity.h"
#include "../../game/core/component.h"
#include <QVector3D>
#include <vector>
#include <cmath>
#include <memory>

namespace Render::GL {

// Capsule builder local to the archer renderer
static Mesh* createCapsuleMesh() {
    using namespace Render::GL;
    const int radial = 24;
    const int heightSegments = 1;
    const float radius = 0.25f;
    const float halfH = 0.5f;
    std::vector<Vertex> verts;
    std::vector<unsigned int> idx;
    // Sides
    for (int y = 0; y <= heightSegments; ++y) {
        float v = float(y) / float(heightSegments);
        float py = -halfH + v * (2.0f * halfH);
        for (int i = 0; i <= radial; ++i) {
            float u = float(i) / float(radial);
            float ang = u * 6.2831853f;
            float px = radius * std::cos(ang);
            float pz = radius * std::sin(ang);
            QVector3D n(px, 0.0f, pz);
            n.normalize();
            verts.push_back({{px, py, pz}, {n.x(), n.y(), n.z()}, {u, v}});
        }
    }
    int row = radial + 1;
    for (int y = 0; y < heightSegments; ++y) {
        for (int i = 0; i < radial; ++i) {
            int a = y * row + i;
            int b = y * row + i + 1;
            int c = (y + 1) * row + i + 1;
            int d = (y + 1) * row + i;
            idx.push_back(a); idx.push_back(b); idx.push_back(c);
            idx.push_back(c); idx.push_back(d); idx.push_back(a);
        }
    }
    // Top cap
    int baseTop = verts.size();
    verts.push_back({{0.0f, halfH, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.5f, 0.5f}});
    for (int i = 0; i <= radial; ++i) {
        float u = float(i) / float(radial);
        float ang = u * 6.2831853f;
        float px = radius * std::cos(ang);
        float pz = radius * std::sin(ang);
        verts.push_back({{px, halfH, pz}, {0.0f, 1.0f, 0.0f}, {0.5f + 0.5f*std::cos(ang), 0.5f + 0.5f*std::sin(ang)}});
    }
    for (int i = 1; i <= radial; ++i) {
        idx.push_back(baseTop); idx.push_back(baseTop + i); idx.push_back(baseTop + i + 1);
    }
    // Bottom cap
    int baseBot = verts.size();
    verts.push_back({{0.0f, -halfH, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.5f, 0.5f}});
    for (int i = 0; i <= radial; ++i) {
        float u = float(i) / float(radial);
        float ang = u * 6.2831853f;
        float px = radius * std::cos(ang);
        float pz = radius * std::sin(ang);
        verts.push_back({{px, -halfH, pz}, {0.0f, -1.0f, 0.0f}, {0.5f + 0.5f*std::cos(ang), 0.5f + 0.5f*std::sin(ang)}});
    }
    for (int i = 1; i <= radial; ++i) {
        idx.push_back(baseBot); idx.push_back(baseBot + i + 1); idx.push_back(baseBot + i);
    }
    return new Mesh(verts, idx);
}

static Mesh* getArcherCapsule() {
    static std::unique_ptr<Mesh> mesh(createCapsuleMesh());
    return mesh.get();
}

void registerArcherRenderer(EntityRendererRegistry& registry) {
    registry.registerRenderer("archer", [](const DrawParams& p){
        if (!p.renderer) return;
        QVector3D color(0.8f, 0.9f, 1.0f);
        Engine::Core::UnitComponent* unit = nullptr;
        Engine::Core::RenderableComponent* rc = nullptr;
        if (p.entity) {
            unit = p.entity->getComponent<Engine::Core::UnitComponent>();
            rc = p.entity->getComponent<Engine::Core::RenderableComponent>();
        }
        // Prefer team color based on ownerId if available; else fall back to Renderable color
        if (unit && unit->ownerId > 0) {
            color = Game::Visuals::teamColorForOwner(unit->ownerId);
        } else if (rc) {
            color = QVector3D(rc->color[0], rc->color[1], rc->color[2]);
        }
        // Draw capsule (archer body)
        p.renderer->drawMeshColored(getArcherCapsule(), p.model, color, nullptr);
        // Draw selection ring if selected
        if (unit && unit->selected) {
            QMatrix4x4 ringM;
            QVector3D pos = p.model.column(3).toVector3D();
            ringM.translate(pos.x(), 0.01f, pos.z());
            ringM.scale(0.5f, 1.0f, 0.5f);
            p.renderer->drawMeshColored(Render::Geom::SelectionRing::get(), ringM, QVector3D(0.2f, 0.8f, 0.2f), nullptr);
        }
    });
}

} // namespace Render::GL
