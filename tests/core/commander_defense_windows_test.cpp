#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <vector>

#include "app/commander/commander_control_controller.h"
#include "app/core/player_feedback.h"
#include "game/core/component_commander.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/combat_actions/commander_defense_timeline.h"
#include "game/systems/nav_grid.h"
#include "game/systems/rpg_combat_system/rpg_commander_damage.h"
#include "game/systems/rpg_combat_system/rpg_targeting.h"

namespace {

using Game::Systems::CombatActions::k_commander_dodge_timeline;
using Game::Systems::CombatActions::k_commander_guard_timeline;

class CommanderDefenseWindowsTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::NavGrid::initialize(32, 32);
  }

  void TearDown() override {
    Game::Map::TerrainService::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
  }

  static auto make_unit(Engine::Core::World& world,
                        float x,
                        float z,
                        int owner) -> Engine::Core::Entity* {
    auto* entity = world.create_entity();
    if (entity == nullptr) {
      return nullptr;
    }
    auto* transform =
        entity->add_component<Engine::Core::TransformComponent>(x, 0.0F, z);
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    entity->add_component<Engine::Core::MovementComponent>();
    if (transform == nullptr || unit == nullptr) {
      return nullptr;
    }
    unit->health = 1000;
    unit->max_health = 1000;
    unit->owner_id = owner;
    unit->speed = 3.0F;
    unit->spawn_type = Game::Units::SpawnType::Knight;
    return entity;
  }

  static auto make_commander(Engine::Core::World& world) -> Engine::Core::Entity* {
    auto* entity = make_unit(world, 0.0F, 0.0F, 1);
    if (entity == nullptr) {
      return nullptr;
    }
    auto* commander = entity->add_component<Engine::Core::CommanderComponent>();
    auto* rpg = entity->add_component<Engine::Core::RpgHealthComponent>();
    if (commander == nullptr || rpg == nullptr) {
      return nullptr;
    }
    commander->fpv_controlled = true;
    rpg->active = true;
    rpg->crit_chance = 0.0F;
    return entity;
  }

  struct ContactResult {
    bool blocked{false};
    bool perfect{false};
    bool dodged{false};
    int damage{0};
  };

  static auto body_point(Engine::Core::Entity& commander,
                         float forward_offset) -> QVector3D {
    auto const* transform = commander.get_component<Engine::Core::TransformComponent>();
    QVector3D const origin = transform != nullptr ? QVector3D(transform->position.x,
                                                              transform->position.y,
                                                              transform->position.z)
                                                  : QVector3D();
    return origin + QVector3D(0.0F,
                              Game::Systems::RpgCombat::k_hurt_body_chest_height,
                              forward_offset);
  }

  static auto strike_the_commander(Engine::Core::World& world,
                                   Engine::Core::Entity& commander,
                                   Engine::Core::Entity& attacker) -> ContactResult {

    auto const result = Game::Systems::RpgCombat::deal_damage_to_rpg_commander(
        &world, &commander, 24, attacker.get_id(), {}, body_point(commander, 0.34F));
    return {.blocked = result.blocked,
            .perfect = result.perfect_guarded,
            .dodged = result.dodged,
            .damage = result.effective_damage};
  }
};

TEST_F(CommanderDefenseWindowsTest, PerfectGuardOnlyResolvesInsideTheAuthoredWindow) {
  constexpr float k_dt = 1.0F / 60.0F;
  auto const window = k_commander_guard_timeline.perfect_window_seconds;

  struct Case {
    const char* name;
    float delay_seconds;
    bool expect_perfect;
  };
  const std::vector<Case> cases{
      {"one tick inside the window", window - k_dt, true},
      {"one tick after the window", window + k_dt, false},
      {"well after the window", window + 0.40F, false},
  };

  for (auto const& scenario : cases) {
    Engine::Core::World world;
    auto* commander = make_commander(world);
    auto* attacker = make_unit(world, 0.0F, 1.6F, 2);
    ASSERT_NE(commander, nullptr) << scenario.name;
    ASSERT_NE(attacker, nullptr) << scenario.name;

    CommanderControlController controller;
    controller.set_view_yaw(0.0F);
    controller.secondary_action_down();
    ASSERT_TRUE(controller.update_simulation(world, commander->get_id(), 1, k_dt))
        << scenario.name;

    auto const ticks = static_cast<int>(std::lround(scenario.delay_seconds / k_dt));
    for (int tick = 0; tick < ticks; ++tick) {
      ASSERT_TRUE(controller.update_simulation(world, commander->get_id(), 1, k_dt))
          << scenario.name;
    }

    auto const contact = strike_the_commander(world, *commander, *attacker);
    EXPECT_EQ(contact.perfect, scenario.expect_perfect) << scenario.name;

    EXPECT_TRUE(contact.blocked) << scenario.name;
    EXPECT_EQ(contact.damage, 0) << scenario.name;

    auto const* rpg = commander->get_component<Engine::Core::RpgHealthComponent>();
    ASSERT_NE(rpg, nullptr) << scenario.name;
    EXPECT_EQ(rpg->perfect_guard_contacts, scenario.expect_perfect ? 1U : 0U)
        << scenario.name;
    EXPECT_EQ(rpg->blocked_contacts, scenario.expect_perfect ? 0U : 1U)
        << scenario.name;
  }
}

