#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "game/command/command.h"
#include "game/command/command_queue.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/command_service.h"
#include "game/systems/default_content.h"
#include "game/systems/nation_registry.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/pathfinding.h"
#include "game/systems/route_follow_system.h"
#include "game/systems/runtime_system_registry.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"

namespace {

using Engine::Core::EntityID;
using Engine::Core::TransformComponent;
using Engine::Core::UnitComponent;
using Game::Session::SessionContext;
using Game::Systems::CommandService;
using Game::Systems::NavGrid;
using Game::Systems::Point;

constexpr int k_owner = 1;
constexpr int k_map = 64;

constexpr float k_final_approach_metres = 2.5F;
constexpr float k_set_off_fraction = 0.5F;
constexpr float k_full_pace_fraction = 0.9F;
constexpr float k_crawl_fraction = 0.5F;

struct PaceRecord {
  std::string label;
  float pace{0.0F};
  int order_tick{0};
  int set_off_tick{-1};
  int full_pace_tick{-1};
  int under_way_ticks{0};
  int below_full_ticks{0};
  int crawl_run{0};
  int longest_crawl_run{0};
  float min_fraction{1.0F};
  float sum_fraction{0.0F};
  bool arrived{false};

  void start(int tick) {
    order_tick = tick;
    set_off_tick = -1;
    full_pace_tick = -1;
    under_way_ticks = 0;
    below_full_ticks = 0;
    crawl_run = 0;
    longest_crawl_run = 0;
    min_fraction = 1.0F;
    sum_fraction = 0.0F;
  }

  void sample(int tick, float speed, float remaining, bool has_target) {
    if (!has_target || pace <= 0.0F) {
      crawl_run = 0;
      return;
    }
    float const fraction = speed / pace;
    if (set_off_tick < 0 && fraction >= k_set_off_fraction) {
      set_off_tick = tick;
    }
    if (full_pace_tick < 0 && fraction >= k_full_pace_fraction) {
      full_pace_tick = tick;
    }
    if (remaining <= k_final_approach_metres) {
      crawl_run = 0;
      return;
    }
    ++under_way_ticks;
    sum_fraction += fraction;
    min_fraction = std::min(min_fraction, fraction);
    if (fraction < k_full_pace_fraction) {
      ++below_full_ticks;
    }
    if (fraction < k_crawl_fraction) {
      ++crawl_run;
      longest_crawl_run = std::max(longest_crawl_run, crawl_run);
    } else {
      crawl_run = 0;
    }
  }

  [[nodiscard]] auto mean_fraction() const -> float {
    return under_way_ticks > 0 ? sum_fraction / static_cast<float>(under_way_ticks)
                               : 0.0F;
  }

