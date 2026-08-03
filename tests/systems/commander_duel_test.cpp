#include <cmath>
#include <gtest/gtest.h>
#include <set>
#include <string>

#include "core/component.h"
#include "core/entity.h"
#include "core/world.h"
#include "game/map/terrain_service.h"
#include "game/systems/combat_actions/combat_action_definition.h"
#include "game/systems/command_service.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/runtime_system_registry.h"
#include "units/commander_catalog.h"
#include "units/factory.h"
#include "units/unit.h"

namespace {

using Engine::Core::CommanderComponent;
using Engine::Core::RpgCommanderActionComponent;
using Engine::Core::TransformComponent;
using Engine::Core::UnitComponent;
using Game::Systems::CombatActions::CombatActionId;

struct CommanderCase {
  Game::Units::SpawnType commander;
  Game::Units::TroopType troop;
  CombatActionId signature_action;
  const char* name;
};

class CommanderDuelTest : public ::testing::Test,
                          public ::testing::WithParamInterface<CommanderCase> {
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
        unit_comp->health = unit_comp->max_health = 200000;
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

  std::shared_ptr<Game::Units::UnitFactoryRegistry> registry;
};

} // namespace

TEST(CommanderCatalogTest, EveryCommanderCarriesItsOwnSignatureMove) {
  auto const& definitions = Game::Units::all_commander_definitions();
  ASSERT_FALSE(definitions.empty());

  std::set<Game::Units::CommanderSignatureMove> moves;
  for (auto const& definition : definitions) {
    EXPECT_NE(definition.signature.move, Game::Units::CommanderSignatureMove::None)
        << definition.id << " has no signature duel move";
    EXPECT_FALSE(definition.signature.display_name.empty()) << definition.id;
    EXPECT_GT(definition.signature.cooldown_seconds, 0.0F) << definition.id;
    EXPECT_GE(definition.signature.damage_multiplier, 1.0F) << definition.id;
    moves.insert(definition.signature.move);
  }

  EXPECT_EQ(moves.size(), definitions.size())
      << "two commanders share a signature move; each one has to fight like itself";
}

TEST_P(CommanderDuelTest, CommanderThrowsItsSignatureInADuel) {
  Engine::Core::World world;
  Game::Systems::register_runtime_systems(world);

  auto* commander = spawn(world,
                          GetParam().commander,
                          1,
                          QVector3D(-4.0F, 0.0F, 0.0F),
                          Game::Systems::NationID::RomanRepublic);
  auto* rival = spawn(world,
                      Game::Units::SpawnType::Knight,
                      2,
                      QVector3D(0.0F, 0.0F, 0.0F),
                      Game::Systems::NationID::Carthage);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(rival, nullptr);

  auto const* definition = Game::Units::commander_definition(GetParam().troop);
  ASSERT_NE(definition, nullptr);

  auto* commander_comp = commander->get_component<CommanderComponent>();
  ASSERT_NE(commander_comp, nullptr);
  EXPECT_EQ(commander_comp->signature_move,
            static_cast<std::uint8_t>(definition->signature.move));

  order_attack(*commander, *rival);

  bool threw_signature = false;
  bool threw_routine = false;
  for (int tick = 0; tick < 600; ++tick) {
    world.update(0.05F);
    const auto* action = commander->get_component<RpgCommanderActionComponent>();
    if (action == nullptr || action->combat_action_id == 0U) {
      continue;
    }
    auto const id = static_cast<CombatActionId>(action->combat_action_id);
    threw_signature = threw_signature || id == GetParam().signature_action;
    threw_routine = threw_routine || (id == CombatActionId::RtsSwordStrike ||
                                      id == CombatActionId::RtsSpearThrust);
  }

  EXPECT_TRUE(threw_signature)
      << GetParam().name << " never used " << definition->signature.display_name;
  EXPECT_TRUE(threw_routine)
      << GetParam().name
      << " threw nothing but its signature; the move has to stay a moment, not a habit";
}

INSTANTIATE_TEST_SUITE_P(
    Commanders,
    CommanderDuelTest,
    ::testing::Values(CommanderCase{Game::Units::SpawnType::RomanLegionOrganizer,
                                    Game::Units::TroopType::RomanLegionOrganizer,
                                    CombatActionId::RtsCommanderThrust,
                                    "Fabius"},
                      CommanderCase{Game::Units::SpawnType::RomanVeteranConsul,
                                    Game::Units::TroopType::RomanVeteranConsul,
                                    CombatActionId::RtsCommanderCut,
                                    "Scipio"},
                      CommanderCase{Game::Units::SpawnType::CarthageSpearCommander,
                                    Game::Units::TroopType::CarthageSpearCommander,
                                    CombatActionId::RtsCommanderThrust,
                                    "Carthaginian spear commander"},
                      CommanderCase{Game::Units::SpawnType::CarthageSwordCommander,
                                    Game::Units::TroopType::CarthageSwordCommander,
                                    CombatActionId::RtsCommanderCut,
                                    "Hannibal"},
                      CommanderCase{Game::Units::SpawnType::RomanFieldCommander,
                                    Game::Units::TroopType::RomanFieldCommander,
                                    CombatActionId::RtsCommanderShot,
                                    "Marcellus"},
                      CommanderCase{Game::Units::SpawnType::CarthageBowCommander,
                                    Game::Units::TroopType::CarthageBowCommander,
                                    CombatActionId::RtsCommanderShot,
                                    "Carthaginian bow commander"}),
    [](const testing::TestParamInfo<CommanderCase>& info) {
      std::string name(info.param.name);
      for (char& character : name) {
        if (character == ' ') {
          character = '_';
        }
      }
      return name;
    });