TEST_F(CommanderDefenseWindowsTest, AGuardedCommanderTakesNoDamageFromTheFront) {
  constexpr float k_dt = 1.0F / 60.0F;
  Engine::Core::World world;
  auto* commander = make_commander(world);
  auto* attacker = make_unit(world, 0.0F, 1.6F, 2);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(attacker, nullptr);

  CommanderControlController controller;
  controller.set_view_yaw(0.0F);
  controller.secondary_action_down();
  for (int tick = 0; tick < 40; ++tick) {
    ASSERT_TRUE(controller.update_simulation(world, commander->get_id(), 1, k_dt));
  }

  auto const contact = strike_the_commander(world, *commander, *attacker);
  EXPECT_TRUE(contact.blocked);
  EXPECT_FALSE(contact.perfect);
  EXPECT_EQ(contact.damage, 0);
}

TEST_F(CommanderDefenseWindowsTest, AGuardDoesNotCoverTheCommandersBack) {
  constexpr float k_dt = 1.0F / 60.0F;
  Engine::Core::World world;
  auto* commander = make_commander(world);
  auto* attacker = make_unit(world, 0.0F, -1.6F, 2);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(attacker, nullptr);

  CommanderControlController controller;
  controller.set_view_yaw(0.0F);
  controller.secondary_action_down();
  for (int tick = 0; tick < 40; ++tick) {
    ASSERT_TRUE(controller.update_simulation(world, commander->get_id(), 1, k_dt));
  }

  auto const result = Game::Systems::RpgCombat::deal_damage_to_rpg_commander(
      &world, commander, 24, attacker->get_id(), {}, body_point(*commander, -0.34F));
  EXPECT_FALSE(result.blocked);
  EXPECT_GT(result.effective_damage, 0);
}

TEST_F(CommanderDefenseWindowsTest, DodgeIFramesRejectOnlyContactsInsideTheirWindow) {
  const std::vector<float> rates{30.0F, 60.0F, 120.0F};
  auto const invulnerable = k_commander_dodge_timeline.invulnerable_end_seconds;

  for (float const rate : rates) {
    float const dt = 1.0F / rate;

    struct Case {
      const char* name;
      float delay_seconds;
      bool expect_dodged;
    };
    const std::vector<Case> cases{
        {"one tick inside the i-frames", invulnerable - (2.0F * dt), true},
        {"one tick after the i-frames", invulnerable + dt, false},
    };

    for (auto const& scenario : cases) {
      Engine::Core::World world;
      auto* commander = make_commander(world);
      auto* attacker = make_unit(world, 0.0F, 1.6F, 2);
      ASSERT_NE(commander, nullptr) << scenario.name << " at " << rate << " Hz";
      ASSERT_NE(attacker, nullptr) << scenario.name << " at " << rate << " Hz";

      CommanderControlController controller;
      controller.set_view_yaw(0.0F);

      controller.request_dodge(QVector3D(0.0F, 0.0F, -1.0F));
      ASSERT_TRUE(controller.update_simulation(world, commander->get_id(), 1, dt))
          << scenario.name << " at " << rate << " Hz";

      auto const ticks = static_cast<int>(std::floor(scenario.delay_seconds / dt));
      for (int tick = 0; tick < ticks; ++tick) {
        ASSERT_TRUE(controller.update_simulation(world, commander->get_id(), 1, dt))
            << scenario.name << " at " << rate << " Hz";
      }

      auto const contact = strike_the_commander(world, *commander, *attacker);
      EXPECT_EQ(contact.dodged, scenario.expect_dodged)
          << scenario.name << " at " << rate << " Hz";

      auto const* rpg = commander->get_component<Engine::Core::RpgHealthComponent>();
      ASSERT_NE(rpg, nullptr) << scenario.name << " at " << rate << " Hz";
      EXPECT_EQ(rpg->dodged_contacts, scenario.expect_dodged ? 1U : 0U)
          << scenario.name << " at " << rate << " Hz";
    }
  }
}

