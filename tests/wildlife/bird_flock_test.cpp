#include <cmath>
#include <gtest/gtest.h>

#include "game/wildlife/bird_flock.h"
#include "game/wildlife/wildlife_config.h"
#include "game/wildlife/wildlife_terrain_probe.h"
#include "game/wildlife/wildlife_threats.h"

namespace {

using Game::Wildlife::Behavior;
using Game::Wildlife::BirdFlockManager;
using Game::Wildlife::BirdTier;
using Game::Wildlife::SpeciesConfig;
using Game::Wildlife::ThreatField;
using Game::Wildlife::ThreatSource;
using Game::Wildlife::WorldBounds;

auto make_config(int flocks, int size) -> SpeciesConfig {
  SpeciesConfig config = Game::Wildlife::default_bird_config();
  config.group_count = flocks;
  config.group_size_min = size;
  config.group_size_max = size;
  config.roam_radius = 16.0F;
  config.alert_radius = 10.0F;
  config.spawn_areas = {{0.0F, 0.0F, 4.0F}};
  return config;
}

class BirdFlockTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_probe = std::make_unique<Game::Wildlife::FlatTerrainProbe>(
        WorldBounds{-60.0F, -60.0F, 60.0F, 60.0F}, 0.0F);
    BirdFlockManager::instance().set_terrain_probe(m_probe.get());
    BirdFlockManager::instance().reset();
  }

  void TearDown() override {
    BirdFlockManager::instance().reset();
    BirdFlockManager::instance().set_terrain_probe(nullptr);
  }

  static void advance(float seconds, const ThreatField& threats) {
    const float step = 1.0F / 30.0F;
    for (float elapsed = 0.0F; elapsed < seconds; elapsed += step) {
      BirdFlockManager::instance().update(step, threats);
    }
  }

  std::unique_ptr<Game::Wildlife::FlatTerrainProbe> m_probe;
};

TEST_F(BirdFlockTest, ConfigureSpawnsRequestedFlockPopulation) {
  BirdFlockManager::instance().configure(make_config(2, 5), 11U, 40.0F, 80.0F);

  EXPECT_EQ(BirdFlockManager::instance().flocks().size(), 2U);
  EXPECT_EQ(BirdFlockManager::instance().birds().size(), 10U);
  for (const auto& bird : BirdFlockManager::instance().birds()) {
    EXPECT_LT(bird.flock, 2U);
  }
}

TEST_F(BirdFlockTest, DisabledConfigurationSpawnsNothing) {
  SpeciesConfig config = make_config(2, 5);
  config.enabled = false;
  BirdFlockManager::instance().configure(config, 11U, 40.0F, 80.0F);

  EXPECT_TRUE(BirdFlockManager::instance().birds().empty());
  EXPECT_FALSE(BirdFlockManager::instance().is_enabled());
}

TEST_F(BirdFlockTest, BirdsScatterAwayFromThreats) {
  BirdFlockManager::instance().configure(make_config(1, 6), 7U, 200.0F, 400.0F);
  ThreatField quiet;
  quiet.finalize();
  advance(2.0F, quiet);

  ThreatField threats;
  threats.add(ThreatSource{0.0F, 0.0F, 1.0F, false});
  threats.finalize();

  float const before = [] {
    float sum = 0.0F;
    for (const auto& bird : BirdFlockManager::instance().birds()) {
      sum += std::sqrt((bird.x * bird.x) + (bird.z * bird.z));
    }
    return sum;
  }();

  advance(2.5F, threats);

  int scattering = 0;
  float after = 0.0F;
  for (const auto& bird : BirdFlockManager::instance().birds()) {
    if (bird.behavior == Behavior::Scatter) {
      ++scattering;
    }
    after += std::sqrt((bird.x * bird.x) + (bird.z * bird.z));
  }

  EXPECT_GT(scattering, 0);
  EXPECT_GT(after, before);
  EXPECT_GT(BirdFlockManager::instance().stats().scatter_events, 0U);
}

