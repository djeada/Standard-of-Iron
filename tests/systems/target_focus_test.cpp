#include <algorithm>
#include <gtest/gtest.h>
#include <memory>

#include "core/component_combat.h"
#include "core/entity.h"
#include "core/world.h"
#include "systems/owner_registry.h"
#include "systems/target_focus.h"

using namespace Engine::Core;
using namespace Game::Systems;

namespace {

constexpr int k_local_owner = 1;
constexpr int k_enemy_owner = 2;
constexpr int k_allied_owner = 3;

auto spawn_unit(World& world, int owner_id, float x, float z, int health = 100)
    -> Entity* {
  auto* entity = world.create_entity();
  entity->add_component<TransformComponent>(x, 0.0F, z);
  auto* unit = entity->add_component<UnitComponent>(health, 100, 1.0F, 12.0F);
  unit->owner_id = owner_id;
  return entity;
}

void attack(Entity* attacker, EntityID target) {
  attacker->add_component<AttackTargetComponent>()->target_id = target;
}

auto find_marker(const std::vector<TargetFocusMarker>& markers,
                 EntityID id) -> const TargetFocusMarker* {
  auto it = std::find_if(markers.begin(), markers.end(), [id](const auto& marker) {
    return marker.entity_id == id;
  });
  return it != markers.end() ? &*it : nullptr;
}

auto count_role(const std::vector<TargetFocusMarker>& markers,
                TargetFocusRole role) -> std::size_t {
  return static_cast<std::size_t>(
      std::count_if(markers.begin(), markers.end(), [role](const auto& m) {
        return m.role == role;
      }));
}

} // namespace

class TargetFocusTest : public ::testing::Test {
protected:
  void SetUp() override {
    world = std::make_unique<World>();
    auto& owners = OwnerRegistry::instance();
    owners.clear();
    owners.register_owner_with_id(k_local_owner, OwnerType::Player, "local");
    owners.register_owner_with_id(k_enemy_owner, OwnerType::AI, "enemy");
    owners.register_owner_with_id(k_allied_owner, OwnerType::AI, "friend");
    owners.set_local_player_id(k_local_owner);
    owners.set_owner_team(k_local_owner, 1);
    owners.set_owner_team(k_allied_owner, 1);
    owners.set_owner_team(k_enemy_owner, 2);
  }

  void TearDown() override {
    world.reset();
    OwnerRegistry::instance().clear();
  }

  auto request_for(const std::vector<EntityID>& selection,
                   EntityID inspected = 0) const -> TargetFocusRequest {
    TargetFocusRequest request;
    request.world = world.get();
    request.local_owner_id = k_local_owner;
    request.selection = &selection;
    request.inspected = inspected;
    request.owners = &OwnerRegistry::instance();
    return request;
  }

  std::unique_ptr<World> world;
};

TEST_F(TargetFocusTest, TargetsOfTheSelectionGetLockedRingsWeightedByAttackers) {
  auto* a = spawn_unit(*world, k_local_owner, 0.0F, 0.0F);
  auto* b = spawn_unit(*world, k_local_owner, 1.0F, 0.0F);
  auto* enemy = spawn_unit(*world, k_enemy_owner, 5.0F, 0.0F);
  attack(a, enemy->get_id());
  attack(b, enemy->get_id());
  const std::vector<EntityID> selection{a->get_id(), b->get_id()};

  const auto markers = collect_target_focus_markers(request_for(selection));

  ASSERT_EQ(markers.size(), 1U);
  EXPECT_EQ(markers.front().role, TargetFocusRole::LockedTarget);
  EXPECT_EQ(markers.front().entity_id, enemy->get_id());
  EXPECT_TRUE(markers.front().hostile);
  EXPECT_EQ(markers.front().weight, 2);
  EXPECT_FLOAT_EQ(markers.front().world_x, 5.0F);
}

