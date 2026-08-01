#include <QJsonObject>
#include <QVector3D>

#include <cmath>
#include <gtest/gtest.h>
#include <vector>

#include "core/component.h"
#include "core/entity.h"
#include "core/serialization.h"
#include "core/world.h"
#include "game/command/command.h"
#include "game/command/command_dispatcher.h"
#include "systems/combat_system/damage_processor.h"
#include "systems/command_service.h"
#include "systems/defense_formation_service.h"
#include "systems/movement_system.h"
#include "systems/nation_loader.h"
#include "systems/nation_registry.h"
#include "systems/owner_registry.h"
#include "units/spawn_type.h"

using namespace Engine::Core;
using namespace Game::Systems;

namespace {

class DefenseFormationTest : public ::testing::Test {
protected:
  void SetUp() override {
    world = std::make_unique<World>();
    OwnerRegistry::instance().clear();
    CommandService::initialize(64, 64);

    auto const nations = NationLoader::load_default_nations();
    ASSERT_FALSE(nations.empty());
    auto& registry = NationRegistry::instance();
    registry.clear();
    registry.clear_player_assignments();
    for (const auto& nation : nations) {
      registry.register_nation(nation);
    }
  }

  void TearDown() override { world.reset(); }

  auto spawn(NationID nation,
             Game::Units::SpawnType type,
             float x,
             float z,
             int owner_id = 1,
             int health = 400) -> Entity* {
    auto* entity = world->create_entity();
    entity->add_component<TransformComponent>(x, 0.0F, z);
    auto* unit = entity->add_component<UnitComponent>(health, health, 2.0F, 12.0F);
    unit->owner_id = owner_id;
    unit->spawn_type = type;
    unit->nation_id = nation;
    entity->add_component<MovementComponent>();
    auto* attack = entity->add_component<AttackComponent>(8.0F, 20, 1.0F);
    attack->current_mode = AttackComponent::CombatMode::Melee;
    return entity;
  }

  auto spawn_block(NationID nation, int count, float z = 0.0F) -> std::vector<Entity*> {
    std::vector<Entity*> units;
    units.reserve(static_cast<std::size_t>(count));
    for (int index = 0; index < count; ++index) {
      float const offset =
          (static_cast<float>(index) - (static_cast<float>(count) - 1.0F) * 0.5F) *
          2.0F;
      units.push_back(spawn(nation, Game::Units::SpawnType::Knight, offset, z));
    }
    return units;
  }

  static auto
  ids_of(const std::vector<Entity*>& units) -> std::vector<Engine::Core::EntityID> {
    std::vector<Engine::Core::EntityID> ids;
    ids.reserve(units.size());
    for (auto* unit : units) {
      ids.push_back(unit->get_id());
    }
    return ids;
  }

  void issue_guard(const std::vector<Entity*>& units,
                   bool active = true,
                   QVector3D anchor = QVector3D(0.0F, 0.0F, 12.0F)) {
    Game::Command::Command command{};
    command.source = Game::Command::Source::LocalPlayer;
    command.owner_id = 1;
    command.payload = Game::Command::SetGuard{.units = ids_of(units),
                                              .active = active,
                                              .anchor = anchor,
                                              .has_anchor = active};
    Game::Command::dispatch(*world, command);
  }

  void advance(float seconds, float step = 1.0F / 30.0F) {
    for (float elapsed = 0.0F; elapsed < seconds; elapsed += step) {
      movement.update(world.get(), step);
      formations.update(world.get(), step);
    }
  }

  void snap_to_slots(const std::vector<Entity*>& units) {
    for (auto* unit : units) {
      auto const* formation = unit->get_component<DefenseFormationComponent>();
      auto* transform = unit->get_component<TransformComponent>();
      if (formation != nullptr && transform != nullptr) {
        transform->position.x = formation->slot_x;
        transform->position.z = formation->slot_z;
      }
    }
  }

