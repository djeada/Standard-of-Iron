#pragma once

#include <cstddef>
#include <functional>
#include <tuple>
#include <utility>
#include <vector>

#include "entity.h"

namespace Engine::Core {

class World;

namespace Detail {

auto entity_in(World& world, EntityID entity_id) -> Entity*;
void destroy_in(World& world, EntityID entity_id);

} // namespace Detail

class DeferredMutations {
public:
  using Action = std::function<void(World&)>;

  void destroy_entity(EntityID entity_id);

  template <typename T, typename... Args>
  void add_component(EntityID entity_id, Args&&... args) {
    m_actions.emplace_back(
        [entity_id,
         packed = std::make_tuple(std::forward<Args>(args)...)](World& world) mutable {
          if (Entity* entity = Detail::entity_in(world, entity_id)) {
            std::apply(
                [entity](auto&&... unpacked) {
                  entity->template add_component<T>(
                      std::forward<decltype(unpacked)>(unpacked)...);
                },
                std::move(packed));
          }
        });
  }

  template <typename T>
  void remove_component(EntityID entity_id) {
    m_actions.emplace_back([entity_id](World& world) {
      if (Entity* entity = Detail::entity_in(world, entity_id)) {
        entity->template remove_component<T>();
      }
    });
  }

  void run(Action action);

  [[nodiscard]] auto empty() const noexcept -> bool { return m_actions.empty(); }
  [[nodiscard]] auto pending() const noexcept -> std::size_t {
    return m_actions.size();
  }

  void apply(World& world);

  void clear() noexcept { m_actions.clear(); }

private:
  std::vector<Action> m_actions;
  std::vector<Action> m_applying;
};

} // namespace Engine::Core
