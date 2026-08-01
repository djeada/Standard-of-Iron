#pragma once

#include <QVector3D>

#include <cstdint>
#include <vector>

#include "../core/system.h"
#include "nation_registry.h"

namespace Engine::Core {
class Entity;
class World;
} // namespace Engine::Core

namespace Game::Systems {

struct DefenseFormationDamageContext {
  bool is_missile{false};
  bool is_cavalry_impact{false};
  QVector3D attack_origin;
};

class DefenseFormationService {
public:
  [[nodiscard]] static auto
  profile_for(const Engine::Core::Entity& entity) -> const DefenseFormationProfile*;

  [[nodiscard]] static auto
  can_form(Engine::Core::World& world,
           const std::vector<Engine::Core::EntityID>& units) -> bool;

  static auto begin(Engine::Core::World& world,
                    const std::vector<Engine::Core::EntityID>& units,
                    const QVector3D& anchor,
                    bool has_anchor) -> bool;

  static void begin_break(Engine::Core::Entity* entity);

  static void clear(Engine::Core::Entity* entity);

  [[nodiscard]] static auto
  damage_multiplier(const Engine::Core::Entity& target,
                    const DefenseFormationDamageContext& context) -> float;

  [[nodiscard]] static auto
  attack_output_multiplier(const Engine::Core::Entity& entity) -> float;

  [[nodiscard]] static auto
  move_speed_multiplier(const Engine::Core::Entity& entity) -> float;

  [[nodiscard]] static auto
  turn_speed_multiplier(const Engine::Core::Entity& entity) -> float;

  [[nodiscard]] static auto blocks_charge(const Engine::Core::Entity& entity) -> bool;
};

class DefenseFormationSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World* world, float delta_time) override;
};

} // namespace Game::Systems