  [[nodiscard]] auto below_full_share() const -> float {
    return under_way_ticks > 0 ? static_cast<float>(below_full_ticks) /
                                     static_cast<float>(under_way_ticks)
                               : 0.0F;
  }
};

class MovementPaceTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::NationRegistry::instance().clear();
    Game::Systems::initialize_default_content(
        Game::Systems::NationRegistry::instance());
    NavGrid::initialize(k_map, k_map);
    m_factory = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*m_factory);
  }

  void TearDown() override {
    m_scope.reset();
    m_session.reset();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
  }

  auto open_field() -> SessionContext& {
    Game::Map::MapDefinition map;
    map.grid.width = k_map;
    map.grid.height = k_map;
    map.grid.tile_size = 1.0F;
    map.biome.procedural_boulders_enabled = false;
    map.biome.procedural_iron_ore_enabled = false;
    map.biome.procedural_trees_enabled = false;

    m_scope.reset();
    m_session.reset();
    m_session = std::make_unique<SessionContext>();
    m_session->world().set_presentation_enabled(true);
    m_scope = std::make_unique<Game::Session::ScopedSession>(*m_session);
    m_session->owners().register_owner_with_id(
        k_owner, Game::Systems::OwnerType::Player, "carthage");
    m_session->owners().set_owner_team(k_owner, 1);
    Game::Systems::initialize_default_content(m_session->nations());
    Game::Systems::register_runtime_systems(m_session->world());
    m_session->terrain().initialize(map);
    NavGrid::initialize(map.grid.width, map.grid.height);
    m_tick = 0;
    return *m_session;
  }

  static auto world_of(int grid_x, int grid_z) -> QVector3D {
    return NavGrid::grid_to_world(Point(grid_x, grid_z));
  }

  auto spawn(Game::Units::SpawnType type,
             const QVector3D& position,
             float rotation_y) -> EntityID {
    Game::Units::SpawnParams params;
    params.position = position;
    params.player_id = k_owner;
    params.spawn_type = type;
    params.rotation_y = rotation_y;
    params.nation_id = Game::Systems::NationID::Carthage;
    auto unit = m_factory->create(type, m_session->world(), params);
    return unit ? unit->id() : 0;
  }

  void house(int min_x, int min_z, int cells_x, int cells_z) {
    float const centre_x = world_of(min_x, min_z).x() + (cells_x - 1) * 0.5F;
    float const centre_z = world_of(min_x, min_z).z() + (cells_z - 1) * 0.5F;
    auto* entity = m_session->world().create_entity();
    entity->add_component<TransformComponent>(centre_x, 0.0F, centre_z);
    auto* unit = entity->add_component<UnitComponent>(400, 400, 0.0F, 0.0F);
    unit->owner_id = k_owner;
    unit->spawn_type = Game::Units::SpawnType::Barracks;
    entity->add_component<Engine::Core::BuildingComponent>();
    Game::Systems::BuildingCollisionRegistry::instance().register_building(
        entity->get_id(),
        "house",
        centre_x,
        centre_z,
        k_owner,
        {.width = static_cast<float>(cells_x), .depth = static_cast<float>(cells_z)});
  }

  void wall_cell(int grid_x, int grid_z) {
    auto const position = world_of(grid_x, grid_z);
    auto* entity = m_session->world().create_entity();
    entity->add_component<TransformComponent>(position.x(), 0.0F, position.z());
    auto* unit = entity->add_component<UnitComponent>(400, 400, 0.0F, 0.0F);
    unit->owner_id = k_owner;
    unit->spawn_type = Game::Units::SpawnType::WallSegment;
    entity->add_component<Engine::Core::BuildingComponent>();
    Game::Systems::BuildingCollisionRegistry::instance().register_building(
        entity->get_id(),
        "wall_segment",
        position.x(),
        position.z(),
        k_owner,
        {.width = 1.0F, .depth = 1.0F});
  }

  void refresh_grid() {
    auto* pathfinder = NavGrid::get_pathfinder();
    ASSERT_NE(pathfinder, nullptr);
    pathfinder->mark_navigation_grid_dirty();
    pathfinder->update_navigation_grid();
  }

  void street(int min_x, int max_x, int centre_z, int clear_cells, int house_depth) {
    int const half = clear_cells / 2;
    int const south_min = centre_z - half - house_depth;
    int const north_min = centre_z - half + clear_cells;
    for (int x = min_x; x <= max_x; x += 5) {
      int const span = std::min(4, max_x - x + 1);
      house(x, south_min, span, house_depth);
      house(x, north_min, span, house_depth);
    }
    refresh_grid();
  }

  void
  town(int min_x, int min_z, int blocks_x, int blocks_z, int block, int street_width) {
    for (int bx = 0; bx < blocks_x; ++bx) {
      for (int bz = 0; bz < blocks_z; ++bz) {
        house(min_x + bx * (block + street_width),
              min_z + bz * (block + street_width),
              block,
              block);
      }
    }
    refresh_grid();
  }

  void describe_route(EntityID id) {
    auto* entity = m_session->world().get_entity(id);
    auto const* movement =
        entity != nullptr ? entity->get_component<Engine::Core::MovementComponent>()
                          : nullptr;
    if (movement == nullptr) {
      return;
    }
    std::printf("[route] entity %llu has_target=%d goal=(%.1f,%.1f) waypoints=%zu:",
                static_cast<unsigned long long>(id),
                static_cast<int>(movement->get_has_target()),
                movement->get_goal_x(),
                movement->get_goal_y(),
                movement->get_path().size());
    for (auto const& point : movement->get_path()) {
      std::printf(" (%.1f,%.1f)", point.first, point.second);
    }
    std::printf("\n");
  }

  void order(const std::vector<EntityID>& units, const QVector3D& target) {
    auto const plan =
        CommandService::plan_ground_move(m_session->world(), units, target);
    Game::Command::Move move;
    move.units = units;
    move.targets = plan.target_positions();
    move.facing_angles = plan.facing_angles();
    move.kind = Game::Systems::MoveOrderKind::PlayerMove;
    move.preserve_formation_mode = plan.preserve_formation_mode;
    Game::Command::submit(m_session->world(),
                          Game::Command::Source::LocalPlayer,
                          k_owner,
                          std::move(move));
    for (auto& record : m_records) {
      record.start(m_tick);
    }
  }

  void track(const std::vector<EntityID>& units,
             const std::vector<std::string>& labels) {
    m_tracked = units;
    m_records.assign(units.size(), PaceRecord{});
    float group_pace = 1.0e9F;
    for (std::size_t index = 0; index < units.size(); ++index) {
      auto* entity = m_session->world().get_entity(units[index]);
      ASSERT_NE(entity, nullptr);
      auto const* unit = entity->get_component<UnitComponent>();
      ASSERT_NE(unit, nullptr);
      group_pace = std::min(group_pace, unit->speed);
      m_records[index].label = labels[index];
    }
    for (auto& record : m_records) {
      record.pace = group_pace;
    }
  }

  void run_for(double seconds) {
    double const step = m_session->clock().tick_seconds();
    for (double elapsed = 0.0; elapsed < seconds - 1e-9; elapsed += step) {
      m_session->clock().advance(step);
      while (m_session->clock().consume_tick()) {
        m_session->world().update(static_cast<float>(step));
        ++m_tick;
        sample();
      }
    }
  }

  void sample() {
    for (std::size_t index = 0; index < m_tracked.size(); ++index) {
      auto* entity = m_session->world().get_entity(m_tracked[index]);
      if (entity == nullptr) {
        continue;
      }
      auto const* facts = entity->get_component<Engine::Core::MovementFactsComponent>();
      auto const* movement = entity->get_component<Engine::Core::MovementComponent>();
      auto const* transform = entity->get_component<TransformComponent>();
      if (facts == nullptr || movement == nullptr || transform == nullptr) {
        continue;
      }
      float const remaining = Game::Systems::RouteFollowSystem::remaining_route_length(
          *movement, transform->position.x, transform->position.z);
      m_records[index].sample(
          m_tick, facts->last_accepted_speed, remaining, movement->get_has_target());
      static bool const dump = std::getenv("SOI_PACE_DUMP") != nullptr;
      static bool const dump_traversal = std::getenv("SOI_PACE_TRAVERSAL") != nullptr;
      if (dump_traversal && movement->get_has_target()) {
        auto const* traversal =
            entity->get_component<Engine::Core::UnitTraversalLayoutStateComponent>();
        if (traversal != nullptr && traversal->active &&
            (m_tick - m_records[index].order_tick) % 60 == 30) {
          for (auto const& slot : traversal->slot_states) {
            if (!slot.alive) {
              continue;
            }
            std::printf("[slot] t=%d #%u cur=(%.2f,%.2f) tgt=(%.2f,%.2f) v=(%.2f,%.2f) "
                        "blocked=%d for %.2fs\n",
                        m_tick - m_records[index].order_tick,
                        slot.slot_index,
                        slot.current_local_x,
                        slot.current_local_z,
                        slot.target_local_x,
                        slot.target_local_z,
                        slot.velocity_x,
                        slot.velocity_z,
                        static_cast<int>(slot.blocked),
                        slot.blocked_seconds);
          }
        }
        if (traversal != nullptr && m_tick % 6 == 0) {
          std::printf("[trav] t=%5d %-8s pos=(%.1f,%.1f) v=%.2f active=%d files=%u->%u "
                      "progress=%.2f blocked=%u scale=%.2f avail=%.2f pinch=%.1f\n",
                      m_tick - m_records[index].order_tick,
                      m_records[index].label.c_str(),
                      transform->position.x,
                      transform->position.z,
                      facts->last_accepted_speed,
                      static_cast<int>(traversal->active),
                      traversal->current_files,
                      traversal->target_files,
                      traversal->transition_progress,
                      traversal->blocked_slot_count,
                      traversal->lateral_scale,
                      traversal->available_half_width,
                      traversal->constriction_distance);
        }
      }
      if (dump && movement->get_has_target() && remaining > k_final_approach_metres &&
          facts->last_accepted_speed < m_records[index].pace * k_crawl_fraction &&
          m_tick - m_records[index].order_tick > 6) {
        auto const* traversal =
            entity->get_component<Engine::Core::UnitTraversalLayoutStateComponent>();
        std::string leader = "-";
        {
          float best = 4.0F;
          float const tx = facts->desired.velocity_x;
          float const tz = facts->desired.velocity_z;
          float const tl = std::hypot(tx, tz);
          for (std::size_t other = 0; other < m_tracked.size(); ++other) {
            if (other == index) {
              continue;
            }
            auto* candidate = m_session->world().get_entity(m_tracked[other]);
            auto const* ct = candidate != nullptr
                                 ? candidate->get_component<TransformComponent>()
                                 : nullptr;
            auto const* cf =
                candidate != nullptr
                    ? candidate->get_component<Engine::Core::MovementFactsComponent>()
                    : nullptr;
            auto const* cm =
                candidate != nullptr
                    ? candidate->get_component<Engine::Core::MovementComponent>()
                    : nullptr;
            if (ct == nullptr || cf == nullptr || cm == nullptr || tl < 1.0e-4F) {
              continue;
            }
            float const px = ct->position.x - transform->position.x;
            float const pz = ct->position.z - transform->position.z;
            float const along = (px * tx + pz * tz) / tl;
            float const cross = std::abs(tx * pz - tz * px) / tl;
            if (along > 0.0F && along < best && cross < 1.3F) {
              best = along;
              char buffer[96];
              std::snprintf(buffer,
                            sizeof buffer,
                            "%s@%.1fm v=%.2f %s%s",
                            m_records[other].label.c_str(),
                            along,
                            cf->last_accepted_speed,
                            Engine::Core::movement_state_name(cf->progress.state),
                            cm->get_has_target() ? "" : "(idle)");
              leader = buffer;
            }
          }
        }
        float const desired =
            std::hypot(facts->desired.velocity_x, facts->desired.velocity_z);
        float const steered =
            std::hypot(facts->steering.velocity_x, facts->steering.velocity_z);
        std::printf(
            "[dump] t=%5d %-8s pos=(%.1f,%.1f) yaw=%.0f v=%.2f des=%.2f steer=%.2f "
            "res=%d nb=%u overlap=%.2f push=%.2f motor_blk=%d acc=%.2f "
            "state=%s rung=%d trav=%d scale=%.2f remaining=%.1f ahead=[%s]\n",
            m_tick - m_records[index].order_tick,
            m_records[index].label.c_str(),
            transform->position.x,
            transform->position.z,
            transform->rotation.y,
            facts->last_accepted_speed,
            desired,
            steered,
            static_cast<int>(facts->steering.result),
            facts->steering.neighbor_count,
            facts->steering.body_overlap,
            std::hypot(facts->steering.contact_push_x, facts->steering.contact_push_z),
            static_cast<int>(facts->motor.blocked),
            facts->motor.accepted_fraction,
            Engine::Core::movement_state_name(facts->progress.state),
            static_cast<int>(facts->progress.stall.rung),
            traversal != nullptr ? static_cast<int>(traversal->active) : -1,
            traversal != nullptr ? traversal->lateral_scale : -1.0F,
            remaining,
            leader.c_str());
      }
    }
  }

  auto position_of(EntityID id) -> QVector3D {
    auto* entity = m_session->world().get_entity(id);
    if (entity == nullptr) {
      return {};
    }
    auto const* transform = entity->get_component<TransformComponent>();
    return transform == nullptr
               ? QVector3D()
               : QVector3D(transform->position.x, 0.0F, transform->position.z);
  }

  void print_records(const char* scenario) {
    std::printf("[pace] %s\n", scenario);
    for (auto const& record : m_records) {
      std::printf(
          "[pace]   %-10s set_off=%3d full=%3d ticks  under_way=%4d  "
          "mean=%.2f  min=%.2f  below90=%4.0f%%  crawl_run=%3d ticks\n",
          record.label.c_str(),
          record.set_off_tick < 0 ? -1 : record.set_off_tick - record.order_tick,
          record.full_pace_tick < 0 ? -1 : record.full_pace_tick - record.order_tick,
          record.under_way_ticks,
          record.mean_fraction(),
          record.min_fraction,
          record.below_full_share() * 100.0F,
          record.longest_crawl_run);
    }
  }

  void expect_clean(const PaceRecord& record,
                    int set_off_ticks,
                    int full_pace_ticks,
                    float below_full_share,
                    int crawl_ticks) {
    EXPECT_GE(record.set_off_tick, 0) << record.label << " never set off";
    EXPECT_LE(record.set_off_tick - record.order_tick, set_off_ticks)
        << record.label << " took too long to set off";
    EXPECT_GE(record.full_pace_tick, 0) << record.label << " never reached pace";
    EXPECT_LE(record.full_pace_tick - record.order_tick, full_pace_ticks)
        << record.label << " took too long to reach pace";
    EXPECT_LE(record.below_full_share(), below_full_share)
        << record.label << " spent too long below pace";
    EXPECT_LE(record.longest_crawl_run, crawl_ticks)
        << record.label << " crawled for too long";
  }

  int m_tick{0};
  std::vector<EntityID> m_tracked;
  std::vector<PaceRecord> m_records;
  std::unique_ptr<SessionContext> m_session;
  std::unique_ptr<Game::Session::ScopedSession> m_scope;
  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory;
};

