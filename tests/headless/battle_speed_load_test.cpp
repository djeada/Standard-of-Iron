#include <QtGlobal>

#include <chrono>
#include <cstdio>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "game/command/command.h"
#include "game/command/command_queue.h"
#include "game/core/component_combat.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/session/world_digest.h"
#include "game/systems/default_content.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/runtime_system_registry.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"

namespace {

using Engine::Core::TransformComponent;
using Engine::Core::UnitComponent;
using Game::Session::ScopedSession;
using Game::Session::SessionContext;

constexpr int k_map_size = 96;
constexpr int k_left = 1;
constexpr int k_right = 2;
constexpr int k_default_ranks = 3;
constexpr int k_files = 12;
constexpr int k_base_steps_per_frame = 8;
constexpr int k_step_ceiling = 64;

auto ranks_per_side() -> int {
  bool ok = false;
  const int requested = qEnvironmentVariableIntValue("SOI_SPEED_LOAD_RANKS", &ok);
  return (ok && requested > 0) ? requested : k_default_ranks;
}

auto step_budget(double time_scale) -> int {
  const double scaled = std::ceil(static_cast<double>(k_base_steps_per_frame) *
                                  std::max(1.0, time_scale));
  return std::clamp(static_cast<int>(scaled), k_base_steps_per_frame, k_step_ceiling);
}

struct LoadResult {
  std::uint64_t ticks = 0;
  std::uint64_t dropped = 0;
  std::uint64_t digest = 0;
  double wall_seconds = 0.0;
  std::size_t units = 0;
};

class BattleSpeedLoadTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::NavGrid::initialize(k_map_size, k_map_size);
    m_factory = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*m_factory);
  }

  static void
  muster(SessionContext& session, int owner_id, float origin_z, float facing_z) {
    for (int rank = 0; rank < ranks_per_side(); ++rank) {
      for (int file = 0; file < k_files; ++file) {
        auto* entity = session.world().create_entity();
        auto* transform = entity->add_component<TransformComponent>();
        transform->position.x = 12.0F + static_cast<float>(file) * 1.4F;
        transform->position.z = origin_z + static_cast<float>(rank) * 1.4F * facing_z;
        auto* unit = entity->add_component<UnitComponent>(120, 120, 2.4F, 14.0F);
        unit->owner_id = owner_id;
        unit->spawn_type = (file % 4 == 0) ? Game::Units::SpawnType::Archer
                                           : Game::Units::SpawnType::Spearman;
        entity->add_component<Engine::Core::MovementComponent>();
        entity->add_component<Engine::Core::AttackComponent>(12.0F, 8.0F, 1.0F);
      }
    }
  }

  auto make_battle() -> std::unique_ptr<SessionContext> {
    auto session = std::make_unique<SessionContext>();
    auto& owners = session->owners();
    owners.register_owner_with_id(k_left, Game::Systems::OwnerType::Player, "left");
    owners.register_owner_with_id(k_right, Game::Systems::OwnerType::AI, "right");
    owners.set_owner_team(k_left, 1);
    owners.set_owner_team(k_right, 2);
    Game::Systems::initialize_default_content(session->nations());
    Game::Systems::register_runtime_systems(session->world());

    Game::Map::MapDefinition map_definition;
    map_definition.grid.width = k_map_size;
    map_definition.grid.height = k_map_size;
    map_definition.grid.tile_size = 1.0F;
    session->terrain().initialize(map_definition);

    muster(*session, k_left, 40.0F, -1.0F);
    muster(*session, k_right, 56.0F, 1.0F);
    return session;
  }

  auto run_battle(double time_scale, double real_dt, int frames) -> LoadResult {
    auto session = make_battle();
    const ScopedSession scope(*session);

    LoadResult result;
    result.units = session->world().entity_count();
    session->clock().set_time_scale(time_scale);
    const int budget = step_budget(time_scale);

    const auto started = std::chrono::steady_clock::now();
    for (int frame = 0; frame < frames; ++frame) {
      const int steps = session->advance(real_dt, budget, {});
      result.ticks += static_cast<std::uint64_t>(steps);
      result.dropped += session->clock().consume_dropped_ticks();
    }
    result.wall_seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - started)
            .count();
    result.digest = Game::Session::session_digest(*session);
    return result;
  }

  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory;
};

