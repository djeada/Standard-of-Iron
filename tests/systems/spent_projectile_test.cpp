#include <QVector3D>

#include <cmath>
#include <gtest/gtest.h>

#include "core/world.h"
#include "systems/projectile_system.h"

namespace {

auto fire_and_land(Game::Systems::ProjectileSystem& system,
                   Engine::Core::World& world,
                   Game::Systems::ProjectileKind kind,
                   bool ballista_bolt = false) -> void {
  system.spawn_arrow(QVector3D(0.0F, 1.6F, 0.0F),
                     QVector3D(0.0F, 1.2F, 9.0F),
                     QVector3D(0.8F, 0.2F, 0.15F),
                     30.0F,
                     ballista_bolt,
                     kind);
  for (int step = 0; step < 60 && system.projectiles().size() > 0U; ++step) {
    system.update(&world, 0.05F);
  }
}

TEST(SpentProjectileTest, LandedArrowsStayPlantedInTheGround) {
  Engine::Core::World world;
  Game::Systems::ProjectileSystem system;

  fire_and_land(system, world, Game::Systems::ProjectileKind::Arrow);

  ASSERT_EQ(system.spent_projectiles().size(), 1U);
  const auto& spent = system.spent_projectiles().front();

  EXPECT_NEAR(spent.position.y(), 0.0F, 1.0e-3F);
  EXPECT_LT(spent.direction.y(), -0.4F);
  EXPECT_NEAR(spent.direction.length(), 1.0F, 1.0e-4F);
  EXPECT_GT(spent.embed, 0.0F);
  EXPECT_GT(spent.lifetime, 5.0F);
  EXPECT_NEAR(Game::Systems::spent_projectile_alpha(spent), 1.0F, 1.0e-4F);
}

TEST(SpentProjectileTest, StonesAndFireballsLeaveNoShaft) {
  Engine::Core::World world;
  Game::Systems::ProjectileSystem system;

  fire_and_land(system, world, Game::Systems::ProjectileKind::Fireball);
  system.spawn_stone(QVector3D(0.0F, 1.6F, 0.0F),
                     QVector3D(0.0F, 0.0F, 6.0F),
                     QVector3D(0.5F, 0.5F, 0.5F),
                     30.0F);
  for (int step = 0; step < 60 && !system.projectiles().empty(); ++step) {
    system.update(&world, 0.05F);
  }

  EXPECT_TRUE(system.spent_projectiles().empty());
}

TEST(SpentProjectileTest, SpentArrowsFadeOutAndExpire) {
  Engine::Core::World world;
  Game::Systems::ProjectileSystem system;

  fire_and_land(system, world, Game::Systems::ProjectileKind::Arrow);
  ASSERT_EQ(system.spent_projectiles().size(), 1U);

  float const lifetime = system.spent_projectiles().front().lifetime;
  float elapsed = 0.0F;
  float last_alpha = 1.0F;
  while (elapsed < lifetime - 1.0F) {
    system.update(&world, 0.5F);
    elapsed += 0.5F;
    ASSERT_EQ(system.spent_projectiles().size(), 1U);
    float const alpha =
        Game::Systems::spent_projectile_alpha(system.spent_projectiles().front());
    EXPECT_LE(alpha, last_alpha + 1.0e-4F);
    last_alpha = alpha;
  }
  EXPECT_LT(last_alpha, 1.0F);

  for (int step = 0; step < 10; ++step) {
    system.update(&world, 0.5F);
  }
  EXPECT_TRUE(system.spent_projectiles().empty());
}

TEST(SpentProjectileTest, SpentArrowCountIsBounded) {
  Engine::Core::World world;
  Game::Systems::ProjectileSystem system;

  for (std::size_t shot = 0; shot < Game::Systems::k_max_spent_projectiles + 25U;
       ++shot) {
    fire_and_land(system, world, Game::Systems::ProjectileKind::Arrow);
  }

  EXPECT_LE(system.spent_projectiles().size(), Game::Systems::k_max_spent_projectiles);
  EXPECT_GT(system.spent_projectiles().size(), 0U);
}

} // namespace