TEST_F(MovementPaceTest, ABlockOrderedSidewaysSetsOffAtOnceAndHoldsPace) {
  open_field();
  EntityID const block =
      spawn(Game::Units::SpawnType::Spearman, world_of(12, 32), 0.0F);
  ASSERT_NE(block, 0U);
  run_for(1.0);
  track({block}, {"spear"});

  QVector3D const start = position_of(block);
  order({block}, world_of(44, 32));
  run_for(20.0);
  print_records("sideways order, open field");

  EXPECT_GT((position_of(block) - start).length(), 28.0F);
  expect_clean(m_records[0], 6, 30, 0.05F, 6);
}

TEST_F(MovementPaceTest, ABlockReversedMidMarchTurnsWithoutStopping) {
  open_field();
  EntityID const block =
      spawn(Game::Units::SpawnType::Spearman, world_of(12, 32), 90.0F);
  ASSERT_NE(block, 0U);
  run_for(1.0);
  track({block}, {"spear"});

  order({block}, world_of(52, 32));
  run_for(4.0);
  print_records("outbound leg");
  expect_clean(m_records[0], 6, 30, 0.05F, 6);

  order({block}, world_of(8, 32));
  run_for(12.0);
  print_records("reversal");
  expect_clean(m_records[0], 6, 30, 0.05F, 6);
  EXPECT_LT(position_of(block).x(), world_of(12, 32).x());
}

