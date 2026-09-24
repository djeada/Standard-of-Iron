

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <gtest/gtest.h>
#include <vector>

#include "core/component_core.h"
#include "core/world.h"
#include "formation/army_formation_planner.h"
#include "formation/army_formation_registry.h"
#include "formation/formation_doctrine.h"
#include "systems/formation_combat_geometry.h"
#include "systems/nation_registry.h"
#include "systems/nav_grid.h"
#include "systems/pathfinding.h"
#include "systems/troop_profile_service.h"

namespace {

using Game::Formation::ArmyFormationIntent;
using Game::Formation::ArmyFormationMember;
using Game::Formation::ArmyFormationPlan;
using Game::Formation::ArmyFormationPlanner;
using Game::Formation::ArmyFormationRegistry;
using Game::Formation::ArmyFormationRequest;
using Game::Systems::NationID;

class FormationFootprintTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::NavGrid::initialize(256, 256);
    if (auto* pathfinder = Game::Systems::NavGrid::get_pathfinder()) {
      pathfinder->update_navigation_grid();
    }
    auto& nations = Game::Systems::NationRegistry::instance();
    nations.clear();
    nations.register_nation({.id = NationID::RomanRepublic,
                             .display_name = "Roman Republic",
                             .doctrine = "rome"});
    nations.register_nation(
        {.id = NationID::Carthage, .display_name = "Carthage", .doctrine = "carthage"});
    Game::Systems::TroopProfileService::instance().clear();
    Game::Formation::DoctrineRegistry::instance().reset_to_defaults();
    ArmyFormationRegistry::instance().clear();
  }

  void TearDown() override {
    ArmyFormationRegistry::instance().clear();
    Game::Systems::TroopProfileService::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
  }

  static auto add(Engine::Core::World& world,
                  Game::Units::SpawnType spawn,
                  NationID nation,
                  float x,
                  float z) -> Engine::Core::EntityID {
    auto* entity = world.create_entity();
    auto* transform = entity->add_component<Engine::Core::TransformComponent>();
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    transform->position = {x, 0.0F, z};
    unit->spawn_type = spawn;
    unit->nation_id = nation;
    unit->health = 100;
    unit->max_health = 100;
    return entity->get_id();
  }

  static auto host(Engine::Core::World& world,
                   NationID nation) -> std::vector<Engine::Core::EntityID> {
    std::vector<Engine::Core::EntityID> ids;
    auto fill = [&](Game::Units::SpawnType spawn, int count) {
      for (int i = 0; i < count; ++i) {
        ids.push_back(add(world,
                          spawn,
                          nation,
                          static_cast<float>(ids.size() % 8) * 4.0F,
                          static_cast<float>(ids.size() / 8) * 4.0F));
      }
    };
    fill(Game::Units::SpawnType::Swordsman, 12);
    fill(Game::Units::SpawnType::Spearman, 8);
    fill(Game::Units::SpawnType::Archer, 6);
    fill(Game::Units::SpawnType::Catapult, 2);
    fill(Game::Units::SpawnType::MountedSwordsman, 4);
    return ids;
  }

  static auto plan_for(Engine::Core::World& world,
                       const std::vector<Engine::Core::EntityID>& ids,
                       ArmyFormationIntent intent,
                       float frontage = 0.0F) -> ArmyFormationPlan {
    ArmyFormationRequest request;
    request.members = ids;
    request.anchor = QVector3D(0.0F, 0.0F, 0.0F);
    request.facing = 0.0F;
    request.frontage = frontage;
    request.intent = intent;
    request.spacing = 1.0F;
    request.resolve_terrain = false;
    request.preserve_previous_slots = false;
    return ArmyFormationPlanner::plan(world, request);
  }

  static auto worst_overlap(const ArmyFormationPlan& plan) -> float {
    float worst = -1e9F;
    for (std::size_t i = 0; i < plan.slot_list.size(); ++i) {
      for (std::size_t j = i + 1; j < plan.slot_list.size(); ++j) {
        auto const delta =
            plan.slot_list[i].local_offset - plan.slot_list[j].local_offset;

        float const gap_x =
            std::abs(delta.x()) - (plan.slot_half_width[i] + plan.slot_half_width[j]);
        float const gap_z =
            std::abs(delta.z()) - (plan.slot_half_depth[i] + plan.slot_half_depth[j]);
        worst = std::max(worst, -std::max(gap_x, gap_z));
      }
    }
    return worst;
  }
};

TEST_F(FormationFootprintTest, NoTwoTroopsStandInsideEachOtherInAnyIntent) {
  for (auto const nation : {NationID::RomanRepublic, NationID::Carthage}) {
    Engine::Core::World world;
    auto const ids = host(world, nation);
    for (auto const intent : {ArmyFormationIntent::FactionDefault,
                              ArmyFormationIntent::Line,
                              ArmyFormationIntent::Column,
                              ArmyFormationIntent::Defensive,
                              ArmyFormationIntent::Assault,
                              ArmyFormationIntent::Encirclement,
                              ArmyFormationIntent::SiegeEscort}) {
      auto const plan = plan_for(world, ids, intent);
      ASSERT_TRUE(plan.valid) << plan.rejection_reason;
      EXPECT_LE(worst_overlap(plan), 0.0F)
          << "intent " << Game::Formation::intent_to_string(intent)
          << " stands two troops inside each other";
    }
  }
}

