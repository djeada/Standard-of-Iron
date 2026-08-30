#include <cmath>
#include <gtest/gtest.h>
#include <string>

#include "core/component.h"
#include "core/entity.h"
#include "core/world.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/combat_system/combat_utils.h"
#include "game/systems/default_content.h"
#include "game/systems/formation_combat_geometry.h"
#include "game/systems/nation_registry.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/pathfinding.h"
#include "game/systems/runtime_system_registry.h"
#include "units/factory.h"
#include "units/unit.h"

namespace {

using Engine::Core::AttackComponent;
using Engine::Core::CreaturePresentationComponent;
using Engine::Core::TransformComponent;
using Engine::Core::UnitComponent;

class MeleeEngagementTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto& owners = Game::Systems::OwnerRegistry::instance();
    owners.clear();
    owners.register_owner_with_id(1, Game::Systems::OwnerType::Player, "Player");
    owners.set_owner_team(1, 1);
    owners.register_owner_with_id(2, Game::Systems::OwnerType::AI, "Enemy");
    owners.set_owner_team(2, 2);
    owners.set_local_player_id(1);

    auto& nations = Game::Systems::NationRegistry::instance();
    nations.clear();
    Game::Systems::initialize_default_content(nations);

    Game::Map::MapDefinition map_definition;
    map_definition.grid.width = 48;
    map_definition.grid.height = 48;
    map_definition.grid.tile_size = 1.0F;
    map_definition.biome.procedural_boulders_enabled = false;
    map_definition.biome.procedural_iron_ore_enabled = false;
    map_definition.biome.procedural_trees_enabled = false;
    Game::Map::TerrainService::instance().initialize(map_definition);
    Game::Systems::NavGrid::initialize(48, 48);

    registry = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*registry);
  }

  void TearDown() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
    Game::Systems::OwnerRegistry::instance().clear();
  }

  auto spawn(Engine::Core::World& world,
             Game::Units::SpawnType type,
             int owner_id,
             const QVector3D& position,
             Game::Systems::NationID nation) -> Engine::Core::Entity* {
    Game::Units::SpawnParams params;
    params.position = position;
    params.player_id = owner_id;
    params.spawn_type = type;
    params.nation_id = nation;
    params.is_initial_spawn = true;
    auto unit = registry->create(type, world, params);
    if (!unit) {
      return nullptr;
    }
    auto* entity = world.get_entity(unit->id());
    if (entity != nullptr) {
      if (auto* unit_comp = entity->get_component<UnitComponent>()) {

        unit_comp->render_individuals_per_unit_override = 1;
      }
    }
    return entity;
  }

  auto spawn_formation(Engine::Core::World& world,
                       Game::Units::SpawnType type,
                       int owner_id,
                       const QVector3D& position,
                       Game::Systems::NationID nation) -> Engine::Core::Entity* {
    Game::Units::SpawnParams params;
    params.position = position;
    params.player_id = owner_id;
    params.spawn_type = type;
    params.nation_id = nation;
    params.is_initial_spawn = true;
    auto unit = registry->create(type, world, params);
    if (!unit) {
      return nullptr;
    }
    return world.get_entity(unit->id());
  }

  static void order_attack(Engine::Core::Entity& attacker,
                           const Engine::Core::Entity& target) {
    auto* attack_target = attacker.add_component<Engine::Core::AttackTargetComponent>();
    attack_target->target_id = target.get_id();
    attack_target->should_chase = true;
  }

  static auto separation(const Engine::Core::Entity& first,
                         const Engine::Core::Entity& second) -> float {
    const auto* from = first.get_component<TransformComponent>();
    const auto* to = second.get_component<TransformComponent>();
    if (from == nullptr || to == nullptr) {
      return 0.0F;
    }
    return std::hypot(to->position.x - from->position.x,
                      to->position.z - from->position.z);
  }

  static auto strike_distance(const Engine::Core::Entity& attacker,
                              const Engine::Core::Entity& target) -> float {
    auto const geometry =
        Game::Systems::FormationCombat::contact_geometry(attacker, target);
    return Game::Systems::FormationCombat::single_combat_strike_distance(
        attacker, target, geometry);
  }

  std::shared_ptr<Game::Units::UnitFactoryRegistry> registry;
};

