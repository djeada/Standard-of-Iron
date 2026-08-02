#include <QVector3D>

#include <gtest/gtest.h>
#include <vector>

#include "core/component.h"
#include "core/world.h"
#include "formation/army_formation_planner.h"
#include "formation/army_formation_registry.h"
#include "formation/army_formation_service.h"
#include "formation/formation_doctrine.h"
#include "systems/combat_system/damage_application.h"
#include "systems/command_service.h"
#include "systems/nation_registry.h"
#include "systems/pathfinding.h"
#include "systems/troop_profile_service.h"

namespace {

using Game::Formation::ArmyFormationIntent;
using Game::Formation::ArmyFormationRegistry;
using Game::Formation::ArmyFormationRequest;
using Game::Formation::ArmyFormationRuntime;
using Game::Formation::ArmyFormationService;
using Game::Formation::FormationPhase;
using Game::Systems::NationID;

class FormationCohesionTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::CommandService::initialize(256, 256);
    auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
    if (pathfinder != nullptr) {
      pathfinder->update_navigation_grid();
    }
    auto& nations = Game::Systems::NationRegistry::instance();
    nations.clear();
    nations.register_nation({.id = NationID::RomanRepublic,
                             .display_name = "Roman Republic",
                             .doctrine = "rome"});
    Game::Systems::TroopProfileService::instance().clear();
    Game::Formation::DoctrineRegistry::instance().reset_to_defaults();
    ArmyFormationRegistry::instance().clear();
  }

  void TearDown() override { ArmyFormationRegistry::instance().clear(); }

  auto build_army(int count) -> std::vector<Engine::Core::EntityID> {
    std::vector<Engine::Core::EntityID> ids;
    ids.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
      auto* entity = m_world.create_entity();
      auto* transform = entity->add_component<Engine::Core::TransformComponent>();
      auto* unit = entity->add_component<Engine::Core::UnitComponent>();
      transform->position = {
          static_cast<float>(i % 8) + 40.0F, 0.0F, static_cast<float>(i / 8) + 40.0F};
      unit->spawn_type = Game::Units::SpawnType::Knight;
      unit->nation_id = NationID::RomanRepublic;
      unit->health = 100;
      ids.push_back(entity->get_id());
    }
    return ids;
  }

  auto commit_group(const std::vector<Engine::Core::EntityID>& ids)
      -> Game::Formation::ArmyFormation* {
    ArmyFormationRequest request;
    request.members = ids;
    request.anchor = QVector3D(120.0F, 0.0F, 120.0F);
    request.facing = 0.0F;
    request.intent = ArmyFormationIntent::Line;
    request.spacing = 1.0F;

    auto const result = ArmyFormationService::commit(m_world, request);
    EXPECT_TRUE(result.valid);
    return ArmyFormationRegistry::instance().find(result.group_id);
  }

  void stand_in_slots(const Game::Formation::ArmyFormation& formation) {
    for (const auto& slot : formation.slot_list) {
      if (slot.occupant == 0U) {
        continue;
      }
      auto* entity = m_world.get_entity(slot.occupant);
      ASSERT_NE(entity, nullptr);
      auto* transform = entity->get_component<Engine::Core::TransformComponent>();
      ASSERT_NE(transform, nullptr);
      transform->position.x = slot.world_position.x();
      transform->position.z = slot.world_position.z();
    }
  }

  Engine::Core::World m_world;
};

TEST_F(FormationCohesionTest, UnitsStandingInTheirSlotsReadAsFormed) {
  auto const ids = build_army(16);
  auto* formation = commit_group(ids);
  ASSERT_NE(formation, nullptr);

  stand_in_slots(*formation);
  ArmyFormationRuntime::refresh_cohesion(m_world, *formation);

  EXPECT_FLOAT_EQ(formation->cohesion, 1.0F);
  EXPECT_EQ(formation->phase, FormationPhase::Formed);
  EXPECT_TRUE(formation->is_formed());
}

TEST_F(FormationCohesionTest, AScatteredGroupReadsAsDisrupted) {
  auto const ids = build_army(16);
  auto* formation = commit_group(ids);
  ASSERT_NE(formation, nullptr);

  for (auto const id : ids) {
    auto* entity = m_world.get_entity(id);
    ASSERT_NE(entity, nullptr);
    auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    transform->position.x = 20.0F;
    transform->position.z = 20.0F;
  }
  ArmyFormationRuntime::refresh_cohesion(m_world, *formation);

  EXPECT_FLOAT_EQ(formation->cohesion, 0.0F);
  EXPECT_EQ(formation->phase, FormationPhase::Disrupted);
}

TEST_F(FormationCohesionTest, APartlyArrivedGroupIsStillFormingUp) {
  auto const ids = build_army(16);
  auto* formation = commit_group(ids);
  ASSERT_NE(formation, nullptr);

  stand_in_slots(*formation);
  int moved_away = 0;
  for (const auto& slot : formation->slot_list) {
    if (slot.occupant == 0U || moved_away >= 6) {
      continue;
    }
    auto* entity = m_world.get_entity(slot.occupant);
    auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    transform->position.x = 20.0F;
    transform->position.z = 20.0F;
    ++moved_away;
  }
  ArmyFormationRuntime::refresh_cohesion(m_world, *formation);

  EXPECT_GT(formation->cohesion, 0.45F);
  EXPECT_LT(formation->cohesion, 0.8F);
  EXPECT_EQ(formation->phase, FormationPhase::Forming);
}

