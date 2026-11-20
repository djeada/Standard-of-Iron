#include <cmath>
#include <gtest/gtest.h>

#include "app/commander/commander_status_builder.h"
#include "game/core/component_commander.h"
#include "game/core/world.h"

namespace {

auto make_commander(Engine::Core::World& world) -> Engine::Core::Entity* {
  auto* entity = world.create_entity();
  if (entity == nullptr) {
    return nullptr;
  }
  entity->add_component<Engine::Core::TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* unit = entity->add_component<Engine::Core::UnitComponent>();
  auto* commander = entity->add_component<Engine::Core::CommanderComponent>();
  if (unit == nullptr || commander == nullptr) {
    return nullptr;
  }
  unit->health = 500;
  unit->max_health = 500;
  unit->owner_id = 1;
  unit->spawn_type = Game::Units::SpawnType::Swordsman;
  commander->fpv_controlled = true;
  return entity;
}

// The engagement ring's own measure of where an attacker stands: the angle
// from the commander's facing to the attacker, growing toward +yaw.
auto ring_angle_degrees(float facing_x, float facing_z, float to_x, float to_z)
    -> float {
  float const length = std::hypot(to_x, to_z);
  to_x /= length;
  to_z /= length;
  float const dot = (facing_x * to_x) + (facing_z * to_z);
  float const cross = (facing_z * to_x) - (facing_x * to_z);
  return std::atan2(cross, dot) * 57.29577951308232F;
}

auto status_with_attacker_at(float x, float z) -> QVariantMap {
  Engine::Core::World world;
  auto* commander = make_commander(world);
  EXPECT_NE(commander, nullptr);
  auto* engagement =
      commander->add_component<Engine::Core::RpgEngagementComponent>();
  EXPECT_NE(engagement, nullptr);
  engagement->fight_context = Engine::Core::FightContext::Skirmish;
  Engine::Core::RpgEngagementComponent::Slot slot;
  slot.entity_id = 99;
  slot.distance = std::hypot(x, z);
  // The commander faces +z.
  slot.signed_angle_degrees = ring_angle_degrees(0.0F, 1.0F, x, z);
  slot.pressing = true;
  engagement->engagement_slots.push_back(slot);

  App::Core::CommanderStatusInput input;
  input.world = &world;
  input.controlled_commander_id = commander->get_id();
  return App::Core::build_controlled_commander_status(input);
}

} // namespace

// Facing +z in a right-handed world, the commander's right hand points at -x.
// The warning pips used to be wired the other way round: an attacker closing
// from the right lit the left edge of the screen.
TEST(CommanderStatusBuilder, AnAttackerOnTheRightLightsTheRightHandPip) {
  auto const status = status_with_attacker_at(-3.0F, 1.5F);
  ASSERT_TRUE(status.value("has_commander").toBool());
  EXPECT_TRUE(status.value("threat_right").toBool());
  EXPECT_FALSE(status.value("threat_left").toBool());
}

TEST(CommanderStatusBuilder, AnAttackerOnTheLeftLightsTheLeftHandPip) {
  auto const status = status_with_attacker_at(3.0F, 1.5F);
  ASSERT_TRUE(status.value("has_commander").toBool());
  EXPECT_TRUE(status.value("threat_left").toBool());
  EXPECT_FALSE(status.value("threat_right").toBool());
}

TEST(CommanderStatusBuilder, AnAttackerDeadAheadLightsNeitherPip) {
  auto const status = status_with_attacker_at(0.2F, 3.0F);
  EXPECT_FALSE(status.value("threat_left").toBool());
  EXPECT_FALSE(status.value("threat_right").toBool());
}
