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

class PersistentRenderRegistry {
public:
  PersistentRenderRegistry() = default;
  ~PersistentRenderRegistry();

  PersistentRenderRegistry(const PersistentRenderRegistry &) = delete;
  auto operator=(const PersistentRenderRegistry &)
      -> PersistentRenderRegistry & = delete;
  PersistentRenderRegistry(PersistentRenderRegistry &&) = delete;
  auto
  operator=(PersistentRenderRegistry &&) -> PersistentRenderRegistry & = delete;

  void attach(Engine::Core::World *world);

  void detach();

  [[nodiscard]] auto
  is_attached_to(const Engine::Core::World *world) const -> bool {
    return m_world == world;
  }

  [[nodiscard]] auto unit_ids() const -> const std::vector<std::uint32_t> & {
    return m_unit_ids;
  }
  [[nodiscard]] auto
  building_ids() const -> const std::vector<std::uint32_t> & {
    return m_building_ids;
  }
  [[nodiscard]] auto other_ids() const -> const std::vector<std::uint32_t> & {
    return m_other_ids;
  }

private:
  void on_component_changed(std::uint32_t entity_id, std::type_index type,
                            bool added);
  void on_entity_destroyed(std::uint32_t entity_id);
  void on_world_cleared();

  void reclassify(std::uint32_t entity_id);
  void remove_from_lists(std::uint32_t entity_id);
  static void remove_id(std::vector<std::uint32_t> &vec, std::uint32_t id);

  Engine::Core::World *m_world{nullptr};
  std::uint64_t m_component_observer_handle{0};
  std::uint64_t m_entity_destroyed_observer_handle{0};
  std::uint64_t m_world_cleared_observer_handle{0};

  std::unordered_set<std::uint32_t> m_renderable_ids;

  std::vector<std::uint32_t> m_unit_ids;
  std::vector<std::uint32_t> m_building_ids;
  std::vector<std::uint32_t> m_other_ids;
};

} // namespace Render
