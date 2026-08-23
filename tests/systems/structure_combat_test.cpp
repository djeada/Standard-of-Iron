#include <cmath>
#include <gtest/gtest.h>
#include <vector>

#include "core/component.h"
#include "core/event_manager.h"
#include "core/world.h"
#include "systems/building_collision_registry.h"
#include "systems/combat_system/damage_application.h"
#include "systems/combat_system/structure_combat.h"
#include "systems/nav_grid.h"
#include "systems/pathfinding.h"
#include "units/spawn_type.h"

namespace {

auto add_attacker(Engine::Core::World& world,
                  Game::Units::SpawnType type,
                  float x = 0.0F,
                  float z = 0.0F,
                  int individuals = 1) -> Engine::Core::Entity* {
  auto* entity = world.create_entity();
  auto* transform = entity->add_component<Engine::Core::TransformComponent>(x, 0.0F, z);
  transform->scale = {0.55F, 0.55F, 0.55F};
  auto* unit =
      entity->add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  unit->owner_id = 1;
  unit->spawn_type = type;
  unit->render_individuals_per_unit_override = individuals;
  auto* attack = entity->add_component<Engine::Core::AttackComponent>();
  attack->can_melee = true;
  attack->current_mode = Engine::Core::AttackComponent::CombatMode::Melee;
  return entity;
}

auto add_structure(Engine::Core::World& world,
                   Game::Units::SpawnType type,
                   float x,
                   float z,
                   float yaw = 0.0F) -> Engine::Core::Entity* {
  auto* entity = world.create_entity();
  auto* transform = entity->add_component<Engine::Core::TransformComponent>(x, 0.0F, z);
  transform->rotation.y = yaw;
  auto* unit =
      entity->add_component<Engine::Core::UnitComponent>(1000, 1000, 0.0F, 12.0F);
  unit->owner_id = 2;
  unit->spawn_type = type;
  entity->add_component<Engine::Core::BuildingComponent>();
  Game::Systems::BuildingCollisionRegistry::instance().register_building(
      entity->get_id(), Game::Units::spawn_typeToString(type), x, z, 2);
  return entity;
}

class StructureCombatTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
  }

  void TearDown() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
  }
};

} // namespace

TEST_F(StructureCombatTest, AttackClassesHaveExplicitStructureEffectiveness) {
  Engine::Core::World world;

  EXPECT_EQ(Game::Systems::Combat::resolve_structure_damage(
                add_attacker(world, Game::Units::SpawnType::Archer), 100),
            0);
  EXPECT_EQ(Game::Systems::Combat::resolve_structure_damage(
                add_attacker(world, Game::Units::SpawnType::SkeletonArcher), 100),
            0);
  EXPECT_EQ(Game::Systems::Combat::resolve_structure_damage(
                add_attacker(world, Game::Units::SpawnType::Knight), 100),
            12);
  EXPECT_EQ(Game::Systems::Combat::resolve_structure_damage(
                add_attacker(world, Game::Units::SpawnType::Spearman), 100),
            12);
  EXPECT_EQ(Game::Systems::Combat::resolve_structure_damage(
                add_attacker(world, Game::Units::SpawnType::Ballista), 100),
            125);
  EXPECT_EQ(Game::Systems::Combat::resolve_structure_damage(
                add_attacker(world, Game::Units::SpawnType::Elephant), 100),
            160);
  EXPECT_EQ(Game::Systems::Combat::resolve_structure_damage(
                add_attacker(world, Game::Units::SpawnType::Catapult), 100),
            175);
}

TEST_F(StructureCombatTest, AppliedDamageAndEventsUseEffectiveStructureDamage) {
  Engine::Core::World world;
  auto* attacker = add_attacker(world, Game::Units::SpawnType::Knight);
  auto* structure = add_structure(world, Game::Units::SpawnType::Barracks, 5.0F, 0.0F);

  std::vector<int> hit_damage;
  std::vector<int> attacked_damage;
  Engine::Core::ScopedEventSubscription<Engine::Core::CombatHitEvent> const hit(
      [&hit_damage](Engine::Core::CombatHitEvent const& event) {
        hit_damage.push_back(event.damage);
      });
  Engine::Core::ScopedEventSubscription<Engine::Core::BuildingAttackedEvent> const
      attacked([&attacked_damage](Engine::Core::BuildingAttackedEvent const& event) {
        attacked_damage.push_back(event.damage);
      });

  auto const result = Game::Systems::Combat::apply_unit_damage(
      &world, structure, 100, attacker->get_id());

  EXPECT_EQ(result.applied_damage, 12);
  EXPECT_EQ(result.new_health, 988);
  ASSERT_EQ(hit_damage.size(), 1U);
  EXPECT_EQ(hit_damage.front(), 12);
  ASSERT_EQ(attacked_damage.size(), 1U);
  EXPECT_EQ(attacked_damage.front(), 12);
  auto const* presentation =
      structure->get_component<Engine::Core::StructureDamagePresentationComponent>();
  ASSERT_NE(presentation, nullptr);
  ASSERT_EQ(presentation->impacts.size(), 1U);
  EXPECT_NEAR(presentation->impacts.front().x, 3.0F, 0.001F);
}