struct DuelCase {
  Game::Units::SpawnType attacker;
  Game::Units::SpawnType defender;
  const char* name;
};

class MeleeDuelDistanceTest : public MeleeEngagementTest,
                              public ::testing::WithParamInterface<DuelCase> {};

struct BareHandedCase {
  Game::Units::SpawnType defender;
  const char* name;
};

class BareHandedMeleeTest : public MeleeEngagementTest,
                            public ::testing::WithParamInterface<BareHandedCase> {};

auto sanitized_case_name(const char* name) -> std::string {
  std::string sanitized(name);
  for (char& character : sanitized) {
    if (character == ' ') {
      character = '_';
    }
  }
  return sanitized;
}

} // namespace

TEST_P(MeleeDuelDistanceTest, DuellistsCloseToWeaponContactBeforeSwinging) {
  Engine::Core::World world;
  Game::Systems::register_runtime_systems(world);

  auto* attacker = spawn(world,
                         GetParam().attacker,
                         1,
                         QVector3D(-5.0F, 0.0F, 0.0F),
                         Game::Systems::NationID::RomanRepublic);
  auto* defender = spawn(world,
                         GetParam().defender,
                         2,
                         QVector3D(0.0F, 0.0F, 0.0F),
                         Game::Systems::NationID::Carthage);
  ASSERT_NE(attacker, nullptr);
  ASSERT_NE(defender, nullptr);

  auto* attacker_unit = attacker->get_component<UnitComponent>();
  auto* defender_unit = defender->get_component<UnitComponent>();
  ASSERT_NE(attacker_unit, nullptr);
  ASSERT_NE(defender_unit, nullptr);

  attacker_unit->health = attacker_unit->max_health = 100000;
  defender_unit->health = defender_unit->max_health = 100000;

  order_attack(*attacker, *defender);

  float closest = 1000.0F;
  bool struck = false;
  for (int tick = 0; tick < 300; ++tick) {
    world.update(0.05F);
    closest = std::min(closest, separation(*attacker, *defender));
    const auto* presentation = attacker->get_component<CreaturePresentationComponent>();
    struck = struck || (presentation != nullptr && presentation->is_attacking);
  }

  const float settled = separation(*attacker, *defender);
  const float allowed = strike_distance(*attacker, *defender);

  EXPECT_TRUE(struck) << GetParam().name << " never threw a blow (settled at "
                      << settled << ", reach " << allowed << ", closest " << closest
                      << ")";
  EXPECT_LE(settled, allowed + 0.15F) << GetParam().name << " stopped at " << settled
                                      << " but only reaches " << allowed;
  EXPECT_GE(closest, 0.4F) << GetParam().name << " walked through its enemy";
}

INSTANTIATE_TEST_SUITE_P(
    Matchups,
    MeleeDuelDistanceTest,
    ::testing::Values(DuelCase{Game::Units::SpawnType::Knight,
                               Game::Units::SpawnType::GravePriest,
                               "swordsman vs caster"},
                      DuelCase{Game::Units::SpawnType::Knight,
                               Game::Units::SpawnType::Knight,
                               "swordsman duel"},
                      DuelCase{Game::Units::SpawnType::Spearman,
                               Game::Units::SpawnType::Knight,
                               "spearman vs swordsman"},
                      DuelCase{Game::Units::SpawnType::RomanVeteranConsul,
                               Game::Units::SpawnType::CarthageSwordCommander,
                               "commander duel"},
                      DuelCase{Game::Units::SpawnType::CarthageSpearCommander,
                               Game::Units::SpawnType::RomanFieldCommander,
                               "spear commander vs bow commander"}),
    [](const testing::TestParamInfo<DuelCase>& info) {
      return sanitized_case_name(info.param.name);
    });

