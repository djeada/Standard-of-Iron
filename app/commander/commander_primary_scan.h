#pragma once

#include <QVector3D>

#include <cstdint>
#include <optional>
#include <vector>

#include "game/systems/rpg_combat_system/rpg_targeting.h"

namespace Engine::Core {
class World;
class Entity;
class AttackComponent;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace App::Core {

struct PrimaryScan {
  Engine::Core::World& world;
  Engine::Core::Entity* commander;
  int local_owner_id;
  QVector3D origin;
  QVector3D forward;
  float max_range;

  [[nodiscard]] auto eligible_samples(Engine::Core::EntityID entity_id) const
      -> std::vector<Game::Systems::RpgCombat::SoldierTarget>;

  [[nodiscard]] auto choose_sample(Engine::Core::EntityID entity_id,
                                   std::uint16_t preferred_slot,
                                   float minimum_facing) const
      -> std::optional<Game::Systems::RpgCombat::SoldierTarget>;

  [[nodiscard]] auto
  best_in_world() const -> std::optional<Game::Systems::RpgCombat::SoldierTarget>;
};

[[nodiscard]] auto
commander_reach(const Engine::Core::Entity& commander,
                const Engine::Core::AttackComponent* attack) -> float;

[[nodiscard]] auto pick_primary(const PrimaryScan& scan,
                                Engine::Core::Entity& commander,
                                Engine::Core::EntityID locked_id,
                                std::uint16_t& locked_slot,
                                Engine::Core::EntityID soft_id,
                                std::uint16_t soft_slot)
    -> std::optional<Game::Systems::RpgCombat::SoldierTarget>;

} // namespace App::Core
