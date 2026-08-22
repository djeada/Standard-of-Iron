#include <QFile>
#include <QSet>
#include <QTemporaryDir>

#include <algorithm>
#include <gtest/gtest.h>

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/commander_system.h"
#include "game/systems/default_content.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/undead_awakening_system.h"
#include "game/units/spawn_type.h"
#include "render/profiling/combat_animation_diagnostics.h"
#include "tools/arena/arena_scenario.h"
#include "tools/arena/arena_scenarios.h"

namespace {

auto make_entity_host(Engine::Core::World& world) -> Arena::ArenaScenarioHost {
  Arena::ArenaScenarioHost host;
  host.spawn_unit = [&world](const Arena::ArenaScenarioGroup& group,
                             const QVector3D& position) {
    auto* entity = world.create_entity();
    entity->add_component<Engine::Core::TransformComponent>(
        position.x(), position.y(), position.z());
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    unit->owner_id = group.owner_id;
    unit->spawn_type = Game::Units::spawn_typeFromTroopType(group.troop_type);
    unit->health = group.health_override > 0 ? group.health_override : 100;
    unit->max_health = group.max_health_override > 0 ? group.max_health_override : 100;
    entity->add_component<Engine::Core::MovementComponent>();
    entity->add_component<Engine::Core::AttackComponent>();
    if (Game::Units::is_commander_troop(group.troop_type)) {
      entity->add_component<Engine::Core::CommanderComponent>();
    }
    return entity->get_id();
  };
  host.find_unit = [](Engine::Core::EntityID) -> Game::Units::Unit* {
    return nullptr;
  };
  host.set_camera = [](const auto&, const auto&) {
  };
  host.set_force_full_creature_lod = [](bool) {
  };
  return host;
}

auto minimal_definition() -> Arena::ArenaScenarioDefinition {
  Arena::ArenaScenarioDefinition scenario;
  scenario.id = QStringLiteral("runner_test");
  scenario.label = QStringLiteral("Runner Test");
  scenario.duration_seconds = 1.0F;
  scenario.groups = {{QStringLiteral("blue"),
                      Game::Units::TroopType::Swordsman,
                      Game::Systems::NationID::RomanRepublic,
                      1,
                      2,
                      1,
                      QVector3D(-2.0F, 0.0F, 0.0F),
                      QVector3D(1.0F, 0.0F, 0.0F)},
                     {QStringLiteral("red"),
                      Game::Units::TroopType::Spearman,
                      Game::Systems::NationID::Carthage,
                      2,
                      1,
                      1,
                      QVector3D(2.0F, 0.0F, 0.0F)}};
  Arena::ArenaScenarioStep attack;
  attack.trigger = {Arena::ScenarioTriggerKind::AtTime, 0.0F};
  attack.command = Arena::ScenarioCommandKind::Attack;
  attack.group = QStringLiteral("blue");
  attack.target_group = QStringLiteral("red");
  scenario.steps = {attack};
  scenario.expectations = {{Arena::ArenaExpectationKind::NoRootTeleport}};
  return scenario;
}

constexpr int k_guardian_owner_id = 99;

auto undead_zone_definition() -> Arena::ArenaScenarioDefinition {
  auto scenario = minimal_definition();
  scenario.groups.resize(1);
  scenario.steps.clear();
  scenario.duration_seconds = 10.0F;

  Game::Map::UndeadZone zone;
  zone.id = QStringLiteral("crypt");
  zone.anchor_type = Game::Map::WorldProp::Type::MagicShrine;
  zone.radius = 6.0F;
  zone.owner_id = k_guardian_owner_id;
  zone.team_id = k_guardian_owner_id;
  zone.awaken_on = {QStringLiteral("unit_enters_radius")};
  Game::Map::UndeadWave wave;
  wave.trigger = QStringLiteral("initial");
  wave.units.push_back({Game::Units::SpawnType::SkeletonSwordsman, 2});
  zone.waves.push_back(wave);
  scenario.undead_zones = {zone};
  return scenario;
}

auto spawn_guardian(Engine::Core::World& world) -> Engine::Core::UnitComponent* {
  auto* entity = world.create_entity();
  entity->add_component<Engine::Core::TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* unit = entity->add_component<Engine::Core::UnitComponent>();
  unit->owner_id = k_guardian_owner_id;
  unit->spawn_type = Game::Units::SpawnType::SkeletonSwordsman;
  unit->health = 100;
  unit->max_health = 100;
  return unit;
}

class UndeadZoneWorld {
public:
  explicit UndeadZoneWorld(const Arena::ArenaScenarioDefinition& scenario) {
    auto& owners = Game::Systems::OwnerRegistry::instance();
    owners.clear();
    owners.register_owner_with_id(1, Game::Systems::OwnerType::Player, "Player");
    owners.set_owner_team(1, 1);
    owners.set_local_player_id(1);

    auto& nations = Game::Systems::NationRegistry::instance();
    nations.clear();
    Game::Systems::initialize_default_content(nations);
    nations.set_player_nation(1, Game::Systems::NationID::RomanRepublic);

    Game::Systems::BuildingCollisionRegistry::instance().clear();

    m_map.coordSystem = Game::Map::CoordSystem::World;
    m_map.grid.width = 64;
    m_map.grid.height = 64;
    m_map.grid.tile_size = 1.0F;
    m_map.undead_zones = scenario.undead_zones;
    Game::Map::TerrainService::instance().initialize(m_map);

    m_world.add_system(std::make_unique<Game::Systems::UndeadAwakeningSystem>());
    undead().configure(m_map);
  }

  ~UndeadZoneWorld() {
    Game::Map::TerrainService::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
    Game::Systems::OwnerRegistry::instance().clear();
  }

  UndeadZoneWorld(const UndeadZoneWorld&) = delete;
  UndeadZoneWorld(UndeadZoneWorld&&) = delete;
  auto operator=(const UndeadZoneWorld&) -> UndeadZoneWorld& = delete;
  auto operator=(UndeadZoneWorld&&) -> UndeadZoneWorld& = delete;

  [[nodiscard]] auto world() -> Engine::Core::World& { return m_world; }
  [[nodiscard]] auto undead() -> Game::Systems::UndeadAwakeningSystem& {
    return *m_world.get_system<Game::Systems::UndeadAwakeningSystem>();
  }

  void run(Arena::ArenaScenarioRunner& runner) {
    while (!runner.finished()) {
      m_world.update(0.1F);
      runner.update(0.1F);
    }
  }

private:
  Engine::Core::World m_world;
  Game::Map::MapDefinition m_map;
};

auto issue_codes(const Arena::ArenaScenarioReport& report) -> std::vector<QString> {
  std::vector<QString> codes;
  codes.reserve(report.issues.size());
  for (auto const& issue : report.issues) {
    codes.push_back(issue.code);
  }
  return codes;
}

auto contains_code(const Arena::ArenaScenarioReport& report,
                   const QString& code) -> bool {
  auto const codes = issue_codes(report);
  return std::find(codes.begin(), codes.end(), code) != codes.end();
}

} // namespace