TEST_P(BareHandedMeleeTest, NonCombatantsFightOnceLockedInMelee) {
  Engine::Core::World world;
  Game::Systems::register_runtime_systems(world);

  auto* swordsman = spawn(world,
                          Game::Units::SpawnType::Knight,
                          1,
                          QVector3D(-2.0F, 0.0F, 0.0F),
                          Game::Systems::NationID::RomanRepublic);
  auto* defender = spawn(world,
                         GetParam().defender,
                         2,
                         QVector3D(0.0F, 0.0F, 0.0F),
                         Game::Systems::NationID::IronSepulcher);
  ASSERT_NE(swordsman, nullptr);
  ASSERT_NE(defender, nullptr);

  auto* swordsman_unit = swordsman->get_component<UnitComponent>();
  auto* defender_unit = defender->get_component<UnitComponent>();
  ASSERT_NE(swordsman_unit, nullptr);
  ASSERT_NE(defender_unit, nullptr);

  swordsman_unit->health = swordsman_unit->max_health = 100000;
  defender_unit->health = defender_unit->max_health = 100000;
  const int swordsman_health_before = swordsman_unit->health;

  order_attack(*swordsman, *defender);

  bool locked = false;
  bool swung = false;
  bool channelled_while_locked = false;
  for (int tick = 0; tick < 400; ++tick) {
    world.update(0.05F);
    const auto* attack = defender->get_component<AttackComponent>();
    const auto* presentation = defender->get_component<CreaturePresentationComponent>();
    if (attack == nullptr || !attack->in_melee_lock) {
      continue;
    }
    locked = true;
    if (presentation != nullptr) {
      swung = swung || (presentation->is_attacking && presentation->is_melee);
      channelled_while_locked = channelled_while_locked || presentation->is_healing ||
                                presentation->is_constructing;
    }
  }

  EXPECT_TRUE(locked) << GetParam().name << " was never dragged into the melee";
  EXPECT_TRUE(swung) << GetParam().name
                     << " stood there instead of fighting bare-handed";
  EXPECT_FALSE(channelled_while_locked)
      << GetParam().name << " kept channelling while an enemy was on top of it";
  EXPECT_LT(swordsman_unit->health, swordsman_health_before)
      << GetParam().name << " never landed a blow of its own";
}

INSTANTIATE_TEST_SUITE_P(
    Noncombatants,
    BareHandedMeleeTest,
    ::testing::Values(BareHandedCase{Game::Units::SpawnType::GravePriest, "caster"},
                      BareHandedCase{Game::Units::SpawnType::Archer, "archer"},
                      BareHandedCase{Game::Units::SpawnType::Healer, "healer"},
                      BareHandedCase{Game::Units::SpawnType::Civilian, "civilian"},
                      BareHandedCase{Game::Units::SpawnType::Builder, "builder"}),
    [](const testing::TestParamInfo<BareHandedCase>& info) {
      return sanitized_case_name(info.param.name);
    });

TEST_F(MeleeEngagementTest, AHealerNeverPicksItsOwnFight) {
  Engine::Core::World world;
  Game::Systems::register_runtime_systems(world);

  auto* healer = spawn(world,
                       Game::Units::SpawnType::Healer,
                       1,
                       QVector3D(0.0F, 0.0F, 0.0F),
                       Game::Systems::NationID::RomanRepublic);
  auto* bystander = spawn(world,
                          Game::Units::SpawnType::Civilian,
                          2,
                          QVector3D(4.0F, 0.0F, 0.0F),
                          Game::Systems::NationID::Carthage);
  ASSERT_NE(healer, nullptr);
  ASSERT_NE(bystander, nullptr);

  auto* bystander_unit = bystander->get_component<UnitComponent>();
  ASSERT_NE(bystander_unit, nullptr);
  const int bystander_health_before = bystander_unit->health;

  for (int tick = 0; tick < 200; ++tick) {
    world.update(0.05F);
    const auto* target = healer->get_component<Engine::Core::AttackTargetComponent>();
    ASSERT_TRUE(target == nullptr || target->target_id == 0)
        << "the healer went hunting on its own at tick " << tick;
  }

  EXPECT_EQ(bystander_unit->health, bystander_health_before)
      << "the healer attacked an enemy nobody told it to attack";
  EXPECT_GT(separation(*healer, *bystander), 3.0F)
      << "the healer closed on an enemy it was never ordered to fight";
}

