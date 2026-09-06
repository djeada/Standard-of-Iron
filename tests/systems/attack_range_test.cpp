#include <algorithm>
#include <gtest/gtest.h>

#include "core/component_gameplay.h"
#include "core/entity.h"
#include "core/world.h"
#include "systems/attack_range.h"
#include "systems/owner_registry.h"
#include "units/spawn_type.h"

using namespace Engine::Core;
using namespace Game::Systems;

namespace {

constexpr int k_local_owner = 1;
constexpr int k_enemy_owner = 2;

struct SpawnOptions {
  Game::Units::SpawnType spawn_type{Game::Units::SpawnType::Archer};
  int owner_id{k_local_owner};
  float x{0.0F};
  float z{0.0F};
  float ranged_range{7.5F};
  float melee_range{1.5F};
  bool can_ranged{true};
};

auto spawn(World& world, const SpawnOptions& options) -> Entity* {
  auto* entity = world.create_entity();
  entity->add_component<TransformComponent>(options.x, 0.0F, options.z);
  auto* unit = entity->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit->owner_id = options.owner_id;
  unit->spawn_type = options.spawn_type;
  auto* attack = entity->add_component<AttackComponent>(options.ranged_range, 10, 1.0F);
  attack->melee_range = options.melee_range;
  attack->can_ranged = options.can_ranged;
  attack->current_mode = AttackComponent::CombatMode::Ranged;
  return entity;
}

auto ring_for(const std::vector<AttackRangeRing>& rings,
              EntityID entity_id) -> const AttackRangeRing* {
  auto const found = std::find_if(
      rings.begin(), rings.end(), [entity_id](const AttackRangeRing& ring) {
        return ring.entity_id == entity_id;
      });
  return found == rings.end() ? nullptr : &*found;
}

} // namespace

class AttackRangeTest : public ::testing::Test {
protected:
  void SetUp() override {
    world = std::make_unique<World>();
    auto& owners = OwnerRegistry::instance();
    owners.clear();
    owners.register_owner_with_id(k_local_owner, OwnerType::Player, "local");
    owners.register_owner_with_id(k_enemy_owner, OwnerType::AI, "enemy");
    owners.set_local_player_id(k_local_owner);
    owners.set_owner_team(k_local_owner, 1);
    owners.set_owner_team(k_enemy_owner, 2);
  }

  void TearDown() override {
    world.reset();
    OwnerRegistry::instance().clear();
  }

  auto rings_for(const std::vector<EntityID>& selection,
                 EntityID focus = 0,
                 std::size_t cap = k_attack_range_max_rings)
      -> std::vector<AttackRangeRing> {
    AttackRangeRingRequest request;
    request.world = world.get();
    request.local_owner_id = k_local_owner;
    request.selection = selection;
    request.focus_entity_id = focus;
    request.max_rings = cap;
    return collect_attack_range_rings(request);
  }

  std::unique_ptr<World> world;
};

TEST_F(AttackRangeTest, ProfileReportsTheWeaponRangeAndClass) {
  auto* archer = spawn(*world, {});
  auto* catapult = spawn(*world,
                         {.spawn_type = Game::Units::SpawnType::Catapult,
                          .x = 10.0F,
                          .ranged_range = 18.0F});
  auto* swordsman = spawn(*world,
                          {.spawn_type = Game::Units::SpawnType::Knight,
                           .x = 20.0F,
                           .ranged_range = 1.6F,
                           .melee_range = 1.6F,
                           .can_ranged = false});

  const auto bow = resolve_attack_range(*archer);
  EXPECT_TRUE(bow.ranged);
  EXPECT_FLOAT_EQ(bow.max_range, 7.5F);
  EXPECT_EQ(bow.weapon_class, RangeWeaponClass::Bow);

  const auto siege = resolve_attack_range(*catapult);
  EXPECT_TRUE(siege.ranged);
  EXPECT_FLOAT_EQ(siege.max_range, 18.0F);
  EXPECT_EQ(siege.weapon_class, RangeWeaponClass::Siege);

  EXPECT_FALSE(resolve_attack_range(*swordsman).ranged);
}

