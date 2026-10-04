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
#include "game/systems/building_collision_registry.h"
#include "game/systems/default_content.h"
#include "game/systems/economy/build_site.h"
#include "game/systems/economy/construction_cost_catalog.h"
#include "game/systems/navigation/nav_grid.h"
#include "game/systems/navigation/pathfinding.h"
#include "game/systems/owner_registry.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/resource_types.h"
#include "game/systems/runtime_system_registry.h"
#include "game/systems/structure_placement_service.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"
#include "game/units/squad.h"

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

  auto spawn_squad(int grid_x, int grid_z) -> EntityID {
    Game::Units::SpawnParams params;
    params.position = Game::Systems::NavGrid::grid_to_world({grid_x, grid_z});
    params.player_id = k_player;
    params.spawn_type = Game::Units::SpawnType::Spearman;
    params.is_initial_spawn = true;
    auto unit =
        m_factory->create(Game::Units::SpawnType::Spearman, m_session->world(), params);
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

  auto position_of(EntityID id) -> QVector3D {
    const auto* t = m_session->world().try_get<Engine::Core::TransformComponent>(id);
    return t == nullptr ? QVector3D() : QVector3D(t->position.x, 0.0F, t->position.z);
  }

  auto closest_pair(const std::vector<EntityID>& crews) -> float {
    float closest = 1.0e9F;
    for (std::size_t i = 0; i < crews.size(); ++i) {
      for (std::size_t j = i + 1; j < crews.size(); ++j) {
        closest =
            std::min(closest, (position_of(crews[i]) - position_of(crews[j])).length());
      }
    }
    return closest;
  }

  auto tree_at(float x, float z) -> std::uint64_t {
    Game::Map::WorldProp tree;
    tree.type = Game::Map::WorldProp::Type::PineTree;
    return m_session->terrain().add_world_prop_at_world(tree, x, z);
  }

  void run(double seconds) {
    const double tick = m_session->clock().tick_seconds();
    for (double elapsed = 0.0; elapsed < seconds; elapsed += tick) {
      step();
    }
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
  float farthest_from_site = 0.0F;
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
    for (const auto id : crews) {
      const auto* builder = builder_of(id);
      if (builder != nullptr && builder->at_construction_site && builder->in_progress) {
        const QVector3D at = position_of(id);
        farthest_from_site = std::max(farthest_from_site,
                                      std::hypot(at.x() - site.x(), at.z() - site.z()));
      }
    }
    if (built_after < 0.0 && count_of(Game::Units::SpawnType::Home) > 0) {
      built_after = elapsed;
    }
    if (built_after >= 0.0 && elapsed > built_after + 10.0) {
      break;
    }
  }

  EXPECT_EQ(most_at_work, static_cast<int>(crews.size()))
      << "every crew named in the order must reach the site and work it";
  EXPECT_LT(farthest_from_site, 0.5F)
      << "a crew worked the house from beside it instead of standing on it";
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

TEST_F(BuilderCrewsTest, ACrewWalksOntoItsSiteAndToItsPostsWithoutJumping) {
  const EntityID crew = spawn_builder(30, 48);
  ASSERT_NE(crew, 0U);
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
          .units = {crew}, .construction_type = "home", .site = site}));

  auto& world = m_session->world();
  const double tick = m_session->clock().tick_seconds();
  const auto* unit = world.try_get<Engine::Core::UnitComponent>(crew);
  const float stride = unit->speed * static_cast<float>(tick) * 2.0F + 0.02F;
  QVector3D last_root = position_of(crew);
  std::vector<std::pair<float, float>> last_men;
  float worst_root_jump = 0.0F;
  float worst_man_jump = 0.0F;
  float progress_while_walking = 0.0F;
  float fastest_man = 0.0F;
  bool worked = false;
  for (double elapsed = 0.0; elapsed < 60.0; elapsed += tick) {
    step();
    const QVector3D root = position_of(crew);
    worst_root_jump = std::max(worst_root_jump, (root - last_root).length());
    last_root = root;

    const auto* builder = builder_of(crew);
    const auto* formation =
        world.try_get<Engine::Core::FormationPresentationComponent>(crew);
    bool any_walking = false;
    if (formation != nullptr) {
      if (last_men.size() != formation->soldiers.size()) {
        last_men.clear();
        for (const auto& man : formation->soldiers) {
          last_men.emplace_back(man.world_x, man.world_z);
        }
      }
      for (std::size_t i = 0; i < formation->soldiers.size(); ++i) {
        const auto& man = formation->soldiers[i];
        if (!man.alive || !man.world_motion_valid) {
          continue;
        }
        const float moved = std::hypot(man.world_x - last_men[i].first,
                                       man.world_z - last_men[i].second);
        last_men[i] = {man.world_x, man.world_z};
        worst_man_jump = std::max(worst_man_jump, moved);
        const float speed = moved / static_cast<float>(tick);
        if (builder != nullptr && builder->in_progress) {
          fastest_man = std::max(fastest_man, speed);
        }
        any_walking = any_walking || speed > 0.5F;
      }
    }
    if (builder != nullptr && builder->at_construction_site && builder->in_progress) {
      worked = true;
      if (any_walking && builder->build_time > 0.0F) {
        progress_while_walking =
            std::max(progress_while_walking,
                     1.0F - builder->time_remaining / builder->build_time);
      }
    }
    if (count_of(Game::Units::SpawnType::Home) > 0) {
      break;
    }
  }

  ASSERT_TRUE(worked) << "the crew never started on the house";
  EXPECT_LE(fastest_man, unit->speed * 1.05F)
      << "a builder ran " << fastest_man << " m/s to his post";
  EXPECT_LE(worst_root_jump, stride)
      << "the crew jumped " << worst_root_jump << " m in one tick onto its site";
  EXPECT_LE(worst_man_jump, stride)
      << "a builder jumped " << worst_man_jump << " m in one tick to his post";
  EXPECT_LT(progress_while_walking, 0.01F)
      << "the house rose while the crew was still walking to its posts";
}