TEST(ArenaScenarioRunnerTest, UndeadZoneExpectationsRequireAZoneReference) {
  auto scenario = undead_zone_definition();
  scenario.expectations = {{Arena::ArenaExpectationKind::UndeadZoneAwakened}};
  EXPECT_FALSE(Arena::validate_scenario(scenario).empty());

  scenario.expectations.front().zone_id = QStringLiteral("not_a_zone");
  EXPECT_FALSE(Arena::validate_scenario(scenario).empty());

  scenario.expectations.front().zone_id = QStringLiteral("crypt");
  EXPECT_TRUE(Arena::validate_scenario(scenario).empty());
}

TEST(ArenaScenarioRunnerTest, UndeadZoneAwakensOnlyAfterTheDormantWindow) {
  Engine::Core::World world;
  auto scenario = undead_zone_definition();
  Arena::ArenaExpectation dormant;
  dormant.kind = Arena::ArenaExpectationKind::UndeadZoneDormantBefore;
  dormant.zone_id = QStringLiteral("crypt");
  dormant.end_seconds = 3.0F;
  Arena::ArenaExpectation awakened;
  awakened.kind = Arena::ArenaExpectationKind::UndeadZoneAwakened;
  awakened.zone_id = QStringLiteral("crypt");
  awakened.threshold = 2.0F;
  scenario.expectations = {dormant, awakened};

  Arena::ArenaScenarioRunner runner(world, make_entity_host(world), scenario);
  ASSERT_TRUE(runner.start());

  for (int step = 0; step < 40; ++step) {
    runner.update(0.1F);
  }
  spawn_guardian(world);
  spawn_guardian(world);
  while (!runner.finished()) {
    runner.update(0.1F);
  }

  EXPECT_TRUE(runner.report().passed()) << runner.report().summary().toStdString();
}

TEST(ArenaScenarioRunnerTest, UndeadZoneThatSpawnsInsideTheDormantWindowFails) {
  Engine::Core::World world;
  auto scenario = undead_zone_definition();
  Arena::ArenaExpectation dormant;
  dormant.kind = Arena::ArenaExpectationKind::UndeadZoneDormantBefore;
  dormant.zone_id = QStringLiteral("crypt");
  dormant.end_seconds = 3.0F;
  scenario.expectations = {dormant};

  Arena::ArenaScenarioRunner runner(world, make_entity_host(world), scenario);
  ASSERT_TRUE(runner.start());
  spawn_guardian(world);
  while (!runner.finished()) {
    runner.update(0.1F);
  }

  EXPECT_FALSE(runner.report().passed());
  EXPECT_TRUE(
      contains_code(runner.report(), QStringLiteral("undead_zone_woke_too_early")));
}

TEST(ArenaScenarioRunnerTest, UndeadZoneThatNeverSpawnsFailsTheAwakeningContract) {
  Engine::Core::World world;
  auto scenario = undead_zone_definition();
  Arena::ArenaExpectation awakened;
  awakened.kind = Arena::ArenaExpectationKind::UndeadZoneAwakened;
  awakened.zone_id = QStringLiteral("crypt");
  scenario.expectations = {awakened};

  Arena::ArenaScenarioRunner runner(world, make_entity_host(world), scenario);
  ASSERT_TRUE(runner.start());
  while (!runner.finished()) {
    runner.update(0.1F);
  }

  EXPECT_FALSE(runner.report().passed());
  EXPECT_TRUE(
      contains_code(runner.report(), QStringLiteral("undead_zone_never_awakened")));
}

TEST(ArenaScenarioRunnerTest, UndeadZoneIsClearedOnlyWhenEveryGuardianIsDead) {
  Engine::Core::World world;
  auto scenario = undead_zone_definition();
  Arena::ArenaExpectation cleared;
  cleared.kind = Arena::ArenaExpectationKind::UndeadZoneCleared;
  cleared.zone_id = QStringLiteral("crypt");
  scenario.expectations = {cleared};

  Arena::ArenaScenarioRunner runner(world, make_entity_host(world), scenario);
  ASSERT_TRUE(runner.start());
  auto* first = spawn_guardian(world);
  auto* second = spawn_guardian(world);
  runner.update(0.1F);

  first->health = 0;
  for (int step = 0; step < 40; ++step) {
    runner.update(0.1F);
  }
  ASSERT_FALSE(runner.finished());

  second->health = 0;
  while (!runner.finished()) {
    runner.update(0.1F);
  }

  EXPECT_TRUE(runner.report().passed()) << runner.report().summary().toStdString();
}

TEST(ArenaScenarioRunnerTest, UndeadZoneWithSurvivingGuardiansIsNotCleared) {
  Engine::Core::World world;
  auto scenario = undead_zone_definition();
  Arena::ArenaExpectation cleared;
  cleared.kind = Arena::ArenaExpectationKind::UndeadZoneCleared;
  cleared.zone_id = QStringLiteral("crypt");
  scenario.expectations = {cleared};

  Arena::ArenaScenarioRunner runner(world, make_entity_host(world), scenario);
  ASSERT_TRUE(runner.start());
  spawn_guardian(world);
  while (!runner.finished()) {
    runner.update(0.1F);
  }

  EXPECT_FALSE(runner.report().passed());
  EXPECT_TRUE(
      contains_code(runner.report(), QStringLiteral("undead_zone_not_cleared")));
}

TEST(ArenaScenarioRunnerTest, ZoneShrineStandsWithoutAnAuthoredProp) {
  auto scenario = undead_zone_definition();
  Arena::ArenaExpectation stands;
  stands.kind = Arena::ArenaExpectationKind::UndeadZoneShrineStands;
  stands.zone_id = QStringLiteral("crypt");
  scenario.expectations = {stands};

  UndeadZoneWorld fixture(scenario);
  Arena::ArenaScenarioRunner runner(
      fixture.world(), make_entity_host(fixture.world()), scenario);
  ASSERT_TRUE(runner.start());
  fixture.run(runner);

  EXPECT_TRUE(runner.report().passed()) << runner.report().summary().toStdString();
  EXPECT_NE(fixture.undead().anchor_entity(QStringLiteral("crypt")), 0U);
}

TEST(ArenaScenarioRunnerTest, RazingTheZoneShrineSatisfiesTheDestructionContract) {
  auto scenario = undead_zone_definition();
  Arena::ArenaExpectation destroyed;
  destroyed.kind = Arena::ArenaExpectationKind::UndeadZoneShrineDestroyed;
  destroyed.zone_id = QStringLiteral("crypt");
  scenario.expectations = {destroyed};

  Arena::ArenaScenarioStep raze;
  raze.trigger = {Arena::ScenarioTriggerKind::AtTime, 2.0F};
  raze.command = Arena::ScenarioCommandKind::ApplyDamage;
  raze.zone_id = QStringLiteral("crypt");
  raze.value = 100000;
  scenario.steps = {raze};
  ASSERT_TRUE(Arena::validate_scenario(scenario).empty());

  UndeadZoneWorld fixture(scenario);
  Arena::ArenaScenarioRunner runner(
      fixture.world(), make_entity_host(fixture.world()), scenario);
  ASSERT_TRUE(runner.start());
  fixture.run(runner);

  EXPECT_TRUE(runner.report().passed()) << runner.report().summary().toStdString();
}

