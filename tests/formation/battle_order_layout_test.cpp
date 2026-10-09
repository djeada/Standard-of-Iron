#include <QString>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <vector>

#include "formation/army_formation_manoeuvre.h"
#include "formation/army_formation_planner.h"
#include "formation/army_formation_registry.h"
#include "formation/formation_battle_orders.h"
#include "formation/formation_doctrine.h"
#include "formation/troop_role_registry.h"
#include "units/troop_type.h"

namespace {

using Game::Formation::ArmyFormation;
using Game::Formation::ArmyFormationIntent;
using Game::Formation::ArmyFormationLayout;
using Game::Formation::ArmyFormationMember;
using Game::Formation::ArmyFormationPlanner;
using Game::Formation::ArmyFormationRequest;
using Game::Formation::BattleBand;
using Game::Formation::FormationSlot;
using Game::Units::TroopType;

constexpr float k_eps = 0.05F;

struct Army {
  std::vector<ArmyFormationMember> members;
  Game::Formation::EntityID next_id{1};

  void add(TroopType type,
           int count,
           const Game::Formation::FormationDoctrineId& doctrine,
           bool allied = false,
           float half_width = 3.0F,
           float half_depth = 2.0F) {
    for (int i = 0; i < count; ++i) {
      auto member = ArmyFormationPlanner::make_member(
          next_id,
          type,
          QVector3D(static_cast<float>(members.size()) * 7.0F, 0.0F, 0.0F),
          doctrine);
      member.half_width = half_width;
      member.half_depth = half_depth;
      member.allied = allied;
      ++next_id;
      members.push_back(member);
    }
  }
};

auto layout(const Army& army,
            ArmyFormationIntent intent,
            const Game::Formation::FormationDoctrineId& doctrine) -> ArmyFormationLayout {
  ArmyFormationRequest request;
  for (const auto& member : army.members) {
    request.members.push_back(member.entity_id);
  }
  request.intent = intent;
  request.doctrine = doctrine;
  request.spacing = 1.5F;
  request.resolve_terrain = false;
  return ArmyFormationPlanner::build_layout(army.members, request);
}

auto band(const ArmyFormationLayout& plan, BattleBand which) -> std::vector<FormationSlot> {
  std::vector<FormationSlot> out;
  for (const auto& slot : plan.slot_list) {
    if (slot.band == which) {
      out.push_back(slot);
    }
  }
  std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
    return a.local_offset.x() < b.local_offset.x();
  });
  return out;
}

auto mean_z(const std::vector<FormationSlot>& slot_list) -> float {
  float sum = 0.0F;
  for (const auto& slot : slot_list) {
    sum += slot.local_offset.z();
  }
  return slot_list.empty() ? 0.0F : sum / static_cast<float>(slot_list.size());
}

auto troop_of(const Army& army, Game::Formation::EntityID id) -> TroopType {
  for (const auto& member : army.members) {
    if (member.entity_id == id) {
      return member.troop_type;
    }
  }
  return TroopType::Archer;
}

class BattleOrderLayoutTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Formation::DoctrineRegistry::instance().reset_to_defaults();
    Game::Formation::TroopRoleRegistry::instance().reset_to_defaults();
  }
};

TEST_F(BattleOrderLayoutTest, IntentsHaveStableNames) {
  for (auto const& [intent, name] :
       {std::pair{ArmyFormationIntent::TriplexAcies, "triplex_acies"},
        std::pair{ArmyFormationIntent::ConvexCrescent, "convex_crescent"},
        std::pair{ArmyFormationIntent::ElephantScreen, "elephant_screen"}}) {
    EXPECT_STREQ(Game::Formation::intent_to_string(intent), name);
    auto const parsed = Game::Formation::try_parse_intent(QString::fromLatin1(name));
    ASSERT_TRUE(parsed.has_value()) << name;
    EXPECT_EQ(*parsed, intent);
  }
  for (auto band : {BattleBand::Screen,
                    BattleBand::Hastati,
                    BattleBand::Principes,
                    BattleBand::Triarii,
                    BattleBand::CrescentCentre,
                    BattleBand::CrescentWing,
                    BattleBand::Elephants}) {
    auto const parsed = Game::Formation::try_parse_battle_band(
        QString::fromLatin1(Game::Formation::battle_band_to_string(band)));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(*parsed, band);
  }
}