TEST_F(MovementPaceTest, ABlockKeepsPaceThroughAStreet) {
  open_field();
  street(20, 44, 32, 4, 4);
  EntityID const block =
      spawn(Game::Units::SpawnType::Spearman, world_of(8, 32), 90.0F);
  ASSERT_NE(block, 0U);
  run_for(1.0);
  track({block}, {"spear"});

  order({block}, world_of(56, 32));
  run_for(30.0);
  print_records("four-cell street");

  EXPECT_GT(position_of(block).x(), world_of(52, 32).x());
  expect_clean(m_records[0], 6, 30, 0.05F, 6);
}

TEST_F(MovementPaceTest, ABlockKeepsPaceThroughAThreeCellStreet) {
  open_field();
  street(20, 44, 32, 3, 4);
  EntityID const block =
      spawn(Game::Units::SpawnType::Spearman, world_of(8, 32), 90.0F);
  ASSERT_NE(block, 0U);
  run_for(1.0);
  track({block}, {"spear"});

  order({block}, world_of(56, 32));
  run_for(30.0);
  print_records("three-cell street");

  EXPECT_GT(position_of(block).x(), world_of(52, 32).x());
  expect_clean(m_records[0], 6, 30, 0.05F, 6);
}

TEST_F(MovementPaceTest, ABlockKeepsPaceThroughATwoCellStreet) {
  open_field();
  street(20, 44, 32, 2, 4);
  EntityID const block =
      spawn(Game::Units::SpawnType::Spearman, world_of(8, 32), 90.0F);
  ASSERT_NE(block, 0U);
  run_for(1.0);
  track({block}, {"spear"});

  order({block}, world_of(56, 32));
  run_for(40.0);
  print_records("two-cell street");

  EXPECT_GT(position_of(block).x(), world_of(52, 32).x());
  expect_clean(m_records[0], 6, 30, 0.05F, 6);
}

