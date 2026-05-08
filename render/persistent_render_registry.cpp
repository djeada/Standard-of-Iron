#include "persistent_render_registry.h"

#include "../game/core/component.h"
#include "../game/core/entity.h"
#include "../game/core/world.h"
#include <algorithm>
#include <typeindex>

namespace Render {

namespace {

const std::type_index k_renderable_type =
    std::type_index(typeid(Engine::Core::RenderableComponent));
const std::type_index k_unit_type =
    std::type_index(typeid(Engine::Core::UnitComponent));
const std::type_index k_building_type =
    std::type_index(typeid(Engine::Core::BuildingComponent));
const std::type_index k_pending_removal_type =
    std::type_index(typeid(Engine::Core::PendingRemovalComponent));

} // namespace

PersistentRenderRegistry::~PersistentRenderRegistry() { detach(); }

void PersistentRenderRegistry::attach(Engine::Core::World *world) {
  if (m_world == world) {
    return;
  }
  detach();
  if (world == nullptr) {
    return;
  }
  m_world = world;

  // Register observers on the new world.  Lambda captures a raw pointer to
  // this registry.  The registry must outlive the world or call detach() first.
  world->add_component_observer(
      [this](std::uint32_t entity_id, std::type_index type, bool added) {
        if (m_world == nullptr) {
          return;
        }
        on_component_changed(entity_id, type, added);
      });

  world->add_entity_destroyed_observer([this](std::uint32_t entity_id) {
    if (m_world == nullptr) {
      return;
    }
    on_entity_destroyed(entity_id);
  });

  world->add_world_cleared_observer([this] {
    if (m_world == nullptr) {
      return;
    }
    on_world_cleared();
  });

  // Perform initial scan of existing entities.
  for (const auto &[id, entity] : world->get_entities()) {
    if (entity->has_component<Engine::Core::RenderableComponent>()) {
      m_renderable_ids.insert(id);
      reclassify(id);
    }
  }
}

void PersistentRenderRegistry::detach() {
  // Clear all cached state.  The observers remain registered on the world but
  // are no-ops because they check m_world == nullptr.
  m_world = nullptr;
  m_renderable_ids.clear();
  m_unit_ids.clear();
  m_building_ids.clear();
  m_other_ids.clear();
}

void PersistentRenderRegistry::on_component_changed(std::uint32_t entity_id,
                                                     std::type_index type,
                                                     bool added) {
  // Only care about components that affect classification.
  if (type != k_renderable_type && type != k_unit_type &&
      type != k_building_type && type != k_pending_removal_type) {
    return;
  }

  if (type == k_renderable_type) {
    if (added) {
      m_renderable_ids.insert(entity_id);
      reclassify(entity_id);
    } else {
      m_renderable_ids.erase(entity_id);
      remove_from_lists(entity_id);
    }
    return;
  }

  // For unit/building/pending-removal changes, reclassify if the entity is
  // already tracked (has a RenderableComponent).
  if (m_renderable_ids.count(entity_id) != 0U) {
    reclassify(entity_id);
  }
}

void PersistentRenderRegistry::on_entity_destroyed(std::uint32_t entity_id) {
  m_renderable_ids.erase(entity_id);
  remove_from_lists(entity_id);
}

void PersistentRenderRegistry::on_world_cleared() {
  m_renderable_ids.clear();
  m_unit_ids.clear();
  m_building_ids.clear();
  m_other_ids.clear();
}

void PersistentRenderRegistry::reclassify(std::uint32_t entity_id) {
  if (m_world == nullptr) {
    return;
  }
  Engine::Core::Entity *entity = m_world->get_entity(entity_id);
  if (entity == nullptr) {
    remove_from_lists(entity_id);
    return;
  }

  // If the entity has PendingRemovalComponent or no longer has
  // RenderableComponent, remove it from lists.
  if (!entity->has_component<Engine::Core::RenderableComponent>() ||
      entity->has_component<Engine::Core::PendingRemovalComponent>()) {
    m_renderable_ids.erase(entity_id);
    remove_from_lists(entity_id);
    return;
  }

  const bool has_unit = entity->has_component<Engine::Core::UnitComponent>();
  const bool has_building =
      entity->has_component<Engine::Core::BuildingComponent>();

  // Determine target list.
  std::vector<std::uint32_t> *target_list{nullptr};
  if (has_unit) {
    target_list = &m_unit_ids;
  } else if (has_building) {
    target_list = &m_building_ids;
  } else {
    target_list = &m_other_ids;
  }

  // Remove from any other list first, then add to target if not already there.
  if (target_list != &m_unit_ids) {
    remove_id(m_unit_ids, entity_id);
  }
  if (target_list != &m_building_ids) {
    remove_id(m_building_ids, entity_id);
  }
  if (target_list != &m_other_ids) {
    remove_id(m_other_ids, entity_id);
  }

  const auto it =
      std::find(target_list->begin(), target_list->end(), entity_id);
  if (it == target_list->end()) {
    target_list->push_back(entity_id);
  }
}

void PersistentRenderRegistry::remove_from_lists(std::uint32_t entity_id) {
  remove_id(m_unit_ids, entity_id);
  remove_id(m_building_ids, entity_id);
  remove_id(m_other_ids, entity_id);
}

void PersistentRenderRegistry::remove_id(std::vector<std::uint32_t> &vec,
                                          std::uint32_t id) {
  // Swap-and-pop is O(1) but does not preserve insertion order.  Rendering
  // does not rely on a stable iteration order within a category, so this is
  // acceptable.  If deterministic ordering is ever required, switch to
  // std::erase with std::find.
  const auto it = std::find(vec.begin(), vec.end(), id);
  if (it != vec.end()) {
    *it = vec.back();
    vec.pop_back();
  }
}

} // namespace Render