TEST_F(BuilderCrewsTest, ACrewThatCannotWalkStraightToItsTreeWorksFromWhereItStands) {
  const EntityID crew = spawn_builder(44, 48);
  ASSERT_NE(crew, 0U);
  const QVector3D start = position_of(crew);
  const QVector3D tree_spot(start.x() + 4.0F, 0.0F, start.z());
  const auto tree = tree_at(tree_spot.x(), tree_spot.z());
  m_session->building_collision().register_building(
      9999U,
      "shed",
      start.x() + 2.0F,
      start.z(),
      k_player,
      Game::Systems::BuildingCollisionRegistry::BuildingSize{1.0F, 4.0F});
  if (auto* pathfinder = Game::Systems::NavGrid::get_pathfinder()) {
    pathfinder->update_navigation_grid();
  }
  ASSERT_TRUE(
      Game::Command::submit(m_session->world(),
                            Game::Command::Source::LocalPlayer,
                            k_player,
                            Game::Command::StartHarvest{.units = {crew},
                                                        .construction_type = "cut_tree",
                                                        .resource_target = tree,
                                                        .site = tree_spot}));

  const double tick = m_session->clock().tick_seconds();
  QVector3D last = position_of(crew);
  QVector3D heading;
  int reversals = 0;
  bool worked = false;
  for (double elapsed = 0.0; elapsed < 20.0; elapsed += tick) {
    step();
    const QVector3D at = position_of(crew);
    const QVector3D moved = at - last;
    last = at;
    if (moved.length() > 0.001F) {
      if (QVector3D::dotProduct(moved.normalized(), heading) < -0.5F) {
        ++reversals;
      }
      heading = moved.normalized();
    }
    const auto* builder = builder_of(crew);
    worked = worked || (builder->at_construction_site && builder->in_progress);
  }

  EXPECT_TRUE(worked) << "the crew never set to work on a tree within its reach";
  EXPECT_LE(reversals, 2)
      << "the crew jittered back and forth " << reversals
      << " times between its tree and the ground it cannot stand on";
  EXPECT_NE(builder_of(crew)->fault, Engine::Core::BuilderTaskFault::Unreachable);
}

