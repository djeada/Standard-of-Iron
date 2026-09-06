#include <gtest/gtest.h>
#include <memory>

#include "game/core/component_gameplay.h"
#include "game/core/world.h"
#include "game/session/session_context.h"
#include "game/systems/capture_system.h"
#include "game/systems/owner_registry.h"

namespace {

constexpr int k_defender = 1;
constexpr int k_attacker = 2;
constexpr int k_bystander = 3;

class CaptureSystemTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_session = std::make_unique<Game::Session::SessionContext>();
    auto& owners = m_session->owners();
    owners.register_owner_with_id(
        k_defender, Game::Systems::OwnerType::Player, "Defender");
    owners.register_owner_with_id(k_attacker, Game::Systems::OwnerType::AI, "Attacker");
    owners.register_owner_with_id(
        k_bystander, Game::Systems::OwnerType::AI, "Bystander");
    owners.set_owner_team(k_defender, 1);
    owners.set_owner_team(k_attacker, 2);
    owners.set_owner_team(k_bystander, 3);
  }

  [[nodiscard]] auto world() -> Engine::Core::World& { return m_session->world(); }

  auto add_barracks(int owner_id, float x, float z) -> Engine::Core::Entity* {
    auto* entity = world().create_entity();
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    unit->owner_id = owner_id;
    unit->spawn_type = Game::Units::SpawnType::Barracks;
    unit->health = 500;
    unit->max_health = 500;
    entity->add_component<Engine::Core::TransformComponent>(x, 0.0F, z);
    entity->add_component<Engine::Core::BuildingComponent>();
    return entity;
  }

  auto add_troop(int owner_id, float x, float z) -> Engine::Core::Entity* {
    auto* entity = world().create_entity();
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    unit->owner_id = owner_id;
    unit->spawn_type = Game::Units::SpawnType::Spearman;
    unit->health = 100;
    unit->max_health = 100;
    entity->add_component<Engine::Core::TransformComponent>(x, 0.0F, z);
    return entity;
  }

  std::unique_ptr<Game::Session::SessionContext> m_session;
  Game::Systems::CaptureSystem m_system;
};

TEST_F(CaptureSystemTest, TheStrongestBesiegerIsTheOneCapturing) {
  auto* barracks = add_barracks(k_defender, 0.0F, 0.0F);
  for (int i = 0; i < 4; ++i) {
    add_troop(k_attacker, 1.0F + static_cast<float>(i), 0.0F);
  }
  add_troop(k_bystander, 2.0F, 1.0F);

  m_system.update(&world(), 0.1F);

  auto* capture = barracks->get_component<Engine::Core::CaptureComponent>();
  ASSERT_NE(capture, nullptr);
  EXPECT_TRUE(capture->is_being_captured);
  EXPECT_EQ(capture->capturing_player_id, k_attacker)
      << "the owner with the most troops in the ring takes the barracks";
}

TEST_F(CaptureSystemTest, DefendersInTheRingHoldTheBarracks) {
  auto* barracks = add_barracks(k_defender, 0.0F, 0.0F);
  for (int i = 0; i < 3; ++i) {
    add_troop(k_attacker, 1.0F + static_cast<float>(i), 0.0F);
    add_troop(k_defender, 0.0F, 1.0F + static_cast<float>(i));
  }

  m_system.update(&world(), 0.1F);

  auto* capture = barracks->get_component<Engine::Core::CaptureComponent>();
  ASSERT_NE(capture, nullptr);
  EXPECT_FALSE(capture->is_being_captured)
      << "an equal garrison must deny the three-to-one advantage a capture needs";
}

TEST_F(CaptureSystemTest, TroopsOutsideTheRingDoNotCount) {
  auto* barracks = add_barracks(k_defender, 0.0F, 0.0F);
  for (int i = 0; i < 6; ++i) {
    add_troop(k_attacker, 40.0F + static_cast<float>(i), 40.0F);
  }

  m_system.update(&world(), 0.1F);

  auto* capture = barracks->get_component<Engine::Core::CaptureComponent>();
  ASSERT_NE(capture, nullptr);
  EXPECT_FALSE(capture->is_being_captured);
}

TEST_F(CaptureSystemTest, EachBarracksIsJudgedByItsOwnNeighbourhood) {
  auto* near_barracks = add_barracks(k_defender, 0.0F, 0.0F);
  auto* far_barracks = add_barracks(k_defender, 100.0F, 100.0F);
  for (int i = 0; i < 4; ++i) {
    add_troop(k_attacker, 1.0F + static_cast<float>(i), 0.0F);
  }

  m_system.update(&world(), 0.1F);

  EXPECT_TRUE(near_barracks->get_component<Engine::Core::CaptureComponent>()
                  ->is_being_captured);
  EXPECT_FALSE(
      far_barracks->get_component<Engine::Core::CaptureComponent>()->is_being_captured)
      << "one pass over the units must still keep each barracks' ring separate";
}

} // namespace
