#include <QJsonObject>

#include <gtest/gtest.h>

#include "core/component.h"
#include "core/entity.h"
#include "core/world.h"
#include "map/terrain_service.h"
#include "save/serialization.h"
#include "units/spawn_type.h"

using namespace Engine::Core;

namespace {

class CommanderStateRoundTripTest : public ::testing::Test {
protected:
  void SetUp() override { world = std::make_unique<World>(); }

  void TearDown() override {
    Game::Map::TerrainService::instance().clear();
    world.reset();
  }

  auto make_commander() -> Entity* {
    auto* entity = world->create_entity();
    entity->add_component<TransformComponent>(3.0F, 0.0F, 4.0F);
    auto* unit = entity->add_component<UnitComponent>();
    unit->health = 80;
    unit->max_health = 100;
    unit->owner_id = 1;
    unit->spawn_type = Game::Units::SpawnType::Knight;
    auto* commander = entity->add_component<CommanderComponent>();
    commander->fpv_controlled = true;
    auto* rpg = entity->add_component<RpgHealthComponent>();
    rpg->active = true;
    rpg->armor = 3.0F;
    rpg->incoming_damage_scale = 0.5F;
    return entity;
  }

  auto round_trip(Entity* source) -> Entity* {
    QJsonObject const json = Serialization::serialize_entity(source);
    auto* restored = world->create_entity();
    Serialization::deserialize_entity(restored, json);
    return restored;
  }

  std::unique_ptr<World> world;
};

TEST_F(CommanderStateRoundTripTest, AnIdleCommanderKeepsItsDurableRpgState) {
  auto* commander = make_commander();
  auto* restored = round_trip(commander);

  auto const* rpg = restored->get_component<RpgHealthComponent>();
  ASSERT_NE(rpg, nullptr);
  EXPECT_FLOAT_EQ(rpg->armor, 3.0F);
  EXPECT_FLOAT_EQ(rpg->incoming_damage_scale, 0.5F);

  auto const* unit = restored->get_component<UnitComponent>();
  ASSERT_NE(unit, nullptr);
  EXPECT_EQ(unit->health, 80);
}

TEST_F(CommanderStateRoundTripTest, DirectControlIsNotRestoredFromASave) {
  auto* commander = make_commander();
  auto* restored = round_trip(commander);

  auto const* rpg = restored->get_component<RpgHealthComponent>();
  ASSERT_NE(rpg, nullptr);
  EXPECT_FALSE(rpg->active)
      << "direct control is entered by the player, never by loading a save";
}

TEST_F(CommanderStateRoundTripTest, TransientDodgeStateNormalizesOnLoad) {
  auto* commander = make_commander();
  auto* rpg = commander->get_component<RpgHealthComponent>();
  ASSERT_NE(rpg, nullptr);
  rpg->dodge_grace_remaining = 0.12F;
  rpg->dodge_dir_x = 1.0F;
  rpg->dodge_dir_z = -1.0F;
  rpg->dodged_contacts = 4U;
  rpg->blocked_contacts = 7U;

  auto const* restored = round_trip(commander)->get_component<RpgHealthComponent>();
  ASSERT_NE(restored, nullptr);
  EXPECT_FLOAT_EQ(restored->dodge_grace_remaining, 0.0F)
      << "a save must not restore mid-roll invulnerability";
  EXPECT_EQ(restored->dodged_contacts, 0U);
  EXPECT_EQ(restored->blocked_contacts, 0U);
}

TEST_F(CommanderStateRoundTripTest, TransientGuardStateNormalizesOnLoad) {
  auto* commander = make_commander();
  auto* guard = commander->add_component<CommanderGuardComponent>();
  guard->active = true;
  guard->perfect_guard_remaining = 0.16F;
  guard->guard_break_remaining = 0.9F;
  guard->rearm_requires_release = true;

  auto const* restored =
      round_trip(commander)->get_component<CommanderGuardComponent>();
  if (restored != nullptr) {
    EXPECT_FLOAT_EQ(restored->perfect_guard_remaining, 0.0F)
        << "a save must not restore an armed perfect-guard window";
  }
}

TEST_F(CommanderStateRoundTripTest, TransientActionStateNormalizesOnLoad) {
  auto* commander = make_commander();
  auto* action = commander->add_component<RpgCommanderActionComponent>();
  action->action_running = true;
  action->normalized_action_time = 0.5F;
  action->hit_target_count = 1U;

  auto* queue = commander->add_component<CombatIntentQueueComponent>();
  CombatActionIntent intent;
  intent.pressed_at = 0.25F;
  queue->push(intent);
  queue->record(CombatIntentOutcome::Buffered);

  auto* restored = round_trip(commander);
  auto const* restored_action = restored->get_component<RpgCommanderActionComponent>();
  if (restored_action != nullptr) {
    EXPECT_FALSE(restored_action->action_running)
        << "a save must not resume a half-finished swing";
    EXPECT_EQ(restored_action->hit_target_count, 0U);
  }
  auto const* restored_queue = restored->get_component<CombatIntentQueueComponent>();
  if (restored_queue != nullptr) {
    EXPECT_EQ(restored_queue->count, 0U) << "a buffered press does not survive a load";
  }
}

TEST_F(CommanderStateRoundTripTest, ADeadCommanderLoadsDead) {
  auto* commander = make_commander();
  auto* unit = commander->get_component<UnitComponent>();
  ASSERT_NE(unit, nullptr);
  unit->health = 0;

  auto const* restored = round_trip(commander)->get_component<UnitComponent>();
  ASSERT_NE(restored, nullptr);
  EXPECT_EQ(restored->health, 0);

  auto const* rpg = round_trip(commander)->get_component<RpgHealthComponent>();
  ASSERT_NE(rpg, nullptr);
  EXPECT_FALSE(rpg->active);
}

} // namespace
