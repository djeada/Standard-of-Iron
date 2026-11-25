

#include <QVector3D>

#include <algorithm>
#include <cstdio>
#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "game/command/command.h"
#include "game/command/command_queue.h"
#include "game/core/component_core.h"
#include "game/core/component_gameplay.h"
#include "game/core/component_structures.h"
#include "game/core/world.h"
#include "game/game_config.h"
#include "game/map/map_definition.h"
#include "game/map/map_transformer.h"
#include "game/map/skirmish_camp_start.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/systems/ai_system.h"
#include "game/systems/ai_system/ai_strategy.h"
#include "game/systems/default_content.h"
#include "game/systems/economy/build_site.h"
#include "game/systems/economy/production_service.h"
#include "game/systems/nation_registry.h"
#include "game/systems/navigation/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/runtime_system_registry.h"
#include "game/systems/troop_count_registry.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"

namespace {

using Engine::Core::EntityID;
using Engine::Core::UnitComponent;
using Game::Session::SessionContext;
using Game::Systems::ResourceType;

constexpr int k_map_size = 160;
constexpr int k_ai = 1;
constexpr int k_proxy = 2;
constexpr int k_ai_grid_x = 40;
constexpr int k_proxy_grid_x = 120;
constexpr float k_think_seconds = 5.0F;
constexpr int k_orders_per_think = 3;

struct Census {
  int men = 0;
  int fighting_men = 0;
  int homes = 0;
  int barracks = 0;
};

auto census(SessionContext& session, int owner) -> Census {
  Census out;
  for (auto [id, unit] : session.world().view<UnitComponent>()) {
    (void)id;
    if (unit.owner_id != owner || unit.health <= 0) {
      continue;
    }
    if (unit.spawn_type == Game::Units::SpawnType::Home) {
      ++out.homes;
    }
    if (unit.spawn_type == Game::Units::SpawnType::Barracks) {
      ++out.barracks;
    }
    if (Game::Units::is_building_spawn(unit.spawn_type)) {
      continue;
    }
    const int men = Game::Systems::squad_men(unit);
    out.men += men;
    if (unit.spawn_type != Game::Units::SpawnType::Builder &&
        unit.spawn_type != Game::Units::SpawnType::Civilian &&
        unit.spawn_type != Game::Units::SpawnType::Healer) {
      out.fighting_men += men;
    }
  }
  return out;
}

class PlayerProxyGrowthTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_saved_cap = Game::GameConfig::instance().get_max_troops_per_player();
    Game::GameConfig::instance().set_max_troops_per_player(900);
    Game::Systems::NavGrid::initialize(k_map_size, k_map_size);
    m_factory = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*m_factory);
    Game::Map::MapTransformer::setFactoryRegistry(m_factory);
  }

  void TearDown() override {
    m_scope.reset();
    m_session.reset();
    Game::Map::MapTransformer::setFactoryRegistry(nullptr);
    Game::Map::TerrainService::instance().clear();
    Game::GameConfig::instance().set_max_troops_per_player(m_saved_cap);
  }

  auto make_match() -> SessionContext& {
    m_session = std::make_unique<SessionContext>();
    auto& session = *m_session;
    session.world().set_presentation_enabled(false);
    m_scope = std::make_unique<Game::Session::ScopedSession>(session);
    Game::Map::MapTransformer::setFactoryRegistry(m_factory);
    Game::Systems::NavGrid::initialize(k_map_size, k_map_size);

    auto& owners = session.owners();
    owners.register_owner_with_id(k_ai, Game::Systems::OwnerType::AI, "computer");
    owners.register_owner_with_id(k_proxy, Game::Systems::OwnerType::Player, "proxy");
    owners.set_owner_team(k_ai, 1);
    owners.set_owner_team(k_proxy, 1);

    Game::Systems::initialize_default_content(session.nations());
    session.nations().set_player_nation(k_ai, Game::Systems::NationID::RomanRepublic);
    session.nations().set_player_nation(k_proxy,
                                        Game::Systems::NationID::RomanRepublic);

    Game::Map::MapDefinition map_definition;
    map_definition.grid.width = k_map_size;
    map_definition.grid.height = k_map_size;
    map_definition.grid.tile_size = 1.0F;
    scatter_resources(map_definition, k_ai_grid_x);
    scatter_resources(map_definition, k_proxy_grid_x);
    session.terrain().initialize(map_definition);

    Game::Systems::register_runtime_systems(session.world());

    for (const int owner : {k_ai, k_proxy}) {
      auto& economy = session.economy();
      economy.ensure_owner(owner);
      economy.set(owner, ResourceType::Gold, 500);
      economy.set(owner, ResourceType::Food, 300);
      economy.set(owner, ResourceType::Wood, 300);
      economy.set(owner, ResourceType::Stone, 150);
      economy.set(owner, ResourceType::Iron, 200);
    }

    seat(session, k_ai, k_ai_grid_x);
    seat(session, k_proxy, k_proxy_grid_x);
    (void)Game::Map::apply_skirmish_camp_start(session.world());

    if (auto* ai = session.world().get_system<Game::Systems::AISystem>()) {
      ai->reinitialize();
      Game::Systems::AI::AIPlayerProfile profile;
      profile.strategy = Game::Systems::AI::AIStrategy::Expansionist;
      profile.posture = Game::Systems::AI::AIPosture::Field;
      profile.personality.aggression = 0.6F;
      profile.personality.defense = 0.4F;
      profile.personality.harassment = 0.35F;
      ai->set_ai_profile(k_ai, profile);
    }
    return session;
  }

  static void scatter_resources(Game::Map::MapDefinition& map, int grid_x) {
    const int grid_z = k_map_size / 2;
    const auto add = [&map](Game::Map::WorldProp::Type type, int x, int z) {
      Game::Map::WorldProp prop;
      prop.type = type;
      prop.x = static_cast<float>(x);
      prop.z = static_cast<float>(z);
      map.world_props.push_back(prop);
    };
    for (int ring = 0; ring < 10; ++ring) {
      const int offset = 18 + ring * 2;
      for (int lane = -2; lane <= 2; ++lane) {
        add(Game::Map::WorldProp::Type::PineTree, grid_x + lane * 3, grid_z + offset);
        add(Game::Map::WorldProp::Type::OliveTree, grid_x + lane * 3, grid_z - offset);
      }
      add(Game::Map::WorldProp::Type::IronOre, grid_x - offset - 6, grid_z + 4);
      add(Game::Map::WorldProp::Type::IronOre, grid_x - offset - 6, grid_z - 4);
      add(Game::Map::WorldProp::Type::Boulder, grid_x + offset + 6, grid_z);
    }
  }

  void seat(SessionContext& session, int owner, int grid_x) {
    const QVector3D at = Game::Systems::NavGrid::grid_to_world(
        Game::Systems::Point(grid_x, k_map_size / 2));
    Game::Units::SpawnParams params;
    params.player_id = owner;
    params.ai_controlled = owner == k_ai;
    params.is_initial_spawn = true;
    params.nation_id = Game::Systems::NationID::RomanRepublic;
    params.max_population = 140;
    params.spawn_type = Game::Units::SpawnType::Barracks;
    params.position = at;
    (void)m_factory->create(params.spawn_type, session.world(), params);
    params.spawn_type = Game::Units::SpawnType::Builder;
    params.position = at + QVector3D(3.0F, 0.0F, 2.0F);
    (void)m_factory->create(params.spawn_type, session.world(), params);
  }

  static void run_for(SessionContext& session, double seconds) {
    const double step = session.clock().tick_seconds();
    for (double elapsed = 0.0; elapsed < seconds; elapsed += step) {
      session.clock().advance(step);
      while (session.clock().consume_tick()) {
        session.world().update(static_cast<float>(step));
      }
    }
  }

  static void proxy_thinks(SessionContext& session, float minute) {
    auto& world = session.world();
    auto& economy = session.economy();
    int orders = 0;
    const auto give = [&](Game::Command::Payload payload) {
      if (orders >= k_orders_per_think) {
        return false;
      }
      ++orders;
      return Game::Command::submit(
          world, Game::Command::Source::LocalPlayer, k_proxy, std::move(payload));
    };

    std::vector<EntityID> idle_builders;
    std::vector<EntityID> gathering_builders;
    std::vector<EntityID> barracks;
    int homes = 0;
    int farms = 0;
    int markets = 0;
    int builders = 0;
    QVector3D base;
    for (auto [id, unit, transform] :
         world.view<UnitComponent, Engine::Core::TransformComponent>()) {
      if (unit.owner_id != k_proxy || unit.health <= 0) {
        continue;
      }
      switch (unit.spawn_type) {
      case Game::Units::SpawnType::Builder: {
        ++builders;
        const auto* job = world.try_get<Engine::Core::BuilderProductionComponent>(id);
        if (job == nullptr) {
          break;
        }
        if (job->has_construction_site || job->in_progress) {
          break;
        }
        if (job->auto_gather || job->has_gather_order) {
          gathering_builders.push_back(id);
        } else {
          idle_builders.push_back(id);
        }
        break;
      }
      case Game::Units::SpawnType::Barracks:
        barracks.push_back(id);
        if (barracks.size() == 1U) {
          base = QVector3D(transform.position.x, 0.0F, transform.position.z);
        }
        break;
      case Game::Units::SpawnType::Home:
        ++homes;
        break;
      case Game::Units::SpawnType::Farm:
        ++farms;
        break;
      case Game::Units::SpawnType::Marketplace:
        ++markets;
        break;
      default:
        break;
      }
    }
    std::sort(barracks.begin(), barracks.end());

    if (!idle_builders.empty()) {
      give(Game::Command::SetAutoGather{.units = idle_builders, .active = true});
    }

    const auto stock = [&](ResourceType type) {
      return economy.get(k_proxy, type);
    };
    const auto build = [&](const char* type, float distance) {
      if (gathering_builders.empty()) {
        return;
      }
      const auto site = Game::Systems::find_clear_site(
          world, type, base + QVector3D(0.0F, 0.0F, -distance), 26.0F);
      if (!site.has_value()) {
        return;
      }
      if (give(Game::Command::StartConstruction{.units = {gathering_builders.back()},
                                                .construction_type = type,
                                                .site = *site})) {
        gathering_builders.pop_back();
      }
    };

    if (markets == 0 && stock(ResourceType::Gold) >= 50 &&
        stock(ResourceType::Wood) >= 60 && stock(ResourceType::Stone) >= 40) {
      build("marketplace", 14.0F);
    } else if (homes < 2 + static_cast<int>(minute) &&
               stock(ResourceType::Wood) >= 50 && stock(ResourceType::Stone) >= 15) {
      build("home", 18.0F);
    } else if (farms < 1 + homes / 3 && stock(ResourceType::Wood) >= 40 &&
               stock(ResourceType::Stone) >= 10) {
      build("farm", 24.0F);
    } else if (barracks.size() < 2U && minute >= 4.0F &&
               stock(ResourceType::Wood) >= 100 && stock(ResourceType::Stone) >= 60) {
      build("barracks", 16.0F);
    }

    if (markets > 0 && stock(ResourceType::Iron) < 30 &&
        stock(ResourceType::Gold) >= 20) {
      give(Game::Command::Trade{.resource = ResourceType::Iron,
                                .direction = Game::Command::TradeDirection::Buy});
    }

    for (const auto hall : barracks) {
      const auto* production = world.try_get<Engine::Core::ProductionComponent>(hall);
      if (production == nullptr || production->production_queue.size() >= 3U) {
        continue;
      }
      const auto product = builders < 4 ? Game::Units::TroopType::Builder
                           : (static_cast<int>(minute * 12.0F) % 2 == 0)
                               ? Game::Units::TroopType::Swordsman
                               : Game::Units::TroopType::Velites;
      if (Game::Systems::ProductionService::can_start_production(
              world, hall, product) == Game::Systems::ProductionResult::Success) {
        give(Game::Command::Produce{.building = hall, .product = product});
      }
    }
  }

  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory;
  std::unique_ptr<SessionContext> m_session;
  std::unique_ptr<Game::Session::ScopedSession> m_scope;
  int m_saved_cap = 0;
};

