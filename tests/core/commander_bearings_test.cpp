#include <algorithm>
#include <gtest/gtest.h>

#include "app/commander/commander_bearings.h"
#include "game/core/component_commander.h"
#include "game/core/world.h"
#include "game/systems/owner_registry.h"

using App::Core::CommanderBearing;

namespace {

class CommanderBearingsTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_owners.register_owner_with_id(1, Game::Systems::OwnerType::Player, "player");
    m_owners.register_owner_with_id(2, Game::Systems::OwnerType::AI, "enemy");
    m_owners.register_owner_with_id(3, Game::Systems::OwnerType::AI, "ally");
    m_owners.set_owner_team(1, 1);
    m_owners.set_owner_team(2, 2);
    m_owners.set_owner_team(3, 1);
  }

  auto make_unit(float x, float z, int owner) -> Engine::Core::Entity* {
    auto* entity = m_world.create_entity();
    entity->add_component<Engine::Core::TransformComponent>(x, 0.0F, z);
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    unit->health = 100;
    unit->max_health = 100;
    unit->owner_id = owner;
    unit->spawn_type = Game::Units::SpawnType::Swordsman;
    return entity;
  }

  auto make_commander(float x, float z, int owner) -> Engine::Core::Entity* {
    auto* entity = make_unit(x, z, owner);
    entity->add_component<Engine::Core::CommanderComponent>();
    return entity;
  }

  // The view looks along +z unless a test says otherwise.
  auto bearings(Engine::Core::EntityID commander, float view_yaw = 0.0F)
      -> std::vector<CommanderBearing> {
    m_world.spatial_index().rebuild(m_world);
    return App::Core::collect_commander_bearings({.world = &m_world,
                                                  .commander_id = commander,
                                                  .local_owner_id = 1,
                                                  .view_yaw_degrees = view_yaw,
                                                  .owners = &m_owners,
                                                  .visibility = nullptr});
  }

  static auto find(const std::vector<CommanderBearing>& all,
                   CommanderBearing::Kind kind) -> const CommanderBearing* {
    auto const it = std::find_if(
        all.begin(), all.end(), [kind](const CommanderBearing& bearing) {
          return bearing.kind == kind;
        });
    return it != all.end() ? &*it : nullptr;
  }

  Engine::Core::World m_world;
  Game::Systems::OwnerRegistry m_owners;
};

} // namespace

TEST_F(CommanderBearingsTest, AnEnemyOnThePlayersRightReadsToTheRight) {
  auto* commander = make_commander(0.0F, 0.0F, 1);
  // Facing +z in a right-handed world the right hand points at -x.
  make_unit(-10.0F, 10.0F, 2);

  auto const all = bearings(commander->get_id());
  auto const* enemy = find(all, CommanderBearing::Kind::Enemy);
  ASSERT_NE(enemy, nullptr);
  EXPECT_NEAR(enemy->right_degrees, 45.0F, 0.5F);
  EXPECT_NEAR(enemy->distance, 14.14F, 0.05F);
}

TEST_F(CommanderBearingsTest, TheBearingFollowsTheViewNotTheMap) {
  auto* commander = make_commander(0.0F, 0.0F, 1);
  make_unit(0.0F, 12.0F, 2);

  auto ahead = bearings(commander->get_id(), 0.0F);
  ASSERT_NE(find(ahead, CommanderBearing::Kind::Enemy), nullptr);
  EXPECT_NEAR(find(ahead, CommanderBearing::Kind::Enemy)->right_degrees, 0.0F, 0.5F);

  // The player has turned a quarter turn to the right (yaw falls): what was
  // ahead is now abeam on his left.
  auto turned = bearings(commander->get_id(), 270.0F);
  ASSERT_NE(find(turned, CommanderBearing::Kind::Enemy), nullptr);
  EXPECT_NEAR(find(turned, CommanderBearing::Kind::Enemy)->right_degrees, -90.0F, 0.5F);
}

TEST_F(CommanderBearingsTest, OnlyTheNearestEnemyTroopIsMarkedButEveryGeneralIs) {
  auto* commander = make_commander(0.0F, 0.0F, 1);
  make_unit(0.0F, 40.0F, 2);
  make_unit(3.0F, 9.0F, 2);
  auto* general = make_commander(20.0F, 30.0F, 2);
  general->get_component<Engine::Core::CommanderComponent>()->display_name =
      "Hannibal Barca";

  auto const all = bearings(commander->get_id());
  EXPECT_EQ(std::count_if(all.begin(),
                          all.end(),
                          [](const CommanderBearing& bearing) {
                            return bearing.kind == CommanderBearing::Kind::Enemy;
                          }),
            1);
  auto const* enemy = find(all, CommanderBearing::Kind::Enemy);
  ASSERT_NE(enemy, nullptr);
  EXPECT_NEAR(enemy->distance, 9.49F, 0.05F);

  auto const* chief = find(all, CommanderBearing::Kind::EnemyCommander);
  ASSERT_NE(chief, nullptr);
  EXPECT_FALSE(chief->label.isEmpty());
  EXPECT_LT(chief->right_degrees, 0.0F) << "+x is on the left of a view along +z";
}

TEST_F(CommanderBearingsTest, AlliesAndTheDeadAreNotEnemies) {
  auto* commander = make_commander(0.0F, 0.0F, 1);
  make_unit(0.0F, 8.0F, 3);
  auto* dead = make_unit(0.0F, 6.0F, 2);
  dead->get_component<Engine::Core::UnitComponent>()->health = 0;

  auto const all = bearings(commander->get_id());
  EXPECT_EQ(find(all, CommanderBearing::Kind::Enemy), nullptr);
  EXPECT_EQ(find(all, CommanderBearing::Kind::EnemyCommander), nullptr);
}

TEST_F(CommanderBearingsTest, TheArmyIsMarkedOnlyWhenTheCommanderHasLeftIt) {
  auto* commander = make_commander(0.0F, 0.0F, 1);
  make_unit(2.0F, 3.0F, 1);
  make_unit(-2.0F, 3.0F, 1);
  EXPECT_EQ(find(bearings(commander->get_id()), CommanderBearing::Kind::Army), nullptr)
      << "standing in his own ranks he does not need to be shown where they are";

  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  transform->position.z = -60.0F;
  auto const far = bearings(commander->get_id());
  auto const* army = find(far, CommanderBearing::Kind::Army);
  ASSERT_NE(army, nullptr);
  EXPECT_NEAR(army->right_degrees, 0.0F, 0.5F);
  EXPECT_NEAR(army->distance, 63.0F, 0.5F);
}

TEST_F(CommanderBearingsTest, APlantedRallyFlagIsMarked) {
  auto* commander = make_commander(0.0F, 0.0F, 1);
  auto* data = commander->get_component<Engine::Core::CommanderComponent>();
  EXPECT_EQ(find(bearings(commander->get_id()), CommanderBearing::Kind::RallyFlag),
            nullptr);

  data->flag_rally_flag_active = true;
  data->flag_rally_flag_x = 0.0F;
  data->flag_rally_flag_z = -25.0F;
  auto const all = bearings(commander->get_id());
  auto const* flag = find(all, CommanderBearing::Kind::RallyFlag);
  ASSERT_NE(flag, nullptr);
  EXPECT_NEAR(std::abs(flag->right_degrees), 180.0F, 0.5F)
      << "a flag straight behind him sits at the end of the strip";
}