TEST(ArenaScenarioRunnerTest, AShrineThatStandsFailsTheDestructionContract) {
  auto scenario = undead_zone_definition();
  Arena::ArenaExpectation destroyed;
  destroyed.kind = Arena::ArenaExpectationKind::UndeadZoneShrineDestroyed;
  destroyed.zone_id = QStringLiteral("crypt");
  scenario.expectations = {destroyed};

  UndeadZoneWorld fixture(scenario);
  Arena::ArenaScenarioRunner runner(
      fixture.world(), make_entity_host(fixture.world()), scenario);
  ASSERT_TRUE(runner.start());
  fixture.run(runner);

  EXPECT_FALSE(runner.report().passed());
  EXPECT_TRUE(
      contains_code(runner.report(), QStringLiteral("undead_zone_shrine_survived")));
}

TEST(ArenaScenarioRunnerTest, ReloadingZoneStateKeepsTheShrineItAlreadyPlanted) {
  auto scenario = undead_zone_definition();
  Arena::ArenaExpectation stands;
  stands.kind = Arena::ArenaExpectationKind::UndeadZoneShrineStands;
  stands.zone_id = QStringLiteral("crypt");
  scenario.expectations = {stands};

  Arena::ArenaScenarioStep reload;
  reload.trigger = {Arena::ScenarioTriggerKind::AtTime, 2.0F};
  reload.command = Arena::ScenarioCommandKind::ReloadUndeadZoneState;
  scenario.steps = {reload};
  ASSERT_TRUE(Arena::validate_scenario(scenario).empty());

  UndeadZoneWorld fixture(scenario);
  Arena::ArenaScenarioRunner runner(
      fixture.world(), make_entity_host(fixture.world()), scenario);
  ASSERT_TRUE(runner.start());
  fixture.world().update(0.1F);
  runner.update(0.1F);
  const Engine::Core::EntityID shrine_before =
      fixture.undead().anchor_entity(QStringLiteral("crypt"));
  ASSERT_NE(shrine_before, 0U);

  fixture.run(runner);

  EXPECT_EQ(fixture.undead().anchor_entity(QStringLiteral("crypt")), shrine_before);
  EXPECT_TRUE(runner.report().passed()) << runner.report().summary().toStdString();
}

TEST(ArenaScenarioRunnerTest, StepsRejectAnUnknownUndeadZoneReference) {
  auto scenario = undead_zone_definition();
  Arena::ArenaScenarioStep raze;
  raze.trigger = {Arena::ScenarioTriggerKind::AtTime, 1.0F};
  raze.command = Arena::ScenarioCommandKind::ApplyDamage;
  raze.zone_id = QStringLiteral("not_a_zone");
  raze.value = 10;
  scenario.steps = {raze};

  EXPECT_FALSE(Arena::validate_scenario(scenario).empty());
}

TEST(ArenaScenarioDefinitionTest, EntireLocalCatalogIsValidAndUniquelyAddressable) {
  QSet<QString> ids;
  ASSERT_GE(Arena::Scenarios::definitions().size(), 20U);
  for (auto const& scenario : Arena::Scenarios::definitions()) {
    {
      auto const problems = Arena::validate_scenario(scenario);
      std::string detail;
      for (auto const& problem : problems) {
        detail += " [" + problem.field.toStdString() + ": " +
                  problem.message.toStdString() + "]";
      }
      EXPECT_TRUE(problems.empty()) << scenario.id.toStdString() << detail;
    }
    EXPECT_FALSE(ids.contains(scenario.id)) << scenario.id.toStdString();
    ids.insert(scenario.id);
    EXPECT_EQ(Arena::Scenarios::find_definition(scenario.id), &scenario);
  }
}

TEST(ArenaScenarioDefinitionTest, CatalogIncludesRealBattlefieldRegressionCases) {
  for (auto const* id : {Arena::Scenarios::k_three_swords_vs_two_spears_id,
                         Arena::Scenarios::k_spear_walk_contact_id,
                         Arena::Scenarios::k_archer_stability_id,
                         Arena::Scenarios::k_archer_melee_lock_id,
                         Arena::Scenarios::k_flank_ambush_id,
                         Arena::Scenarios::k_reserve_release_id,
                         Arena::Scenarios::k_bot_skirmish_id}) {
    auto const* scenario = Arena::Scenarios::find_definition(QString::fromLatin1(id));
    ASSERT_NE(scenario, nullptr) << id;
    EXPECT_FALSE(scenario->steps.empty());
    EXPECT_FALSE(scenario->expectations.empty());
  }
}

TEST(ArenaScenarioDefinitionTest, RenderContinuityScenarioExercisesUltraPolicy) {
  auto const* scenario = Arena::Scenarios::find_definition(
      QString::fromLatin1(Arena::Scenarios::k_render_continuity_id));
  ASSERT_NE(scenario, nullptr);
  EXPECT_FALSE(scenario->force_full_creature_lod);
  EXPECT_EQ(scenario->graphics_quality, Render::GraphicsQuality::Ultra);
  EXPECT_TRUE(scenario->collect_animation_diagnostics);
  EXPECT_TRUE(scenario->suppress_ui_overlays);
  EXPECT_GE(scenario->groups.size(), 8U);

  EXPECT_NE(std::find_if(scenario->expectations.begin(),
                         scenario->expectations.end(),
                         [](auto const& expectation) {
                           return expectation.kind ==
                                  Arena::ArenaExpectationKind::NoFullscreenFlash;
                         }),
            scenario->expectations.end());
  for (auto const& group : scenario->groups) {
    EXPECT_NE(std::find_if(
                  scenario->expectations.begin(),
                  scenario->expectations.end(),
                  [&](auto const& expectation) {
                    return expectation.kind ==
                               Arena::ArenaExpectationKind::NoRenderVisibilityChurn &&
                           expectation.group == group.name;
                  }),
              scenario->expectations.end())
        << group.name.toStdString();
    EXPECT_NE(
        std::find_if(scenario->expectations.begin(),
                     scenario->expectations.end(),
                     [&](auto const& expectation) {
                       return expectation.kind ==
                                  Arena::ArenaExpectationKind::FullCreatureDetailOnly &&
                              expectation.group == group.name;
                     }),
        scenario->expectations.end())
        << group.name.toStdString();
  }
}