TEST_F(MeleeEngagementTest, AHealerDoesNotRetaliateWhenStruck) {
  Engine::Core::World world;
  Game::Systems::register_runtime_systems(world);

  auto* healer = spawn(world,
                       Game::Units::SpawnType::Healer,
                       1,
                       QVector3D(0.0F, 0.0F, 0.0F),
                       Game::Systems::NationID::RomanRepublic);
  auto* archer = spawn(world,
                       Game::Units::SpawnType::Archer,
                       2,
                       QVector3D(6.0F, 0.0F, 0.0F),
                       Game::Systems::NationID::Carthage);
  ASSERT_NE(healer, nullptr);
  ASSERT_NE(archer, nullptr);

  auto* healer_unit = healer->get_component<UnitComponent>();
  ASSERT_NE(healer_unit, nullptr);
  healer_unit->health = healer_unit->max_health = 100000;

  order_attack(*archer, *healer);

  bool was_hit = false;
  for (int tick = 0; tick < 400; ++tick) {
    world.update(0.05F);
    was_hit = was_hit || healer_unit->health < healer_unit->max_health;
    const auto* target = healer->get_component<Engine::Core::AttackTargetComponent>();
    ASSERT_TRUE(target == nullptr || target->target_id == 0)
        << "the healer answered fire with fire at tick " << tick;
  }

  EXPECT_TRUE(was_hit) << "the archer never landed a shot, so nothing was proven";
}

TEST_F(MeleeEngagementTest, AHealerStopsFightingWhenTheMeleeLetsGo) {
  Engine::Core::World world;
  Game::Systems::register_runtime_systems(world);

  auto* healer = spawn(world,
                       Game::Units::SpawnType::Healer,
                       1,
                       QVector3D(0.0F, 0.0F, 0.0F),
                       Game::Systems::NationID::RomanRepublic);
  auto* swordsman = spawn(world,
                          Game::Units::SpawnType::Knight,
                          2,
                          QVector3D(-2.0F, 0.0F, 0.0F),
                          Game::Systems::NationID::Carthage);
  ASSERT_NE(healer, nullptr);
  ASSERT_NE(swordsman, nullptr);

  auto* healer_unit = healer->get_component<UnitComponent>();
  auto* swordsman_unit = swordsman->get_component<UnitComponent>();
  ASSERT_NE(healer_unit, nullptr);
  ASSERT_NE(swordsman_unit, nullptr);
  healer_unit->health = healer_unit->max_health = 100000;
  swordsman_unit->health = swordsman_unit->max_health = 100000;
  const int swordsman_health_before = swordsman_unit->health;

  order_attack(*swordsman, *healer);

  bool locked = false;
  for (int tick = 0; tick < 400; ++tick) {
    world.update(0.05F);
    const auto* attack = healer->get_component<AttackComponent>();
    locked = locked || ((attack != nullptr) && attack->in_melee_lock);
  }
  ASSERT_TRUE(locked) << "the healer was never dragged into the melee";
  EXPECT_LT(swordsman_unit->health, swordsman_health_before)
      << "the healer refused to defend itself in a melee it could not leave";

  swordsman->add_component<Engine::Core::PendingRemovalComponent>();
  for (int tick = 0; tick < 200; ++tick) {
    world.update(0.05F);
  }

  const auto* attack = healer->get_component<AttackComponent>();
  EXPECT_TRUE(attack == nullptr || !attack->in_melee_lock);
  const auto* target = healer->get_component<Engine::Core::AttackTargetComponent>();
  EXPECT_TRUE(target == nullptr || target->target_id == 0)
      << "the healer kept the target the melee left behind";
}