TEST_F(AttackRangeTest, HoldModeGrowsTheArcherRangeLikeCombatDoes) {
  auto* archer = spawn(*world, {});
  EXPECT_FLOAT_EQ(resolve_attack_range(*archer).max_range, 7.5F);

  auto* hold = archer->add_component<HoldModeComponent>();
  hold->active = true;

  EXPECT_FLOAT_EQ(hold_mode_range_multiplier(*archer, Game::Units::SpawnType::Archer),
                  1.5F);
  EXPECT_FLOAT_EQ(resolve_attack_range(*archer).max_range, 11.25F);
  EXPECT_FLOAT_EQ(rings_for({archer->get_id()}).front().max_radius, 11.25F);
}

TEST_F(AttackRangeTest, OnlyRangedLocalUnitsGetRings) {
  auto* archer = spawn(*world, {});
  auto* swordsman = spawn(
      *world,
      {.spawn_type = Game::Units::SpawnType::Knight, .x = 4.0F, .can_ranged = false});
  auto* enemy_archer = spawn(*world, {.owner_id = k_enemy_owner, .x = 8.0F});
  auto* corpse = spawn(*world, {.x = 12.0F});
  corpse->get_component<UnitComponent>()->health = 0;

  const auto rings = rings_for({archer->get_id(),
                                swordsman->get_id(),
                                enemy_archer->get_id(),
                                corpse->get_id()});

  ASSERT_EQ(rings.size(), 1U);
  EXPECT_EQ(rings.front().entity_id, archer->get_id());
}

TEST_F(AttackRangeTest, ASingleSelectionIsAlwaysFocused) {
  auto* archer = spawn(*world, {});
  const auto rings = rings_for({archer->get_id()});

  ASSERT_EQ(rings.size(), 1U);
  EXPECT_TRUE(rings.front().focused);
}

TEST_F(AttackRangeTest, HoveredUnitBecomesTheFocusedRing) {
  auto* first = spawn(*world, {.x = 0.0F});
  auto* second = spawn(*world, {.x = 30.0F});

  const auto rings = rings_for({first->get_id(), second->get_id()}, second->get_id());

  ASSERT_EQ(rings.size(), 2U);
  const auto* focused = ring_for(rings, second->get_id());
  ASSERT_NE(focused, nullptr);
  EXPECT_TRUE(focused->focused);
  const auto* plain = ring_for(rings, first->get_id());
  ASSERT_NE(plain, nullptr);
  EXPECT_FALSE(plain->focused);
}

TEST_F(AttackRangeTest, OverlappingIdenticalRingsCollapseToOne) {
  auto* first = spawn(*world, {.x = 0.0F});
  auto* second = spawn(*world, {.x = 1.0F});
  auto* far = spawn(*world, {.x = 40.0F});

  const auto rings = rings_for({first->get_id(), second->get_id(), far->get_id()});

  EXPECT_EQ(rings.size(), 2U);
  EXPECT_NE(ring_for(rings, far->get_id()), nullptr);
  EXPECT_EQ(ring_for(rings, second->get_id()), nullptr);
}

TEST_F(AttackRangeTest, RingCountIsCappedKeepingTheLongestReach) {
  std::vector<EntityID> selection;
  EntityID longest = 0;
  for (int index = 0; index < 8; ++index) {
    auto* archer = spawn(*world,
                         {.x = static_cast<float>(index) * 40.0F,
                          .ranged_range = 6.0F + static_cast<float>(index)});
    selection.push_back(archer->get_id());
    longest = archer->get_id();
  }

  const auto rings = rings_for(selection, 0, 3);

  EXPECT_EQ(rings.size(), 3U);
  EXPECT_NE(ring_for(rings, longest), nullptr);
}