TEST_F(PlayerProxyGrowthTest, APlainOpeningKeepsUpWithTheComputer) {
  auto& session = make_match();
  const bool narrate = qEnvironmentVariableIsSet("SOI_PROXY_NARRATE");
  Census ai_at_7;
  Census proxy_at_7;
  float elapsed = 0.0F;
  const int minutes = narrate ? 15 : 7;
  for (int minute = 1; minute <= minutes; ++minute) {
    while (elapsed < static_cast<float>(minute) * 60.0F) {
      proxy_thinks(session, elapsed / 60.0F);
      run_for(session, k_think_seconds);
      elapsed += k_think_seconds;
    }
    const auto ai = census(session, k_ai);
    const auto proxy = census(session, k_proxy);
    if (minute == 7) {
      ai_at_7 = ai;
      proxy_at_7 = proxy;
    }
    if (narrate) {
      std::printf("t=%2d min  computer men %4d (fighting %4d, homes %2d, barracks %d)"
                  "  proxy men %4d (fighting %4d, homes %2d, barracks %d)"
                  "  proxy food %d wood %d stone %d iron %d gold %d\n",
                  minute,
                  ai.men,
                  ai.fighting_men,
                  ai.homes,
                  ai.barracks,
                  proxy.men,
                  proxy.fighting_men,
                  proxy.homes,
                  proxy.barracks,
                  session.economy().get(k_proxy, ResourceType::Food),
                  session.economy().get(k_proxy, ResourceType::Wood),
                  session.economy().get(k_proxy, ResourceType::Stone),
                  session.economy().get(k_proxy, ResourceType::Iron),
                  session.economy().get(k_proxy, ResourceType::Gold));
    }
  }

  EXPECT_GE(proxy_at_7.men * 100, ai_at_7.men * 75)
      << "at seven minutes the proxy fields " << proxy_at_7.men << " men to the "
      << "computer's " << ai_at_7.men << "; a plain opening should keep within 75%";
}

} // namespace