TEST_F(BattleOrderLayoutTest, TriplexAciesFormsThreeLinesWithOpenLanes) {
  Army army;
  army.add(TroopType::Swordsman, 12, "rome");
  army.add(TroopType::Spearman, 3, "rome");
  auto const plan = layout(army, ArmyFormationIntent::TriplexAcies, "rome");
  ASSERT_TRUE(plan.valid) << plan.rejection_reason;

  auto const hastati = band(plan, BattleBand::Hastati);
  auto const principes = band(plan, BattleBand::Principes);
  auto const triarii = band(plan, BattleBand::Triarii);
  ASSERT_GE(hastati.size(), 3U);
  EXPECT_EQ(principes.size() + 1U, hastati.size());
  EXPECT_GE(triarii.size(), 3U);
  EXPECT_EQ(hastati.size() + principes.size() + triarii.size(), 15U);

  // The veterans (spearmen) stand in the rear line.
  for (const auto& slot : plan.slot_list) {
    if (troop_of(army, slot.occupant) == TroopType::Spearman) {
      EXPECT_EQ(slot.band, BattleBand::Triarii);
    }
  }

  // Lines run front to back.
  EXPECT_GT(mean_z(hastati), mean_z(principes) + 2.0F);
  EXPECT_GT(mean_z(principes), mean_z(triarii) + 2.0F);
  for (const auto& slot : hastati) {
    EXPECT_NEAR(slot.local_offset.z(), hastati.front().local_offset.z(), k_eps);
  }

  // Every lane between neighbouring maniples is at least a maniple wide.
  float const maniple = 2.0F * 3.0F;
  for (const auto* line : {&hastati, &principes, &triarii}) {
    for (std::size_t i = 1; i < line->size(); ++i) {
      float const lane = ((*line)[i].local_offset.x() - (*line)[i].half_width) -
                         ((*line)[i - 1].local_offset.x() + (*line)[i - 1].half_width);
      EXPECT_GE(lane + k_eps, std::max(maniple, Game::Formation::planning::k_min_maniple_lane));
    }
  }
}

TEST_F(BattleOrderLayoutTest, TriplexPrincipesCoverTheHastatiGaps) {
  Army army;
  army.add(TroopType::Swordsman, 13, "rome");
  auto const plan = layout(army, ArmyFormationIntent::TriplexAcies, "rome");
  ASSERT_TRUE(plan.valid);
  auto const hastati = band(plan, BattleBand::Hastati);
  auto const principes = band(plan, BattleBand::Principes);
  auto const triarii = band(plan, BattleBand::Triarii);
  ASSERT_GE(hastati.size(), 2U);
  float const pitch = hastati[1].local_offset.x() - hastati[0].local_offset.x();
  ASSERT_GT(pitch, 0.0F);

  // Quincunx: each principes maniple stands behind the middle of a hastati gap.
  for (const auto& slot : principes) {
    float const steps = (slot.local_offset.x() - hastati.front().local_offset.x()) / pitch;
    EXPECT_NEAR(steps - std::floor(steps), 0.5F, 0.01F) << slot.local_offset.x();
    EXPECT_GT(slot.local_offset.x(), hastati.front().local_offset.x());
    EXPECT_LT(slot.local_offset.x(), hastati.back().local_offset.x());
  }
  // Triarii stand on the hastati files again.
  for (const auto& slot : triarii) {
    float const steps = (slot.local_offset.x() - hastati.front().local_offset.x()) / pitch;
    EXPECT_NEAR(steps, std::round(steps), 0.01F) << slot.local_offset.x();
  }
  // The principes are centred on the hastati.
  float sum = 0.0F;
  for (const auto& slot : principes) {
    sum += slot.local_offset.x();
  }
  EXPECT_NEAR(sum / static_cast<float>(principes.size()),
              (hastati.front().local_offset.x() + hastati.back().local_offset.x()) * 0.5F,
              k_eps);
}

TEST_F(BattleOrderLayoutTest, TriplexScreensWithVelitesAndPutsCavalryOnTheWings) {
  Army army;
  army.add(TroopType::Swordsman, 9, "rome");
  army.add(TroopType::Velites, 3, "rome");
  army.add(TroopType::MountedSwordsman, 4, "rome", false, 2.5F, 2.5F);
  auto const plan = layout(army, ArmyFormationIntent::TriplexAcies, "rome");
  ASSERT_TRUE(plan.valid);
  auto const hastati = band(plan, BattleBand::Hastati);
  auto const screen = band(plan, BattleBand::Screen);
  ASSERT_EQ(screen.size(), 3U);
  float const hastati_front = hastati.front().local_offset.z() + hastati.front().half_depth;
  for (const auto& slot : screen) {
    EXPECT_GT(slot.local_offset.z() - slot.half_depth, hastati_front + 2.0F);
  }
  float const span = hastati.back().local_offset.x() + hastati.back().half_width;
  int cavalry = 0;
  for (const auto& slot : plan.slot_list) {
    if (troop_of(army, slot.occupant) == TroopType::MountedSwordsman) {
      ++cavalry;
      EXPECT_GT(std::abs(slot.local_offset.x()) - slot.half_width, span - k_eps);
    }
  }
  EXPECT_EQ(cavalry, 4);
}