TEST(ArenaScenarioDefinitionTest, BattlefieldScenariosUseNationFormationProfiles) {
  auto const* scenario = Arena::Scenarios::find_definition(
      QString::fromLatin1(Arena::Scenarios::k_spear_walk_contact_id));
  ASSERT_NE(scenario, nullptr);
  ASSERT_EQ(scenario->groups.size(), 2U);
  EXPECT_EQ(scenario->groups[0].individuals_per_unit, 0);
  EXPECT_EQ(scenario->groups[1].individuals_per_unit, 0);
}

TEST(ArenaScenarioDefinitionTest, WaterShowcaseOwnsDistinctRiverAndLakeGeometry) {
  auto const* scenario = Arena::Scenarios::find_definition(
      QString::fromLatin1(Arena::Scenarios::k_water_showcase_id));
  ASSERT_NE(scenario, nullptr);
  ASSERT_EQ(scenario->rivers.size(), 1U);
  ASSERT_EQ(scenario->lakes.size(), 1U);
  EXPECT_GT(scenario->rivers.front().width, 0.0F);
  EXPECT_GT(scenario->lakes.front().width, scenario->lakes.front().depth);
}

TEST(ArenaScenarioDefinitionTest, SettlementScenariosOwnHistoricalBuildingProfiles) {
  auto const* roman = Arena::Scenarios::find_definition(
      QString::fromLatin1(Arena::Scenarios::k_roman_marching_camp_id));
  auto const* carthage = Arena::Scenarios::find_definition(
      QString::fromLatin1(Arena::Scenarios::k_carthage_trade_town_id));
  ASSERT_NE(roman, nullptr);
  ASSERT_NE(carthage, nullptr);

  auto verify = [](auto const& scenario, Game::Systems::NationID nation) {
    int building_groups = 0;
    for (auto const& group : scenario.groups) {
      if (!group.spawn_type.has_value() ||
          !Game::Units::is_building_spawn(*group.spawn_type)) {
        continue;
      }
      ++building_groups;
      EXPECT_EQ(group.nation_id, nation);
    }
    EXPECT_GE(building_groups, 5);
  };
  verify(*roman, Game::Systems::NationID::RomanRepublic);
  verify(*carthage, Game::Systems::NationID::Carthage);
}

TEST(ArenaScenarioDefinitionTest, FortificationShowcasesContainWallsTowersAndGateRuns) {
  auto verify = [](const char* id, Game::Systems::NationID nation) {
    auto const* scenario = Arena::Scenarios::find_definition(QString::fromLatin1(id));
    ASSERT_NE(scenario, nullptr);

    int wall_groups = 0;
    int wall_segments = 0;
    int tower_groups = 0;
    for (auto const& group : scenario->groups) {
      if (!group.spawn_type.has_value()) {
        continue;
      }
      EXPECT_EQ(group.nation_id, nation);
      if (*group.spawn_type == Game::Units::SpawnType::WallSegment) {
        ++wall_groups;
        wall_segments += group.count;
      } else if (*group.spawn_type == Game::Units::SpawnType::DefenseTower) {
        ++tower_groups;
      }
    }

    EXPECT_GE(wall_groups, 5);
    EXPECT_GE(wall_segments, 30);
    EXPECT_GE(tower_groups, 6);
    EXPECT_TRUE(scenario->suppress_ui_overlays);
  };

  verify(Arena::Scenarios::k_roman_fortification_showcase_id,
         Game::Systems::NationID::RomanRepublic);
  verify(Arena::Scenarios::k_carthage_fortification_showcase_id,
         Game::Systems::NationID::Carthage);
}

TEST(ArenaScenarioDefinitionTest, RejectsDuplicateAndUnknownGroupReferences) {
  auto scenario = minimal_definition();
  scenario.groups[1].name = scenario.groups[0].name;
  scenario.steps[0].target_group = QStringLiteral("missing");
  auto const errors = Arena::validate_scenario(scenario);
  EXPECT_GE(errors.size(), 2U);
}

TEST(ArenaScenarioRunnerTest, SpawnsNamedGroupsAndUsesProductionAttackCommandShape) {
  Engine::Core::World world;
  auto scenario = minimal_definition();
  Arena::ArenaScenarioRunner runner(world, make_entity_host(world), scenario);
  ASSERT_TRUE(runner.start());
  ASSERT_EQ(runner.group_entities(QStringLiteral("blue")).size(), 2U);
  ASSERT_EQ(runner.group_entities(QStringLiteral("red")).size(), 1U);

  auto const target_id = runner.group_entities(QStringLiteral("red")).front();
  for (auto const attacker_id : runner.group_entities(QStringLiteral("blue"))) {
    auto* entity = world.get_entity(attacker_id);
    auto const* target = entity->get_component<Engine::Core::AttackTargetComponent>();
    ASSERT_NE(target, nullptr);
    EXPECT_EQ(target->target_id, target_id);
  }
}

TEST(ArenaScenarioRunnerTest, RenderProbeRejectsAliveSoldierWithFallenSubmittedBody) {
  Engine::Core::World world;
  auto scenario = minimal_definition();
  scenario.groups.resize(1);
  scenario.steps.clear();
  scenario.expectations = {
      {Arena::ArenaExpectationKind::NoUnexpectedFallPose, QStringLiteral("blue")}};
  Arena::ArenaScenarioRunner runner(world, make_entity_host(world), scenario);
  ASSERT_TRUE(runner.start());

  auto const entity_id = runner.group_entities(QStringLiteral("blue")).front();
  auto& diagnostics = Render::Profiling::CombatAnimationDiagnostics::instance();
  diagnostics.set_enabled(true);
  diagnostics.begin_frame(1);
  Render::Profiling::SoldierAnimationDebugSample sample;
  sample.soldier_index = 0;
  sample.root_position = QVector3D(-2.0F, 0.0F, 0.0F);
  sample.root_up_y = 1.0F;
  sample.visual_state = Render::Profiling::SoldierVisualState::Idle;
  diagnostics.record_soldier_sample(entity_id, sample);
  diagnostics.record_submitted_body_pose(entity_id, 0U, 0.1F, 0.55F);
  runner.observe_rendered_frame(4.0);

  ASSERT_FALSE(runner.report().passed());
  EXPECT_EQ(runner.report().issues.front().code,
            QStringLiteral("unexpected_fall_pose"));
  diagnostics.set_enabled(false);
}

