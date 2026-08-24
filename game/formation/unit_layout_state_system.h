#pragma once

#include "../core/system.h"
#include "unit_layout.h"

namespace Engine::Core {
class Entity;
class World;
} // namespace Engine::Core

namespace Game::Formation {

enum class LayoutPhase : std::uint8_t {
  Forming = 0,
  Formed = 1,
  Disrupted = 2,
  Breaking = 3
};

class UnitLayoutStateSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World* world, float delta_time) override;

  [[nodiscard]] auto access() const -> Engine::Core::SystemAccess override;

  [[nodiscard]] static auto
  desired_state(const Engine::Core::Entity& entity) -> UnitLayoutState;

  [[nodiscard]] static auto
  is_layout_formed(const Engine::Core::Entity& entity) -> bool;

  struct LayoutBlend {

    float formed_ratio{1.0F};

    UnitLayoutId blend_from{k_invalid_layout};

    float blend_ratio{1.0F};
  };

  [[nodiscard]] static auto
  layout_blend(const Engine::Core::Entity& entity) -> LayoutBlend;

  [[nodiscard]] static auto formed_ratio(const Engine::Core::Entity& entity) -> float;

  [[nodiscard]] static auto transition_seconds_for(UnitLayoutState from,
                                                   UnitLayoutState to) -> float;

  static void mark_disrupted(Engine::Core::Entity& entity);
};

} // namespace Game::Formation