TEST_F(BattleOrderLayoutTest, LanesWidenToTheDraggedFrontage) {
  Army army;
  army.add(TroopType::Swordsman, 9, "rome");
  ArmyFormationRequest request;
  for (const auto& member : army.members) {
    request.members.push_back(member.entity_id);
  }
  request.intent = ArmyFormationIntent::TriplexAcies;
  request.spacing = 1.5F;
  request.frontage = 120.0F;
  request.resolve_terrain = false;
  auto const plan = ArmyFormationPlanner::build_layout(army.members, request);
  ASSERT_TRUE(plan.valid);
  auto const hastati = band(plan, BattleBand::Hastati);
  ASSERT_GE(hastati.size(), 2U);
  EXPECT_NEAR(hastati.back().local_offset.x() - hastati.front().local_offset.x(), 120.0F, 0.5F);
}

TEST_F(BattleOrderLayoutTest, ConvexCrescentBulgesTowardTheEnemy) {
  Army army;
  army.add(TroopType::Swordsman, 4, "carthage");
  army.add(TroopType::Swordsman, 6, "carthage", true);
  army.add(TroopType::MountedSwordsman, 2, "carthage", false, 2.5F, 2.5F);
  auto const plan = layout(army, ArmyFormationIntent::ConvexCrescent, "carthage");
  ASSERT_TRUE(plan.valid) << plan.rejection_reason;
  auto const centre = band(plan, BattleBand::CrescentCentre);
  auto const wings = band(plan, BattleBand::CrescentWing);
  ASSERT_EQ(centre.size(), 6U);
  ASSERT_EQ(wings.size(), 4U);

  // The allied contingents hold the centre and the home infantry the wings.
  std::vector<Game::Formation::EntityID> allied_ids;
  for (const auto& member : army.members) {
    if (member.allied) {
      allied_ids.push_back(member.entity_id);
    }
  }
  for (const auto& slot : centre) {
    EXPECT_NE(std::find(allied_ids.begin(), allied_ids.end(), slot.occupant),
              allied_ids.end());
  }

  // Convex: the centre stands ahead of the wings, its middle ahead of its ends.
  EXPECT_GT(mean_z(centre), mean_z(wings) + 2.0F);
  float const middle = std::max(centre[2].local_offset.z(), centre[3].local_offset.z());
  EXPECT_GT(middle, centre.front().local_offset.z() + 1.0F);
  EXPECT_GT(middle, centre.back().local_offset.z() + 1.0F);
  float const centre_half = centre.back().local_offset.x() + centre.back().half_width;
  for (const auto& slot : wings) {
    EXPECT_GT(std::abs(slot.local_offset.x()), centre_half);
    EXPECT_EQ(slot.yield_depth, 0.0F);
  }
  // Only the centre gives ground, the middle most.
  EXPECT_GT(centre[2].yield_depth, centre.front().yield_depth);
  // The ends of the arc turn outward.
  EXPECT_LT(centre.front().local_facing, 0.0F);
  EXPECT_GT(centre.back().local_facing, 0.0F);
}

TEST_F(BattleOrderLayoutTest, CrescentYieldsIntoAConcaveLineAndWheelsItsWings) {
  Army army;
  army.add(TroopType::Swordsman, 4, "carthage");
  army.add(TroopType::Swordsman, 6, "carthage", true);
  auto const plan = layout(army, ArmyFormationIntent::ConvexCrescent, "carthage");
  ASSERT_TRUE(plan.valid);

  ArmyFormation formation;
  formation.intent = ArmyFormationIntent::ConvexCrescent;
  formation.slot_list = plan.slot_list;
  formation.manoeuvre.centre_yield = 1.0F;
  formation.manoeuvre.wing_wheel = 1.0F;

  auto yielded_z = [&](const FormationSlot& slot) {
    return slot.local_offset.z() +
           Game::Formation::Manoeuvre::slot_manoeuvre(formation, slot).offset.z();
  };
  std::vector<FormationSlot> centre;
  for (const auto& slot : formation.slot_list) {
    if (slot.band == BattleBand::CrescentCentre) {
      centre.push_back(slot);
    }
  }
  std::sort(centre.begin(), centre.end(), [](const auto& a, const auto& b) {
    return a.local_offset.x() < b.local_offset.x();
  });
  // Concave: the middle of the centre is now behind its ends.
  float const middle = std::min(yielded_z(centre[2]), yielded_z(centre[3]));
  EXPECT_LT(middle, yielded_z(centre.front()) - 1.0F);
  EXPECT_LT(middle, yielded_z(centre.back()) - 1.0F);

  // Wings have wheeled to face inward.
  for (const auto& slot : formation.slot_list) {
    if (slot.band != BattleBand::CrescentWing) {
      continue;
    }
    auto const m = Game::Formation::Manoeuvre::slot_manoeuvre(formation, slot);
    EXPECT_NEAR(std::abs(m.facing), 90.0F, 0.01F);
    EXPECT_EQ(m.facing > 0.0F, slot.local_offset.x() < 0.0F);
  }
}