TEST(ArenaScenarioRunnerTest, RenderProbeRejectsOverextendedAliveSoldierArm) {
  Engine::Core::World world;
  auto scenario = minimal_definition();
  scenario.groups.resize(1);
  scenario.steps.clear();
  scenario.expectations = {
      {Arena::ArenaExpectationKind::NoLimbOverextension, QStringLiteral("blue")}};
  Arena::ArenaScenarioRunner runner(world, make_entity_host(world), scenario);
  ASSERT_TRUE(runner.start());

  auto const entity_id = runner.group_entities(QStringLiteral("blue")).front();
  auto& diagnostics = Render::Profiling::CombatAnimationDiagnostics::instance();
  diagnostics.set_enabled(true);
  diagnostics.begin_frame(1);
  Render::Profiling::SoldierAnimationDebugSample sample;
  sample.soldier_index = 0;
  sample.root_position = QVector3D(-2.0F, 0.0F, 0.0F);
  sample.visual_state = Render::Profiling::SoldierVisualState::Attack;
  diagnostics.record_soldier_sample(entity_id, sample);
  diagnostics.record_submitted_body_pose(entity_id, 0U, 1.0F, 1.10F);
  runner.observe_rendered_frame(4.0);

  ASSERT_FALSE(runner.report().passed());
  EXPECT_EQ(runner.report().issues.front().code, QStringLiteral("limb_overextension"));
  diagnostics.set_enabled(false);
}

TEST(ArenaScenarioRunnerTest, RenderProbeRejectsAttackClipReplacingActiveHoldPose) {
  Engine::Core::World world;
  auto scenario = minimal_definition();
  scenario.groups.resize(1);
  scenario.steps.clear();
  scenario.expectations = {
      {Arena::ArenaExpectationKind::HoldPoseMaintained, QStringLiteral("blue")}};
  Arena::ArenaScenarioRunner runner(world, make_entity_host(world), scenario);
  ASSERT_TRUE(runner.start());

  auto const entity_id = runner.group_entities(QStringLiteral("blue")).front();
  auto* entity = world.get_entity(entity_id);
  ASSERT_NE(entity, nullptr);
  auto* hold = entity->add_component<Engine::Core::HoldModeComponent>();
  ASSERT_NE(hold, nullptr);
  hold->active = true;
  hold->kneel_entry_progress = 1.0F;

  auto& diagnostics = Render::Profiling::CombatAnimationDiagnostics::instance();
  diagnostics.set_enabled(true);
  diagnostics.begin_frame(1);
  Render::Profiling::SoldierAnimationDebugSample sample;
  sample.soldier_index = 0;
  sample.animation_state = Render::Creature::AnimationStateId::AttackSpear;
  sample.is_attacking = true;
  diagnostics.record_soldier_sample(entity_id, sample);
  runner.observe_rendered_frame(4.0);

  ASSERT_FALSE(runner.report().passed());
  EXPECT_EQ(runner.report().issues.front().code, QStringLiteral("hold_pose_replaced"));
  diagnostics.set_enabled(false);
}

TEST(ArenaScenarioRunnerTest, RenderProbeRejectsMissingIndicatorDuringMeleeLock) {
  Engine::Core::World world;
  auto scenario = minimal_definition();
  scenario.groups.resize(1);
  scenario.steps.clear();
  scenario.expectations = {{Arena::ArenaExpectationKind::CombatIndicatorIsContinuous,
                            QStringLiteral("blue")}};
  Arena::ArenaScenarioRunner runner(world, make_entity_host(world), scenario);
  ASSERT_TRUE(runner.start());

  auto const entity_id = runner.group_entities(QStringLiteral("blue")).front();
  auto* attack =
      world.get_entity(entity_id)->get_component<Engine::Core::AttackComponent>();
  ASSERT_NE(attack, nullptr);
  attack->in_melee_lock = true;
  attack->melee_lock_target_id = 99U;

  auto& diagnostics = Render::Profiling::CombatAnimationDiagnostics::instance();
  diagnostics.set_enabled(true);
  diagnostics.begin_frame(1);
  runner.observe_rendered_frame(0.5);

  ASSERT_FALSE(runner.report().passed());
  EXPECT_EQ(runner.report().issues.front().code,
            QStringLiteral("missing_combat_indicator"));
  diagnostics.set_enabled(false);
}

TEST(ArenaScenarioRunnerTest, RenderProbeRejectsVisibleSoldierBecomingCulled) {
  Engine::Core::World world;
  auto scenario = minimal_definition();
  scenario.groups.resize(1);
  scenario.steps.clear();
  scenario.expectations = {
      {Arena::ArenaExpectationKind::NoRenderVisibilityChurn, QStringLiteral("blue")}};
  Arena::ArenaScenarioRunner runner(world, make_entity_host(world), scenario);
  ASSERT_TRUE(runner.start());

  auto const entity_id = runner.group_entities(QStringLiteral("blue")).front();
  auto& diagnostics = Render::Profiling::CombatAnimationDiagnostics::instance();
  diagnostics.set_enabled(true);

  Render::Profiling::SoldierAnimationDebugSample sample;
  sample.soldier_index = 0;
  sample.sample_time = 0.0F;
  diagnostics.begin_frame(1);
  diagnostics.record_soldier_sample(entity_id, sample);
  runner.observe_rendered_frame(1.0);
  ASSERT_TRUE(runner.report().passed());

  sample.sample_time = 1.0F / 60.0F;
  sample.cull_reason = Render::Profiling::SoldierCullReason::Distance;
  diagnostics.begin_frame(2);
  diagnostics.record_soldier_sample(entity_id, sample);
  runner.observe_rendered_frame(1.0);

  ASSERT_FALSE(runner.report().passed());
  ASSERT_FALSE(runner.report().issues.empty());
  EXPECT_EQ(runner.report().issues.front().code,
            QStringLiteral("soldier_submission_disappeared"));
  EXPECT_TRUE(runner.report().issues.front().message.contains(
      QStringLiteral("distance"), Qt::CaseInsensitive));
  diagnostics.set_enabled(false);
}

TEST(ArenaScenarioRunnerTest, RenderProbeAllowsDeadSoldierLeavingFrustum) {
  Engine::Core::World world;
  auto scenario = minimal_definition();
  scenario.groups.resize(1);
  scenario.steps.clear();
  scenario.expectations = {
      {Arena::ArenaExpectationKind::NoRenderVisibilityChurn, QStringLiteral("blue")}};
  Arena::ArenaScenarioRunner runner(world, make_entity_host(world), scenario);
  ASSERT_TRUE(runner.start());

  auto const entity_id = runner.group_entities(QStringLiteral("blue")).front();
  auto& diagnostics = Render::Profiling::CombatAnimationDiagnostics::instance();
  diagnostics.set_enabled(true);

  Render::Profiling::SoldierAnimationDebugSample sample;
  sample.soldier_index = 0;
  sample.animation_state = Render::Creature::AnimationStateId::Dead;
  diagnostics.begin_frame(1);
  diagnostics.record_soldier_sample(entity_id, sample);
  runner.observe_rendered_frame(1.0);

  sample.sample_time = 1.0F / 60.0F;
  sample.cull_reason = Render::Profiling::SoldierCullReason::Frustum;
  diagnostics.begin_frame(2);
  diagnostics.record_soldier_sample(entity_id, sample);
  runner.observe_rendered_frame(1.0);

  EXPECT_TRUE(runner.report().passed()) << runner.report().summary().toStdString();
  diagnostics.set_enabled(false);
}