TEST_F(MeleeEngagementTest, SiegingUnitDropsTheWallForAnEnemySoldierInReach) {
  Engine::Core::World world;
  Game::Systems::register_runtime_systems(world);

  auto* besieger = spawn(world,
                         Game::Units::SpawnType::Spearman,
                         2,
                         QVector3D(0.0F, 0.0F, 0.0F),
                         Game::Systems::NationID::Carthage);
  ASSERT_NE(besieger, nullptr);
  (void)besieger->add_component<Engine::Core::AIControlledComponent>();

  auto* structure = world.create_entity();
  (void)structure->add_component<TransformComponent>(3.0F, 0.0F, 0.0F);
  auto* structure_unit =
      structure->add_component<UnitComponent>(100000, 100000, 0.0F, 12.0F);
  structure_unit->owner_id = 1;
  structure_unit->spawn_type = Game::Units::SpawnType::Barracks;
  (void)structure->add_component<Engine::Core::BuildingComponent>();

  order_attack(*besieger, *structure);

  bool locked_onto_structure = false;
  for (int tick = 0; tick < 200 && !locked_onto_structure; ++tick) {
    world.update(0.05F);
    const auto* attack = besieger->get_component<AttackComponent>();
    locked_onto_structure = attack != nullptr && attack->in_melee_lock &&
                            attack->melee_lock_target_id == structure->get_id();
  }
  ASSERT_TRUE(locked_onto_structure) << "besieger never engaged the structure";

  auto* soldier = spawn(world,
                        Game::Units::SpawnType::Knight,
                        1,
                        QVector3D(-1.2F, 0.0F, 0.0F),
                        Game::Systems::NationID::RomanRepublic);
  ASSERT_NE(soldier, nullptr);
  auto* soldier_unit = soldier->get_component<UnitComponent>();
  ASSERT_NE(soldier_unit, nullptr);
  soldier_unit->health = soldier_unit->max_health = 100000;

  bool switched_to_soldier = false;
  for (int tick = 0; tick < 120 && !switched_to_soldier; ++tick) {
    world.update(0.05F);
    const auto* attack_target =
        besieger->get_component<Engine::Core::AttackTargetComponent>();
    switched_to_soldier =
        attack_target != nullptr && attack_target->target_id == soldier->get_id();
  }

  EXPECT_TRUE(switched_to_soldier)
      << "besieger kept hitting the barracks with an enemy soldier on top of it";
}

TEST_F(MeleeEngagementTest, GuardMeleeClosesOnIntrudersButHoldsItsLeash) {
  Engine::Core::World world;
  Game::Systems::register_runtime_systems(world);

  auto* guard = spawn(world,
                      Game::Units::SpawnType::Spearman,
                      2,
                      QVector3D(0.0F, 0.0F, 0.0F),
                      Game::Systems::NationID::Carthage);
  ASSERT_NE(guard, nullptr);
  auto* guard_unit = guard->get_component<UnitComponent>();
  ASSERT_NE(guard_unit, nullptr);
  guard_unit->health = guard_unit->max_health = 100000;
  auto* guard_mode = guard->add_component<Engine::Core::GuardModeComponent>();
  guard_mode->active = true;
  guard_mode->has_guard_target = true;
  guard_mode->guard_position_x = 0.0F;
  guard_mode->guard_position_z = 0.0F;
  guard_mode->guard_radius = 8.0F;

  auto* far_intruder = spawn(world,
                             Game::Units::SpawnType::Knight,
                             1,
                             QVector3D(11.0F, 0.0F, 0.0F),
                             Game::Systems::NationID::RomanRepublic);
  ASSERT_NE(far_intruder, nullptr);
  auto* far_unit = far_intruder->get_component<UnitComponent>();
  ASSERT_NE(far_unit, nullptr);
  far_unit->health = far_unit->max_health = 100000;
  far_intruder->add_component<Engine::Core::HoldModeComponent>()->active = true;

  for (int tick = 0; tick < 60; ++tick) {
    world.update(0.05F);
  }
  EXPECT_LT(guard->get_component<TransformComponent>()->position.x, 1.0F)
      << "guard left its post for an enemy outside the guard radius";

  auto* near_intruder = spawn(world,
                              Game::Units::SpawnType::Knight,
                              1,
                              QVector3D(5.0F, 0.0F, 0.0F),
                              Game::Systems::NationID::RomanRepublic);
  ASSERT_NE(near_intruder, nullptr);
  auto* near_unit = near_intruder->get_component<UnitComponent>();
  ASSERT_NE(near_unit, nullptr);
  near_unit->health = near_unit->max_health = 100000;
  near_intruder->add_component<Engine::Core::HoldModeComponent>()->active = true;

  bool struck = false;
  for (int tick = 0; tick < 200 && !struck; ++tick) {
    world.update(0.05F);
    const auto* presentation = guard->get_component<CreaturePresentationComponent>();
    struck = struck || (presentation != nullptr && presentation->is_attacking);
  }
  EXPECT_TRUE(struck) << "guard never closed on an intruder inside its radius";
  EXPECT_GT(guard->get_component<TransformComponent>()->position.x, 1.0F);
}

