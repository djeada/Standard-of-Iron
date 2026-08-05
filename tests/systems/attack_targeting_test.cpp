#include <algorithm>
#include <gtest/gtest.h>

#include "core/component.h"
#include "core/entity.h"
#include "core/world.h"
#include "map/visibility_service.h"
#include "systems/attack_targeting.h"
#include "systems/owner_registry.h"

using namespace Engine::Core;
using namespace Game::Systems;

namespace {

constexpr int k_local_owner = 1;
constexpr int k_enemy_owner = 2;
constexpr int k_allied_owner = 3;

auto spawn_unit(World& world, int owner_id, float x, float z) -> Entity* {
  auto* entity = world.create_entity();
  entity->add_component<TransformComponent>(x, 0.0F, z);
  auto* unit = entity->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit->owner_id = owner_id;
  return entity;
}

auto contains_marker(const AttackTargetingHighlights& highlights,
                     EntityID entity_id) -> bool {
  return std::any_of(highlights.markers.begin(),
                     highlights.markers.end(),
                     [entity_id](const AttackTargetMarker& marker) {
                       return marker.entity_id == entity_id;
                     });
}

} // namespace

class AttackTargetingTest : public ::testing::Test {
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

  auto make_request() const -> AttackTargetingRequest {
    AttackTargetingRequest request;
    request.world = world.get();
    request.local_owner_id = k_local_owner;
    request.has_attackers = true;
    request.max_distance = k_attack_highlight_max_distance;
    request.max_markers = k_attack_highlight_max_markers;
    return request;
  }

  std::unique_ptr<World> world;
};

TEST_F(AttackTargetingTest, EnemyUnitsAndBuildingsAreHighlighted) {
  auto* enemy = spawn_unit(*world, k_enemy_owner, 4.0F, 0.0F);
  auto* enemy_building = spawn_unit(*world, k_enemy_owner, 6.0F, 0.0F);
  enemy_building->add_component<BuildingComponent>();

  const auto highlights = collect_attack_target_highlights(make_request());

  EXPECT_EQ(highlights.markers.size(), 2U);
  EXPECT_TRUE(contains_marker(highlights, enemy->get_id()));
  EXPECT_TRUE(contains_marker(highlights, enemy_building->get_id()));
}

TEST_F(AttackTargetingTest, OwnAlliedNeutralAndDeadUnitsAreNotHighlighted) {
  auto* own = spawn_unit(*world, k_local_owner, 1.0F, 0.0F);
  auto* ally = spawn_unit(*world, k_allied_owner, 2.0F, 0.0F);
  auto* wildlife = spawn_unit(*world, k_enemy_owner, 3.0F, 0.0F);
  wildlife->add_component<WildlifeComponent>();
  auto* corpse = spawn_unit(*world, k_enemy_owner, 4.0F, 0.0F);
  corpse->get_component<UnitComponent>()->health = 0;

  const auto highlights = collect_attack_target_highlights(make_request());

  EXPECT_TRUE(highlights.markers.empty());
  EXPECT_FALSE(contains_marker(highlights, own->get_id()));
  EXPECT_FALSE(contains_marker(highlights, ally->get_id()));
  EXPECT_FALSE(contains_marker(highlights, wildlife->get_id()));
  EXPECT_FALSE(contains_marker(highlights, corpse->get_id()));
}

TEST_F(AttackTargetingTest, NothingIsHighlightedWithoutAttackers) {
  spawn_unit(*world, k_enemy_owner, 4.0F, 0.0F);

  auto request = make_request();
  request.has_attackers = false;
  const auto highlights = collect_attack_target_highlights(request);

  EXPECT_TRUE(highlights.markers.empty());
}

TEST_F(AttackTargetingTest, TargetsBeyondTheDistanceLimitAreDropped) {
  auto* near_enemy = spawn_unit(*world, k_enemy_owner, 5.0F, 0.0F);
  auto* far_enemy = spawn_unit(*world, k_enemy_owner, 200.0F, 0.0F);

  const auto highlights = collect_attack_target_highlights(make_request());

  EXPECT_TRUE(contains_marker(highlights, near_enemy->get_id()));
  EXPECT_FALSE(contains_marker(highlights, far_enemy->get_id()));
}