TEST_F(BattleSpeedLoadTest, AFullBattleLosesNoSimulationTimeAtAnyOfferedSpeed) {
  constexpr double k_frame = 1.0 / 60.0;
  constexpr int k_normal_frames = 240;

  const LoadResult normal = run_battle(1.0, k_frame, k_normal_frames);
  ASSERT_GE(normal.units, static_cast<std::size_t>(ranks_per_side() * k_files * 2));
  EXPECT_EQ(normal.ticks, static_cast<std::uint64_t>(k_normal_frames));
  EXPECT_EQ(normal.dropped, 0U);

  for (const double speed : {0.5, 2.0, 3.0, 4.0}) {
    const int frames = static_cast<int>(static_cast<double>(k_normal_frames) / speed);
    const LoadResult faster = run_battle(speed, k_frame, frames);

    EXPECT_EQ(faster.dropped, 0U) << "at " << speed << "x";
    EXPECT_EQ(faster.ticks, normal.ticks) << "at " << speed << "x";
    EXPECT_EQ(faster.digest, normal.digest) << "at " << speed << "x";

    std::printf("battle speed %.1fx: %llu units, %llu ticks in %.2f s wall "
                "(%.0f ticks/s, %.1fx the 4x frame demand)\n",
                speed,
                static_cast<unsigned long long>(faster.units),
                static_cast<unsigned long long>(faster.ticks),
                faster.wall_seconds,
                static_cast<double>(faster.ticks) / faster.wall_seconds,
                (static_cast<double>(faster.ticks) / faster.wall_seconds) / 240.0);
  }
}

TEST_F(BattleSpeedLoadTest, AMoveOrderIsUnderwayWithinTwoTicksAtNormalSpeed) {
  auto session = make_battle();
  const ScopedSession scope(*session);

  std::vector<Engine::Core::EntityID> squad;
  session->world().for_each_entity([&squad](const Engine::Core::Entity& entity) {
    if (const auto* unit = entity.get_component<UnitComponent>()) {
      if (unit->owner_id == k_left && squad.size() < 8U) {
        squad.push_back(entity.get_id());
      }
    }
  });
  ASSERT_FALSE(squad.empty());

  std::vector<TransformComponent::Vec3> before;
  before.reserve(squad.size());
  for (const auto id : squad) {
    before.push_back(
        session->world().get_entity(id)->get_component<TransformComponent>()->position);
  }

  Game::Command::Move move;
  move.units = squad;
  for (std::size_t index = 0; index < squad.size(); ++index) {
    move.targets.emplace_back(70.0F + static_cast<float>(index), 0.0F, 80.0F);
  }
  session->commands().submit(
      Game::Command::Source::LocalPlayer, k_left, Game::Command::Payload{move});

  const auto step = static_cast<float>(session->clock().tick_seconds());
  int ticks_until_motion = 0;
  bool moving = false;
  for (int tick = 1; tick <= 12 && !moving; ++tick) {
    session->world().update(step);
    for (std::size_t index = 0; index < squad.size(); ++index) {
      const auto* transform = session->world()
                                  .get_entity(squad[index])
                                  ->get_component<TransformComponent>();
      const float dx = transform->position.x - before[index].x;
      const float dz = transform->position.z - before[index].z;
      if ((dx * dx) + (dz * dz) > 1e-8F) {
        moving = true;
        ticks_until_motion = tick;
        break;
      }
    }
  }

  EXPECT_TRUE(moving) << "a move order produced no motion within 12 ticks";
  EXPECT_LE(ticks_until_motion, 2)
      << "a move order took " << ticks_until_motion
      << " ticks to move anything; command feedback should not wait on a delay";
}

TEST_F(BattleSpeedLoadTest, AStalledFrameAtQuadrupleSpeedReportsWhatItCouldNotRun) {
  auto session = make_battle();
  const ScopedSession scope(*session);
  session->clock().set_time_scale(4.0);

  const int steps = session->advance(2.0, step_budget(4.0), {});

  EXPECT_EQ(steps, step_budget(4.0));
  EXPECT_GT(session->clock().dropped_ticks(), 0U);
}

} // namespace
