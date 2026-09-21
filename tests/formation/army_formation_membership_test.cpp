#include <algorithm>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "game/core/component_core.h"
#include "game/core/component_gameplay.h"
#include "game/core/world.h"
#include "game/formation/army_formation_registry.h"
#include "game/session/session_context.h"

namespace {

using Game::Formation::ArmyFormationIntent;
using Game::Formation::ArmyFormationRegistry;
using Game::Formation::ArmyFormationRuntime;

auto add_soldier(Engine::Core::World& world, float x) -> Engine::Core::EntityID {
  auto* entity = world.create_entity();
  auto* transform = entity->add_component<Engine::Core::TransformComponent>();
  transform->position = {x, 0.0F, 0.0F};
  auto* unit = entity->add_component<Engine::Core::UnitComponent>();
  unit->health = 100;
  unit->max_health = 100;
  unit->speed = 2.0F;
  return entity->get_id();
}

auto membership_group(Engine::Core::World& world,
                      Engine::Core::EntityID entity) -> std::uint32_t {
  auto* component =
      world.try_get<Engine::Core::ArmyFormationMembershipComponent>(entity);
  return component == nullptr ? 0U : component->group_id;
}

class ArmyFormationMembershipTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_session = std::make_unique<Game::Session::SessionContext>();
    m_scope = std::make_unique<Game::Session::ScopedSession>(*m_session);
  }

  void TearDown() override {
    m_scope.reset();
    m_session.reset();
  }

  [[nodiscard]] auto world() -> Engine::Core::World& { return m_session->world(); }
  [[nodiscard]] auto registry() -> ArmyFormationRegistry& {
    return ArmyFormationRegistry::for_world(world());
  }

  std::unique_ptr<Game::Session::SessionContext> m_session;
  std::unique_ptr<Game::Session::ScopedSession> m_scope;
};

TEST_F(ArmyFormationMembershipTest, DroppingAMemberClearsEveryRecordOfIt) {
  const auto a = add_soldier(world(), 0.0F);
  const auto b = add_soldier(world(), 2.0F);
  const auto c = add_soldier(world(), 4.0F);

  const auto group =
      registry().create_group("rome", ArmyFormationIntent::Line, {a, b, c});
  auto* formation = registry().find(group);
  ASSERT_NE(formation, nullptr);
  formation->slot_list.resize(3);
  for (std::size_t i = 0; i < 3; ++i) {
    formation->slot_list[i].id = static_cast<int>(i);
  }
  formation->slot_list[0].occupant = a;
  formation->slot_list[1].occupant = b;
  formation->slot_list[2].occupant = c;
  ArmyFormationRuntime::sync_membership_components(world(), *formation);

  ASSERT_EQ(registry().group_of(c), group);
  ASSERT_EQ(membership_group(world(), c), group);

  const auto dropped = registry().replace_members(group, {a, b});
  for (const auto member : dropped) {
    ArmyFormationRuntime::clear_membership_component(world(), member);
  }

  ASSERT_EQ(dropped.size(), 1U);
  EXPECT_EQ(dropped.front(), c);

  const auto* after = registry().find(group);
  ASSERT_NE(after, nullptr);
  EXPECT_FALSE(after->has_member(c)) << "the roster still lists the dropped troop";
  EXPECT_NE(registry().group_of(c), group)
      << "the reverse index still points at the group";
  EXPECT_EQ(membership_group(world(), c), 0U)
      << "the troop's own component still names the group it left";
  EXPECT_TRUE(std::none_of(after->slot_list.begin(),
                           after->slot_list.end(),
                           [c](const auto& slot) { return slot.occupant == c; }))
      << "the dropped troop still holds a slot";

  EXPECT_TRUE(after->has_member(a));
  EXPECT_TRUE(after->has_member(b));
  EXPECT_EQ(membership_group(world(), a), group);
}

TEST_F(ArmyFormationMembershipTest, ModifiersIgnoreAComponentTheRosterDisagreesWith) {
  const auto a = add_soldier(world(), 0.0F);
  const auto stranger = add_soldier(world(), 30.0F);

  const auto group = registry().create_group("rome", ArmyFormationIntent::Line, {a});
  auto* formation = registry().find(group);
  ASSERT_NE(formation, nullptr);
  formation->phase = Game::Formation::FormationPhase::Disrupted;

  auto* membership =
      world().emplace<Engine::Core::ArmyFormationMembershipComponent>(stranger);
  ASSERT_NE(membership, nullptr);
  membership->group_id = group;
  membership->slot_id = 0;

  auto* entity = world().get_entity(stranger);
  ASSERT_NE(entity, nullptr);
  EXPECT_FLOAT_EQ(ArmyFormationRuntime::damage_taken_multiplier(*entity), 1.0F)
      << "a troop outside the formation took its disruption penalty";
  EXPECT_FLOAT_EQ(ArmyFormationRuntime::move_speed_multiplier(*entity), 1.0F)
      << "a troop outside the formation was paced by it";
}

TEST_F(ArmyFormationMembershipTest, DisbandingAGroupClearsItsMembersComponents) {
  const auto a = add_soldier(world(), 0.0F);
  const auto b = add_soldier(world(), 2.0F);

  const auto group = registry().create_group("rome", ArmyFormationIntent::Line, {a, b});
  auto* formation = registry().find(group);
  ASSERT_NE(formation, nullptr);
  formation->slot_list.resize(2);
  formation->slot_list[0].occupant = a;
  formation->slot_list[1].occupant = b;
  ArmyFormationRuntime::sync_membership_components(world(), *formation);
  ASSERT_EQ(membership_group(world(), a), group);

  ArmyFormationRuntime::disband(world(), group);

  EXPECT_EQ(registry().find(group), nullptr);
  EXPECT_EQ(membership_group(world(), a), 0U);
  EXPECT_EQ(membership_group(world(), b), 0U);
  EXPECT_NE(registry().group_of(a), group);
}

} // namespace
