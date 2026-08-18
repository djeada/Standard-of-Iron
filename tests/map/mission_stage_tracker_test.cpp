#include <gtest/gtest.h>
#include <memory>

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/mission_stage_tracker.h"
#include "game/session/session_context.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"

namespace {

constexpr int k_local_owner = 1;
constexpr int k_enemy_owner = 2;

class MissionStageTrackerTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_session = std::make_unique<Game::Session::SessionContext>();
    auto& owners = m_session->owners();
    owners.register_owner_with_id(
        k_local_owner, Game::Systems::OwnerType::Player, "Player");
    owners.register_owner_with_id(k_enemy_owner, Game::Systems::OwnerType::AI, "Rome");
    owners.set_owner_team(k_local_owner, 0);
    owners.set_owner_team(k_enemy_owner, 1);
  }

  void TearDown() override { m_session.reset(); }

  [[nodiscard]] auto session() -> Game::Session::SessionContext& { return *m_session; }
  [[nodiscard]] auto world() -> Engine::Core::World& { return m_session->world(); }

  std::unique_ptr<Game::Session::SessionContext> m_session;

  static auto spawn_unit(Engine::Core::World& world,
                         int owner_id,
                         Game::Units::SpawnType type,
                         float x,
                         float z) -> Engine::Core::Entity* {
    auto* entity = world.create_entity();
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    unit->owner_id = owner_id;
    unit->spawn_type = type;
    unit->health = 100;
    entity->add_component<Engine::Core::TransformComponent>(x, 0.0F, z);
    return entity;
  }

  static auto identity_to_world() -> Game::Mission::MissionPositionToWorld {
    return [](const Game::Mission::Position& position) {
      return QVector3D(position.x, 0.0F, position.z);
    };
  }
};

TEST_F(MissionStageTrackerTest, ActiveStageIsTheFirstIncompleteOne) {
  Game::Mission::MissionDefinition mission;

  Game::Mission::MissionStage reach;
  reach.id = QStringLiteral("reach");
  reach.type = QStringLiteral("reach_position");
  reach.target = Game::Mission::Position{100.0F, 100.0F};
  reach.target_radius = 5.0F;

  Game::Mission::MissionStage capture;
  capture.id = QStringLiteral("capture");
  capture.type = QStringLiteral("capture_structures");
  capture.structure_types = {QStringLiteral("barracks")};
  capture.required_count = 2;

  mission.stages = {reach, capture};

  auto& world = this->world();
  spawn_unit(world, k_local_owner, Game::Units::SpawnType::Spearman, 0.0F, 0.0F);

  Game::Mission::MissionStageTracker tracker;
  tracker.configure(mission, k_local_owner, identity_to_world());
  tracker.update(session(), {});

  ASSERT_EQ(tracker.stages().size(), 2U);
  EXPECT_EQ(tracker.active_index(), 0);
  EXPECT_TRUE(tracker.stages()[0].active);
  EXPECT_FALSE(tracker.stages()[0].complete);

  const auto target = tracker.active_target();
  ASSERT_TRUE(target.has_value());
  EXPECT_FLOAT_EQ(target->x(), 100.0F);
  EXPECT_FLOAT_EQ(target->z(), 100.0F);
}

TEST_F(MissionStageTrackerTest, ReachingTheTargetAdvancesToTheNextStage) {
  Game::Mission::MissionDefinition mission;

  Game::Mission::MissionStage reach;
  reach.id = QStringLiteral("reach");
  reach.type = QStringLiteral("reach_position");
  reach.target = Game::Mission::Position{100.0F, 100.0F};
  reach.target_radius = 5.0F;

  Game::Mission::MissionStage capture;
  capture.id = QStringLiteral("capture");
  capture.type = QStringLiteral("capture_structures");
  capture.structure_types = {QStringLiteral("barracks")};
  capture.required_count = 1;

  mission.stages = {reach, capture};

  auto& world = this->world();
  auto* scout =
      spawn_unit(world, k_local_owner, Game::Units::SpawnType::Spearman, 0.0F, 0.0F);

  Game::Mission::MissionStageTracker tracker;
  tracker.configure(mission, k_local_owner, identity_to_world());
  tracker.update(session(), {});
  ASSERT_EQ(tracker.active_index(), 0);

  auto* transform = scout->get_component<Engine::Core::TransformComponent>();
  transform->position.x = 102.0F;
  transform->position.z = 99.0F;

  EXPECT_TRUE(tracker.update(session(), {}));
  EXPECT_TRUE(tracker.stages()[0].complete);
  EXPECT_EQ(tracker.active_index(), 1);
}

