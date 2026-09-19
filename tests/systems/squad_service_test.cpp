#include <algorithm>
#include <cstdlib>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "game/core/component_core.h"
#include "game/core/world.h"
#include "game/map/map_transformer.h"
#include "game/session/session_context.h"
#include "game/systems/nav_grid.h"
#include "game/systems/squad_service.h"
#include "game/systems/troop_count_registry.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"
#include "game/units/squad.h"

namespace {

using Engine::Core::EntityID;
using Engine::Core::UnitComponent;
using Game::Systems::SquadService;
using Game::Units::SpawnType;

constexpr int k_owner = 2;

class SquadServiceTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::NavGrid::initialize(64, 64);
    m_factory = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*m_factory);

    m_session = std::make_unique<Game::Session::SessionContext>();
    m_session->world().set_presentation_enabled(false);
    m_scope = std::make_unique<Game::Session::ScopedSession>(*m_session);
    Game::Map::MapTransformer::setFactoryRegistry(m_factory);
  }

  void TearDown() override {
    Game::Map::MapTransformer::setFactoryRegistry(nullptr);
    m_scope.reset();
    m_session.reset();
  }

  auto spawn(SpawnType type, float x, float z) -> EntityID {
    Game::Units::SpawnParams params;
    params.position = QVector3D(x, 0.0F, z);
    params.player_id = k_owner;
    params.spawn_type = type;
    params.is_initial_spawn = true;
    auto unit = m_factory->create(type, m_session->world(), params);
    return unit ? unit->id() : 0;
  }

  auto unit_of(EntityID id) -> UnitComponent* {
    return m_session->world().try_get<UnitComponent>(id);
  }

  void wound(EntityID id, float remaining) {
    auto* unit = unit_of(id);
    ASSERT_NE(unit, nullptr);
    unit->health =
        std::max(1, static_cast<int>(static_cast<float>(unit->max_health) * remaining));
  }

  auto survivors_of(EntityID id) -> int {
    const auto* unit = unit_of(id);
    return unit != nullptr ? Game::Units::squad_survivors(*unit) : 0;
  }

  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory;
  std::unique_ptr<Game::Session::SessionContext> m_session;
  std::unique_ptr<Game::Session::ScopedSession> m_scope;
};

TEST_F(SquadServiceTest, DividingASquadHalvesItsMenAndItsHealthPool) {
  const auto id = spawn(SpawnType::Builder, 10.0F, 10.0F);
  ASSERT_NE(id, 0U);
  const auto* before = unit_of(id);
  ASSERT_NE(before, nullptr);
  const int establishment = Game::Units::squad_establishment(SpawnType::Builder);
  const int full_health = before->max_health;
  ASSERT_GE(establishment, 4);

  const auto division = SquadService::divide(m_session->world(), id);
  ASSERT_NE(division.detachment, 0U);

  const auto* parent = unit_of(division.parent);
  const auto* detachment = unit_of(division.detachment);
  ASSERT_NE(parent, nullptr);
  ASSERT_NE(detachment, nullptr);

  EXPECT_EQ(Game::Units::squad_strength(*parent) +
                Game::Units::squad_strength(*detachment),
            establishment);
  EXPECT_NEAR(static_cast<float>(parent->max_health + detachment->max_health),
              static_cast<float>(full_health),
              2.0F)
      << "the two halves must share one establishment's worth of health";
  EXPECT_GT(parent->health, 0);
  EXPECT_GT(detachment->health, 0);
}

TEST_F(SquadServiceTest, ASquadTooSmallToHalveIsLeftAlone) {
  const auto id = spawn(SpawnType::Builder, 10.0F, 10.0F);
  ASSERT_NE(id, 0U);
  SquadService::apply_strength(m_session->world(), id, 2);

  EXPECT_FALSE(SquadService::can_divide(m_session->world(), id));
  const auto division = SquadService::divide(m_session->world(), id);
  EXPECT_EQ(division.detachment, 0U);
}

TEST_F(SquadServiceTest, ACommanderIsNeverDivided) {
  const auto id = spawn(SpawnType::RomanVeteranConsul, 10.0F, 10.0F);
  ASSERT_NE(id, 0U);
  EXPECT_FALSE(SquadService::can_divide(m_session->world(), id));
}