TEST_F(FormationCohesionTest, PhaseDrivesTheDamageTakenMultiplier) {
  auto const ids = build_army(16);
  auto* formation = commit_group(ids);
  ASSERT_NE(formation, nullptr);
  stand_in_slots(*formation);
  ArmyFormationRuntime::refresh_cohesion(m_world, *formation);

  auto* member = m_world.get_entity(ids.front());
  ASSERT_NE(member, nullptr);

  float const formed = ArmyFormationRuntime::damage_taken_multiplier(*member);
  EXPECT_LT(formed, 1.0F) << "a formed line should shrug off part of a hit";

  formation->phase = FormationPhase::Disrupted;
  float const disrupted = ArmyFormationRuntime::damage_taken_multiplier(*member);
  EXPECT_GT(disrupted, 1.0F) << "a disrupted line should take extra damage";

  formation->phase = FormationPhase::Forming;
  EXPECT_FLOAT_EQ(ArmyFormationRuntime::damage_taken_multiplier(*member), 1.0F);
}

TEST_F(FormationCohesionTest, UnitsOutsideAnyFormationAreUnaffected) {
  auto const ids = build_army(4);
  auto* loner = m_world.get_entity(ids.front());
  ASSERT_NE(loner, nullptr);
  EXPECT_FLOAT_EQ(ArmyFormationRuntime::damage_taken_multiplier(*loner), 1.0F);
}

TEST_F(FormationCohesionTest, TheBonusScalesWithHowTightTheLineIs) {
  auto const ids = build_army(16);
  auto* formation = commit_group(ids);
  ASSERT_NE(formation, nullptr);
  stand_in_slots(*formation);

  auto* member = m_world.get_entity(ids.front());
  ASSERT_NE(member, nullptr);

  formation->phase = FormationPhase::Formed;
  formation->cohesion = 1.0F;
  float const tight = ArmyFormationRuntime::damage_taken_multiplier(*member);

  formation->cohesion = 0.8F;
  float const loose = ArmyFormationRuntime::damage_taken_multiplier(*member);

  EXPECT_LT(tight, loose) << "a fully closed line should beat a barely formed one";
  EXPECT_FLOAT_EQ(loose, 1.0F);
}

TEST_F(FormationCohesionTest, TheRuntimeTickMeasuresCohesionWithoutBeingAsked) {
  auto const ids = build_army(16);
  auto* formation = commit_group(ids);
  ASSERT_NE(formation, nullptr);
  stand_in_slots(*formation);

  auto const group_id = formation->id;
  ArmyFormationRegistry::instance().find(group_id)->cohesion = 0.0F;
  ArmyFormationRegistry::instance().find(group_id)->phase = FormationPhase::Disrupted;

  ArmyFormationRuntime runtime;
  runtime.update(&m_world, 1.0F);

  const auto* measured = ArmyFormationRegistry::instance().find(group_id);
  ASSERT_NE(measured, nullptr);
  EXPECT_FLOAT_EQ(measured->cohesion, 1.0F);
  EXPECT_EQ(measured->phase, FormationPhase::Formed);
}

TEST_F(FormationCohesionTest, FormingUpActuallyChangesDamageTakenInCombat) {
  auto const ids = build_army(16);
  auto* formation = commit_group(ids);
  ASSERT_NE(formation, nullptr);
  stand_in_slots(*formation);
  ArmyFormationRuntime::refresh_cohesion(m_world, *formation);
  ASSERT_EQ(formation->phase, FormationPhase::Formed);

  constexpr int k_raw_damage = 40;

  auto* formed_target = m_world.get_entity(ids.front());
  ASSERT_NE(formed_target, nullptr);
  auto const formed_before =
      formed_target->get_component<Engine::Core::UnitComponent>()->health;
  Game::Systems::Combat::apply_unit_damage(&m_world, formed_target, k_raw_damage, 0U);
  auto const formed_lost =
      formed_before -
      formed_target->get_component<Engine::Core::UnitComponent>()->health;

  formation->phase = FormationPhase::Disrupted;
  auto* disrupted_target = m_world.get_entity(ids.back());
  ASSERT_NE(disrupted_target, nullptr);
  auto const disrupted_before =
      disrupted_target->get_component<Engine::Core::UnitComponent>()->health;
  Game::Systems::Combat::apply_unit_damage(
      &m_world, disrupted_target, k_raw_damage, 0U);
  auto const disrupted_lost =
      disrupted_before -
      disrupted_target->get_component<Engine::Core::UnitComponent>()->health;

  EXPECT_LT(formed_lost, k_raw_damage);
  EXPECT_GT(disrupted_lost, k_raw_damage);
  EXPECT_LT(formed_lost, disrupted_lost);
}

TEST_F(FormationCohesionTest, CohesionSurvivesASaveAndLoad) {
  auto const ids = build_army(16);
  auto* formation = commit_group(ids);
  ASSERT_NE(formation, nullptr);
  stand_in_slots(*formation);
  ArmyFormationRuntime::refresh_cohesion(m_world, *formation);

  auto const group_id = formation->id;
  auto const saved_cohesion = formation->cohesion;
  auto const saved_phase = formation->phase;

  auto& registry = ArmyFormationRegistry::instance();
  auto const json = registry.to_json();
  registry.clear();
  registry.from_json(json);

  const auto* restored = registry.find(group_id);
  ASSERT_NE(restored, nullptr);
  EXPECT_FLOAT_EQ(restored->cohesion, saved_cohesion);
  EXPECT_EQ(restored->phase, saved_phase);
}

} // namespace
