#include "map_transformer.h"

#include "../../engine/core/world.h"
#include "../../engine/core/component.h"

namespace Game::Map {

MapRuntime MapTransformer::applyToWorld(const MapDefinition& def, Engine::Core::World& world, const Game::Visuals::VisualCatalog* visuals) {
    MapRuntime rt;
    rt.unitIds.reserve(def.spawns.size());

    for (const auto& s : def.spawns) {
        auto* e = world.createEntity();
        if (!e) continue;

        auto* t = e->addComponent<Engine::Core::TransformComponent>();
        t->position = {s.x, 0.0f, s.z};
        t->scale = {0.5f, 0.5f, 0.5f};

        auto* r = e->addComponent<Engine::Core::RenderableComponent>("", "");
        r->visible = true;
        // Apply visuals from catalog if provided, else use simple defaults
        if (visuals) {
            Game::Visuals::VisualDef def;
            if (visuals->lookup(s.type.toStdString(), def)) {
                Game::Visuals::applyToRenderable(def, *r);
            }
        }
        if (r->color[0] == 0.0f && r->color[1] == 0.0f && r->color[2] == 0.0f) {
            // fallback color if not set
            r->color[0] = r->color[1] = r->color[2] = 1.0f;
        }

        auto* u = e->addComponent<Engine::Core::UnitComponent>();
        u->unitType = s.type.toStdString();
        if (s.type == "archer") {
            u->health = 80; u->maxHealth = 80; u->speed = 3.0f;
        }

        e->addComponent<Engine::Core::MovementComponent>();

        rt.unitIds.push_back(e->getId());
    }

    return rt;
}

} // namespace Game::Map