TEST_F(SquadServiceTest, JoiningTwoDecimatedSquadsRebuildsOne) {
  const auto left = spawn(SpawnType::Swordsman, 10.0F, 10.0F);
  const auto right = spawn(SpawnType::Swordsman, 12.0F, 10.0F);
  ASSERT_NE(left, 0U);
  ASSERT_NE(right, 0U);

  const int establishment = Game::Units::squad_establishment(SpawnType::Swordsman);
  SquadService::apply_strength(m_session->world(), left, establishment / 3);
  SquadService::apply_strength(m_session->world(), right, establishment / 3);

  ASSERT_TRUE(SquadService::can_merge(m_session->world(), left, right));
  ASSERT_TRUE(SquadService::merge(m_session->world(), left, right));

  const auto* kept = unit_of(left);
  ASSERT_NE(kept, nullptr);
  EXPECT_EQ(Game::Units::squad_strength(*kept), 2 * (establishment / 3));
  EXPECT_EQ(unit_of(right), nullptr) << "the absorbed squad must be gone";
}

TEST_F(SquadServiceTest, SquadsBatteredInBattleCanJoin) {
  const auto left = spawn(SpawnType::Swordsman, 10.0F, 10.0F);
  const auto right = spawn(SpawnType::Swordsman, 12.0F, 10.0F);
  ASSERT_NE(left, 0U);
  ASSERT_NE(right, 0U);
  const int establishment = Game::Units::squad_establishment(SpawnType::Swordsman);
  const int full_health = unit_of(left)->max_health;
  wound(left, 0.3F);
  wound(right, 0.3F);
  const int survivors = survivors_of(left) + survivors_of(right);
  ASSERT_LT(survivors, establishment);

  ASSERT_TRUE(SquadService::can_merge(m_session->world(), left, right))
      << "squads that lost men in battle are below strength";
  ASSERT_TRUE(SquadService::merge(m_session->world(), left, right));

  const auto* kept = unit_of(left);
  ASSERT_NE(kept, nullptr);
  EXPECT_EQ(unit_of(right), nullptr);
  EXPECT_EQ(Game::Units::squad_strength(*kept), survivors);
  EXPECT_EQ(survivors_of(left), survivors) << "no man is lost or invented";
  EXPECT_LE(kept->health, full_health);
}

TEST_F(SquadServiceTest, AFullSquadHasNoRoomForMoreMen) {
  const auto full = spawn(SpawnType::Swordsman, 10.0F, 10.0F);
  const auto battered = spawn(SpawnType::Swordsman, 12.0F, 10.0F);
  wound(battered, 0.3F);

  EXPECT_FALSE(SquadService::can_merge(m_session->world(), full, battered));
  EXPECT_TRUE(SquadService::merge_all(m_session->world(), {full, battered}).empty());
  EXPECT_NE(unit_of(battered), nullptr);
}

TEST_F(SquadServiceTest, ANearlyFullSquadIsToppedUpFromABatteredOne) {
  const int establishment = Game::Units::squad_establishment(SpawnType::Swordsman);
  ASSERT_GE(establishment, 6);
  const auto nearly = spawn(SpawnType::Swordsman, 10.0F, 10.0F);
  const auto battered = spawn(SpawnType::Swordsman, 12.0F, 10.0F);
  SquadService::apply_strength(m_session->world(), nearly, establishment - 1);
  SquadService::apply_strength(m_session->world(), battered, 3);
  const int men = survivors_of(nearly) + survivors_of(battered);

  EXPECT_TRUE(SquadService::can_merge(m_session->world(), battered, nearly));
  ASSERT_FALSE(SquadService::merge_all(m_session->world(), {battered, nearly}).empty());
  EXPECT_EQ(survivors_of(nearly), establishment) << "the fuller squad fills up first";
  EXPECT_EQ(survivors_of(battered), men - establishment);
}

TEST_F(SquadServiceTest, TwoFullSquadsDoNotJoin) {
  const auto left = spawn(SpawnType::Swordsman, 10.0F, 10.0F);
  const auto right = spawn(SpawnType::Swordsman, 12.0F, 10.0F);
  EXPECT_FALSE(SquadService::can_merge(m_session->world(), left, right));
}

TEST_F(SquadServiceTest, DividingABatteredSquadSplitsItsSurvivors) {
  const auto id = spawn(SpawnType::Swordsman, 10.0F, 10.0F);
  wound(id, 0.5F);
  const int survivors = survivors_of(id);
  ASSERT_GE(survivors, 4);

  const auto division = SquadService::divide(m_session->world(), id);
  ASSERT_NE(division.detachment, 0U);
  EXPECT_EQ(survivors_of(division.parent) + survivors_of(division.detachment),
            survivors);
  EXPECT_LE(std::abs(survivors_of(division.parent) - survivors_of(division.detachment)),
            1);
}