TEST_F(MovementPaceTest, ABlockFilesThroughAOneCellGapWithoutSlowing) {
  open_field();
  for (int z = 0; z < k_map; ++z) {
    if (z != 32) {
      wall_cell(30, z);
    }
  }
  refresh_grid();
  EntityID const block =
      spawn(Game::Units::SpawnType::Spearman, world_of(12, 32), 90.0F);
  ASSERT_NE(block, 0U);
  run_for(1.0);
  track({block}, {"spear"});

  order({block}, world_of(52, 32));
  run_for(0.1);
  if (std::getenv("SOI_PACE_TRAVERSAL") != nullptr) {
    describe_route(block);
    auto* pathfinder = NavGrid::get_pathfinder();
    for (int z = 30; z <= 34; ++z) {
      std::printf("[grid] z=%d:", z);
      for (int x = 28; x <= 32; ++x) {
        std::printf(
            " %d/%d/%d",
            static_cast<int>(pathfinder->is_walkable(x, z)),
            static_cast<int>(pathfinder->is_world_position_walkable(
                world_of(x, z), Game::Systems::Pathfinding::Passability::Light)),
            static_cast<int>(pathfinder->is_world_position_walkable(
                world_of(x, z), Game::Systems::Pathfinding::Passability::Heavy)));
      }
      std::printf("\n");
    }
  }
  run_for(39.9);
  print_records("one-cell gap");

  EXPECT_GT(position_of(block).x(), world_of(48, 32).x());
  expect_clean(m_records[0], 6, 30, 0.05F, 6);
}