TEST_F(FormationFootprintTest, NoDraggedFrontageMakesTroopsOverlap) {
  Engine::Core::World world;
  auto const ids = host(world, NationID::RomanRepublic);
  for (float const frontage : {6.0F, 12.0F, 20.0F, 35.0F, 60.0F, 120.0F}) {
    auto const plan = plan_for(world, ids, ArmyFormationIntent::Line, frontage);
    ASSERT_TRUE(plan.valid) << plan.rejection_reason;
    EXPECT_LE(worst_overlap(plan), 0.0F)
        << "a " << frontage << " m drag squeezed two troops into each other";
  }
}

TEST_F(FormationFootprintTest, ASquadIsMeasuredByItsSoldiersNotItsSelectionRing) {
  Engine::Core::World world;
  auto const id = add(
      world, Game::Units::SpawnType::Swordsman, NationID::RomanRepublic, 0.0F, 0.0F);
  auto const members = ArmyFormationPlanner::collect_members(world, {id});
  ASSERT_EQ(members.size(), 1U);

  auto const layout =
      Game::Systems::FormationCombat::resolve_layout(*world.get_entity(id));
  ASSERT_GT(layout.all_slots.size(), 1U) << "a legionary unit is a block of men";
  float extent_x = 0.0F;
  for (const auto& slot : layout.all_slots) {
    extent_x = std::max(extent_x, std::abs(slot.local_x));
  }

  EXPECT_NEAR(members.front().half_width, extent_x + layout.body_radius, 1e-3F);
  EXPECT_GT(members.front().half_width, members.front().footprint * 1.5F)
      << "the selection ring is far smaller than the ground the men cover";
}

TEST_F(FormationFootprintTest, SiegeStandsBehindTheLineAndSpearsInFrontOfIt) {
  Engine::Core::World world;
  auto const ids = host(world, NationID::RomanRepublic);
  auto const plan = plan_for(world, ids, ArmyFormationIntent::Line);
  ASSERT_TRUE(plan.valid) << plan.rejection_reason;

  auto band = [&](Game::Formation::ArmyRole role) {
    float total = 0.0F;
    int count = 0;
    for (const auto& slot : plan.slot_list) {
      if (slot.role == role) {
        total += slot.local_offset.z();
        ++count;
      }
    }
    EXPECT_GT(count, 0) << "no slots took the " << army_role_to_string(role) << " role";
    return count > 0 ? total / static_cast<float>(count) : 0.0F;
  };

  float const screen = band(Game::Formation::ArmyRole::Screen);
  float const centre = band(Game::Formation::ArmyRole::Centre);
  float const ranged = band(Game::Formation::ArmyRole::Ranged);
  float const siege = band(Game::Formation::ArmyRole::Siege);

  EXPECT_GT(screen, centre) << "the spear screen stands in front of the heavy centre";
  EXPECT_GT(centre, ranged) << "archers shoot over the line, not from it";
  EXPECT_GT(ranged, siege) << "the engines stand at the back";
}

TEST_F(FormationFootprintTest, EveryIntentStaysInsideTheGroundItDeclares) {
  Engine::Core::World world;
  auto const ids = host(world, NationID::RomanRepublic);
  auto const& registry = Game::Formation::DoctrineRegistry::instance();
  const auto* doctrine = registry.find("rome");
  ASSERT_NE(doctrine, nullptr);

  for (auto const intent : {ArmyFormationIntent::FactionDefault,
                            ArmyFormationIntent::Line,
                            ArmyFormationIntent::Column,
                            ArmyFormationIntent::Defensive,
                            ArmyFormationIntent::Assault,
                            ArmyFormationIntent::Encirclement,
                            ArmyFormationIntent::SiegeEscort}) {
    auto const found = doctrine->intents.find(static_cast<int>(intent));
    ASSERT_NE(found, doctrine->intents.end());
    auto const plan = plan_for(world, ids, intent);
    ASSERT_TRUE(plan.valid) << plan.rejection_reason;

    if (found->second.max_frontage > 0.1F) {
      EXPECT_LE(plan.frontage, found->second.max_frontage * 1.02F)
          << Game::Formation::intent_to_string(intent) << " spread past its bound";
    }
    if (found->second.max_depth > 0.1F) {
      EXPECT_LE(plan.depth, found->second.max_depth * 1.02F)
          << Game::Formation::intent_to_string(intent) << " reached past its bound";
    }
  }
}