TEST_F(CommanderDefenseWindowsTest, DodgeInvulnerabilityLastsTheSameTimeAtEveryRate) {
  const std::vector<float> rates{30.0F, 60.0F, 120.0F};
  for (float const rate : rates) {
    float const dt = 1.0F / rate;
    Engine::Core::World world;
    auto* commander = make_commander(world);
    ASSERT_NE(commander, nullptr) << rate;

    CommanderControlController controller;
    controller.set_view_yaw(0.0F);
    controller.request_dodge(QVector3D(0.0F, 0.0F, -1.0F));

    float invulnerable_for = 0.0F;
    for (int tick = 0; tick < static_cast<int>(rate); ++tick) {
      ASSERT_TRUE(controller.update_simulation(world, commander->get_id(), 1, dt))
          << rate;
      auto const* rpg = commander->get_component<Engine::Core::RpgHealthComponent>();
      ASSERT_NE(rpg, nullptr) << rate;
      if (rpg->dodge_grace_remaining > 0.0F) {
        invulnerable_for += dt;
      }
    }

    EXPECT_NEAR(invulnerable_for,
                k_commander_dodge_timeline.invulnerable_seconds(),
                (2.0F * dt) + 1.0e-4F)
        << rate;
  }
}

TEST_F(CommanderDefenseWindowsTest, DefenceFeedbackFollowsAResolvedContactNotARequest) {
  constexpr float k_dt = 1.0F / 60.0F;
  Engine::Core::World world;
  auto* commander = make_commander(world);
  auto* attacker = make_unit(world, 0.0F, 1.6F, 2);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(attacker, nullptr);

  App::Core::PlayerFeedbackBus bus;
  CommanderControlController controller;
  controller.set_feedback_bus(&bus);
  controller.set_view_yaw(0.0F);

  controller.request_dodge(QVector3D(0.0F, 0.0F, -1.0F));
  ASSERT_TRUE(controller.update_simulation(world, commander->get_id(), 1, k_dt));

  auto const requested = bus.drain();
  EXPECT_EQ(std::count_if(requested.begin(),
                          requested.end(),
                          [](auto const& event) {
                            return event.type ==
                                   App::Core::PlayerFeedbackType::DodgeSuccess;
                          }),
            0)
      << "requesting a dodge is not a successful dodge";

  auto const contact = strike_the_commander(world, *commander, *attacker);
  ASSERT_TRUE(contact.dodged);
  ASSERT_TRUE(controller.update_simulation(world, commander->get_id(), 1, k_dt));

  auto const resolved = bus.drain();
  EXPECT_EQ(std::count_if(resolved.begin(),
                          resolved.end(),
                          [](auto const& event) {
                            return event.type ==
                                   App::Core::PlayerFeedbackType::DodgeSuccess;
                          }),
            1)
      << "a contact rejected by i-frames must publish exactly one DodgeSuccess";
}

TEST_F(CommanderDefenseWindowsTest, GuardFeedbackWaitsForAContactToBlock) {
  constexpr float k_dt = 1.0F / 60.0F;
  Engine::Core::World world;
  auto* commander = make_commander(world);
  auto* attacker = make_unit(world, 0.0F, 1.6F, 2);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(attacker, nullptr);

  App::Core::PlayerFeedbackBus bus;
  CommanderControlController controller;
  controller.set_feedback_bus(&bus);
  controller.set_view_yaw(0.0F);
  controller.secondary_action_down();
  ASSERT_TRUE(controller.update_simulation(world, commander->get_id(), 1, k_dt));

  auto const raised = bus.drain();
  EXPECT_EQ(std::count_if(raised.begin(),
                          raised.end(),
                          [](auto const& event) {
                            return event.type ==
                                   App::Core::PlayerFeedbackType::PerfectGuard;
                          }),
            0)
      << "raising the guard is not a perfect guard";

  auto const contact = strike_the_commander(world, *commander, *attacker);
  ASSERT_TRUE(contact.perfect);
  ASSERT_TRUE(controller.update_simulation(world, commander->get_id(), 1, k_dt));

  auto const resolved = bus.drain();
  EXPECT_EQ(std::count_if(resolved.begin(),
                          resolved.end(),
                          [](auto const& event) {
                            return event.type ==
                                   App::Core::PlayerFeedbackType::PerfectGuard;
                          }),
            1)
      << "a contact resolved as perfect must publish exactly one PerfectGuard";
}