TEST_F(MeleeEngagementTest, BesiegerShotOverTheWallKeepsBreachingInsteadOfChasing) {
  Engine::Core::World world;
  Game::Systems::register_runtime_systems(world);

  auto* besieger = spawn(world,
                         Game::Units::SpawnType::Spearman,
                         2,
                         QVector3D(0.0F, 0.0F, 0.0F),
                         Game::Systems::NationID::Carthage);
  ASSERT_NE(besieger, nullptr);
  auto* besieger_unit = besieger->get_component<UnitComponent>();
  ASSERT_NE(besieger_unit, nullptr);
  besieger_unit->health = besieger_unit->max_health = 100000;
  (void)besieger->add_component<Engine::Core::AIControlledComponent>();

  auto* wall = spawn(world,
                     Game::Units::SpawnType::WallSegment,
                     1,
                     QVector3D(4.0F, 0.0F, 0.0F),
                     Game::Systems::NationID::RomanRepublic);
  ASSERT_NE(wall, nullptr);
  auto* wall_unit = wall->get_component<UnitComponent>();
  ASSERT_NE(wall_unit, nullptr);
  wall_unit->health = wall_unit->max_health = 100000;

  auto* archer = spawn(world,
                       Game::Units::SpawnType::Archer,
                       1,
                       QVector3D(9.0F, 0.0F, 0.0F),
                       Game::Systems::NationID::RomanRepublic);
  ASSERT_NE(archer, nullptr);
  archer->add_component<Engine::Core::HoldModeComponent>()->active = true;

  order_attack(*besieger, *wall);

  bool took_damage = false;
  for (int tick = 0; tick < 400; ++tick) {
    world.update(0.05F);
    took_damage = took_damage || besieger_unit->health < besieger_unit->max_health;
    const auto* attack_target =
        besieger->get_component<Engine::Core::AttackTargetComponent>();
    ASSERT_TRUE(attack_target == nullptr ||
                attack_target->target_id != archer->get_id())
        << "besieger dropped the wall to chase an archer it cannot reach at tick "
        << tick;
  }
  EXPECT_TRUE(took_damage) << "the archer never shot the besieger, test is vacuous";
  EXPECT_LT(besieger->get_component<TransformComponent>()->position.x, 4.0F);
}

TEST_F(MeleeEngagementTest, MeleeAttackerWalksAroundTheWallBeforeFighting) {
  Engine::Core::World world;
  Game::Systems::register_runtime_systems(world);

  for (float wall_z = -5.0F; wall_z <= 5.0F; wall_z += 2.0F) {
    auto* wall = spawn(world,
                       Game::Units::SpawnType::WallSegment,
                       1,
                       QVector3D(4.0F, 0.0F, wall_z),
                       Game::Systems::NationID::RomanRepublic);
    ASSERT_NE(wall, nullptr);
    auto* wall_unit = wall->get_component<UnitComponent>();
    ASSERT_NE(wall_unit, nullptr);
    wall_unit->health = wall_unit->max_health = 1000000;
  }

  auto* attacker = spawn(world,
                         Game::Units::SpawnType::Knight,
                         2,
                         QVector3D(0.0F, 0.0F, 0.0F),
                         Game::Systems::NationID::Carthage);
  auto* defender = spawn(world,
                         Game::Units::SpawnType::Knight,
                         1,
                         QVector3D(8.0F, 0.0F, 0.0F),
                         Game::Systems::NationID::RomanRepublic);
  ASSERT_NE(attacker, nullptr);
  ASSERT_NE(defender, nullptr);

  auto* attacker_unit = attacker->get_component<UnitComponent>();
  auto* defender_unit = defender->get_component<UnitComponent>();
  ASSERT_NE(attacker_unit, nullptr);
  ASSERT_NE(defender_unit, nullptr);
  attacker_unit->health = attacker_unit->max_health = 1000000;
  defender_unit->health = defender_unit->max_health = 1000000;

  order_attack(*attacker, *defender);

  const auto* attacker_transform = attacker->get_component<TransformComponent>();
  ASSERT_NE(attacker_transform, nullptr);

  bool crossed = false;
  bool struck_through_wall = false;
  float best_x = attacker_transform->position.x;
  for (int tick = 0; tick < 1200; ++tick) {
    world.update(0.05F);
    best_x = std::max(best_x, attacker_transform->position.x);
    crossed = crossed || attacker_transform->position.x > 5.5F;
    if (!crossed && defender_unit->health < defender_unit->max_health) {
      struck_through_wall = true;
    }
  }

  EXPECT_FALSE(struck_through_wall) << "the defender was hit through the wall";
  EXPECT_TRUE(crossed) << "the attacker never walked around the wall (reached x="
                       << best_x << ")";
  EXPECT_LT(defender_unit->health, defender_unit->max_health)
      << "the attacker never landed a blow after reaching the defender's side";
}