  std::unique_ptr<World> world;
  MovementSystem movement;
  DefenseFormationSystem formations;
};

TEST_F(DefenseFormationTest, FactionsCarryTheirOwnDefensiveFormation) {
  auto const& registry = NationRegistry::instance();

  auto const* roman = registry.get_nation(NationID::RomanRepublic);
  auto const* carthage = registry.get_nation(NationID::Carthage);
  auto const* sepulcher = registry.get_nation(NationID::IronSepulcher);

  ASSERT_NE(roman, nullptr);
  ASSERT_NE(carthage, nullptr);
  ASSERT_NE(sepulcher, nullptr);

  ASSERT_TRUE(roman->defense_formation.has_value());
  ASSERT_TRUE(carthage->defense_formation.has_value());
  EXPECT_FALSE(sepulcher->defense_formation.has_value());

  EXPECT_EQ(roman->defense_formation->id, "testudo");
  EXPECT_EQ(carthage->defense_formation->id, "shield_wall");

  EXPECT_LT(roman->defense_formation->frontal_missile_multiplier,
            carthage->defense_formation->frontal_missile_multiplier);
  EXPECT_GT(carthage->defense_formation->max_units_per_rank,
            roman->defense_formation->max_units_per_rank);
  EXPECT_GT(carthage->defense_formation->move_speed_multiplier,
            roman->defense_formation->move_speed_multiplier);
  EXPECT_GT(carthage->defense_formation->cavalry_impact_multiplier,
            roman->defense_formation->cavalry_impact_multiplier);
  EXPECT_FALSE(roman->defense_formation->allows_charge);
  EXPECT_FALSE(carthage->defense_formation->allows_charge);
}

TEST_F(DefenseFormationTest, GuardOrderWalksUnitsIntoSlotsAndForms) {
  auto units = spawn_block(NationID::RomanRepublic, 4);
  issue_guard(units);

  for (auto* unit : units) {
    auto const* formation = unit->get_component<DefenseFormationComponent>();
    ASSERT_NE(formation, nullptr);
    EXPECT_EQ(formation->state, DefenseFormationState::Forming);
  }

  advance(0.5F);
  snap_to_slots(units);
  advance(0.5F);

  for (auto* unit : units) {
    auto const* formation = unit->get_component<DefenseFormationComponent>();
    ASSERT_NE(formation, nullptr);
    EXPECT_EQ(formation->state, DefenseFormationState::Formed);
  }
}

TEST_F(DefenseFormationTest, FormationIsRefusedBelowTheMinimumUnitCount) {
  auto units = spawn_block(NationID::RomanRepublic, 2);
  EXPECT_FALSE(DefenseFormationService::can_form(*world, ids_of(units)));

  issue_guard(units);
  for (auto* unit : units) {
    EXPECT_EQ(unit->get_component<DefenseFormationComponent>(), nullptr);
  }
}

TEST_F(DefenseFormationTest, NationsWithoutAProfileNeverForm) {
  auto units = spawn_block(NationID::IronSepulcher, 5);
  EXPECT_FALSE(DefenseFormationService::can_form(*world, ids_of(units)));

  issue_guard(units);
  for (auto* unit : units) {
    EXPECT_EQ(unit->get_component<DefenseFormationComponent>(), nullptr);
  }
}

TEST_F(DefenseFormationTest, IneligibleTroopsAreLeftOutOfTheFormation) {
  auto units = spawn_block(NationID::RomanRepublic, 3);
  auto* archer =
      spawn(NationID::RomanRepublic, Game::Units::SpawnType::Archer, 6.0F, 0.0F);
  units.push_back(archer);

  issue_guard(units);

  EXPECT_EQ(archer->get_component<DefenseFormationComponent>(), nullptr);
  for (std::size_t index = 0; index + 1 < units.size(); ++index) {
    EXPECT_NE(units[index]->get_component<DefenseFormationComponent>(), nullptr);
  }
}

TEST_F(DefenseFormationTest, SlotsFaceTheAnchorAndFillRanks) {
  auto units = spawn_block(NationID::RomanRepublic, 5);
  issue_guard(units, true, QVector3D(0.0F, 0.0F, 20.0F));

  int max_rank = 0;
  for (auto* unit : units) {
    auto const* formation = unit->get_component<DefenseFormationComponent>();
    ASSERT_NE(formation, nullptr);
    EXPECT_NEAR(formation->facing_degrees, 0.0F, 1.0F);
    max_rank = std::max(max_rank, formation->rank);
  }

  auto const* roman = NationRegistry::instance().get_nation(NationID::RomanRepublic);
  ASSERT_NE(roman, nullptr);
  EXPECT_EQ(max_rank, (5 - 1) / roman->defense_formation->max_units_per_rank);
}

TEST_F(DefenseFormationTest, RotatingTheAnchorRotatesEveryFacing) {
  auto units = spawn_block(NationID::RomanRepublic, 4);
  issue_guard(units, true, QVector3D(20.0F, 0.0F, 0.0F));

  for (auto* unit : units) {
    auto const* formation = unit->get_component<DefenseFormationComponent>();
    ASSERT_NE(formation, nullptr);
    EXPECT_NEAR(formation->facing_degrees, 90.0F, 1.0F);
  }
}

TEST_F(DefenseFormationTest, FrontalMissilesAreHeavilyMitigated) {
  auto units = spawn_block(NationID::RomanRepublic, 4);
  issue_guard(units);
  snap_to_slots(units);
  advance(0.5F);

  auto* target = units.front();
  ASSERT_TRUE(target->get_component<DefenseFormationComponent>()->is_formed());

  auto* transform = target->get_component<TransformComponent>();
  QVector3D const front(transform->position.x, 0.0F, transform->position.z + 10.0F);
  QVector3D const rear(transform->position.x, 0.0F, transform->position.z - 10.0F);

  float const front_missile = DefenseFormationService::damage_multiplier(
      *target,
      {.is_missile = true, .is_cavalry_impact = false, .attack_origin = front});
  float const front_melee = DefenseFormationService::damage_multiplier(
      *target,
      {.is_missile = false, .is_cavalry_impact = false, .attack_origin = front});
  float const rear_melee = DefenseFormationService::damage_multiplier(
      *target,
      {.is_missile = false, .is_cavalry_impact = false, .attack_origin = rear});

  EXPECT_LT(front_missile, 0.3F);
  EXPECT_LT(front_missile, front_melee);
  EXPECT_LT(front_melee, 1.0F);
  EXPECT_GT(rear_melee, 1.0F);
}

TEST_F(DefenseFormationTest, FlankAndRearHitsBypassTheShieldFace) {
  auto units = spawn_block(NationID::RomanRepublic, 4);
  issue_guard(units);
  snap_to_slots(units);
  advance(0.5F);

  auto* target = units.front();
  auto* transform = target->get_component<TransformComponent>();
  QVector3D const flank(transform->position.x + 10.0F, 0.0F, transform->position.z);
  QVector3D const rear(transform->position.x, 0.0F, transform->position.z - 10.0F);

  float const flank_hit = DefenseFormationService::damage_multiplier(
      *target,
      {.is_missile = false, .is_cavalry_impact = false, .attack_origin = flank});
  float const rear_hit = DefenseFormationService::damage_multiplier(
      *target,
      {.is_missile = false, .is_cavalry_impact = false, .attack_origin = rear});

  EXPECT_GT(flank_hit, 1.0F);
  EXPECT_GT(rear_hit, flank_hit);
}

TEST_F(DefenseFormationTest, CarthageTradesMissileCoverForCavalryVulnerability) {
  auto roman = spawn_block(NationID::RomanRepublic, 4, 0.0F);
  auto punic = spawn_block(NationID::Carthage, 4, 20.0F);
  issue_guard(roman);
  issue_guard(punic);
  snap_to_slots(roman);
  snap_to_slots(punic);
  advance(0.5F);

  auto multiplier = [](Entity* unit, bool missile, bool cavalry) {
    auto* transform = unit->get_component<TransformComponent>();
    QVector3D const front(transform->position.x, 0.0F, transform->position.z + 10.0F);
    return DefenseFormationService::damage_multiplier(
        *unit,
        {.is_missile = missile, .is_cavalry_impact = cavalry, .attack_origin = front});
  };

  EXPECT_LT(multiplier(roman.front(), true, false),
            multiplier(punic.front(), true, false));
  EXPECT_LT(multiplier(roman.front(), false, true),
            multiplier(punic.front(), false, true));
}

TEST_F(DefenseFormationTest, FormedUnitsMoveAndTurnSlowlyAndCannotCharge) {
  auto units = spawn_block(NationID::RomanRepublic, 4);
  issue_guard(units);
  snap_to_slots(units);
  advance(0.5F);

  auto* unit = units.front();
  EXPECT_LT(DefenseFormationService::move_speed_multiplier(*unit), 0.5F);
  EXPECT_LT(DefenseFormationService::turn_speed_multiplier(*unit), 0.5F);
  EXPECT_TRUE(DefenseFormationService::blocks_charge(*unit));
}

TEST_F(DefenseFormationTest, FormedUnitsDealReducedDamage) {
  auto units = spawn_block(NationID::RomanRepublic, 4);
  auto* victim =
      spawn(NationID::Carthage, Game::Units::SpawnType::Knight, 30.0F, 0.0F, 2);
  issue_guard(units);
  snap_to_slots(units);
  advance(0.5F);

  auto* attacker = units.front();
  ASSERT_TRUE(attacker->get_component<DefenseFormationComponent>()->is_formed());

  int const before = victim->get_component<UnitComponent>()->health;
  Game::Systems::Combat::deal_damage(world.get(), victim, 100, attacker->get_id());
  int const formed_loss = before - victim->get_component<UnitComponent>()->health;

  DefenseFormationService::clear(attacker);
  int const mid = victim->get_component<UnitComponent>()->health;
  Game::Systems::Combat::deal_damage(world.get(), victim, 100, attacker->get_id());
  int const normal_loss = mid - victim->get_component<UnitComponent>()->health;

  EXPECT_GT(normal_loss, formed_loss);
}

TEST_F(DefenseFormationTest, CasualtiesCollapseTheFormationBelowCohesion) {
  auto units = spawn_block(NationID::RomanRepublic, 4);
  issue_guard(units);
  snap_to_slots(units);
  advance(0.5F);
  ASSERT_TRUE(units.front()->get_component<DefenseFormationComponent>()->is_formed());

  for (std::size_t index = 1; index < units.size(); ++index) {
    units[index]->get_component<UnitComponent>()->health = 0;
  }

  advance(0.5F);

  auto const* survivor = units.front()->get_component<DefenseFormationComponent>();
  EXPECT_TRUE(survivor == nullptr ||
              survivor->state == DefenseFormationState::Breaking);
}

TEST_F(DefenseFormationTest, ScatteredMembersLoseCohesionAndBreak) {
  auto units = spawn_block(NationID::RomanRepublic, 4);
  issue_guard(units);
  snap_to_slots(units);
  advance(0.5F);
  ASSERT_TRUE(units.front()->get_component<DefenseFormationComponent>()->is_formed());

  for (std::size_t index = 1; index < units.size(); ++index) {
    auto* transform = units[index]->get_component<TransformComponent>();
    transform->position.x += 40.0F;
  }

  advance(0.5F);

  auto const* anchor_unit = units.front()->get_component<DefenseFormationComponent>();
  ASSERT_NE(anchor_unit, nullptr);
  EXPECT_EQ(anchor_unit->state, DefenseFormationState::Breaking);
  EXPECT_LT(anchor_unit->cohesion, 1.0F);
}

TEST_F(DefenseFormationTest, MoveOrderBreaksTheFormationAndReleasesTheUnits) {
  auto units = spawn_block(NationID::RomanRepublic, 4);
  issue_guard(units);
  snap_to_slots(units);
  advance(0.5F);

  CommandService::move_unit(
      *world, units.front()->get_id(), QVector3D(20.0F, 0.0F, 20.0F));

  auto const* formation = units.front()->get_component<DefenseFormationComponent>();
  ASSERT_NE(formation, nullptr);
  EXPECT_EQ(formation->state, DefenseFormationState::Breaking);

  advance(2.5F);
  EXPECT_EQ(units.front()->get_component<DefenseFormationComponent>(), nullptr);
}

TEST_F(DefenseFormationTest, GuardOffBreaksTheFormation) {
  auto units = spawn_block(NationID::RomanRepublic, 4);
  issue_guard(units);
  snap_to_slots(units);
  advance(0.5F);

  issue_guard(units, false);

  for (auto* unit : units) {
    auto const* formation = unit->get_component<DefenseFormationComponent>();
    ASSERT_NE(formation, nullptr);
    EXPECT_EQ(formation->state, DefenseFormationState::Breaking);
  }

  advance(2.5F);
  for (auto* unit : units) {
    EXPECT_EQ(unit->get_component<DefenseFormationComponent>(), nullptr);
  }
}

TEST_F(DefenseFormationTest, RepeatedTogglingReformsCleanly) {
  auto units = spawn_block(NationID::RomanRepublic, 4);

  for (int cycle = 0; cycle < 3; ++cycle) {
    issue_guard(units);
    snap_to_slots(units);
    advance(0.5F);
    for (auto* unit : units) {
      auto const* formation = unit->get_component<DefenseFormationComponent>();
      ASSERT_NE(formation, nullptr) << "cycle " << cycle;
      EXPECT_TRUE(formation->is_formed()) << "cycle " << cycle;
    }

    issue_guard(units, false);
    advance(2.5F);
    for (auto* unit : units) {
      EXPECT_EQ(unit->get_component<DefenseFormationComponent>(), nullptr)
          << "cycle " << cycle;
    }
  }
}

TEST_F(DefenseFormationTest, FormationStateSurvivesSaveAndLoad) {
  auto units = spawn_block(NationID::RomanRepublic, 4);
  issue_guard(units);
  snap_to_slots(units);
  advance(0.5F);

  auto* source = units.front();
  auto const* saved = source->get_component<DefenseFormationComponent>();
  ASSERT_NE(saved, nullptr);
  ASSERT_TRUE(saved->is_formed());

  QJsonObject const json = Serialization::serialize_entity(source);

  World restored_world;
  auto* restored = restored_world.create_entity();
  Serialization::deserialize_entity(restored, json);

  auto const* loaded = restored->get_component<DefenseFormationComponent>();
  ASSERT_NE(loaded, nullptr);
  EXPECT_EQ(loaded->state, saved->state);
  EXPECT_EQ(loaded->formation_id, saved->formation_id);
  EXPECT_EQ(loaded->slot_index, saved->slot_index);
  EXPECT_EQ(loaded->rank, saved->rank);
  EXPECT_EQ(loaded->file, saved->file);
  EXPECT_FLOAT_EQ(loaded->slot_x, saved->slot_x);
  EXPECT_FLOAT_EQ(loaded->slot_z, saved->slot_z);
  EXPECT_FLOAT_EQ(loaded->facing_degrees, saved->facing_degrees);
}

TEST_F(DefenseFormationTest, BreakingStateSurvivesSaveAndLoad) {
  auto units = spawn_block(NationID::RomanRepublic, 4);
  issue_guard(units);
  snap_to_slots(units);
  advance(0.5F);
  issue_guard(units, false);

  auto* source = units.front();
  auto const* saved = source->get_component<DefenseFormationComponent>();
  ASSERT_NE(saved, nullptr);
  ASSERT_EQ(saved->state, DefenseFormationState::Breaking);

  QJsonObject const json = Serialization::serialize_entity(source);

  World restored_world;
  auto* restored = restored_world.create_entity();
  Serialization::deserialize_entity(restored, json);

  auto const* loaded = restored->get_component<DefenseFormationComponent>();
  ASSERT_NE(loaded, nullptr);
  EXPECT_EQ(loaded->state, DefenseFormationState::Breaking);
}

} // namespace