TEST_F(SquadServiceTest, ASquadWithTooFewSurvivorsCannotDivide) {
  const auto id = spawn(SpawnType::Swordsman, 10.0F, 10.0F);
  wound(id, 0.15F);
  ASSERT_LT(survivors_of(id), 4);
  EXPECT_FALSE(SquadService::can_divide(m_session->world(), id));
}

TEST_F(SquadServiceTest, ThreeBatteredSquadsFoldIntoAsFewAsTheirMenFill) {
  const int establishment = Game::Units::squad_establishment(SpawnType::Swordsman);
  std::vector<EntityID> squads;
  for (int i = 0; i < 3; ++i) {
    squads.push_back(spawn(SpawnType::Swordsman, 10.0F + (2.0F * i), 10.0F));
    wound(squads.back(), 0.25F);
  }
  int men = 0;
  for (const auto id : squads) {
    men += survivors_of(id);
  }

  const auto merges = SquadService::merge_all(m_session->world(), squads);
  ASSERT_FALSE(merges.empty());

  int remaining_squads = 0;
  int remaining_men = 0;
  for (const auto id : squads) {
    if (unit_of(id) != nullptr) {
      ++remaining_squads;
      remaining_men += survivors_of(id);
      EXPECT_LE(survivors_of(id), establishment);
    }
  }
  EXPECT_EQ(remaining_men, men);
  EXPECT_EQ(remaining_squads, (men + establishment - 1) / establishment);
}

TEST_F(SquadServiceTest, MoreMenThanOneSquadHoldsLeaveAFullSquadAndARemainder) {
  const int establishment = Game::Units::squad_establishment(SpawnType::Swordsman);
  ASSERT_GE(establishment, 6);
  const auto left = spawn(SpawnType::Swordsman, 10.0F, 10.0F);
  const auto right = spawn(SpawnType::Swordsman, 12.0F, 10.0F);
  const int short_by = 2;
  SquadService::apply_strength(m_session->world(), left, establishment - short_by);
  SquadService::apply_strength(m_session->world(), right, establishment - short_by);
  const int men = survivors_of(left) + survivors_of(right);
  ASSERT_GT(men, establishment);

  ASSERT_FALSE(SquadService::merge_all(m_session->world(), {left, right}).empty());

  ASSERT_NE(unit_of(left), nullptr);
  ASSERT_NE(unit_of(right), nullptr) << "the men who do not fit stay a squad";
  EXPECT_EQ(survivors_of(left), establishment);
  EXPECT_EQ(survivors_of(right), men - establishment);
  EXPECT_TRUE(Game::Units::squad_is_at_full_strength(*unit_of(left)));
}

TEST_F(SquadServiceTest, JoiningFullSquadsChangesNothing) {
  std::vector<EntityID> squads{spawn(SpawnType::Swordsman, 10.0F, 10.0F),
                               spawn(SpawnType::Swordsman, 12.0F, 10.0F),
                               spawn(SpawnType::Swordsman, 14.0F, 10.0F)};
  EXPECT_TRUE(SquadService::plan_joins(m_session->world(), squads).empty());
  EXPECT_TRUE(SquadService::merge_all(m_session->world(), squads).empty());
  for (const auto id : squads) {
    EXPECT_NE(unit_of(id), nullptr);
  }
}

TEST_F(SquadServiceTest, AChainOfSquadsJoinsAsOneGroup) {
  const float step = SquadService::k_merge_radius * 0.8F;
  std::vector<EntityID> squads;
  for (int i = 0; i < 3; ++i) {
    squads.push_back(spawn(SpawnType::Swordsman, 10.0F + (step * i), 10.0F));
    SquadService::apply_strength(m_session->world(), squads.back(), 2);
  }
  const auto plans = SquadService::plan_joins(m_session->world(), squads);
  ASSERT_EQ(plans.size(), 1U)
      << "the ends are out of reach of each other, not of the middle";
  EXPECT_EQ(plans.front().members.size(), 3U);
  EXPECT_EQ(plans.front().rosters.size(), 1U);
}

