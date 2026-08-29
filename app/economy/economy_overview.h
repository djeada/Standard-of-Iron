#pragma once

#include <QVariantList>
#include <QVariantMap>

#include "game/systems/nation_id.h"
#include "game/systems/resource_types.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems {
class NationRegistry;
class PlayerResourceRegistry;
} // namespace Game::Systems

namespace App::Core {

struct EconomyOverviewRequest {
  Engine::Core::World* world = nullptr;

  const Game::Systems::NationRegistry* nations = nullptr;
  Game::Systems::PlayerResourceRegistry* resources = nullptr;

  int owner_id = 0;
  Game::Systems::NationID nation_id = Game::Systems::NationID::RomanRepublic;
  int manpower_cap = 0;
  Game::Systems::ResourceAmounts objective_resources{};
};

struct EconomyCoachBaseline {
  int troop_manpower = 0;
  int building_count = 0;
  bool captured = false;
};

inline constexpr int k_economy_coach_army_manpower = 80;

[[nodiscard]] auto
build_resource_overview(const EconomyOverviewRequest& request) -> QVariantList;

[[nodiscard]] auto
build_production_help(const EconomyOverviewRequest& request) -> QVariantMap;

[[nodiscard]] auto capture_economy_coach_baseline(const EconomyOverviewRequest& request)
    -> EconomyCoachBaseline;

[[nodiscard]] auto
build_economy_coach_state(const EconomyOverviewRequest& request,
                          const EconomyCoachBaseline& baseline) -> QVariantMap;

} // namespace App::Core