TEST_F(FormationFootprintTest, AColumnIsNarrowAndALineIsWide) {
  Engine::Core::World world;
  auto const ids = host(world, NationID::RomanRepublic);
  auto const line = plan_for(world, ids, ArmyFormationIntent::Line);
  auto const column = plan_for(world, ids, ArmyFormationIntent::Column);
  ASSERT_TRUE(line.valid);
  ASSERT_TRUE(column.valid);

  EXPECT_GT(line.frontage, column.frontage * 3.0F);
  EXPECT_GT(column.depth, line.depth * 2.0F);
}

TEST_F(FormationFootprintTest, EveryIntentHasItsOwnSilhouette) {
  Engine::Core::World world;
  auto const ids = host(world, NationID::Carthage);
  for (float const frontage : {0.0F, 60.0F}) {
    SCOPED_TRACE(frontage);
    auto const line = plan_for(world, ids, ArmyFormationIntent::Line, frontage);
    auto const column = plan_for(world, ids, ArmyFormationIntent::Column, frontage);
    auto const defensive =
        plan_for(world, ids, ArmyFormationIntent::Defensive, frontage);
    auto const assault = plan_for(world, ids, ArmyFormationIntent::Assault, frontage);
    auto const crescent =
        plan_for(world, ids, ArmyFormationIntent::Encirclement, frontage);
    for (const auto* plan : {&line, &column, &defensive, &assault, &crescent}) {
      ASSERT_TRUE(plan->valid) << plan->rejection_reason;
    }

    EXPECT_GT(line.frontage, line.depth * 2.0F) << "a line is wide and shallow";
    EXPECT_GT(column.depth, column.frontage * 1.8F) << "a column is deep and narrow";

    float const front_z =
        std::max_element(assault.slot_list.begin(),
                         assault.slot_list.end(),
                         [](const auto& a, const auto& b) {
                           return a.local_offset.z() < b.local_offset.z();
                         })
            ->local_offset.z();
    auto rank_width = [&](float z) {
      return std::count_if(
          assault.slot_list.begin(), assault.slot_list.end(), [&](const auto& slot) {
            return std::abs(slot.local_offset.z() - z) < 0.5F;
          });
    };
    std::ptrdiff_t widest_rank = 0;
    for (const auto& slot : assault.slot_list) {
      widest_rank = std::max(widest_rank, rank_width(slot.local_offset.z()));
    }
    EXPECT_LE(rank_width(front_z) * 2, widest_rank)
        << "an assault leads with the point of a wedge";

    auto faces = [&](float local_facing) {
      return std::any_of(defensive.slot_list.begin(),
                         defensive.slot_list.end(),
                         [&](const auto& slot) {
                           return std::abs(slot.local_facing - local_facing) < 1.0F;
                         });
    };
    EXPECT_TRUE(faces(180.0F) && faces(90.0F) && faces(-90.0F))
        << "a defensive square faces out on every side";

    std::vector<const Game::Formation::FormationSlot*> by_reach;
    for (const auto& slot : crescent.slot_list) {
      by_reach.push_back(&slot);
    }
    std::sort(by_reach.begin(), by_reach.end(), [](const auto* a, const auto* b) {
      return std::abs(a->local_offset.x()) < std::abs(b->local_offset.x());
    });
    auto mean_z = [](auto first, auto last) {
      float sum = 0.0F;
      int count = 0;
      for (auto it = first; it != last; ++it, ++count) {
        sum += (*it)->local_offset.z();
      }
      return sum / static_cast<float>(std::max(1, count));
    };
    float const centre_z = mean_z(by_reach.begin(), by_reach.begin() + 4);
    float const horns_z = mean_z(by_reach.end() - 4, by_reach.end());
    EXPECT_GT(horns_z, centre_z + 4.0F) << "an encirclement throws its horns forward";
  }
}

TEST_F(FormationFootprintTest, ReportsTheShapeOfEachIntent) {
  Engine::Core::World world;
  auto const ids = host(world, NationID::RomanRepublic);
  for (auto const intent : {ArmyFormationIntent::FactionDefault,
                            ArmyFormationIntent::Line,
                            ArmyFormationIntent::Column,
                            ArmyFormationIntent::Defensive,
                            ArmyFormationIntent::Assault,
                            ArmyFormationIntent::Encirclement,
                            ArmyFormationIntent::SiegeEscort}) {
    auto const plan = plan_for(world, ids, intent);
    ASSERT_TRUE(plan.valid) << plan.rejection_reason;
    int files = 0;
    for (auto const f : plan.slot_files) {
      files = std::max(files, f);
    }
    std::printf("%-16s %6.1f x %6.1f m  ranks=%2d files=%2d  unit_files=%2d  "
                "worst_overlap=%+.2f\n",
                Game::Formation::intent_to_string(intent),
                plan.frontage,
                plan.depth,
                plan.rank_count(),
                plan.file_count(),
                files,
                worst_overlap(plan));
  }
}

} // namespace