TEST_F(SquadServiceTest, SquadsFarApartJoinOnlyWithTheirNeighbours) {
  const float far = SquadService::k_merge_radius * 4.0F;
  const auto a = spawn(SpawnType::Swordsman, 10.0F, 10.0F);
  const auto b = spawn(SpawnType::Swordsman, 12.0F, 10.0F);
  const auto c = spawn(SpawnType::Swordsman, 10.0F + far, 10.0F);
  const auto d = spawn(SpawnType::Swordsman, 12.0F + far, 10.0F);
  for (const auto id : {a, b, c, d}) {
    SquadService::apply_strength(m_session->world(), id, 2);
  }
  const auto plans = SquadService::plan_joins(m_session->world(), {a, b, c, d});
  ASSERT_EQ(plans.size(), 2U);
  for (const auto& plan : plans) {
    EXPECT_EQ(plan.members.size(), 2U);
  }
}

TEST_F(SquadServiceTest, AMixedSelectionJoinsEachKindWithItsOwn) {
  const auto sword_a = spawn(SpawnType::Swordsman, 10.0F, 10.0F);
  const auto sword_b = spawn(SpawnType::Swordsman, 11.0F, 10.0F);
  const auto spear_a = spawn(SpawnType::Spearman, 12.0F, 10.0F);
  const auto spear_b = spawn(SpawnType::Spearman, 13.0F, 10.0F);
  for (const auto id : {sword_a, sword_b, spear_a, spear_b}) {
    SquadService::apply_strength(m_session->world(), id, 2);
  }
  SquadService::merge_all(m_session->world(), {sword_a, spear_a, sword_b, spear_b});
  EXPECT_NE(unit_of(sword_a), nullptr);
  EXPECT_EQ(unit_of(sword_b), nullptr);
  EXPECT_NE(unit_of(spear_a), nullptr);
  EXPECT_EQ(unit_of(spear_b), nullptr);
  EXPECT_EQ(unit_of(sword_a)->spawn_type, SpawnType::Swordsman);
  EXPECT_EQ(survivors_of(sword_a), 4);
  EXPECT_EQ(survivors_of(spear_a), 4);
}

TEST_F(SquadServiceTest, ASquadListedTwiceIsJoinedOnce) {
  const auto a = spawn(SpawnType::Swordsman, 10.0F, 10.0F);
  const auto b = spawn(SpawnType::Swordsman, 12.0F, 10.0F);
  SquadService::apply_strength(m_session->world(), a, 3);
  SquadService::apply_strength(m_session->world(), b, 3);
  SquadService::merge_all(m_session->world(), {a, a, b, b, a});
  ASSERT_NE(unit_of(a), nullptr);
  EXPECT_EQ(unit_of(b), nullptr);
  EXPECT_EQ(survivors_of(a), 6);
}

TEST_F(SquadServiceTest, EnemySquadsAndBuildingsNeverJoin) {
  const auto mine = spawn(SpawnType::Swordsman, 10.0F, 10.0F);
  Game::Units::SpawnParams params;
  params.position = QVector3D(12.0F, 0.0F, 10.0F);
  params.player_id = k_owner + 1;
  params.spawn_type = SpawnType::Swordsman;
  params.is_initial_spawn = true;
  const auto theirs =
      m_factory->create(SpawnType::Swordsman, m_session->world(), params);
  ASSERT_TRUE(theirs);
  SquadService::apply_strength(m_session->world(), mine, 2);
  SquadService::apply_strength(m_session->world(), theirs->id(), 2);
  EXPECT_TRUE(
      SquadService::plan_joins(m_session->world(), {mine, theirs->id()}).empty());
}

TEST(SquadRosterTest, PackingKeepsEveryManAndShowsEachSquadItsExactCount) {
  for (int establishment = 1; establishment <= 20; ++establishment) {
    for (int men = 1; men <= 70; ++men) {
      const int per_man = 7 + (establishment % 5);
      const int establishment_max_health = per_man * establishment;
      for (const int health : {1,
                               men,
                               (men * per_man) / 3,
                               (men * per_man) / 2,
                               men * per_man - 1,
                               men * per_man}) {
        const auto rosters =
            SquadService::pack(men, health, establishment, establishment_max_health);
        ASSERT_EQ(static_cast<int>(rosters.size()),
                  (men + establishment - 1) / establishment);
        int packed_men = 0;
        int packed_health = 0;
        for (std::size_t i = 0; i < rosters.size(); ++i) {
          const auto& roster = rosters[i];
          ASSERT_GE(roster.men, 1);
          ASSERT_LE(roster.men, establishment);
          if (i + 1 < rosters.size()) {
            ASSERT_EQ(roster.men, establishment) << "only the last squad is short";
          }
          const int ceiling =
              std::max(1,
                       static_cast<int>(
                           std::lround(static_cast<double>(establishment_max_health) *
                                       roster.men / establishment)));
          ASSERT_GE(roster.health, 1);
          ASSERT_LE(roster.health, ceiling);
          ASSERT_EQ(Engine::Core::resolve_surviving_individual_count(
                        roster.health, ceiling, roster.men),
                    roster.men)
              << "establishment " << establishment << " men " << men << " health "
              << health;
          packed_men += roster.men;
          packed_health += roster.health;
        }
        ASSERT_EQ(packed_men, men);
        int floor_sum = 0;
        int ceiling_sum = 0;
        for (const auto& roster : rosters) {
          const int ceiling =
              std::max(1,
                       static_cast<int>(
                           std::lround(static_cast<double>(establishment_max_health) *
                                       roster.men / establishment)));
          floor_sum += ((roster.men - 1) * ceiling) / roster.men + 1;
          ceiling_sum += ceiling;
        }
        ASSERT_EQ(packed_health, std::clamp(health, floor_sum, ceiling_sum))
            << "health is kept unless a squad would otherwise lose a man";
      }
    }
  }
}