TEST_F(AttackRangeTest, MinimumRangeTravelsIntoTheRingAndBlocksCloseShots) {
  auto* catapult = spawn(
      *world, {.spawn_type = Game::Units::SpawnType::Catapult, .ranged_range = 18.0F});
  catapult->get_component<AttackComponent>()->min_range = 5.0F;
  auto* adjacent = spawn(*world, {.owner_id = k_enemy_owner, .x = 2.0F});
  auto* distant = spawn(*world, {.owner_id = k_enemy_owner, .x = 12.0F});

  const auto rings = rings_for({catapult->get_id()});
  ASSERT_EQ(rings.size(), 1U);
  EXPECT_FLOAT_EQ(rings.front().min_radius, 5.0F);

  const std::vector<EntityID> attackers{catapult->get_id()};
  EXPECT_EQ(classify_range_to_target(world.get(), attackers, adjacent->get_id()),
            RangeVerdict::TooClose);
  EXPECT_EQ(classify_range_to_target(world.get(), attackers, distant->get_id()),
            RangeVerdict::InRange);
}

TEST_F(AttackRangeTest, HoverVerdictsFollowTheCombatRangeCheck) {
  auto* archer = spawn(*world, {});
  auto* near_enemy = spawn(*world, {.owner_id = k_enemy_owner, .x = 4.0F});
  auto* far_enemy = spawn(*world, {.owner_id = k_enemy_owner, .x = 40.0F});

  const std::vector<EntityID> attackers{archer->get_id()};
  EXPECT_EQ(classify_range_to_target(world.get(), attackers, near_enemy->get_id()),
            RangeVerdict::InRange);
  EXPECT_EQ(classify_range_to_target(world.get(), attackers, far_enemy->get_id()),
            RangeVerdict::OutOfRange);
  EXPECT_EQ(classify_range_to_target(world.get(), attackers, 0), RangeVerdict::None);
}

TEST_F(AttackRangeTest, AnArcherLockedInMeleeReadsAsBlockedInsideItsRing) {
  auto* archer = spawn(*world, {});
  archer->get_component<AttackComponent>()->current_mode =
      AttackComponent::CombatMode::Melee;
  auto* enemy = spawn(*world, {.owner_id = k_enemy_owner, .x = 5.0F});

  const std::vector<EntityID> attackers{archer->get_id()};
  EXPECT_EQ(classify_range_to_target(world.get(), attackers, enemy->get_id()),
            RangeVerdict::Blocked);
}

TEST_F(AttackRangeTest, MeleeOnlySelectionsProduceNoRangeVerdict) {
  auto* swordsman = spawn(*world,
                          {.spawn_type = Game::Units::SpawnType::Knight,
                           .ranged_range = 1.6F,
                           .melee_range = 1.6F,
                           .can_ranged = false});
  auto* enemy = spawn(*world, {.owner_id = k_enemy_owner, .x = 4.0F});

  const std::vector<EntityID> attackers{swordsman->get_id()};
  EXPECT_EQ(classify_range_to_target(world.get(), attackers, enemy->get_id()),
            RangeVerdict::None);
}

TEST_F(AttackRangeTest, VerdictAndWeaponKeysAreStable) {
  EXPECT_EQ(range_verdict_key(RangeVerdict::InRange), "in_range");
  EXPECT_EQ(range_verdict_key(RangeVerdict::TooClose), "too_close");
  EXPECT_EQ(range_verdict_key(RangeVerdict::OutOfRange), "out_of_range");
  EXPECT_EQ(range_verdict_key(RangeVerdict::Blocked), "blocked");
  EXPECT_EQ(range_verdict_key(RangeVerdict::None), "none");

  EXPECT_EQ(range_weapon_class_key(RangeWeaponClass::Bow), "bow");
  EXPECT_EQ(range_weapon_class_key(RangeWeaponClass::Siege), "siege");
  EXPECT_EQ(range_weapon_class_key(RangeWeaponClass::Arcane), "arcane");
  EXPECT_EQ(range_weapon_class_key(RangeWeaponClass::None), "none");
}
