#include <gtest/gtest.h>
#include <memory>

#include "game/core/component.h"
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
    Game::Map::MapTransformer::setFactoryRegistry(m_factory);

    m_session = std::make_unique<Game::Session::SessionContext>();
    m_session->world().set_presentation_enabled(false);
    m_scope = std::make_unique<Game::Session::ScopedSession>(*m_session);
  }

  void TearDown() override {
    m_scope.reset();
    m_session.reset();
    Game::Map::MapTransformer::setFactoryRegistry(nullptr);
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
  const auto left = spawn(SpawnType::Knight, 10.0F, 10.0F);
  const auto right = spawn(SpawnType::Knight, 12.0F, 10.0F);
  ASSERT_NE(left, 0U);
  ASSERT_NE(right, 0U);

  const int establishment = Game::Units::squad_establishment(SpawnType::Knight);
  SquadService::apply_strength(m_session->world(), left, establishment / 3);
  SquadService::apply_strength(m_session->world(), right, establishment / 3);

  ASSERT_TRUE(SquadService::can_merge(m_session->world(), left, right));
  ASSERT_TRUE(SquadService::merge(m_session->world(), left, right));

  const auto* kept = unit_of(left);
  ASSERT_NE(kept, nullptr);
  EXPECT_EQ(Game::Units::squad_strength(*kept), 2 * (establishment / 3));
  EXPECT_EQ(unit_of(right), nullptr) << "the absorbed squad must be gone";
}

TEST_F(SquadServiceTest, SquadsOfDifferentKindsDoNotJoin) {
  const auto sword = spawn(SpawnType::Knight, 10.0F, 10.0F);
  const auto spear = spawn(SpawnType::Spearman, 12.0F, 10.0F);
  SquadService::apply_strength(m_session->world(), sword, 4);
  SquadService::apply_strength(m_session->world(), spear, 4);

  EXPECT_FALSE(SquadService::can_merge(m_session->world(), sword, spear));
}

TEST_F(SquadServiceTest, SquadsTooFarApartDoNotJoin) {
  const auto near_unit = spawn(SpawnType::Knight, 10.0F, 10.0F);
  const auto far_unit =
      spawn(SpawnType::Knight, 10.0F + SquadService::k_merge_radius + 5.0F, 10.0F);
  SquadService::apply_strength(m_session->world(), near_unit, 4);
  SquadService::apply_strength(m_session->world(), far_unit, 4);

  EXPECT_FALSE(SquadService::can_merge(m_session->world(), near_unit, far_unit));
}

TEST_F(SquadServiceTest, DividingCostsNoExtraPopulation) {
  const auto id = spawn(SpawnType::Knight, 10.0F, 10.0F);
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
