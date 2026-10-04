#pragma once

#include <QVector3D>

#include <optional>

#include "../core/entity.h"
#include "../core/system.h"

namespace Game::Systems {

[[nodiscard]] auto
siege_tower_dock_approach(Engine::Core::World& world,
                          Engine::Core::EntityID tower,
                          Engine::Core::EntityID wall_id) -> std::optional<QVector3D>;

class SiegeTowerSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World* world, float delta_time) override;
};

class WallWalkSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World* world, float delta_time) override;

private:
  static constexpr float k_garrison_check_seconds = 2.0F;
  float m_garrison_timer{0.0F};
};

} // namespace Game::Systems