TEST_F(BattleOrderLayoutTest, OpenedLanesPutThePrincipesBehindTheHastati) {
  Army army;
  army.add(TroopType::Swordsman, 13, "rome");
  auto const plan = layout(army, ArmyFormationIntent::TriplexAcies, "rome");
  ASSERT_TRUE(plan.valid);
  ArmyFormation formation;
  formation.intent = ArmyFormationIntent::TriplexAcies;
  formation.slot_list = plan.slot_list;
  formation.manoeuvre.lanes_opened = true;
  formation.manoeuvre.lane_shift = 1.0F;
  float const pitch = Game::Formation::Manoeuvre::lane_pitch(formation);
  ASSERT_GT(pitch, 0.0F);
  auto const hastati = band(plan, BattleBand::Hastati);
  for (const auto& slot : formation.slot_list) {
    if (slot.band != BattleBand::Principes) {
      continue;
    }
    float const x =
        slot.local_offset.x() +
        Game::Formation::Manoeuvre::slot_manoeuvre(formation, slot).offset.x();
    bool behind = false;
    for (const auto& front : hastati) {
      behind = behind || std::abs(front.local_offset.x() - x) < 0.01F;
    }
    EXPECT_TRUE(behind) << x;
  }
}

TEST_F(BattleOrderLayoutTest, ElephantScreenStandsSpacedAheadOfTheLine) {
  Army army;
  army.add(TroopType::Elephant, 4, "carthage", false, 2.4F, 3.0F);
  army.add(TroopType::Swordsman, 6, "carthage");
  army.add(TroopType::Spearman, 3, "carthage");
  auto const plan = layout(army, ArmyFormationIntent::ElephantScreen, "carthage");
  ASSERT_TRUE(plan.valid) << plan.rejection_reason;
  auto const elephants = band(plan, BattleBand::Elephants);
  ASSERT_EQ(elephants.size(), 4U);
  float line_front = -1000.0F;
  for (const auto& slot : plan.slot_list) {
    if (slot.band != BattleBand::Elephants) {
      line_front = std::max(line_front, slot.local_offset.z() + slot.half_depth);
    }
  }
  for (std::size_t i = 0; i < elephants.size(); ++i) {
    EXPECT_GT(elephants[i].local_offset.z() - elephants[i].half_depth, line_front + 6.0F);
    if (i > 0) {
      float const gap = (elephants[i].local_offset.x() - elephants[i].half_width) -
                        (elephants[i - 1].local_offset.x() + elephants[i - 1].half_width);
      EXPECT_GE(gap, 7.9F);
    }
  }
}

TEST_F(BattleOrderLayoutTest, BattleOrdersBelongToTheirDoctrines) {
  auto& registry = Game::Formation::DoctrineRegistry::instance();
  using Game::Formation::RoleTag;
  using Game::Formation::to_mask;
  auto const infantry = to_mask(RoleTag::LineInfantry);
  auto const with_elephants = infantry | to_mask(RoleTag::Elephant);

  EXPECT_TRUE(
      registry.availability_reason("rome", ArmyFormationIntent::TriplexAcies, infantry, 6)
          .empty());
  EXPECT_FALSE(
      registry.availability_reason("rome", ArmyFormationIntent::ConvexCrescent, infantry, 6)
          .empty());
  EXPECT_FALSE(registry
                   .availability_reason(
                       "carthage", ArmyFormationIntent::TriplexAcies, infantry, 6)
                   .empty());
  EXPECT_TRUE(registry
                  .availability_reason(
                      "carthage", ArmyFormationIntent::ConvexCrescent, infantry, 6)
                  .empty());
  EXPECT_FALSE(registry
                   .availability_reason(
                       "carthage", ArmyFormationIntent::ElephantScreen, infantry, 6)
                   .empty());
  EXPECT_TRUE(registry
                  .availability_reason(
                      "carthage", ArmyFormationIntent::ElephantScreen, with_elephants, 6)
                  .empty());
  EXPECT_FALSE(
      registry.availability_reason("rome", ArmyFormationIntent::TriplexAcies, infantry, 2)
          .empty());
}

} // namespace
