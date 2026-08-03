#include <cmath>
#include <gtest/gtest.h>
#include <string>

#include "core/component.h"
#include "core/entity.h"
#include "core/world.h"
#include "game/map/terrain_service.h"
#include "game/systems/command_service.h"
#include "game/systems/formation_combat_geometry.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
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
    nations.initialize_defaults();

    Game::Map::MapDefinition map_definition;
    map_definition.grid.width = 48;
    map_definition.grid.height = 48;
    map_definition.grid.tile_size = 1.0F;
    Game::Map::TerrainService::instance().initialize(map_definition);
    Game::Systems::CommandService::initialize(48, 48);

    registry = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*registry);
  }

  void TearDown() override {
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