TEST_F(MissionStageTrackerTest, CaptureProgressCountsStructuresTakenFromTheEnemy) {
  Game::Mission::MissionDefinition mission;

  Game::Mission::MissionStage capture;
  capture.id = QStringLiteral("capture");
  capture.type = QStringLiteral("capture_structures");
  capture.structure_types = {QStringLiteral("barracks")};
  capture.required_count = 2;
  mission.stages = {capture};

  auto& world = this->world();

  auto& nations = session().nations();
  const Game::Systems::NationID local_nation = nations.default_nation_id();

  auto* home =
      spawn_unit(world, k_local_owner, Game::Units::SpawnType::Barracks, 10.0F, 10.0F);
  auto* home_building = home->add_component<Engine::Core::BuildingComponent>();
  home_building->original_nation_id = local_nation;

  auto* taken =
      spawn_unit(world, k_local_owner, Game::Units::SpawnType::Barracks, 40.0F, 40.0F);
  auto* taken_building = taken->add_component<Engine::Core::BuildingComponent>();
  taken_building->original_nation_id = local_nation == Game::Systems::NationID::Carthage
                                           ? Game::Systems::NationID::RomanRepublic
                                           : Game::Systems::NationID::Carthage;

  Game::Mission::MissionStageTracker tracker;
  tracker.configure(mission, k_local_owner, identity_to_world());
  tracker.update(session(), {});

  ASSERT_EQ(tracker.stages().size(), 1U);
  EXPECT_EQ(tracker.stages()[0].progress, 1);
  EXPECT_EQ(tracker.stages()[0].required, 2);
  EXPECT_FALSE(tracker.stages()[0].complete);
}

TEST_F(MissionStageTrackerTest, EliminateCommandersSizesItselfFromTheOpposition) {
  Game::Mission::MissionDefinition mission;

  Game::Mission::MissionStage kill;
  kill.id = QStringLiteral("kill");
  kill.type = QStringLiteral("eliminate_commanders");
  mission.stages = {kill};

  auto& world = this->world();
  auto* first = spawn_unit(
      world, k_enemy_owner, Game::Units::SpawnType::RomanFieldCommander, 5.0F, 5.0F);
  first->add_component<Engine::Core::CommanderComponent>();
  auto* second = spawn_unit(
      world, k_enemy_owner, Game::Units::SpawnType::RomanFieldCommander, 8.0F, 5.0F);
  second->add_component<Engine::Core::CommanderComponent>();

  Game::Mission::MissionStageTracker tracker;
  tracker.configure(mission, k_local_owner, identity_to_world());
  tracker.update(session(), {});

  EXPECT_EQ(tracker.stages()[0].required, 2);
  EXPECT_EQ(tracker.stages()[0].progress, 0);

  world.destroy_entity(second->get_id());
  world.update(0.0F);

  EXPECT_TRUE(tracker.update(session(), {}));
  EXPECT_EQ(tracker.stages()[0].progress, 1);
  EXPECT_FALSE(tracker.stages()[0].complete);
}

TEST_F(MissionStageTrackerTest, CompletedStagesDoNotReopen) {
  Game::Mission::MissionDefinition mission;

  Game::Mission::MissionStage reach;
  reach.id = QStringLiteral("reach");
  reach.type = QStringLiteral("reach_position");
  reach.target = Game::Mission::Position{50.0F, 50.0F};
  reach.target_radius = 4.0F;

  Game::Mission::MissionStage kill;
  kill.id = QStringLiteral("kill");
  kill.type = QStringLiteral("eliminate_commanders");

  mission.stages = {reach, kill};

  auto& world = this->world();
  auto* scout =
      spawn_unit(world, k_local_owner, Game::Units::SpawnType::Spearman, 50.0F, 50.0F);

  Game::Mission::MissionStageTracker tracker;
  tracker.configure(mission, k_local_owner, identity_to_world());
  tracker.update(session(), {});
  ASSERT_TRUE(tracker.stages()[0].complete);
  ASSERT_EQ(tracker.active_index(), 1);

  auto* transform = scout->get_component<Engine::Core::TransformComponent>();
  transform->position.x = 0.0F;
  transform->position.z = 0.0F;
  tracker.update(session(), {});

  EXPECT_TRUE(tracker.stages()[0].complete);
  EXPECT_EQ(tracker.active_index(), 1);
}

TEST_F(MissionStageTrackerTest, SurviveWavesReadsTheClearedPhaseCount) {
  Game::Mission::MissionDefinition mission;

  Game::Mission::MissionStage survive;
  survive.id = QStringLiteral("survive");
  survive.type = QStringLiteral("survive_waves");
  survive.wave_count = 3;
  mission.stages = {survive};

  Game::Mission::MissionStageTracker tracker;
  tracker.configure(mission, k_local_owner, identity_to_world());
  tracker.update(session(), {.elapsed_seconds = 0.0F, .cleared_wave_count = 2});

  EXPECT_EQ(tracker.stages()[0].progress, 2);
  EXPECT_EQ(tracker.stages()[0].required, 3);
  EXPECT_FALSE(tracker.stages()[0].complete);

  tracker.update(session(), {.elapsed_seconds = 0.0F, .cleared_wave_count = 3});
  EXPECT_TRUE(tracker.stages()[0].complete);
  EXPECT_EQ(tracker.active_index(), -1);
  EXPECT_FALSE(tracker.active_target().has_value());
}

