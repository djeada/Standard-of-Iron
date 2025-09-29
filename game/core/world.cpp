#include "world.h"

namespace Engine::Core {

World::World() = default;
World::~World() = default;

Entity* World::createEntity() {
    EntityID id = m_nextEntityId++;
    auto entity = std::make_unique<Entity>(id);
    auto ptr = entity.get();
    m_entities[id] = std::move(entity);
    return ptr;
}

void World::destroyEntity(EntityID id) {
    m_entities.erase(id);
}

Entity* World::getEntity(EntityID id) {
    auto it = m_entities.find(id);
    return it != m_entities.end() ? it->second.get() : nullptr;
}

void World::addSystem(std::unique_ptr<System> system) {
    m_systems.push_back(std::move(system));
}

void World::update(float deltaTime) {
    for (auto& system : m_systems) {
        system->update(this, deltaTime);
    }
}

} // namespace Engine::Core