TEST_F(StructureCombatTest, ArrowHitLeavesStructureHealthAndDamageCuesUntouched) {
  Engine::Core::World world;
  auto* archer = add_attacker(world, Game::Units::SpawnType::Archer);
  auto* structure = add_structure(world, Game::Units::SpawnType::Home, 5.0F, 0.0F);

  auto const result = Game::Systems::Combat::apply_unit_damage(
      &world, structure, 80, archer->get_id(), QVector3D(3.5F, 1.0F, 0.0F));

  EXPECT_EQ(result.applied_damage, 0);
  EXPECT_EQ(result.previous_health, 1000);
  EXPECT_EQ(result.new_health, 1000);
  EXPECT_EQ(
      structure->get_component<Engine::Core::StructureDamagePresentationComponent>(),
      nullptr);
}

TEST_F(StructureCombatTest, RotatedFootprintReturnsTheVisibleFacade) {
  Engine::Core::World world;
  auto* structure =
      add_structure(world, Game::Units::SpawnType::Barracks, 10.0F, 0.0F, 90.0F);

  auto const contact = Game::Systems::Combat::closest_structure_surface(
      *structure, QVector3D(10.0F, 0.0F, -5.0F));

  EXPECT_NEAR(contact.point.x(), 10.0F, 0.001F);
  EXPECT_NEAR(contact.point.z(), -2.0F, 0.001F);
  EXPECT_NEAR(contact.distance, 3.0F, 0.001F);
  EXPECT_NEAR(contact.outward_normal.x(), 0.0F, 0.001F);
  EXPECT_NEAR(contact.outward_normal.z(), -1.0F, 0.001F);
}

TEST_F(StructureCombatTest, RegisteredGateFootprintIsNotRotatedTwice) {
  Engine::Core::World world;
  auto* gate =
      add_structure(world, Game::Units::SpawnType::WallGate, 0.0F, 6.0F, 90.0F);
  Game::Systems::BuildingCollisionRegistry::instance().resize_building(
      gate->get_id(), {.width = 3.0F, .depth = 9.0F});

  auto const contact = Game::Systems::Combat::closest_structure_surface(
      *gate, QVector3D(0.0F, 0.0F, -2.0F));

  EXPECT_NEAR(contact.point.x(), 0.0F, 0.001F);
  EXPECT_NEAR(contact.point.z(), 1.5F, 0.001F);
  EXPECT_NEAR(contact.distance, 3.5F, 0.001F);
  EXPECT_NEAR(contact.outward_normal.x(), 0.0F, 0.001F);
  EXPECT_NEAR(contact.outward_normal.z(), -1.0F, 0.001F);
}

TEST_F(StructureCombatTest, FullFormationApproachUsesItsNavigationRoot) {
  Engine::Core::World world;
  auto* attacker =
      add_attacker(world, Game::Units::SpawnType::Spearman, 0.0F, 0.0F, 36);
  auto* wall = add_structure(world, Game::Units::SpawnType::WallSegment, 0.0F, 12.0F);

  auto const approach =
      Game::Systems::Combat::structure_melee_approach(*attacker, *wall);

  EXPECT_FALSE(approach.reached);
  EXPECT_NEAR(approach.current_surface_gap, 11.0F, 0.001F);
  EXPECT_NEAR(approach.destination.x(), 0.0F, 0.001F);
  EXPECT_NEAR(approach.destination.z(), 10.18F, 0.001F);
}

TEST_F(StructureCombatTest, MeleeApproachUsesTheClosestWalkableFacadePosition) {
  Engine::Core::World world;
  auto* attacker = add_attacker(world, Game::Units::SpawnType::Knight, 0.0F, 0.0F);
  auto* structure = add_structure(world, Game::Units::SpawnType::Barracks, 0.0F, 6.0F);

  auto const approach =
      Game::Systems::Combat::structure_melee_approach(*attacker, *structure);
  EXPECT_FALSE(approach.reached);
  EXPECT_NEAR(approach.desired_surface_gap, 0.62F, 0.001F);
  EXPECT_NEAR(approach.destination.z(), 3.38F, 0.001F);

  auto* transform = attacker->get_component<Engine::Core::TransformComponent>();
  transform->position.x = approach.destination.x();
  transform->position.z = approach.destination.z();

  Game::Systems::NavGrid::initialize(33, 33);
  auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->mark_navigation_grid_dirty();
  pathfinder->update_navigation_grid();
  EXPECT_FALSE(pathfinder->is_world_position_walkable(approach.destination));
  EXPECT_FALSE(
      Game::Systems::Combat::structure_melee_contact_active(*attacker, *structure));

  auto const navigated =
      Game::Systems::Combat::structure_navigation_melee_approach(*attacker, *structure);
  EXPECT_FALSE(navigated.reached);
  EXPECT_TRUE(pathfinder->is_world_position_walkable(navigated.destination));
  EXPECT_LT(navigated.destination.z(), approach.destination.z());

  transform->position.x = navigated.destination.x();
  transform->position.z = navigated.destination.z();
  EXPECT_TRUE(
      Game::Systems::Combat::structure_melee_contact_active(*attacker, *structure));
}
