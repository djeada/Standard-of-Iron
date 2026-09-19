#include <algorithm>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "game/command/command.h"
#include "game/command/command_queue.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/map/map_transformer.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/systems/construction_cost_catalog.h"
#include "game/systems/default_content.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/resource_types.h"
#include "game/systems/runtime_system_registry.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"

namespace {

using Engine::Core::EntityID;
using Game::Session::SessionContext;

constexpr int k_player = 1;
constexpr int k_map_size = 96;

class BuilderCrewsTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::NavGrid::initialize(k_map_size, k_map_size);
    m_factory = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*m_factory);

    m_session = std::make_unique<SessionContext>();
    m_scope = std::make_unique<Game::Session::ScopedSession>(*m_session);
    Game::Map::MapTransformer::setFactoryRegistry(m_factory);
    Game::Systems::NavGrid::initialize(k_map_size, k_map_size);
    auto& owners = m_session->owners();
    owners.register_owner_with_id(k_player, Game::Systems::OwnerType::Player, "blue");
    owners.set_owner_team(k_player, 1);
    Game::Systems::initialize_default_content(m_session->nations());
    Game::Systems::register_runtime_systems(m_session->world());

    Game::Map::MapDefinition map_definition;
    map_definition.grid.width = k_map_size;
    map_definition.grid.height = k_map_size;
    map_definition.grid.tile_size = 1.0F;
    m_session->terrain().initialize(map_definition);
  }

  void TearDown() override {
    Game::Map::MapTransformer::setFactoryRegistry(nullptr);
    m_scope.reset();
    m_session.reset();
  }

  auto spawn_builder(int grid_x, int grid_z) -> EntityID {
    Game::Units::SpawnParams params;
    params.position = Game::Systems::NavGrid::grid_to_world({grid_x, grid_z});
    params.player_id = k_player;
    params.spawn_type = Game::Units::SpawnType::Builder;
    params.is_initial_spawn = true;
    auto unit =
        m_factory->create(Game::Units::SpawnType::Builder, m_session->world(), params);
    return unit ? unit->id() : 0;
  }

  auto builder_of(EntityID id) -> Engine::Core::BuilderProductionComponent* {
    return m_session->world().try_get<Engine::Core::BuilderProductionComponent>(id);
  }

  auto count_of(Game::Units::SpawnType type) -> int {
    int count = 0;
    for (auto* entity :
         m_session->world().collect_entities_with<Engine::Core::UnitComponent>()) {
      if (entity->get_component<Engine::Core::UnitComponent>()->spawn_type == type) {
        ++count;
      }
    }
    return count;
  }

  void step() {
    const double tick = m_session->clock().tick_seconds();
    m_session->clock().advance(tick);
    while (m_session->clock().consume_tick()) {
      m_session->world().update(static_cast<float>(tick));
    }
  }

  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory;
  std::unique_ptr<SessionContext> m_session;
  std::unique_ptr<Game::Session::ScopedSession> m_scope;
};

TEST_F(BuilderCrewsTest, EveryCrewInTheOrderWorksTheSiteAndOneHouseRises) {
  const std::vector<EntityID> crews{
      spawn_builder(30, 40), spawn_builder(30, 48), spawn_builder(30, 56)};
  for (const auto id : crews) {
    ASSERT_NE(id, 0U);
  }
  auto& economy = m_session->economy();
  economy.add(k_player, Game::Systems::ResourceType::Wood, 500);
  economy.add(k_player, Game::Systems::ResourceType::Stone, 500);
  economy.add(k_player, Game::Systems::ResourceType::Gold, 500);

  const QVector3D site = Game::Systems::NavGrid::grid_to_world({48, 48});
  ASSERT_TRUE(Game::Command::submit(
      m_session->world(),
      Game::Command::Source::LocalPlayer,
      k_player,
      Game::Command::StartConstruction{
          .units = crews, .construction_type = "home", .site = site}));

  const float one_crew_seconds = Game::Systems::construction_build_time("home");
  const double tick = m_session->clock().tick_seconds();
  int most_at_work = 0;
  double built_after = -1.0;
  int wood_after_order = -1;
  for (double elapsed = 0.0; elapsed < 180.0; elapsed += tick) {
    step();
    if (wood_after_order < 0) {
      wood_after_order = economy.get(k_player, Game::Systems::ResourceType::Wood);
    }
    int at_work = 0;
    for (const auto id : crews) {
      const auto* builder = builder_of(id);
      if (builder != nullptr && builder->at_construction_site && builder->in_progress) {
        ++at_work;
      }
    }
    most_at_work = std::max(most_at_work, at_work);
    if (built_after < 0.0 && count_of(Game::Units::SpawnType::Home) > 0) {
      built_after = elapsed;
    }
    if (built_after >= 0.0 && elapsed > built_after + 10.0) {
      break;
    }
  }

  EXPECT_EQ(most_at_work, static_cast<int>(crews.size()))
      << "every crew named in the order must reach the site and work it";
  EXPECT_EQ(count_of(Game::Units::SpawnType::Home), 1);
  ASSERT_GE(built_after, 0.0) << "the house was never finished";
  EXPECT_LT(built_after, one_crew_seconds + 30.0);
  EXPECT_EQ(economy.get(k_player, Game::Systems::ResourceType::Wood), wood_after_order)
      << "finishing must not refund what the order paid";
  for (const auto id : crews) {
    const auto* builder = builder_of(id);
    ASSERT_NE(builder, nullptr);
    EXPECT_FALSE(builder->has_construction_site) << "crew " << id << " never let go";
    EXPECT_FALSE(builder->in_progress);
  }
}

} // namespace