TEST_F(BuilderCrewsTest, AHouseCannotBeOrderedOnTopOfStandingTroops) {

  const EntityID crew = spawn_builder(48, 48);
  const EntityID squad = spawn_squad(50, 48);
  ASSERT_NE(crew, 0U);
  ASSERT_NE(squad, 0U);
  auto& economy = m_session->economy();
  economy.add(k_player, Game::Systems::ResourceType::Wood, 500);
  economy.add(k_player, Game::Systems::ResourceType::Stone, 500);

  const QVector3D site = Game::Systems::NavGrid::grid_to_world({48, 48});
  EXPECT_TRUE(Game::Systems::troops_stand_on(
      m_session->world(), "home", site.x(), site.z(), 0.0F, {&crew, 1}));
  EXPECT_EQ(Game::Systems::StructurePlacementService::ground_ruling(
                m_session->world(), "home", site.x(), site.z(), 0.0F, {&crew, 1}),
            Game::Systems::PlacementRuling::BlockedByTroops)
      << "the player is told a building stands where only his own men do";
  ASSERT_TRUE(Game::Command::submit(
      m_session->world(),
      Game::Command::Source::LocalPlayer,
      k_player,
      Game::Command::StartConstruction{
          .units = {crew}, .construction_type = "home", .site = site}));
  step();

  const auto* builder = builder_of(crew);
  ASSERT_NE(builder, nullptr);
  EXPECT_FALSE(builder->has_construction_site)
      << "the house was ordered on ground a squad is standing on";

  const QVector3D clear = Game::Systems::NavGrid::grid_to_world({70, 48});
  EXPECT_FALSE(Game::Systems::troops_stand_on(
      m_session->world(), "home", clear.x(), clear.z(), 0.0F, {&crew, 1}))
      << "open ground away from the squad must stay buildable";
}

TEST_F(BuilderCrewsTest, CrewsSentToOneTreeSpreadOverTheGroveApart) {
  const std::vector<EntityID> crews{
      spawn_builder(40, 46), spawn_builder(40, 48), spawn_builder(40, 50)};
  std::vector<std::uint64_t> grove;
  for (int i = 0; i < 6; ++i) {
    const QVector3D spot =
        Game::Systems::NavGrid::grid_to_world({52 + (i % 3) * 2, 46 + (i / 3) * 2});
    grove.push_back(tree_at(spot.x(), spot.z()));
  }
  const QVector3D first = Game::Systems::NavGrid::grid_to_world({52, 46});
  ASSERT_TRUE(Game::Command::submit(
      m_session->world(),
      Game::Command::Source::LocalPlayer,
      k_player,
      Game::Command::StartHarvest{.units = crews,
                                  .construction_type = "cut_tree",
                                  .resource_target = grove.front(),
                                  .site = first}));

  float closest = 1.0e9F;
  int most_working = 0;
  const double tick = m_session->clock().tick_seconds();
  for (double elapsed = 0.0; elapsed < 40.0; elapsed += tick) {
    step();
    int working = 0;
    for (const auto id : crews) {
      const auto* builder = builder_of(id);
      working += builder != nullptr && builder->at_construction_site ? 1 : 0;
    }
    most_working = std::max(most_working, working);
    if (working == static_cast<int>(crews.size())) {
      closest = std::min(closest, closest_pair(crews));
    }
  }
  EXPECT_EQ(most_working, static_cast<int>(crews.size()))
      << "crews in one gather order stood idle while free trees stood beside the one "
         "clicked";
  EXPECT_GE(closest, 1.5F) << "two crews gathered standing on the same tree";
}

TEST_F(BuilderCrewsTest, AHalfCrewGathersFarSlowerThanAWholeOne) {
  auto pace_of = [this](int men_divisor, int grid_z) {
    const EntityID crew = spawn_builder(40, grid_z);
    auto* unit = m_session->world().try_get<Engine::Core::UnitComponent>(crew);
    unit->squad_strength =
        Game::Units::squad_establishment(unit->spawn_type) / men_divisor;
    const QVector3D spot = Game::Systems::NavGrid::grid_to_world({44, grid_z});
    const auto tree = tree_at(spot.x(), spot.z());
    Game::Command::submit(m_session->world(),
                          Game::Command::Source::LocalPlayer,
                          k_player,
                          Game::Command::StartHarvest{.units = {crew},
                                                      .construction_type = "cut_tree",
                                                      .resource_target = tree,
                                                      .site = spot});
    auto* builder = builder_of(crew);
    for (int i = 0;
         i < 1200 && !(builder->at_construction_site && builder->in_progress &&
                       builder->time_remaining < builder->build_time);
         ++i) {
      step();
    }
    const float before = builder->time_remaining;
    run(2.0);
    return (before - builder->time_remaining) / 2.0F;
  };
  const float whole = pace_of(1, 30);
  const float half = pace_of(2, 60);
  ASSERT_GT(whole, 0.0F) << "the whole crew never started gathering";
  EXPECT_NEAR(whole, 1.0F, 0.05F);
  EXPECT_LT(half, whole * 0.40F)
      << "half a crew gathered at " << half << " of a whole crew's " << whole
      << "; splitting a crew must not multiply what it gathers";
  EXPECT_GT(Game::Systems::construction_build_time("cut_tree"), 11.0F)
      << "gathering takes twice as long as it did";
}

} // namespace
