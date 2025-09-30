#include "barracks.h"
#include "../core/world.h"
#include "../core/component.h"
#include "../visuals/team_colors.h"

namespace Game { namespace Units {

Barracks::Barracks(Engine::Core::World& world)
    : Unit(world, "barracks") {}

std::unique_ptr<Barracks> Barracks::Create(Engine::Core::World& world, const SpawnParams& params) {
    auto unit = std::unique_ptr<Barracks>(new Barracks(world));
    unit->init(params);
    return unit;
}

void Barracks::init(const SpawnParams& params) {
    auto* e = m_world->createEntity();
    m_id = e->getId();

    // Transform: bigger static building
    m_t = e->addComponent<Engine::Core::TransformComponent>();
    m_t->position = {params.position.x(), params.position.y(), params.position.z()};
    m_t->scale = {1.8f, 1.2f, 1.8f};

    // Renderable: use generic cube mesh and team color
    m_r = e->addComponent<Engine::Core::RenderableComponent>("", "");
    m_r->visible = true;
    m_r->mesh = Engine::Core::RenderableComponent::MeshKind::Cube;

    m_u = e->addComponent<Engine::Core::UnitComponent>();
    m_u->unitType = m_type;
    m_u->health = 600; m_u->maxHealth = 600; m_u->speed = 0.0f; // static
    m_u->ownerId = params.playerId;

    QVector3D tc = Game::Visuals::teamColorForOwner(m_u->ownerId);
    m_r->color[0] = tc.x(); m_r->color[1] = tc.y(); m_r->color[2] = tc.z();

    // Tag as building (affects selection/display)
    e->addComponent<Engine::Core::BuildingComponent>();

    // Production capability: produces archers, capped by maxUnits
    if (auto* prod = e->addComponent<Engine::Core::ProductionComponent>()) {
        prod->productType = "archer";
        prod->buildTime = 10.0f;  // seconds per archer
        prod->maxUnits = 100;     // cap total archers produced by this barracks
        prod->inProgress = false; // wait for order
        prod->timeRemaining = 0.0f;
        prod->producedCount = 0;
        prod->rallyX = m_t->position.x + 4.0f; // basic rally default
        prod->rallyZ = m_t->position.z + 2.0f;
        prod->rallySet = true; // default rally is set near building
    }
}

} } // namespace Game::Units
