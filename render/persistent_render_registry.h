#pragma once

#include <cstdint>
#include <typeindex>
#include <unordered_set>
#include <vector>

namespace Engine::Core {
class World;
class Entity;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace Render {

class PersistentRenderRegistry {
public:
  PersistentRenderRegistry() = default;
  ~PersistentRenderRegistry();

  PersistentRenderRegistry(const PersistentRenderRegistry&) = delete;
  auto operator=(const PersistentRenderRegistry&) -> PersistentRenderRegistry& = delete;
  PersistentRenderRegistry(PersistentRenderRegistry&&) = delete;
  auto operator=(PersistentRenderRegistry&&) -> PersistentRenderRegistry& = delete;

  void attach(Engine::Core::World* world);

  void detach();

  [[nodiscard]] auto is_attached_to(const Engine::Core::World* world) const -> bool {
    return m_world == world;
  }

  [[nodiscard]] auto unit_ids() const -> const std::vector<Engine::Core::EntityID>& {
    return m_unit_ids;
  }
  [[nodiscard]] auto
  building_ids() const -> const std::vector<Engine::Core::EntityID>& {
    return m_building_ids;
  }
  [[nodiscard]] auto other_ids() const -> const std::vector<Engine::Core::EntityID>& {
    return m_other_ids;
  }

private:
  void on_component_changed(Engine::Core::EntityID entity_id,
                            std::type_index type,
                            bool added);
  void on_entity_destroyed(Engine::Core::EntityID entity_id);
  void on_world_cleared();

  void reclassify(Engine::Core::EntityID entity_id);
  void remove_from_lists(Engine::Core::EntityID entity_id);
  static void remove_id(std::vector<Engine::Core::EntityID>& vec,
                        Engine::Core::EntityID id);

  Engine::Core::World* m_world{nullptr};
  std::uint64_t m_component_observer_handle{0};
  std::uint64_t m_entity_destroyed_observer_handle{0};
  std::uint64_t m_world_cleared_observer_handle{0};

  std::unordered_set<Engine::Core::EntityID> m_renderable_ids;

  std::vector<Engine::Core::EntityID> m_unit_ids;
  std::vector<Engine::Core::EntityID> m_building_ids;
  std::vector<Engine::Core::EntityID> m_other_ids;
};

} // namespace Render