TEST(SquadRosterTest, AJoinOfWoundedSquadsKeepsEveryManStanding) {
  // Three squads of four, each with one man badly hurt: 3.1 men of health.
  const auto rosters = SquadService::share_health({12}, 3 * 31, 12, 120);
  ASSERT_EQ(rosters.size(), 1U);
  EXPECT_EQ(Engine::Core::resolve_surviving_individual_count(
                rosters[0].health, 120, rosters[0].men),
            12);
  EXPECT_LE(rosters[0].health, 120);
}

TEST(SquadRosterTest, HealthIsSharedInProportionToMen) {
  const auto rosters = SquadService::share_health({6, 6}, 230, 12, 240);
  ASSERT_EQ(rosters.size(), 2U);
  EXPECT_EQ(rosters[0].health + rosters[1].health, 230);
  EXPECT_LE(std::abs(rosters[0].health - rosters[1].health), 1);
}

TEST_F(SquadServiceTest, SquadsOfDifferentKindsDoNotJoin) {
  const auto sword = spawn(SpawnType::Swordsman, 10.0F, 10.0F);
  const auto spear = spawn(SpawnType::Spearman, 12.0F, 10.0F);
  SquadService::apply_strength(m_session->world(), sword, 4);
  SquadService::apply_strength(m_session->world(), spear, 4);

  EXPECT_FALSE(SquadService::can_merge(m_session->world(), sword, spear));
}

TEST_F(SquadServiceTest, SquadsTooFarApartDoNotJoin) {
  const auto near_unit = spawn(SpawnType::Swordsman, 10.0F, 10.0F);
  const auto far_unit =
      spawn(SpawnType::Swordsman, 10.0F + SquadService::k_merge_radius + 5.0F, 10.0F);
  SquadService::apply_strength(m_session->world(), near_unit, 4);
  SquadService::apply_strength(m_session->world(), far_unit, 4);

  EXPECT_FALSE(SquadService::can_merge(m_session->world(), near_unit, far_unit));
}

TEST_F(SquadServiceTest, DividingCostsNoExtraPopulation) {
  const auto id = spawn(SpawnType::Swordsman, 10.0F, 10.0F);
  ASSERT_NE(id, 0U);
  auto& counts = m_session->troop_counts();
  counts.rebuild_from_world(m_session->world());
  const int before = counts.get_troop_count(k_owner);
  ASSERT_GT(before, 0);

  const auto division = SquadService::divide(m_session->world(), id);
  ASSERT_NE(division.detachment, 0U);
  counts.rebuild_from_world(m_session->world());

  EXPECT_NEAR(counts.get_troop_count(k_owner), before, 1)
      << "two halves must cost what the whole squad cost";
}

TEST_F(SquadServiceTest, AHalfSquadCountsHalfTheStrengthAndHalfThePopulation) {
  const auto id = spawn(SpawnType::Archer, 10.0F, 10.0F);
  ASSERT_NE(id, 0U);
  const auto* unit = unit_of(id);
  ASSERT_NE(unit, nullptr);
  const int full_population = Game::Units::squad_population_cost(*unit);

  const int establishment = Game::Units::squad_establishment(SpawnType::Archer);
  SquadService::apply_strength(m_session->world(), id, establishment / 2);

  const auto* halved = unit_of(id);
  ASSERT_NE(halved, nullptr);
  EXPECT_NEAR(Game::Units::squad_fraction(*halved), 0.5F, 0.05F);
  EXPECT_LT(Game::Units::squad_population_cost(*halved), full_population);
}

} // namespace
