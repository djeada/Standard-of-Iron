#include <gtest/gtest.h>

#include "core/component_gameplay.h"
#include "core/world.h"
#include "systems/combat_system/combat_hit_resolver.h"
#include "systems/combat_system/combat_status_effect_processor.h"
#include "systems/combat_system/damage_application.h"
#include "systems/combat_system/structure_fire.h"
#include "systems/projectile_kind.h"
#include "units/spawn_type.h"

namespace {

using namespace Engine::Core;
using Game::Systems::ProjectileKind;
namespace Combat = Game::Systems::Combat;

auto add_attacker(World& world,
                  Game::Units::SpawnType type,
                  float x = 0.0F,
                  float z = -6.0F) -> Entity* {
  auto* entity = world.create_entity();
  entity->add_component<TransformComponent>(x, 0.0F, z);
  auto* unit = entity->add_component<UnitComponent>(200, 200, 1.0F, 18.0F);
  unit->owner_id = 1;
  unit->spawn_type = type;
  entity->add_component<AttackComponent>();
  return entity;
}

auto add_structure(World& world, int health = 1000) -> Entity* {
  auto* entity = world.create_entity();
  entity->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* unit = entity->add_component<UnitComponent>(health, health, 0.0F, 12.0F);
  unit->owner_id = 2;
  unit->spawn_type = Game::Units::SpawnType::Barracks;
  entity->add_component<BuildingComponent>();
  return entity;
}

void hit_structure(World& world,
                   Entity& attacker,
                   Entity& structure,
                   ProjectileKind kind,
                   int damage) {
  (void)Combat::resolve_projectile_impact_hit(
      &world,
      {.contact = {.attacker_id = attacker.get_id(),
                   .target_id = structure.get_id(),
                   .from_projectile = true,
                   .projectile_kind = kind},
       .explicit_raw_damage = damage});
}

class StructureFireTest : public ::testing::Test {
protected:
  World world;
};

TEST_F(StructureFireTest, MeleeDamageNeverIgnitesAStructure) {
  auto* attacker = add_attacker(world, Game::Units::SpawnType::Knight);
  auto* structure = add_structure(world);

  for (int blow = 0; blow < 40; ++blow) {
    Combat::apply_unit_damage(&world, structure, 60, attacker->get_id());
  }

  EXPECT_FALSE(Combat::structure_is_burning(*structure));
  EXPECT_EQ(structure->get_component<StructureFireComponent>(), nullptr);
}

TEST_F(StructureFireTest, PlainSiegeStonesDamageWithoutFire) {
  auto* catapult = add_attacker(world, Game::Units::SpawnType::Catapult);
  auto* structure = add_structure(world);

  hit_structure(world, *catapult, *structure, ProjectileKind::Stone, 150);

  EXPECT_LT(structure->get_component<UnitComponent>()->health, 1000);
  EXPECT_FALSE(Combat::structure_is_burning(*structure));
  EXPECT_EQ(structure->get_component<StructureFireComponent>(), nullptr);
}

TEST_F(StructureFireTest, FlamingStoneIgnitesTheStructureItHits) {
  auto* catapult = add_attacker(world, Game::Units::SpawnType::Catapult);
  auto* structure = add_structure(world);

  hit_structure(world, *catapult, *structure, ProjectileKind::FlamingStone, 150);

  ASSERT_TRUE(Combat::structure_is_burning(*structure));
  (void)Combat::process_combat_status_effects(&world, 0.7F);
  EXPECT_GT(Combat::structure_fire_intensity(*structure), 0.0F);
}

TEST_F(StructureFireTest, IncendiaryChipDamageBelowThresholdDoesNotIgnite) {
  auto* priest = add_attacker(world, Game::Units::SpawnType::GravePriest);
  auto* structure = add_structure(world, 4000);

  hit_structure(world, *priest, *structure, ProjectileKind::Fireball, 20);

  auto const* fire = structure->get_component<StructureFireComponent>();
  ASSERT_NE(fire, nullptr);
  EXPECT_FALSE(fire->is_burning());
  EXPECT_FALSE(Combat::structure_is_burning(*structure));
}

TEST_F(StructureFireTest, SustainedIncendiaryDamageEventuallyIgnites) {
  auto* priest = add_attacker(world, Game::Units::SpawnType::GravePriest);
  auto* structure = add_structure(world, 1000);

  for (int hit = 0; hit < 8 && !Combat::structure_is_burning(*structure); ++hit) {
    hit_structure(world, *priest, *structure, ProjectileKind::Fireball, 60);
  }

  EXPECT_TRUE(Combat::structure_is_burning(*structure));
}

TEST_F(StructureFireTest, IgnitionProgressDecaysWhenTheFireNeverCatches) {
  auto* priest = add_attacker(world, Game::Units::SpawnType::GravePriest);
  auto* structure = add_structure(world, 4000);

  hit_structure(world, *priest, *structure, ProjectileKind::Fireball, 20);
  ASSERT_NE(structure->get_component<StructureFireComponent>(), nullptr);

  for (int step = 0; step < 40; ++step) {
    (void)Combat::process_combat_status_effects(&world, 0.5F);
  }

  EXPECT_EQ(structure->get_component<StructureFireComponent>(), nullptr);
}

TEST_F(StructureFireTest, FireBurnsOutAfterItsDuration) {
  auto* catapult = add_attacker(world, Game::Units::SpawnType::Catapult);
  auto* structure = add_structure(world, 100000);

  hit_structure(world, *catapult, *structure, ProjectileKind::FlamingStone, 20000);
  ASSERT_TRUE(Combat::structure_is_burning(*structure));

  for (int step = 0; step < 40 && Combat::structure_is_burning(*structure); ++step) {
    (void)Combat::process_combat_status_effects(&world, 0.5F);
  }

  EXPECT_FALSE(Combat::structure_is_burning(*structure));
  EXPECT_EQ(Combat::structure_fire_intensity(*structure), 0.0F);
}

TEST_F(StructureFireTest, FireStopsWhenTheStructureCollapses) {
  auto* catapult = add_attacker(world, Game::Units::SpawnType::Catapult);
  auto* structure = add_structure(world);

  hit_structure(world, *catapult, *structure, ProjectileKind::FlamingStone, 150);
  ASSERT_TRUE(Combat::structure_is_burning(*structure));

  structure->get_component<UnitComponent>()->health = 0;
  (void)Combat::process_combat_status_effects(&world, 0.1F);

  EXPECT_FALSE(Combat::structure_is_burning(*structure));
  EXPECT_EQ(structure->get_component<StructureFireComponent>(), nullptr);
  EXPECT_EQ(Combat::structure_fire_intensity(*structure), 0.0F);
}

TEST_F(StructureFireTest, FireStopsWhenTheStructureIsRemovedByScript) {
  auto* catapult = add_attacker(world, Game::Units::SpawnType::Catapult);
  auto* structure = add_structure(world);

  hit_structure(world, *catapult, *structure, ProjectileKind::FlamingStone, 150);
  ASSERT_TRUE(Combat::structure_is_burning(*structure));

  structure->add_component<PendingRemovalComponent>();
  (void)Combat::process_combat_status_effects(&world, 0.1F);

  EXPECT_EQ(structure->get_component<StructureFireComponent>(), nullptr);
}

TEST_F(StructureFireTest, BurningStructureTakesFireDamageOverTime) {
  auto* catapult = add_attacker(world, Game::Units::SpawnType::Catapult);
  auto* structure = add_structure(world);

  hit_structure(world, *catapult, *structure, ProjectileKind::FlamingStone, 150);
  int const health_after_impact = structure->get_component<UnitComponent>()->health;

  (void)Combat::process_combat_status_effects(&world, 1.6F);

  EXPECT_LT(structure->get_component<UnitComponent>()->health, health_after_impact);
}

TEST_F(StructureFireTest, StructuresDoNotGainSoldierBurningStatus) {
  auto* priest = add_attacker(world, Game::Units::SpawnType::GravePriest);
  auto* special = priest->add_component<SpecialAttackComponent>();
  special->projectile_kind = ProjectileKind::Fireball;
  special->burn_duration = 1.5F;
  special->burn_tick_interval = 0.5F;
  special->burn_damage_per_tick = 2;
  auto* structure = add_structure(world);

  hit_structure(world, *priest, *structure, ProjectileKind::Fireball, 150);

  EXPECT_EQ(structure->get_component<BurningStatusComponent>(), nullptr);
}

} // namespace