TEST_F(TargetFocusTest, EnemiesAttackingTheSelectionAreMarkedAsIncoming) {
  auto* mine = spawn_unit(*world, k_local_owner, 0.0F, 0.0F);
  auto* enemy = spawn_unit(*world, k_enemy_owner, 5.0F, 0.0F);
  auto* other_enemy = spawn_unit(*world, k_enemy_owner, 6.0F, 0.0F);
  auto* unselected = spawn_unit(*world, k_local_owner, 2.0F, 0.0F);
  attack(enemy, mine->get_id());
  attack(other_enemy, unselected->get_id());
  const std::vector<EntityID> selection{mine->get_id()};

  const auto markers = collect_target_focus_markers(request_for(selection));

  ASSERT_EQ(markers.size(), 1U);
  EXPECT_EQ(markers.front().role, TargetFocusRole::IncomingAttacker);
  EXPECT_EQ(markers.front().entity_id, enemy->get_id());
}

TEST_F(TargetFocusTest, AnEnemyBothLockedAndIncomingIsReportedOnceAsLocked) {
  auto* mine = spawn_unit(*world, k_local_owner, 0.0F, 0.0F);
  auto* enemy = spawn_unit(*world, k_enemy_owner, 5.0F, 0.0F);
  attack(mine, enemy->get_id());
  attack(enemy, mine->get_id());
  const std::vector<EntityID> selection{mine->get_id()};

  const auto markers = collect_target_focus_markers(request_for(selection));

  ASSERT_EQ(markers.size(), 1U);
  EXPECT_EQ(markers.front().role, TargetFocusRole::LockedTarget);
}

TEST_F(TargetFocusTest, TheInspectedEntityGetsItsOwnRingAndAlliesAreNotHostile) {
  auto* ally = spawn_unit(*world, k_allied_owner, 3.0F, 0.0F);
  const std::vector<EntityID> selection;

  const auto markers =
      collect_target_focus_markers(request_for(selection, ally->get_id()));

  ASSERT_EQ(markers.size(), 1U);
  EXPECT_EQ(markers.front().role, TargetFocusRole::Inspected);
  EXPECT_FALSE(markers.front().hostile);
}

TEST_F(TargetFocusTest, DeadAndMissingEntitiesProduceNoMarkers) {
  auto* mine = spawn_unit(*world, k_local_owner, 0.0F, 0.0F);
  auto* corpse = spawn_unit(*world, k_enemy_owner, 5.0F, 0.0F, 0);
  attack(mine, corpse->get_id());
  const std::vector<EntityID> selection{mine->get_id()};

  EXPECT_TRUE(collect_target_focus_markers(request_for(selection, 9999)).empty());
}

TEST_F(TargetFocusTest, CapsKeepLargeBattlesReadable) {
  std::vector<EntityID> selection;
  for (int i = 0; i < 20; ++i) {
    auto* mine = spawn_unit(*world, k_local_owner, static_cast<float>(i), 0.0F);
    auto* enemy = spawn_unit(*world, k_enemy_owner, static_cast<float>(i), 10.0F);
    auto* raider = spawn_unit(*world, k_enemy_owner, static_cast<float>(i), 20.0F);
    attack(mine, enemy->get_id());
    attack(raider, mine->get_id());
    selection.push_back(mine->get_id());
  }
  auto request = request_for(selection);
  request.max_locked_targets = 4;
  request.max_incoming_attackers = 6;

  const auto markers = collect_target_focus_markers(request);

  EXPECT_EQ(count_role(markers, TargetFocusRole::LockedTarget), 4U);
  EXPECT_EQ(count_role(markers, TargetFocusRole::IncomingAttacker), 6U);
  EXPECT_EQ(find_marker(markers, 0), nullptr);
}

TEST_F(TargetFocusTest, SelectedForeignUnitsDoNotProjectLocks) {
  auto* enemy = spawn_unit(*world, k_enemy_owner, 5.0F, 0.0F);
  auto* mine = spawn_unit(*world, k_local_owner, 0.0F, 0.0F);
  attack(enemy, mine->get_id());
  const std::vector<EntityID> selection{enemy->get_id()};

  EXPECT_TRUE(collect_target_focus_markers(request_for(selection)).empty())
      << "only the local player's selection drives lock and incoming rings";
}

TEST_F(TargetFocusTest, DefaultCapsAreBounded) {
  EXPECT_LE(k_target_focus_max_locked, 16U);
  EXPECT_LE(k_target_focus_max_incoming, 24U);
}