TEST_F(MovementPaceTest, ABlockKeepsPaceRoundAStreetCorner) {
  open_field();

  street(16, 36, 32, 4, 4);
  for (int z = 36; z <= 52; z += 5) {
    house(34, z, 4, std::min(4, 56 - z));
    house(43, z, 4, std::min(4, 56 - z));
  }
  house(38, 26, 9, 4);
  refresh_grid();
  EntityID const block =
      spawn(Game::Units::SpawnType::Spearman, world_of(6, 32), 90.0F);
  ASSERT_NE(block, 0U);
  run_for(1.0);
  track({block}, {"spear"});

  order({block}, world_of(40, 58));
  run_for(40.0);
  print_records("street corner");

  EXPECT_GT(position_of(block).z(), world_of(40, 54).z());
  expect_clean(m_records[0], 6, 30, 0.08F, 6);
}

TEST_F(MovementPaceTest, AnArmyReorderedThroughATownHoldsPace) {
  open_field();
  town(22, 8, 3, 5, 6, 5);

  std::vector<EntityID> army;
  std::vector<std::string> labels;
  auto const add = [&](Game::Units::SpawnType type, int gx, int gz, const char* name) {
    EntityID const id = spawn(type, world_of(gx, gz), 90.0F);
    ASSERT_NE(id, 0U);
    army.push_back(id);
    labels.push_back(name + std::to_string(army.size()));
  };
  int const rows[] = {20, 27, 34, 41};
  for (int row : rows) {
    add(Game::Units::SpawnType::Spearman, 6, row, "spear");
    add(Game::Units::SpawnType::Knight, 12, row, "sword");
  }
  add(Game::Units::SpawnType::Archer, 18, 24, "archer");
  add(Game::Units::SpawnType::Archer, 18, 37, "archer");
  add(Game::Units::SpawnType::MountedKnight, 2, 24, "knight");
  add(Game::Units::SpawnType::MountedKnight, 2, 37, "knight");
  run_for(1.0);
  track(army, labels);

  order(army, world_of(58, 30));
  run_for(8.0);
  print_records("army east through town, first 8 s");
  for (auto const& record : m_records) {
    expect_clean(record, 12, 90, 0.20F, 48);
  }

  order(army, world_of(6, 30));
  run_for(8.0);
  print_records("army reversed west, 8 s");
  for (auto const& record : m_records) {
    expect_clean(record, 12, 90, 0.20F, 48);
  }

  order(army, world_of(58, 30));
  run_for(40.0);
  print_records("army east again, to arrival");
  for (auto const& record : m_records) {
    expect_clean(record, 12, 90, 0.20F, 48);
  }
  int arrived = 0;
  for (auto const id : army) {
    if (position_of(id).x() > world_of(50, 30).x()) {
      ++arrived;
    }
  }
  EXPECT_EQ(arrived, static_cast<int>(army.size()));
}