TEST(ArenaScenarioRunnerTest, RenderProbeRejectsReducedCreatureLodOnUltra) {
  Engine::Core::World world;
  auto scenario = minimal_definition();
  scenario.groups.resize(1);
  scenario.steps.clear();
  scenario.expectations = {
      {Arena::ArenaExpectationKind::FullCreatureDetailOnly, QStringLiteral("blue")}};
  Arena::ArenaScenarioRunner runner(world, make_entity_host(world), scenario);
  ASSERT_TRUE(runner.start());

  auto const entity_id = runner.group_entities(QStringLiteral("blue")).front();
  auto& diagnostics = Render::Profiling::CombatAnimationDiagnostics::instance();
  diagnostics.set_enabled(true);
  diagnostics.begin_frame(1);
  Render::Profiling::SoldierAnimationDebugSample sample;
  sample.soldier_index = 0;
  sample.lod = static_cast<std::uint8_t>(Render::Creature::CreatureLOD::Minimal);
  diagnostics.record_soldier_sample(entity_id, sample);
  runner.observe_rendered_frame(1.0);

  ASSERT_FALSE(runner.report().passed());
  ASSERT_FALSE(runner.report().issues.empty());
  EXPECT_EQ(runner.report().issues.front().code,
            QStringLiteral("ultra_creature_lod_used"));
  diagnostics.set_enabled(false);
}

TEST(ArenaScenarioRunnerTest, ExternalFrameProbeFailureIsReported) {
  Engine::Core::World world;
  auto scenario = minimal_definition();
  Arena::ArenaScenarioRunner runner(world, make_entity_host(world), scenario);
  ASSERT_TRUE(runner.start());

  runner.report_external_issue(QStringLiteral("fullscreen_flash"),
                               QStringLiteral("synthetic flash"));

  ASSERT_FALSE(runner.report().passed());
  ASSERT_FALSE(runner.report().issues.empty());
  EXPECT_EQ(runner.report().issues.front().code, QStringLiteral("fullscreen_flash"));
}

TEST(ArenaScenarioRunnerTest, EventTriggerCanApplyScriptedEdgeCaseDamage) {
  Engine::Core::World world;
  auto scenario = minimal_definition();
  scenario.groups.resize(1);
  scenario.steps.clear();
  Arena::ArenaScenarioStep damage;
  damage.trigger.kind = Arena::ScenarioTriggerKind::GroupEnteredArea;
  damage.trigger.group = QStringLiteral("blue");
  damage.trigger.position = QVector3D(-2.0F, 0.0F, 0.0F);
  damage.trigger.distance = 2.0F;
  damage.command = Arena::ScenarioCommandKind::ApplyDamage;
  damage.group = QStringLiteral("blue");
  damage.value = 10;
  scenario.steps.push_back(damage);

  Arena::ArenaScenarioRunner runner(world, make_entity_host(world), scenario);
  ASSERT_TRUE(runner.start());
  for (auto const entity_id : runner.group_entities(QStringLiteral("blue"))) {
    auto const* unit =
        world.get_entity(entity_id)->get_component<Engine::Core::UnitComponent>();
    ASSERT_NE(unit, nullptr);
    EXPECT_EQ(unit->health, 90);
  }
}

TEST(ArenaScenarioRunnerTest, CommanderAuraScenarioObservesActivationRadiusAndExpiry) {
  Engine::Core::World world;
  world.add_system(std::make_unique<Game::Systems::CommanderSystem>());
  auto const* catalog_scenario = Arena::Scenarios::find_definition(
      QString::fromLatin1(Arena::Scenarios::k_commander_aura_pulse_id));
  ASSERT_NE(catalog_scenario, nullptr);
  auto scenario = *catalog_scenario;
  scenario.expectations.erase(
      std::remove_if(scenario.expectations.begin(),
                     scenario.expectations.end(),
                     [](auto const& expectation) {
                       return expectation.kind ==
                                  Arena::ArenaExpectationKind::GroupIsRendered ||
                              expectation.kind ==
                                  Arena::ArenaExpectationKind::FrameBudget;
                     }),
      scenario.expectations.end());

  Arena::ArenaScenarioRunner runner(world, make_entity_host(world), scenario);
  ASSERT_TRUE(runner.start());
  for (int tick = 0; tick < 60 && !runner.finished(); ++tick) {
    world.update(0.1F);
    runner.update(0.1F);
  }
  ASSERT_TRUE(runner.finished());
  EXPECT_TRUE(runner.report().passed()) << runner.report().summary().toStdString();
}

TEST(ArenaScenarioRunnerTest, WritesStructuredReportAndRenderedTraceLocally) {
  Engine::Core::World world;
  auto scenario = minimal_definition();
  Arena::ArenaScenarioRunner runner(world, make_entity_host(world), scenario);
  ASSERT_TRUE(runner.start());
  runner.observe_rendered_frame(5.0);

  QTemporaryDir const directory;
  ASSERT_TRUE(directory.isValid());
  QString error;
  ASSERT_TRUE(runner.write_artifacts(directory.path(), &error)) << error.toStdString();
  EXPECT_TRUE(QFile::exists(directory.filePath(QStringLiteral("report.json"))));
  EXPECT_TRUE(QFile::exists(directory.filePath(QStringLiteral("trace.jsonl"))));
}

TEST(ArenaScenarioRunnerTest, FrameBudgetReportsMeasuredPercentilesAndFps) {
  Engine::Core::World world;
  auto scenario = minimal_definition();
  Arena::ArenaExpectation budget;
  budget.kind = Arena::ArenaExpectationKind::FrameBudget;
  budget.threshold = 9.99F;
  scenario.expectations = {budget};
  Arena::ArenaScenarioRunner runner(world, make_entity_host(world), scenario);
  ASSERT_TRUE(runner.start());

  for (double const frame_ms : {4.0, 5.0, 6.0, 7.0, 8.0}) {
    Arena::ArenaRenderedFrameTimings timings;
    timings.total_ms = frame_ms;
    timings.visible_soldiers = 24U;
    timings.draw_calls = 12U;
    runner.observe_rendered_frame(timings);
  }
  runner.update(1.0F);

  auto const& report = runner.report();
  ASSERT_TRUE(report.passed()) << report.summary().toStdString();
  EXPECT_EQ(report.frame_time_samples, 5U);
  EXPECT_DOUBLE_EQ(report.frame_budget_ms, 9.99F);
  EXPECT_DOUBLE_EQ(report.frame_time_p50_ms, 6.0);
  EXPECT_DOUBLE_EQ(report.frame_time_p95_ms, 8.0);
  EXPECT_DOUBLE_EQ(report.frame_time_max_ms, 8.0);
  EXPECT_EQ(report.peak_visible_soldiers, 24U);
  EXPECT_EQ(report.peak_draw_commands, 12U);
  EXPECT_TRUE(report.summary().contains(QStringLiteral("125.0 FPS")));
  EXPECT_TRUE(report.summary().contains(QStringLiteral("peak rigged 0 commands/0 "
                                                       "instanced instances")));
}