TEST_F(CommanderDuelTest, SignatureLeavesAContactBurstForTheRenderer) {
  Engine::Core::World world;
  Game::Systems::register_runtime_systems(world);

  auto* commander = spawn(world,
                          Game::Units::SpawnType::CarthageSwordCommander,
                          1,
                          QVector3D(-4.0F, 0.0F, 0.0F),
                          Game::Systems::NationID::Carthage);
  auto* rival = spawn(world,
                      Game::Units::SpawnType::Knight,
                      2,
                      QVector3D(0.0F, 0.0F, 0.0F),
                      Game::Systems::NationID::RomanRepublic);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(rival, nullptr);

  order_attack(*commander, *rival);

  bool saw_burst = false;
  for (int tick = 0; tick < 600 && !saw_burst; ++tick) {
    world.update(0.05F);
    auto const* presentation =
        commander
            ->get_component<Engine::Core::CommanderSignaturePresentationComponent>();
    if (presentation == nullptr || presentation->entries.empty()) {
      continue;
    }
    saw_burst = true;
    auto const& entry = presentation->entries.back();
    EXPECT_EQ(entry.form, Engine::Core::CommanderSignatureForm::Cut);
    EXPECT_NEAR(std::hypot(entry.dir_x, entry.dir_z), 1.0F, 0.01F);

    auto const* commander_transform = commander->get_component<TransformComponent>();
    ASSERT_NE(commander_transform, nullptr);
    EXPECT_GT(std::hypot(entry.x - commander_transform->position.x,
                         entry.z - commander_transform->position.z),
              0.2F);
  }

  EXPECT_TRUE(saw_burst) << "a signature strike left no cue for the renderer to draw";
}

TEST_F(CommanderDuelTest, ContactBurstsExpireInsteadOfPilingUp) {
  Engine::Core::World world;
  Game::Systems::register_runtime_systems(world);

  auto* commander = spawn(world,
                          Game::Units::SpawnType::CarthageSwordCommander,
                          1,
                          QVector3D(-4.0F, 0.0F, 0.0F),
                          Game::Systems::NationID::Carthage);
  auto* rival = spawn(world,
                      Game::Units::SpawnType::Knight,
                      2,
                      QVector3D(0.0F, 0.0F, 0.0F),
                      Game::Systems::NationID::RomanRepublic);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(rival, nullptr);

  order_attack(*commander, *rival);

  std::size_t peak_entries = 0;
  for (int tick = 0; tick < 1200; ++tick) {
    world.update(0.05F);
    if (auto const* presentation =
            commander->get_component<
                Engine::Core::CommanderSignaturePresentationComponent>()) {
      peak_entries = std::max(peak_entries, presentation->entries.size());
    }
  }

  EXPECT_LE(peak_entries,
            Engine::Core::CommanderSignaturePresentationComponent::k_max_entries);
}

TEST_F(CommanderDuelTest, SweepSignatureCatchesASecondFighter) {
  Engine::Core::World world;
  Game::Systems::register_runtime_systems(world);

  auto* commander = spawn(world,
                          Game::Units::SpawnType::CarthageSpearCommander,
                          1,
                          QVector3D(-3.0F, 0.0F, 0.0F),
                          Game::Systems::NationID::Carthage);
  auto* rival = spawn(world,
                      Game::Units::SpawnType::Knight,
                      2,
                      QVector3D(0.0F, 0.0F, 0.0F),
                      Game::Systems::NationID::RomanRepublic);
  auto* bystander = spawn(world,
                          Game::Units::SpawnType::Knight,
                          2,
                          QVector3D(-0.2F, 0.0F, 1.2F),
                          Game::Systems::NationID::RomanRepublic);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(rival, nullptr);
  ASSERT_NE(bystander, nullptr);

  auto* bystander_unit = bystander->get_component<UnitComponent>();
  ASSERT_NE(bystander_unit, nullptr);
  const int bystander_health_before = bystander_unit->health;

  order_attack(*commander, *rival);

  for (int tick = 0; tick < 600; ++tick) {
    world.update(0.05F);
    if (bystander_unit->health < bystander_health_before) {
      break;
    }
  }

  EXPECT_LT(bystander_unit->health, bystander_health_before)
      << "a sweeping signature has to catch the fighters crowding the commander";
}