TEST_F(MovementPaceTest, ABlockKeepsPacePastAFriendHoldingGroundInAStreet) {
  open_field();
  street(20, 44, 32, 3, 4);
  EntityID const block =
      spawn(Game::Units::SpawnType::Spearman, world_of(8, 32), 90.0F);
  ASSERT_NE(block, 0U);
  EntityID const sentry =
      spawn(Game::Units::SpawnType::Knight, world_of(32, 32), 90.0F);
  ASSERT_NE(sentry, 0U);
  m_session->world()
      .get_entity(sentry)
      ->add_component<Engine::Core::HoldModeComponent>();
  run_for(1.0);
  track({block}, {"spear"});

  order({block}, world_of(56, 32));
  run_for(30.0);
  print_records("three-cell street, friend holding ground in it");

  EXPECT_GT(position_of(block).x(), world_of(52, 32).x());
  expect_clean(m_records[0], 6, 30, 0.05F, 6);
}

TEST_F(MovementPaceTest, ABlockKeepsPaceThroughAOneCellGapAFriendIsStandingIn) {
  open_field();
  for (int z = 0; z < k_map; ++z) {
    if (z != 32) {
      wall_cell(30, z);
    }
  }
  refresh_grid();
  EntityID const block =
      spawn(Game::Units::SpawnType::Spearman, world_of(12, 32), 90.0F);
  ASSERT_NE(block, 0U);
  EntityID const sentry =
      spawn(Game::Units::SpawnType::Knight, world_of(30, 32), 90.0F);
  ASSERT_NE(sentry, 0U);
  m_session->world()
      .get_entity(sentry)
      ->add_component<Engine::Core::HoldModeComponent>();
  run_for(1.0);
  track({block}, {"spear"});

  order({block}, world_of(52, 32));
  run_for(40.0);
  print_records("one-cell gap, friend holding ground in it");

  EXPECT_GT(position_of(block).x(), world_of(48, 32).x());
  expect_clean(m_records[0], 6, 30, 0.05F, 6);
}