TEST_F(CommanderDefenseWindowsTest, SimultaneousContactsEachResolveOnTheirOwnMerits) {
  constexpr float k_dt = 1.0F / 60.0F;
  Engine::Core::World world;
  auto* commander = make_commander(world);
  auto* front = make_unit(world, 0.0F, 1.6F, 2);
  auto* behind = make_unit(world, 0.0F, -1.6F, 2);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(front, nullptr);
  ASSERT_NE(behind, nullptr);

  CommanderControlController controller;
  controller.set_view_yaw(0.0F);
  controller.secondary_action_down();
  for (int tick = 0; tick < 40; ++tick) {
    ASSERT_TRUE(controller.update_simulation(world, commander->get_id(), 1, k_dt));
  }

  auto const front_contact = strike_the_commander(world, *commander, *front);
  auto const back_contact = Game::Systems::RpgCombat::deal_damage_to_rpg_commander(
      &world, commander, 24, behind->get_id(), {}, body_point(*commander, -0.34F));

  EXPECT_TRUE(front_contact.blocked) << "the guarded side blocks";
  EXPECT_EQ(front_contact.damage, 0);
  EXPECT_FALSE(back_contact.blocked) << "the unguarded side does not";
  EXPECT_GT(back_contact.effective_damage, 0);

  auto const* rpg = commander->get_component<Engine::Core::RpgHealthComponent>();
  ASSERT_NE(rpg, nullptr);
  EXPECT_EQ(rpg->blocked_contacts, 1U);
  EXPECT_EQ(rpg->damaging_contacts, 1U);
}

TEST_F(CommanderDefenseWindowsTest, AnUnblockableContactIsNotCountedAsABlock) {
  constexpr float k_dt = 1.0F / 60.0F;
  Engine::Core::World world;
  auto* commander = make_commander(world);
  auto* attacker = make_unit(world, 0.0F, 1.6F, 2);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(attacker, nullptr);

  CommanderControlController controller;
  controller.set_view_yaw(0.0F);
  controller.secondary_action_down();
  for (int tick = 0; tick < 40; ++tick) {
    ASSERT_TRUE(controller.update_simulation(world, commander->get_id(), 1, k_dt));
  }

  auto const result = Game::Systems::RpgCombat::deal_damage_to_rpg_commander(
      &world,
      commander,
      24,
      attacker->get_id(),
      {.unblockable = true},
      body_point(*commander, 0.34F));

  EXPECT_FALSE(result.blocked);
  EXPECT_GT(result.effective_damage, 0);

  auto const* rpg = commander->get_component<Engine::Core::RpgHealthComponent>();
  ASSERT_NE(rpg, nullptr);
  EXPECT_EQ(rpg->blocked_contacts, 0U);
  EXPECT_EQ(rpg->damaging_contacts, 1U);
}

TEST_F(CommanderDefenseWindowsTest, ADodgeRejectsOnlyTheContactItRolledAwayFrom) {
  constexpr float k_dt = 1.0F / 60.0F;
  Engine::Core::World world;
  auto* commander = make_commander(world);
  auto* front = make_unit(world, 0.0F, 1.6F, 2);
  auto* behind = make_unit(world, 0.0F, -1.6F, 2);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(front, nullptr);
  ASSERT_NE(behind, nullptr);

  CommanderControlController controller;
  controller.set_view_yaw(0.0F);

  controller.request_dodge(QVector3D(0.0F, 0.0F, -1.0F));
  ASSERT_TRUE(controller.update_simulation(world, commander->get_id(), 1, k_dt));

  auto const rolled_from = strike_the_commander(world, *commander, *front);
  EXPECT_TRUE(rolled_from.dodged)
      << "the roll carries the commander away from this one";

  auto const rolled_into = Game::Systems::RpgCombat::deal_damage_to_rpg_commander(
      &world, commander, 24, behind->get_id(), {}, body_point(*commander, -0.34F));
  EXPECT_FALSE(rolled_into.dodged)
      << "i-frames are directional: rolling into a blow does not avoid it";
}

TEST_F(CommanderDefenseWindowsTest, EveryResolvedContactIsCountedExactlyOnce) {
  constexpr float k_dt = 1.0F / 60.0F;
  Engine::Core::World world;
  auto* commander = make_commander(world);
  auto* attacker = make_unit(world, 0.0F, 1.6F, 2);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(attacker, nullptr);

  CommanderControlController controller;
  controller.set_view_yaw(0.0F);
  ASSERT_TRUE(controller.update_simulation(world, commander->get_id(), 1, k_dt));

  auto const first = strike_the_commander(world, *commander, *attacker);
  EXPECT_GT(first.damage, 0);
  auto const second = strike_the_commander(world, *commander, *attacker);
  EXPECT_GT(second.damage, 0);

  auto const* rpg = commander->get_component<Engine::Core::RpgHealthComponent>();
  ASSERT_NE(rpg, nullptr);
  EXPECT_EQ(rpg->damaging_contacts, 2U);
  EXPECT_EQ(rpg->blocked_contacts, 0U);
  EXPECT_EQ(rpg->dodged_contacts, 0U);
  EXPECT_EQ(rpg->perfect_guard_contacts, 0U);
  EXPECT_EQ(rpg->last_contact_outcome, Engine::Core::RpgContactOutcome::Damage);
}

} // namespace