TEST_F(MeleeEngagementTest, SquadOrderedThroughAWallMarchesAroundItBeforeFighting) {
  Engine::Core::World world;
  Game::Systems::register_runtime_systems(world);

  for (float wall_z = -7.0F; wall_z <= 7.0F; wall_z += 2.0F) {
    auto* wall = spawn(world,
                       Game::Units::SpawnType::WallSegment,
                       1,
                       QVector3D(4.0F, 0.0F, wall_z),
                       Game::Systems::NationID::RomanRepublic);
    ASSERT_NE(wall, nullptr);
    auto* wall_unit = wall->get_component<UnitComponent>();
    ASSERT_NE(wall_unit, nullptr);
    wall_unit->health = wall_unit->max_health = 1000000;
  }

  auto* attacker = spawn_formation(world,
                                   Game::Units::SpawnType::Knight,
                                   2,
                                   QVector3D(0.0F, 0.0F, 0.0F),
                                   Game::Systems::NationID::Carthage);
  auto* defender = spawn_formation(world,
                                   Game::Units::SpawnType::Knight,
                                   1,
                                   QVector3D(11.0F, 0.0F, 0.0F),
                                   Game::Systems::NationID::RomanRepublic);
  ASSERT_NE(attacker, nullptr);
  ASSERT_NE(defender, nullptr);

  auto* attacker_unit = attacker->get_component<UnitComponent>();
  auto* defender_unit = defender->get_component<UnitComponent>();
  ASSERT_NE(attacker_unit, nullptr);
  ASSERT_NE(defender_unit, nullptr);
  attacker_unit->health = attacker_unit->max_health = 1000000;
  defender_unit->health = defender_unit->max_health = 1000000;

  order_attack(*attacker, *defender);

  const auto* attacker_transform = attacker->get_component<TransformComponent>();
  ASSERT_NE(attacker_transform, nullptr);

  bool reached_defender_side = false;
  float furthest_x = attacker_transform->position.x;
  for (int tick = 0; tick < 2400; ++tick) {
    world.update(0.05F);
    furthest_x = std::max(furthest_x, attacker_transform->position.x);

    const bool separated =
        Game::Systems::Combat::structure_separates_combatants(attacker, defender);
    if (separated) {
      ASSERT_EQ(defender_unit->health, defender_unit->max_health)
          << "the defender was struck through the wall at tick " << tick;
      const auto* contact =
          attacker->get_component<Engine::Core::FormationContactComponent>();
      ASSERT_FALSE(contact != nullptr && contact->in_contact &&
                   contact->target_id == defender->get_id())
          << "the attacker formed a melee front through the wall at tick " << tick;
    }
    reached_defender_side =
        reached_defender_side || (!separated && attacker_transform->position.x > 5.0F);
    if (reached_defender_side && defender_unit->health < defender_unit->max_health) {
      break;
    }
  }

  EXPECT_TRUE(reached_defender_side)
      << "the attacker never marched around the wall (furthest x=" << furthest_x << ")";
  EXPECT_LT(defender_unit->health, defender_unit->max_health)
      << "the attacker reached the defender's side but never struck";
}

