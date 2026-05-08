#pragma once

#include <cstdint>
#include <typeindex>
#include <unordered_set>
#include <vector>

namespace Engine::Core {
class World;
class Entity;
} // namespace Engine::Core

namespace Render {

// Tracks entities that have a RenderableComponent and classifies them into
// three persistent lists: units, buildings, and others.  The lists are updated
// through observer callbacks registered on the attached World, so the normal
// frame path can iterate pre-classified entity IDs instead of scanning every
// renderable entity every frame.
class PersistentRenderRegistry {
public:
  PersistentRenderRegistry() = default;
  ~PersistentRenderRegistry();

  PersistentRenderRegistry(const PersistentRenderRegistry &) = delete;
  auto operator=(const PersistentRenderRegistry &)
      -> PersistentRenderRegistry & = delete;
  PersistentRenderRegistry(PersistentRenderRegistry &&) = delete;
  auto operator=(PersistentRenderRegistry &&)
      -> PersistentRenderRegistry & = delete;

  // Attach to world: performs an initial scan of all existing entities and
  // registers component/entity-destroyed/world-cleared observers.
  void attach(Engine::Core::World *world);

  // Detach from the currently attached world (if any).  Observers remain in
  // place (World does not support removing them) but are no-ops after
  // detachment because the registry checks m_world internally.
  void detach();

  [[nodiscard]] auto is_attached_to(const Engine::Core::World *world) const
      -> bool {
    return m_world == world;
  }

  // Pre-classified entity ID lists.  Valid while the world's entity mutex is
  // held by the caller.
  [[nodiscard]] auto unit_ids() const -> const std::vector<std::uint32_t> & {
    return m_unit_ids;
  }
  [[nodiscard]] auto building_ids() const
      -> const std::vector<std::uint32_t> & {
    return m_building_ids;
  }
  [[nodiscard]] auto other_ids() const -> const std::vector<std::uint32_t> & {
    return m_other_ids;
  }

private:
  enum class EntityKind : std::uint8_t { Unit, Building, Other };

  void on_component_changed(std::uint32_t entity_id, std::type_index type,
                            bool added);
  void on_entity_destroyed(std::uint32_t entity_id);
  void on_world_cleared();

  void reclassify(std::uint32_t entity_id);
  void remove_from_lists(std::uint32_t entity_id);
  static void remove_id(std::vector<std::uint32_t> &vec, std::uint32_t id);

  Engine::Core::World *m_world{nullptr};

  // Set of entity IDs that currently have a RenderableComponent.
  std::unordered_set<std::uint32_t> m_renderable_ids;

  // Pre-classified lists (entity IDs, no duplicates).
  std::vector<std::uint32_t> m_unit_ids;
  std::vector<std::uint32_t> m_building_ids;
  std::vector<std::uint32_t> m_other_ids;
};

} // namespace Render