TEST(ArenaScenarioRunnerTest, FrameBudgetRejectsAnEmptyTroopRender) {
  Engine::Core::World world;
  auto scenario = minimal_definition();
  Arena::ArenaExpectation budget;
  budget.kind = Arena::ArenaExpectationKind::FrameBudget;
  budget.threshold = 9.99F;
  scenario.expectations = {budget};
  Arena::ArenaScenarioRunner runner(world, make_entity_host(world), scenario);
  ASSERT_TRUE(runner.start());

  runner.observe_rendered_frame(1.0);
  runner.update(1.0F);

  EXPECT_TRUE(contains_code(runner.report(),
                            QStringLiteral("performance_scene_rendered_no_creatures")));
}

TEST(CombatAnimationDiagnosticsTest, NormalAttackCadenceIsNotPoseChurn) {
  auto& diagnostics = Render::Profiling::CombatAnimationDiagnostics::instance();
  diagnostics.set_enabled(true);
  diagnostics.begin_frame(1);
  constexpr std::uint32_t entity_id = 900001U;

  auto record = [&](float time,
                    bool attacking,
                    Render::Creature::MovementAnimationState locomotion) {
    Render::Profiling::SoldierAnimationDebugSample sample;
    sample.soldier_index = 0;
    sample.sample_time = time;
    sample.is_attacking = attacking;
    sample.locomotion_state = locomotion;
    diagnostics.record_soldier_sample(entity_id, sample);
  };
  record(0.0F, false, Render::Creature::MovementAnimationState::Idle);
  record(0.10F, false, Render::Creature::MovementAnimationState::Walk);
  record(0.30F, true, Render::Creature::MovementAnimationState::Idle);
  record(0.55F, false, Render::Creature::MovementAnimationState::Idle);
  record(0.80F, true, Render::Creature::MovementAnimationState::Idle);
  record(1.10F, false, Render::Creature::MovementAnimationState::Idle);

  auto const* unit = diagnostics.find_unit(entity_id);
  ASSERT_NE(unit, nullptr);
  ASSERT_FALSE(unit->soldiers.empty());
  EXPECT_GE(unit->soldiers.back().transitions_last_second, 5U);
  EXPECT_FALSE(unit->soldiers.back().churn_flagged);
  diagnostics.set_enabled(false);
}

TEST(CombatAnimationDiagnosticsTest, RapidStateReversalsArePoseChurn) {
  auto& diagnostics = Render::Profiling::CombatAnimationDiagnostics::instance();
  diagnostics.set_enabled(true);
  diagnostics.begin_frame(2);
  constexpr std::uint32_t entity_id = 900002U;

  for (int index = 0; index < 6; ++index) {
    Render::Profiling::SoldierAnimationDebugSample sample;
    sample.soldier_index = 0;
    sample.sample_time = static_cast<float>(index) * 0.05F;
    sample.is_attacking = (index % 2) != 0;
    diagnostics.record_soldier_sample(entity_id, sample);
  }

  auto const* unit = diagnostics.find_unit(entity_id);
  ASSERT_NE(unit, nullptr);
  ASSERT_FALSE(unit->soldiers.empty());
  EXPECT_TRUE(unit->soldiers.back().churn_flagged);

  Render::Profiling::SoldierAnimationDebugSample dying;
  dying.soldier_index = 0;
  dying.sample_time = 0.30F;
  dying.animation_state = Render::Creature::AnimationStateId::Die;
  diagnostics.record_soldier_sample(entity_id, dying);
  unit = diagnostics.find_unit(entity_id);
  ASSERT_NE(unit, nullptr);
  EXPECT_FALSE(unit->soldiers.back().churn_flagged);
  diagnostics.set_enabled(false);
}

TEST(CombatAnimationDiagnosticsTest, AuthoredHitInterruptsAreTracedButNotPoseChurn) {
  auto& diagnostics = Render::Profiling::CombatAnimationDiagnostics::instance();
  diagnostics.set_enabled(true);
  diagnostics.begin_frame(3);
  constexpr std::uint32_t entity_id = 900003U;

  for (int index = 0; index < 8; ++index) {
    Render::Profiling::SoldierAnimationDebugSample sample;
    sample.soldier_index = 0;
    sample.sample_time = static_cast<float>(index) * 0.08F;
    sample.is_attacking = (index % 2) == 0;
    sample.is_hit_reacting = (index % 2) != 0;
    diagnostics.record_soldier_sample(entity_id, sample);
  }

  auto const* unit = diagnostics.find_unit(entity_id);
  ASSERT_NE(unit, nullptr);
  ASSERT_FALSE(unit->soldiers.empty());
  EXPECT_EQ(unit->soldiers.back().visual_state,
            Render::Profiling::SoldierVisualState::HitReaction);
  EXPECT_FALSE(unit->soldiers.back().churn_flagged);
  diagnostics.set_enabled(false);
}

namespace {

auto joint_pose(const QVector3D& foot_l,
                const QVector3D& foot_r,
                const QVector3D& hand_l,
                const QVector3D& hand_r,
                float pelvis_yaw) -> Render::Profiling::SubmittedBodyPose {
  Render::Profiling::SubmittedBodyPose pose;
  pose.body_up_y = 1.0F;
  pose.max_arm_reach = 0.4F;
  pose.foot_l_world = foot_l;
  pose.foot_r_world = foot_r;
  pose.hand_l_world = hand_l;
  pose.hand_r_world = hand_r;
  pose.pelvis_yaw_degrees = pelvis_yaw;
  pose.joints_valid = true;
  return pose;
}

void submit_joint_frame(Arena::ArenaScenarioRunner& runner,
                        Engine::Core::EntityID entity_id,
                        std::uint64_t frame,
                        double at_seconds,
                        const Render::Profiling::SubmittedBodyPose& pose,
                        Render::Profiling::SoldierVisualState state) {
  auto& diagnostics = Render::Profiling::CombatAnimationDiagnostics::instance();
  diagnostics.begin_frame(frame);
  Render::Profiling::SoldierAnimationDebugSample sample;
  sample.soldier_index = 0;
  sample.root_position = QVector3D(-2.0F, 0.0F, 0.0F);
  sample.visual_state = state;
  diagnostics.record_soldier_sample(entity_id, sample);
  diagnostics.record_submitted_body_pose(entity_id, 0U, pose);
  runner.observe_rendered_frame(at_seconds);
}

} // namespace