TEST_F(MeleeEngagementTest, BypassDestinationStandsOnTheTargetSideOfTheWall) {
  Engine::Core::World world;
  Game::Systems::register_runtime_systems(world);

  for (float wall_z = -7.0F; wall_z <= 7.0F; wall_z += 2.0F) {
    ASSERT_NE(spawn(world,
                    Game::Units::SpawnType::WallSegment,
                    1,
                    QVector3D(4.0F, 0.0F, wall_z),
                    Game::Systems::NationID::RomanRepublic),
              nullptr);
  }

  const QVector3D attacker_position(0.0F, 0.0F, 0.0F);
  const QVector3D target_position(11.0F, 0.0F, 0.0F);
  const auto bypass = Game::Systems::Combat::melee_bypass_destination(
      attacker_position, target_position, 4.0F, 1.0F);

  ASSERT_TRUE(bypass.has_value());
  EXPECT_GT(bypass->x(), 5.0F) << "the bypass stayed on the attacker's side";
  EXPECT_FALSE(
      Game::Systems::Combat::structure_separates_positions(*bypass, target_position))
      << "the bypass cannot reach the target from where it stands";
  EXPECT_TRUE(Game::Systems::Combat::structure_separates_positions(attacker_position,
                                                                   target_position))
      << "the wall did not separate the pair, the case is vacuous";
}

TEST_F(MeleeEngagementTest, BypassDestinationIsRefusedWhenNoStandingSpotClears) {
  Engine::Core::World world;
  Game::Systems::register_runtime_systems(world);

  for (float wall_x = 8.0F; wall_x <= 12.0F; wall_x += 2.0F) {
    for (float wall_z = -2.0F; wall_z <= 2.0F; wall_z += 2.0F) {
      ASSERT_NE(spawn(world,
                      Game::Units::SpawnType::WallSegment,
                      1,
                      QVector3D(wall_x, 0.0F, wall_z),
                      Game::Systems::NationID::RomanRepublic),
                nullptr);
    }
  }

  const auto bypass = Game::Systems::Combat::melee_bypass_destination(
      QVector3D(0.0F, 0.0F, 0.0F), QVector3D(10.0F, 0.0F, 0.0F), 4.0F, 1.75F);
  EXPECT_FALSE(bypass.has_value());
}

TEST_F(MeleeEngagementTest, GridOnlyObstaclePreventsMeleeThroughIt) {
  Engine::Core::World world;
  auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_navigation_grid();
  Game::Systems::Point const obstacle{24, 24};
  pathfinder->set_obstacle(obstacle.x, obstacle.y, true);

  QVector3D const attacker_position =
      Game::Systems::NavGrid::grid_to_world({obstacle.x - 1, obstacle.y});
  QVector3D const defender_position =
      Game::Systems::NavGrid::grid_to_world({obstacle.x + 1, obstacle.y});
  auto* attacker = spawn(world,
                         Game::Units::SpawnType::Knight,
                         1,
                         attacker_position,
                         Game::Systems::NationID::Carthage);
  auto* defender = spawn(world,
                         Game::Units::SpawnType::Knight,
                         2,
                         defender_position,
                         Game::Systems::NationID::RomanRepublic);
  ASSERT_NE(attacker, nullptr);
  ASSERT_NE(defender, nullptr);
  auto* attack = attacker->get_component<AttackComponent>();
  ASSERT_NE(attack, nullptr);
  attack->current_mode = AttackComponent::CombatMode::Melee;

  EXPECT_FALSE(Game::Systems::BuildingCollisionRegistry::instance()
                   .segment_crosses_blocking_building(attacker_position.x(),
                                                      attacker_position.z(),
                                                      defender_position.x(),
                                                      defender_position.z()));
  EXPECT_TRUE(Game::Systems::Combat::structure_separates_positions(attacker_position,
                                                                   defender_position));
  EXPECT_FALSE(Game::Systems::Combat::is_in_range(attacker, defender, 100.0F));
}