TEST_F(MissionStageTrackerTest, RestoreKeepsFinishedStepsFinishedAfterALoad) {
  Game::Mission::MissionDefinition mission;

  Game::Mission::MissionStage reach;
  reach.id = QStringLiteral("reach");
  reach.type = QStringLiteral("reach_position");
  reach.target = Game::Mission::Position{50.0F, 50.0F};
  reach.target_radius = 4.0F;

  Game::Mission::MissionStage kill;
  kill.id = QStringLiteral("kill");
  kill.type = QStringLiteral("eliminate_commanders");

  mission.stages = {reach, kill};

  auto& world = this->world();
  auto* scout =
      spawn_unit(world, k_local_owner, Game::Units::SpawnType::Spearman, 50.0F, 50.0F);

  Game::Mission::MissionStageTracker tracker;
  tracker.configure(mission, k_local_owner, identity_to_world());
  tracker.update(session(), {});
  ASSERT_TRUE(tracker.stages()[0].complete);

  const QJsonObject state = tracker.serialize();

  scout->get_component<Engine::Core::TransformComponent>()->position.x = 0.0F;
  scout->get_component<Engine::Core::TransformComponent>()->position.z = 0.0F;

  Game::Mission::MissionStageTracker reloaded;
  reloaded.configure(mission, k_local_owner, identity_to_world());
  reloaded.update(session(), {});
  ASSERT_FALSE(reloaded.stages()[0].complete);

  reloaded.restore(state);
  EXPECT_TRUE(reloaded.stages()[0].complete);
  EXPECT_EQ(reloaded.active_index(), 1);

  reloaded.update(session(), {});
  EXPECT_TRUE(reloaded.stages()[0].complete);
  EXPECT_EQ(reloaded.active_index(), 1);
}

TEST_F(MissionStageTrackerTest, ReportsWhoHoldsTheSettlementAtACaptureTarget) {
  Game::Mission::MissionDefinition mission;

  Game::Mission::MissionStage capture;
  capture.id = QStringLiteral("take_village");
  capture.type = QStringLiteral("capture_structures");
  capture.structure_types = {QStringLiteral("barracks")};
  capture.required_count = 1;
  capture.target = Game::Mission::Position{200.0F, 200.0F};
  mission.stages = {capture};

  auto& world = this->world();
  auto* village = spawn_unit(
      world, k_enemy_owner, Game::Units::SpawnType::Barracks, 204.0F, 197.0F);
  village->add_component<Engine::Core::BuildingComponent>();

  Game::Mission::MissionStageTracker tracker;
  tracker.configure(mission, k_local_owner, identity_to_world());
  tracker.update(session(), {});

  ASSERT_EQ(tracker.stages().size(), 1U);
  EXPECT_TRUE(tracker.stages()[0].target_structure_present)
      << "a capture target standing on a village must report that village";
  EXPECT_FALSE(tracker.stages()[0].target_structure_is_local);

  village->get_component<Engine::Core::UnitComponent>()->owner_id = k_local_owner;
  EXPECT_TRUE(tracker.update(session(), {}));
  EXPECT_TRUE(tracker.stages()[0].target_structure_present);
  EXPECT_TRUE(tracker.stages()[0].target_structure_is_local)
      << "taking the village must show on the stage that pins it";
}

TEST_F(MissionStageTrackerTest, IgnoresStructuresFarFromTheCaptureTarget) {
  Game::Mission::MissionDefinition mission;

  Game::Mission::MissionStage capture;
  capture.id = QStringLiteral("take_village");
  capture.type = QStringLiteral("capture_structures");
  capture.structure_types = {QStringLiteral("barracks")};
  capture.target = Game::Mission::Position{200.0F, 200.0F};
  mission.stages = {capture};

  auto& world = this->world();
  auto* distant = spawn_unit(
      world, k_enemy_owner, Game::Units::SpawnType::Barracks, 400.0F, 400.0F);
  distant->add_component<Engine::Core::BuildingComponent>();

  Game::Mission::MissionStageTracker tracker;
  tracker.configure(mission, k_local_owner, identity_to_world());
  tracker.update(session(), {});

  EXPECT_FALSE(tracker.stages()[0].target_structure_present)
      << "a camp on the far side of the map is not the settlement this stage names";
}

TEST_F(MissionStageTrackerTest, StagesWithoutATargetReportNoSettlement) {
  Game::Mission::MissionDefinition mission;

  Game::Mission::MissionStage capture;
  capture.id = QStringLiteral("capture_any");
  capture.type = QStringLiteral("capture_structures");
  capture.structure_types = {QStringLiteral("barracks")};
  mission.stages = {capture};

  auto& world = this->world();
  auto* village =
      spawn_unit(world, k_enemy_owner, Game::Units::SpawnType::Barracks, 5.0F, 5.0F);
  village->add_component<Engine::Core::BuildingComponent>();

  Game::Mission::MissionStageTracker tracker;
  tracker.configure(mission, k_local_owner, identity_to_world());
  tracker.update(session(), {});

  EXPECT_FALSE(tracker.stages()[0].target_structure_present);
}

} // namespace