TEST_F(MovementPaceTest, ABlockKeepsPaceThroughAnIdleCrowd) {
  open_field();
  EntityID const block =
      spawn(Game::Units::SpawnType::Spearman, world_of(8, 32), 90.0F);
  ASSERT_NE(block, 0U);
  for (int x = 30; x <= 34; x += 2) {
    for (int z = 28; z <= 36; z += 2) {
      ASSERT_NE(spawn(Game::Units::SpawnType::Knight, world_of(x, z), 0.0F), 0U);
    }
  }
  run_for(1.0);
  track({block}, {"spear"});

  order({block}, world_of(56, 32));
  run_for(30.0);
  print_records("open field, idle crowd across the path");

  EXPECT_GT(position_of(block).x(), world_of(52, 32).x());
  expect_clean(m_records[0], 6, 30, 0.05F, 6);
}

TEST_F(MovementPaceTest, TwoBlocksMeetingHeadOnInAStreetBothKeepPace) {
  open_field();
  street(20, 44, 32, 2, 4);
  EntityID const east = spawn(Game::Units::SpawnType::Spearman, world_of(8, 32), 90.0F);
  EntityID const west =
      spawn(Game::Units::SpawnType::Spearman, world_of(56, 32), 270.0F);
  ASSERT_NE(east, 0U);
  ASSERT_NE(west, 0U);
  run_for(1.0);
  track({east, west}, {"eastbound", "westbound"});

  order({east}, world_of(56, 32));
  order({west}, world_of(8, 32));
  run_for(40.0);
  print_records("two-cell street, blocks meeting head on");

  EXPECT_GT(position_of(east).x(), world_of(52, 32).x());
  EXPECT_LT(position_of(west).x(), world_of(12, 32).x());
  for (auto const& record : m_records) {
    expect_clean(record, 6, 30, 0.10F, 6);
  }
}

TEST_F(MovementPaceTest, AColumnIsNotHeldUpByAFriendStoppedAtTheFront) {
  open_field();
  street(20, 44, 32, 3, 4);
  EntityID const sentry =
      spawn(Game::Units::SpawnType::Knight, world_of(36, 32), 90.0F);
  ASSERT_NE(sentry, 0U);
  m_session->world()
      .get_entity(sentry)
      ->add_component<Engine::Core::HoldModeComponent>();
  std::vector<EntityID> column;
  std::vector<std::string> labels;
  for (int x = 14; x >= 2; x -= 4) {
    EntityID const id = spawn(Game::Units::SpawnType::Spearman, world_of(x, 32), 90.0F);
    ASSERT_NE(id, 0U);
    column.push_back(id);
    labels.push_back("spear" + std::to_string(column.size()));
  }
  run_for(1.0);
  track(column, labels);

  order(column, world_of(58, 32));
  run_for(45.0);
  print_records("column through a street with a friend holding ground");

  for (auto const id : column) {
    EXPECT_GT(position_of(id).x(), world_of(50, 32).x());
  }
  for (auto const& record : m_records) {
    expect_clean(record, 12, 90, 0.20F, 12);
  }
}

} // namespace
