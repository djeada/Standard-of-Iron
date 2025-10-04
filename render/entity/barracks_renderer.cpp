#include "barracks_renderer.h"
#include "registry.h"
#include "../gl/resources.h"
#include "../geom/flag.h"
#include "../../game/core/component.h"

namespace Render::GL {

static void drawBarracks(const DrawContext& p, ISubmitter& out) {
    if (!p.resources || !p.entity) return;
    auto* t = p.entity->getComponent<Engine::Core::TransformComponent>();
    auto* r = p.entity->getComponent<Engine::Core::RenderableComponent>();
    auto* u = p.entity->getComponent<Engine::Core::UnitComponent>();
    if (!t || !r) return;

    // Palette
    const QVector3D stone(0.55f, 0.55f, 0.55f);
    const QVector3D wood(0.50f, 0.33f, 0.18f);
    const QVector3D woodDark(0.35f, 0.23f, 0.12f);
    const QVector3D thatch(0.85f, 0.72f, 0.25f);
    const QVector3D doorColor(0.25f, 0.18f, 0.10f);
    const QVector3D crate(0.45f, 0.30f, 0.14f);
    const QVector3D path(0.60f, 0.58f, 0.50f);
    const QVector3D team = QVector3D(r->color[0], r->color[1], r->color[2]);

    // Stone foundation slab (slightly larger footprint)
    QMatrix4x4 foundation = p.model;
    foundation.translate(0.0f, -0.15f, 0.0f);
    foundation.scale(1.35f, 0.10f, 1.2f);
    out.mesh(p.resources->unit(), foundation, stone, p.resources->white(), 1.0f);

    // Wooden walls (main volume)
    QMatrix4x4 walls = p.model;
    walls.scale(1.15f, 0.65f, 0.95f);
    out.mesh(p.resources->unit(), walls, wood, p.resources->white(), 1.0f);

    // Timber beams: vertical corners
    auto drawBeam = [&](float x, float z) {
        QMatrix4x4 b = p.model;
        b.translate(x, 0.20f, z);
        b.scale(0.06f, 0.75f, 0.06f);
           out.mesh(p.resources->unit(), b, woodDark, p.resources->white(), 1.0f);
    };
    drawBeam( 0.9f,  0.7f);
    drawBeam(-0.9f,  0.7f);
    drawBeam( 0.9f, -0.7f);
    drawBeam(-0.9f, -0.7f);

    // Pitched thatch roof: layered to suggest a gable
    QMatrix4x4 roofBase = p.model;
    roofBase.translate(0.0f, 0.55f, 0.0f);
    roofBase.scale(1.25f, 0.18f, 1.05f);
    out.mesh(p.resources->unit(), roofBase, thatch, p.resources->white(), 1.0f);

    QMatrix4x4 roofMid = p.model;
    roofMid.translate(0.0f, 0.72f, 0.0f);
    roofMid.scale(1.05f, 0.14f, 0.95f);
    out.mesh(p.resources->unit(), roofMid, thatch, p.resources->white(), 1.0f);

    QMatrix4x4 roofRidge = p.model;
    roofRidge.translate(0.0f, 0.86f, 0.0f);
    roofRidge.scale(0.85f, 0.12f, 0.85f);
    out.mesh(p.resources->unit(), roofRidge, thatch, p.resources->white(), 1.0f);

    // Roof ridge beam
    QMatrix4x4 ridge = p.model;
    ridge.translate(0.0f, 0.96f, 0.0f);
    ridge.scale(1.1f, 0.04f, 0.12f);
    out.mesh(p.resources->unit(), ridge, woodDark, p.resources->white(), 1.0f);

    // Door at front
    QMatrix4x4 door = p.model;
    door.translate(0.0f, -0.02f, 0.62f);
    door.scale(0.25f, 0.38f, 0.06f);
    out.mesh(p.resources->unit(), door, doorColor, p.resources->white(), 1.0f);

    // Side annex (shed) on the right
    QMatrix4x4 annex = p.model;
    annex.translate(0.95f, -0.05f, -0.15f);
    annex.scale(0.55f, 0.45f, 0.55f);
    out.mesh(p.resources->unit(), annex, wood, p.resources->white(), 1.0f);

    QMatrix4x4 annexRoof = p.model;
    annexRoof.translate(0.95f, 0.30f, -0.15f);
    annexRoof.scale(0.60f, 0.12f, 0.60f);
    out.mesh(p.resources->unit(), annexRoof, thatch, p.resources->white(), 1.0f);

    // Chimney on the back-left
    QMatrix4x4 chimney = p.model;
    chimney.translate(-0.65f, 0.75f, -0.55f);
    chimney.scale(0.10f, 0.35f, 0.10f);
    out.mesh(p.resources->unit(), chimney, stone, p.resources->white(), 1.0f);

    QMatrix4x4 chimneyCap = p.model;
    chimneyCap.translate(-0.65f, 0.95f, -0.55f);
    chimneyCap.scale(0.16f, 0.05f, 0.16f);
    out.mesh(p.resources->unit(), chimneyCap, stone, p.resources->white(), 1.0f);

    // Path stones leading from door
    auto drawPaver = [&](float ox, float oz, float sx, float sz) {
        QMatrix4x4 paver = p.model;
        paver.translate(ox, -0.14f, oz);
        paver.scale(sx, 0.02f, sz);
    out.mesh(p.resources->unit(), paver, path, p.resources->white(), 1.0f);
    };
    drawPaver( 0.0f,  0.9f, 0.25f, 0.20f);
    drawPaver( 0.0f,  1.15f, 0.22f, 0.18f);
    drawPaver( 0.0f,  1.35f, 0.20f, 0.16f);

    // Crates near the door
    QMatrix4x4 crate1 = p.model; crate1.translate(0.45f, -0.05f, 0.55f); crate1.scale(0.18f, 0.18f, 0.18f);
    out.mesh(p.resources->unit(), crate1, crate, p.resources->white(), 1.0f);
    QMatrix4x4 crate2 = p.model; crate2.translate(0.58f, 0.02f, 0.45f); crate2.scale(0.14f, 0.14f, 0.14f);
    out.mesh(p.resources->unit(), crate2, crate, p.resources->white(), 1.0f);

    // Simple fence posts on front-left
    for (int i=0;i<3;++i) {
        QMatrix4x4 post = p.model;
        post.translate(-0.85f + 0.18f * i, -0.05f, 0.85f);
        post.scale(0.05f, 0.25f, 0.05f);
    out.mesh(p.resources->unit(), post, woodDark, p.resources->white(), 1.0f);
    }

    // Banner pole with team-colored flag (improved proportions)
    QMatrix4x4 pole = p.model;
    pole.translate(-0.9f, 0.55f, -0.55f);
    pole.scale(0.04f, 0.8f, 0.04f);
    out.mesh(p.resources->unit(), pole, woodDark, p.resources->white(), 1.0f);

    // Flag finial (decorative top)
    QMatrix4x4 finial = p.model;
    finial.translate(-0.9f, 0.98f, -0.55f);
    finial.scale(0.08f, 0.08f, 0.08f);
    out.mesh(p.resources->unit(), finial, QVector3D(0.9f, 0.8f, 0.3f), p.resources->white(), 1.0f);

    // Team banner (taller and narrower, more flag-like)
    QMatrix4x4 banner = p.model;
    banner.translate(-0.78f, 0.75f, -0.53f);
    banner.scale(0.28f, 0.32f, 0.02f);
    out.mesh(p.resources->unit(), banner, team, p.resources->white(), 1.0f);
    
    // Banner trim/border (darker accent)
    QMatrix4x4 bannerTrim = p.model;
    bannerTrim.translate(-0.78f, 0.75f, -0.51f);
    bannerTrim.scale(0.30f, 0.04f, 0.01f);
    out.mesh(p.resources->unit(), bannerTrim, QVector3D(team.x() * 0.6f, team.y() * 0.6f, team.z() * 0.6f), p.resources->white(), 1.0f);

    // Rally flag if set (using Flag utility)
    if (auto* prod = p.entity->getComponent<Engine::Core::ProductionComponent>()) {
        if (prod->rallySet && p.resources) {
            auto flag = Render::Geom::Flag::create(
                prod->rallyX, 
                prod->rallyZ,
                QVector3D(1.0f, 0.9f, 0.2f),  // Yellow rally flag
                woodDark,                      // Dark wood pole
                1.0f                           // Normal scale
            );
            
            out.mesh(p.resources->unit(), flag.pole, flag.poleColor, p.resources->white(), 1.0f);
            out.mesh(p.resources->unit(), flag.pennant, flag.pennantColor, p.resources->white(), 1.0f);
            out.mesh(p.resources->unit(), flag.finial, flag.pennantColor, p.resources->white(), 1.0f);
        }
    }

    // Health bar for barracks (positioned above the building)
    if (u && u->maxHealth > 0) {
        float healthRatio = float(u->health) / float(u->maxHealth);
        QVector3D pos = p.model.column(3).toVector3D();
        
        // Health bar background (dark red)
        QMatrix4x4 bgBar;
        bgBar.translate(pos.x(), 1.2f, pos.z()); // Above the building
        bgBar.scale(1.5f, 0.08f, 0.04f); // Wide horizontal bar
        out.mesh(p.resources->unit(), bgBar, QVector3D(0.3f, 0.1f, 0.1f), p.resources->white(), 1.0f);
        
        // Health bar foreground (green to red based on health)
        QVector3D healthColor;
        if (healthRatio > 0.6f) {
            healthColor = QVector3D(0.2f, 0.8f, 0.2f); // Green
        } else if (healthRatio > 0.3f) {
            healthColor = QVector3D(0.9f, 0.7f, 0.2f); // Orange
        } else {
            healthColor = QVector3D(0.9f, 0.2f, 0.2f); // Red
        }
        
        QMatrix4x4 fgBar;
        fgBar.translate(pos.x() - 0.75f + (0.75f * healthRatio), 1.21f, pos.z());
        fgBar.scale(1.5f * healthRatio, 0.06f, 0.03f);
        out.mesh(p.resources->unit(), fgBar, healthColor, p.resources->white(), 1.0f);
    }
    
    // Selection smoke if selected (twice the previous diameter ~ scale *2.0)
    if (p.selected) {
        QMatrix4x4 m;
        QVector3D pos = p.model.column(3).toVector3D();
        // Force ground-level placement; ignore model's Y height for discs
        m.translate(pos.x(), 0.0f, pos.z());
        m.scale(1.8f, 1.0f, 1.8f); // previously ~0.9, now doubled
    out.selectionSmoke(m, QVector3D(0.2f, 0.8f, 0.2f), 0.32f);
    }
    // Hover smoke if hovered and not selected (subtle)
    else if (p.hovered) {
        QMatrix4x4 m;
        QVector3D pos = p.model.column(3).toVector3D();
        m.translate(pos.x(), 0.0f, pos.z());
        m.scale(1.8f, 1.0f, 1.8f);
    out.selectionSmoke(m, QVector3D(0.90f, 0.90f, 0.25f), 0.20f);
    }
}

void registerBarracksRenderer(EntityRendererRegistry& registry) {
    registry.registerRenderer("barracks", drawBarracks);
}

} // namespace Render::GL