TEST_F(AttackTargetingTest, MarkerCountIsCappedAndKeepsTheHoveredTarget) {
  EntityID hovered_id = 0;
  for (int index = 0; index < 12; ++index) {
    auto* enemy = spawn_unit(*world, k_enemy_owner, 2.0F + float(index), 0.0F);
    if (index == 11) {
      hovered_id = enemy->get_id();
    }
  }

  auto request = make_request();
  request.max_markers = 4;
  request.hovered_entity_id = hovered_id;
  const auto highlights = collect_attack_target_highlights(request);

  EXPECT_EQ(highlights.markers.size(), 4U);
  EXPECT_TRUE(contains_marker(highlights, hovered_id));
  EXPECT_TRUE(highlights.hovered_marker_included);
}

TEST_F(AttackTargetingTest, FogHiddenTargetsAreNotHighlighted) {
  auto* enemy = spawn_unit(*world, k_enemy_owner, 4.0F, 0.0F);

  Game::Map::VisibilityService::Snapshot snapshot;
  snapshot.initialized = true;
  snapshot.width = 16;
  snapshot.height = 16;
  snapshot.tile_size = 1.0F;
  snapshot.half_width = 8.0F;
  snapshot.half_height = 8.0F;
  snapshot.cells.assign(static_cast<std::size_t>(snapshot.width * snapshot.height),
                        static_cast<std::uint8_t>(Game::Map::VisibilityState::Unseen));

  auto request = make_request();
  request.visibility = &snapshot;
  EXPECT_TRUE(collect_attack_target_highlights(request).markers.empty());

  snapshot.cells.assign(static_cast<std::size_t>(snapshot.width * snapshot.height),
                        static_cast<std::uint8_t>(Game::Map::VisibilityState::Visible));
  EXPECT_TRUE(
      contains_marker(collect_attack_target_highlights(request), enemy->get_id()));
}

TEST_F(AttackTargetingTest, HoverVerdictsDistinguishEnemiesFromAllies) {
  auto* enemy = spawn_unit(*world, k_enemy_owner, 4.0F, 0.0F);
  auto* ally = spawn_unit(*world, k_allied_owner, 2.0F, 0.0F);
  auto* own = spawn_unit(*world, k_local_owner, 1.0F, 0.0F);

  EXPECT_EQ(classify_attack_target(world.get(), k_local_owner, true, enemy->get_id()),
            AttackTargetVerdict::Valid);
  EXPECT_EQ(classify_attack_target(world.get(), k_local_owner, true, ally->get_id()),
            AttackTargetVerdict::Ally);
  EXPECT_EQ(classify_attack_target(world.get(), k_local_owner, true, own->get_id()),
            AttackTargetVerdict::Ally);
  EXPECT_EQ(classify_attack_target(world.get(), k_local_owner, true, 0),
            AttackTargetVerdict::NoTarget);
  EXPECT_EQ(classify_attack_target(world.get(), k_local_owner, false, enemy->get_id()),
            AttackTargetVerdict::NoAttackers);
}

TEST_F(AttackTargetingTest, HoveringAnAllyEmitsABlockedMarkerOnly) {
  auto* ally = spawn_unit(*world, k_allied_owner, 2.0F, 0.0F);

  auto request = make_request();
  request.hovered_entity_id = ally->get_id();
  const auto highlights = collect_attack_target_highlights(request);

  ASSERT_EQ(highlights.markers.size(), 1U);
  EXPECT_EQ(highlights.markers.front().entity_id, ally->get_id());
  EXPECT_TRUE(highlights.markers.front().hovered);
  EXPECT_FALSE(highlights.markers.front().attackable);
  EXPECT_EQ(highlights.hovered_verdict, AttackTargetVerdict::Ally);
}

TEST_F(AttackTargetingTest, VerdictKeysAreStable) {
  EXPECT_EQ(attack_target_verdict_key(AttackTargetVerdict::Valid), "valid");
  EXPECT_EQ(attack_target_verdict_key(AttackTargetVerdict::Ally), "ally");
  EXPECT_EQ(attack_target_verdict_key(AttackTargetVerdict::Neutral), "neutral");
  EXPECT_EQ(attack_target_verdict_key(AttackTargetVerdict::NoAttackers),
            "no_attackers");
  EXPECT_EQ(attack_target_verdict_key(AttackTargetVerdict::NoTarget), "none");
}
