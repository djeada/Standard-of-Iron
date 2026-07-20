#include <QFile>
#include <QSet>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include "game/core/component.h"
#include "game/core/world.h"
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
    unit->health = group.health_override > 0 ? group.health_override : 100;
    unit->max_health = group.max_health_override > 0 ? group.max_health_override : 100;
    entity->add_component<Engine::Core::MovementComponent>();
    entity->add_component<Engine::Core::AttackComponent>();
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

} // namespace

TEST(ArenaScenarioDefinitionTest, EntireLocalCatalogIsValidAndUniquelyAddressable) {
  QSet<QString> ids;
  ASSERT_GE(Arena::Scenarios::definitions().size(), 20U);
  for (auto const& scenario : Arena::Scenarios::definitions()) {
    EXPECT_TRUE(Arena::validate_scenario(scenario).empty())
        << scenario.id.toStdString();
    EXPECT_FALSE(ids.contains(scenario.id)) << scenario.id.toStdString();
    ids.insert(scenario.id);
    EXPECT_EQ(Arena::Scenarios::find_definition(scenario.id), &scenario);
  }
}

TEST(ArenaScenarioDefinitionTest, CatalogIncludesRealBattlefieldRegressionCases) {
  for (auto const* id : {Arena::Scenarios::k_three_swords_vs_two_spears_id,
                         Arena::Scenarios::k_spear_walk_contact_id,
                         Arena::Scenarios::k_archer_stability_id,
                         Arena::Scenarios::k_flank_ambush_id,
                         Arena::Scenarios::k_reserve_release_id,
                         Arena::Scenarios::k_bot_skirmish_id}) {
    auto const* scenario = Arena::Scenarios::find_definition(QString::fromLatin1(id));
    ASSERT_NE(scenario, nullptr) << id;
    EXPECT_FALSE(scenario->steps.empty());
    EXPECT_FALSE(scenario->expectations.empty());
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