TEST_F(BirdFlockTest, BirdsStayInsideWorldBounds) {
  BirdFlockManager::instance().configure(make_config(1, 8), 3U, 400.0F, 800.0F);
  ThreatField threats;
  threats.add(ThreatSource{0.0F, 0.0F, 1.0F, false});
  threats.finalize();
  advance(30.0F, threats);

  const WorldBounds bounds = m_probe->bounds();
  for (const auto& bird : BirdFlockManager::instance().birds()) {
    EXPECT_GE(bird.x, bounds.min_x);
    EXPECT_LE(bird.x, bounds.max_x);
    EXPECT_GE(bird.z, bounds.min_z);
    EXPECT_LE(bird.z, bounds.max_z);
    EXPECT_GE(bird.y, -0.01F);
  }
}

TEST_F(BirdFlockTest, DistantBirdsStopSimulating) {
  BirdFlockManager::instance().configure(make_config(1, 4), 5U, 10.0F, 20.0F);
  ThreatField threats;
  threats.finalize();

  BirdFlockManager::instance().set_focus(500.0F, 500.0F);
  const auto before = BirdFlockManager::instance().birds();
  advance(3.0F, threats);
  const auto& after = BirdFlockManager::instance().birds();

  ASSERT_EQ(before.size(), after.size());
  for (std::size_t index = 0; index < after.size(); ++index) {
    EXPECT_EQ(after[index].tier, BirdTier::Dormant);
    EXPECT_FLOAT_EQ(after[index].x, before[index].x);
    EXPECT_FLOAT_EQ(after[index].z, before[index].z);
  }
  EXPECT_GT(BirdFlockManager::instance().stats().dormant_skips, 0U);
  EXPECT_EQ(BirdFlockManager::instance().stats().near_updates, 0U);
}

TEST_F(BirdFlockTest, NearBirdsKeepSimulating) {
  BirdFlockManager::instance().configure(make_config(1, 4), 5U, 40.0F, 80.0F);
  ThreatField threats;
  threats.finalize();

  BirdFlockManager::instance().set_focus(0.0F, 0.0F);
  advance(3.0F, threats);

  EXPECT_GT(BirdFlockManager::instance().stats().near_updates, 0U);
  EXPECT_EQ(BirdFlockManager::instance().stats().dormant_skips, 0U);
}

TEST_F(BirdFlockTest, SnapshotRestoreKeepsPopulation) {
  BirdFlockManager::instance().configure(make_config(1, 5), 9U, 60.0F, 120.0F);
  ThreatField threats;
  threats.finalize();
  advance(4.0F, threats);

  const auto snapshot = BirdFlockManager::instance().snapshot();
  BirdFlockManager::instance().reset();
  ASSERT_TRUE(BirdFlockManager::instance().birds().empty());

  BirdFlockManager::instance().restore(snapshot);
  ASSERT_EQ(BirdFlockManager::instance().birds().size(), snapshot.birds.size());
  for (std::size_t index = 0; index < snapshot.birds.size(); ++index) {
    EXPECT_FLOAT_EQ(BirdFlockManager::instance().birds()[index].x,
                    snapshot.birds[index].x);
    EXPECT_FLOAT_EQ(BirdFlockManager::instance().birds()[index].z,
                    snapshot.birds[index].z);
    EXPECT_EQ(BirdFlockManager::instance().birds()[index].behavior,
              snapshot.birds[index].behavior);
  }
}

TEST_F(BirdFlockTest, PublishedFrameTracksLatestPositions) {
  BirdFlockManager::instance().configure(make_config(1, 3), 13U, 60.0F, 120.0F);
  ThreatField threats;
  threats.finalize();
  advance(1.0F, threats);

  const auto frame = BirdFlockManager::instance().frame();
  ASSERT_NE(frame, nullptr);
  ASSERT_EQ(frame->birds.size(), BirdFlockManager::instance().birds().size());
  for (std::size_t index = 0; index < frame->birds.size(); ++index) {
    EXPECT_FLOAT_EQ(frame->birds[index].x,
                    BirdFlockManager::instance().birds()[index].x);
  }
}

} // namespace