TEST(ArenaScenarioRunnerTest, RenderProbeRejectsAPlantedFootThatSlides) {
  Engine::Core::World world;
  auto scenario = minimal_definition();
  scenario.groups.resize(1);
  scenario.steps.clear();
  scenario.expectations = {
      {Arena::ArenaExpectationKind::NoPlantedFootSliding, QStringLiteral("blue")}};
  Arena::ArenaScenarioRunner runner(world, make_entity_host(world), scenario);
  ASSERT_TRUE(runner.start());

  auto const entity_id = runner.group_entities(QStringLiteral("blue")).front();
  auto& diagnostics = Render::Profiling::CombatAnimationDiagnostics::instance();
  diagnostics.set_enabled(true);

  submit_joint_frame(runner,
                     entity_id,
                     1,
                     4.0,
                     joint_pose(QVector3D(-2.1F, 0.02F, 0.0F),
                                QVector3D(-1.9F, 0.02F, 0.0F),
                                QVector3D(-2.0F, 1.0F, 0.2F),
                                QVector3D(-2.0F, 1.0F, -0.2F),
                                0.0F),
                     Render::Profiling::SoldierVisualState::Idle);
  submit_joint_frame(runner,
                     entity_id,
                     2,
                     4.02,
                     joint_pose(QVector3D(-1.7F, 0.02F, 0.0F),
                                QVector3D(-1.5F, 0.02F, 0.0F),
                                QVector3D(-2.0F, 1.0F, 0.2F),
                                QVector3D(-2.0F, 1.0F, -0.2F),
                                0.0F),
                     Render::Profiling::SoldierVisualState::Idle);

  ASSERT_FALSE(runner.report().passed());
  EXPECT_EQ(runner.report().issues.front().code, QStringLiteral("planted_foot_slide"));
  diagnostics.set_enabled(false);
}

TEST(ArenaScenarioRunnerTest, RenderProbeAcceptsAFootLiftedOffTheGround) {
  Engine::Core::World world;
  auto scenario = minimal_definition();
  scenario.groups.resize(1);
  scenario.steps.clear();
  scenario.expectations = {
      {Arena::ArenaExpectationKind::NoPlantedFootSliding, QStringLiteral("blue")}};
  Arena::ArenaScenarioRunner runner(world, make_entity_host(world), scenario);
  ASSERT_TRUE(runner.start());

  auto const entity_id = runner.group_entities(QStringLiteral("blue")).front();
  auto& diagnostics = Render::Profiling::CombatAnimationDiagnostics::instance();
  diagnostics.set_enabled(true);

  submit_joint_frame(runner,
                     entity_id,
                     1,
                     4.0,
                     joint_pose(QVector3D(-2.1F, 0.55F, 0.0F),
                                QVector3D(-1.9F, 0.55F, 0.0F),
                                QVector3D(-2.0F, 1.0F, 0.2F),
                                QVector3D(-2.0F, 1.0F, -0.2F),
                                0.0F),
                     Render::Profiling::SoldierVisualState::Idle);
  submit_joint_frame(runner,
                     entity_id,
                     2,
                     4.02,
                     joint_pose(QVector3D(-1.5F, 0.55F, 0.0F),
                                QVector3D(-1.3F, 0.55F, 0.0F),
                                QVector3D(-2.0F, 1.0F, 0.2F),
                                QVector3D(-2.0F, 1.0F, -0.2F),
                                0.0F),
                     Render::Profiling::SoldierVisualState::Idle);

  EXPECT_TRUE(runner.report().passed());
  diagnostics.set_enabled(false);
}

TEST(ArenaScenarioRunnerTest, RenderProbeRejectsAWeaponHandThatTeleports) {
  Engine::Core::World world;
  auto scenario = minimal_definition();
  scenario.groups.resize(1);
  scenario.steps.clear();
  scenario.expectations = {
      {Arena::ArenaExpectationKind::NoWeaponTeleport, QStringLiteral("blue")}};
  Arena::ArenaScenarioRunner runner(world, make_entity_host(world), scenario);
  ASSERT_TRUE(runner.start());

  auto const entity_id = runner.group_entities(QStringLiteral("blue")).front();
  auto& diagnostics = Render::Profiling::CombatAnimationDiagnostics::instance();
  diagnostics.set_enabled(true);

  submit_joint_frame(runner,
                     entity_id,
                     1,
                     4.0,
                     joint_pose(QVector3D(-2.1F, 0.02F, 0.0F),
                                QVector3D(-1.9F, 0.02F, 0.0F),
                                QVector3D(-2.0F, 1.0F, 0.2F),
                                QVector3D(-2.0F, 1.0F, -0.2F),
                                0.0F),
                     Render::Profiling::SoldierVisualState::Attack);
  submit_joint_frame(runner,
                     entity_id,
                     2,
                     4.02,
                     joint_pose(QVector3D(-2.1F, 0.02F, 0.0F),
                                QVector3D(-1.9F, 0.02F, 0.0F),
                                QVector3D(-2.0F, 1.0F, 2.6F),
                                QVector3D(-2.0F, 1.0F, -0.2F),
                                0.0F),
                     Render::Profiling::SoldierVisualState::Attack);

  ASSERT_FALSE(runner.report().passed());
  EXPECT_EQ(runner.report().issues.front().code,
            QStringLiteral("weapon_hand_teleport"));
  diagnostics.set_enabled(false);
}

TEST(ArenaScenarioRunnerTest, RenderProbeRejectsAPelvisThatSnapsRound) {
  Engine::Core::World world;
  auto scenario = minimal_definition();
  scenario.groups.resize(1);
  scenario.steps.clear();
  scenario.expectations = {
      {Arena::ArenaExpectationKind::NoPelvisSnap, QStringLiteral("blue")}};
  Arena::ArenaScenarioRunner runner(world, make_entity_host(world), scenario);
  ASSERT_TRUE(runner.start());

  auto const entity_id = runner.group_entities(QStringLiteral("blue")).front();
  auto& diagnostics = Render::Profiling::CombatAnimationDiagnostics::instance();
  diagnostics.set_enabled(true);

  submit_joint_frame(runner,
                     entity_id,
                     1,
                     4.0,
                     joint_pose(QVector3D(-2.1F, 0.02F, 0.0F),
                                QVector3D(-1.9F, 0.02F, 0.0F),
                                QVector3D(-2.0F, 1.0F, 0.2F),
                                QVector3D(-2.0F, 1.0F, -0.2F),
                                0.0F),
                     Render::Profiling::SoldierVisualState::Idle);
  submit_joint_frame(runner,
                     entity_id,
                     2,
                     4.02,
                     joint_pose(QVector3D(-2.1F, 0.02F, 0.0F),
                                QVector3D(-1.9F, 0.02F, 0.0F),
                                QVector3D(-2.0F, 1.0F, 0.2F),
                                QVector3D(-2.0F, 1.0F, -0.2F),
                                140.0F),
                     Render::Profiling::SoldierVisualState::Idle);

  ASSERT_FALSE(runner.report().passed());
  EXPECT_EQ(runner.report().issues.front().code, QStringLiteral("pelvis_snap"));
  diagnostics.set_enabled(false);
}
