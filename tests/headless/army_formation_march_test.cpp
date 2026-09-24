#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <gtest/gtest.h>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "game/command/command.h"
#include "game/command/command_queue.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/formation/army_formation_planner.h"
#include "game/formation/army_formation_registry.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/systems/default_content.h"
#include "game/systems/nation_registry.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/pathfinding.h"
#include "game/systems/runtime_system_registry.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"
#include "game/util/planar_math.h"

namespace {

using Engine::Core::EntityID;
using Game::Formation::ArmyFormation;
using Game::Formation::ArmyFormationIntent;
using Game::Formation::ArmyFormationRegistry;
using Game::Formation::FormationPhase;
using Game::Formation::MovementPolicy;
using Game::Session::SessionContext;
using Game::Systems::NavGrid;

constexpr int k_owner = 1;
constexpr int k_map = 120;

constexpr float k_touching = 0.05F;

struct MarchRecord {
  std::map<EntityID, int> slot_of;
  int slot_changes{0};
  int off_ground_samples{0};
  float worst_penetration{0.0F};
  float worst_formed_penetration{0.0F};
  bool saw_compressed{false};
  int ticks{0};
  std::map<EntityID, float> last_yaw;
  std::map<EntityID, float> yaw_travel;
  std::map<EntityID, std::uint64_t> first_order;
  std::map<EntityID, std::uint64_t> last_order;
  std::map<EntityID, QVector3D> last_position;
  float backwards_distance{0.0F};
  float worst_slot_error{0.0F};
};

class ArmyFormationMarchTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::NationRegistry::instance().clear();
    Game::Systems::initialize_default_content(
        Game::Systems::NationRegistry::instance());
    m_factory = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*m_factory);
  }

  void TearDown() override {
    m_scope.reset();
    m_session.reset();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
  }

  void open(Game::Map::MapDefinition map) {
    map.grid.width = k_map;
    map.grid.height = k_map;
    map.grid.tile_size = 1.0F;
    map.coordSystem = Game::Map::CoordSystem::World;
    map.biome.procedural_boulders_enabled = false;
    map.biome.procedural_iron_ore_enabled = false;
    map.biome.procedural_trees_enabled = false;

    m_scope.reset();
    m_session.reset();
    m_session = std::make_unique<SessionContext>();
    m_session->world().set_presentation_enabled(true);
    m_scope = std::make_unique<Game::Session::ScopedSession>(*m_session);
    m_session->owners().register_owner_with_id(
        k_owner, Game::Systems::OwnerType::Player, "rome");
    m_session->owners().set_owner_team(k_owner, 1);
    Game::Systems::initialize_default_content(m_session->nations());
    Game::Systems::register_runtime_systems(m_session->world());
    m_session->terrain().initialize(map);
    NavGrid::initialize(map.grid.width, map.grid.height);
    auto* pathfinder = NavGrid::get_pathfinder();
    ASSERT_NE(pathfinder, nullptr);
    pathfinder->update_navigation_grid();
  }

  auto spawn(Game::Units::SpawnType type, const QVector3D& position) -> EntityID {
    Game::Units::SpawnParams params;
    params.position = position;
    params.player_id = k_owner;
    params.spawn_type = type;
    params.rotation_y = 0.0F;
    params.nation_id = Game::Systems::NationID::RomanRepublic;
    auto unit = m_factory->create(type, m_session->world(), params);
    return unit ? unit->id() : 0;
  }

  auto army(const QVector3D& centre) -> std::vector<EntityID> {
    std::vector<EntityID> units;
    for (int i = 0; i < 6; ++i) {
      auto const type = i % 3 == 2 ? Game::Units::SpawnType::Spearman
                                   : Game::Units::SpawnType::Swordsman;
      units.push_back(spawn(type,
                            centre + QVector3D(static_cast<float>(i % 3) * 6.0F - 6.0F,
                                               0.0F,
                                               static_cast<float>(i / 3) * 6.0F)));
    }
    return units;
  }

  void deploy(const std::vector<EntityID>& units,
              const QVector3D& anchor,
              float facing,
              ArmyFormationIntent intent) {
    Game::Command::DeployFormation order;
    order.units = units;
    order.anchor = anchor;
    order.facing = facing;
    order.intent = intent;
    order.spacing = 1.5F;
    Game::Command::submit(m_session->world(),
                          Game::Command::Source::LocalPlayer,
                          k_owner,
                          std::move(order));
  }

  auto registry() -> ArmyFormationRegistry& {
    return ArmyFormationRegistry::for_world(m_session->world());
  }

  auto formation_of(EntityID member) -> const ArmyFormation* {
    return registry().find(registry().group_of(member));
  }

  auto position_of(EntityID id) -> QVector3D {
    const auto* transform =
        m_session->world().try_get<Engine::Core::TransformComponent>(id);
    return transform == nullptr
               ? QVector3D()
               : QVector3D(transform->position.x, 0.0F, transform->position.z);
  }

  auto worst_penetration(const std::vector<EntityID>& units) -> float {
    const auto* formation = formation_of(units.front());
    if (formation == nullptr) {
      return 0.0F;
    }
    struct Box {
      QVector3D centre;
      QVector3D lateral;
      QVector3D depth;
      float half_width{0.0F};
      float half_depth{0.0F};
    };
    std::vector<Box> boxes;
    for (auto const id : units) {
      const auto* slot = formation->find_slot_for(id);
      const auto* transform =
          m_session->world().try_get<Engine::Core::TransformComponent>(id);
      if (slot == nullptr || transform == nullptr) {
        continue;
      }
      float const yaw = transform->rotation.y * 3.14159265F / 180.0F;
      float half_width = slot->half_width;
      float half_depth = slot->half_depth;

      const auto* traversal =
          m_session->world().try_get<Engine::Core::UnitTraversalLayoutStateComponent>(
              id);
      if (traversal != nullptr && traversal->active) {
        float reach_x = 0.0F;
        float reach_z = 0.0F;
        for (const auto& soldier : traversal->slot_states) {
          if (!soldier.alive) {
            continue;
          }
          reach_x = std::max(reach_x, std::abs(soldier.current_local_x));
          reach_z = std::max(reach_z, std::abs(soldier.current_local_z));
        }
        float const body = std::max(traversal->soldier_body_radius, 0.1F);
        half_width = reach_x + body;
        half_depth = reach_z + body;
      }
      boxes.push_back({position_of(id),
                       QVector3D(std::cos(yaw), 0.0F, -std::sin(yaw)),
                       QVector3D(std::sin(yaw), 0.0F, std::cos(yaw)),
                       half_width,
                       half_depth});
    }
    auto radius_on = [](const Box& box, const QVector3D& axis) {
      return box.half_width * std::abs(QVector3D::dotProduct(box.lateral, axis)) +
             box.half_depth * std::abs(QVector3D::dotProduct(box.depth, axis));
    };
    float worst = 0.0F;
    for (std::size_t i = 0; i < boxes.size(); ++i) {
      for (std::size_t j = i + 1; j < boxes.size(); ++j) {
        const auto& a = boxes[i];
        const auto& b = boxes[j];
        QVector3D const offset = b.centre - a.centre;
        float depth = std::numeric_limits<float>::max();
        for (const auto& axis : {a.lateral, a.depth, b.lateral, b.depth}) {
          float const overlap = radius_on(a, axis) + radius_on(b, axis) -
                                std::abs(QVector3D::dotProduct(offset, axis));
          depth = std::min(depth, overlap);
        }
        worst = std::max(worst, depth);
      }
    }
    return worst;
  }

  void
  run_for(double seconds, const std::vector<EntityID>& units, MarchRecord& record) {
    auto* pathfinder = NavGrid::get_pathfinder();
    double const step = m_session->clock().tick_seconds();
    for (double elapsed = 0.0; elapsed < seconds - 1e-9; elapsed += step) {
      m_session->clock().advance(step);
      while (m_session->clock().consume_tick()) {
        m_session->world().update(static_cast<float>(step));
        ++record.ticks;
        for (auto const id : units) {
          const auto* transform =
              m_session->world().try_get<Engine::Core::TransformComponent>(id);
          const auto* movement =
              m_session->world().try_get<Engine::Core::MovementComponent>(id);
          if (transform != nullptr) {
            if (record.last_yaw.contains(id)) {
              record.yaw_travel[id] += std::abs(Game::Systems::signed_yaw_delta(
                  record.last_yaw[id], transform->rotation.y));
            }
            record.last_yaw[id] = transform->rotation.y;
          }
          if (movement != nullptr) {
            record.first_order.try_emplace(id, movement->get_order_sequence());
            record.last_order[id] = movement->get_order_sequence();
          }
          if (const auto* formation = formation_of(id)) {
            record.worst_slot_error = std::max(
                record.worst_slot_error, formation->slot_error(position_of(id), id));
          }
          const auto* membership =
              m_session->world()
                  .try_get<Engine::Core::ArmyFormationMembershipComponent>(id);
          if (membership != nullptr && membership->is_valid()) {
            auto const found = record.slot_of.find(id);
            if (found == record.slot_of.end()) {
              record.slot_of.emplace(id, membership->slot_id);
            } else if (found->second != membership->slot_id) {
              if (std::getenv("SOI_MARCH_TRACE") != nullptr) {
                const auto* f = formation_of(id);
                std::printf("[slotchange] tick=%d unit=%llu %d->%d comp=%d rev=%u\n",
                            record.ticks,
                            static_cast<unsigned long long>(id),
                            found->second,
                            membership->slot_id,
                            f ? static_cast<int>(f->compressed) : -1,
                            f ? f->plan_revision : 0U);
              }
              ++record.slot_changes;
              found->second = membership->slot_id;
            }
          }
          auto const position = position_of(id);
          if (record.last_position.contains(id)) {
            record.backwards_distance +=
                std::max(0.0F, record.last_position[id].z() - position.z());
          }
          record.last_position[id] = position;
          auto const cell = pathfinder->world_to_grid(position.x(), position.z());
          if (!pathfinder->is_walkable(cell.x, cell.y)) {
            ++record.off_ground_samples;
          }
        }
        float const penetration = worst_penetration(units);
        if (std::getenv("SOI_MARCH_PEN") != nullptr && record.ticks % 30 == 0) {
          const auto* f = formation_of(units.front());
          std::printf("[pen] t=%.1f pen=%.2f phase=%s facing=%.0f morph=%d yaws:",
                      record.ticks / 60.0F,
                      penetration,
                      f ? Game::Formation::phase_to_string(f->phase) : "-",
                      f ? f->facing : 0.0F,
                      f ? static_cast<int>(f->morph.active) : -1);
          for (auto const id : units) {
            const auto* tr =
                m_session->world().try_get<Engine::Core::TransformComponent>(id);
            const auto* sl = f ? f->find_slot_for(id) : nullptr;
            QVector3D const p = position_of(id);
            std::printf(" %.0f/%.1f",
                        tr ? tr->rotation.y : 0.0F,
                        sl ? std::hypot(p.x() - sl->world_position.x(),
                                        p.z() - sl->world_position.z())
                           : -1.0F);
          }
          std::printf("\n");
        }
        record.worst_penetration = std::max(record.worst_penetration, penetration);
        if (const auto* formation = formation_of(units.front());
            formation != nullptr && (formation->phase == FormationPhase::Traversing ||
                                     formation->is_formed())) {
          record.worst_formed_penetration =
              std::max(record.worst_formed_penetration, penetration);
        }
        if (const auto* formation = formation_of(units.front())) {
          record.saw_compressed = record.saw_compressed || formation->compressed;
          static bool const trace = std::getenv("SOI_MARCH_TRACE") != nullptr;
          if (trace && record.ticks % 60 == 0) {
            std::printf("[trace] t=%d anchor=(%.1f,%.1f) facing=%.0f phase=%s comp=%d "
                        "corridor=%zu/%zu dest=%d rev=%u cohesion=%.2f\n",
                        record.ticks / 60,
                        formation->anchor.x(),
                        formation->anchor.z(),
                        formation->facing,
                        Game::Formation::phase_to_string(formation->phase),
                        static_cast<int>(formation->compressed),
                        formation->move_plan.corridor_index,
                        formation->move_plan.corridor.size(),
                        static_cast<int>(formation->has_destination),
                        formation->plan_revision,
                        formation->cohesion);
            for (auto const id : units) {
              const auto* slot = formation->find_slot_for(id);
              auto const p = position_of(id);
              const auto* mv =
                  m_session->world().try_get<Engine::Core::MovementComponent>(id);
              const auto* facts =
                  m_session->world().try_get<Engine::Core::MovementFactsComponent>(id);
              const auto* trav =
                  m_session->world()
                      .try_get<Engine::Core::UnitTraversalLayoutStateComponent>(id);
              std::printf("[trace]   %llu pos=(%.1f,%.1f) slot=(%.1f,%.1f) st=%d "
                          "tgt=%d goal=(%.1f,%.1f) path=%zu state=%s v=%.2f trav=%d "
                          "files=%u blocked=%u\n",
                          static_cast<unsigned long long>(id),
                          p.x(),
                          p.z(),
                          slot ? slot->world_position.x() : 0.0F,
                          slot ? slot->world_position.z() : 0.0F,
                          slot ? static_cast<int>(slot->status) : -1,
                          mv ? static_cast<int>(mv->get_has_target()) : -1,
                          mv ? mv->get_goal_x() : 0.0F,
                          mv ? mv->get_goal_y() : 0.0F,
                          mv ? mv->get_path().size() : 0U,
                          facts
                              ? Engine::Core::movement_state_name(facts->progress.state)
                              : "-",
                          facts ? facts->last_accepted_speed : 0.0F,
                          trav ? static_cast<int>(trav->active) : -1,
                          trav ? trav->current_files : 0U,
                          trav ? trav->blocked_slot_count : 0U);
            }
          }
        }
      }
    }
  }

  void expect_formed_on_slots(const std::vector<EntityID>& units, float tolerance) {
    const auto* formation = formation_of(units.front());
    ASSERT_NE(formation, nullptr);
    for (auto const id : units) {
      const auto* slot = formation->find_slot_for(id);
      ASSERT_NE(slot, nullptr);
      QVector3D const offset = position_of(id) - slot->world_position;
      EXPECT_LT(std::hypot(offset.x(), offset.z()), tolerance)
          << "troop " << id << " did not reach its slot";
    }
  }

  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory;
  std::unique_ptr<SessionContext> m_session;
  std::unique_ptr<Game::Session::ScopedSession> m_scope;
};

} // namespace

TEST_F(ArmyFormationMarchTest, AColumnMarchesAndWheelsWithoutSwappingOrStacking) {
  open({});
  auto const units = army(QVector3D(0.0F, 0.0F, -30.0F));

  deploy(units, QVector3D(0.0F, 0.0F, 0.0F), 0.0F, ArmyFormationIntent::Column);
  MarchRecord north;
  run_for(70.0, units, north);

  const auto* formation = formation_of(units.front());
  ASSERT_NE(formation, nullptr);
  EXPECT_EQ(formation->options.movement_policy, MovementPolicy::MaintainFormation)
      << "Column alone selects the doctrine's march policy";
  EXPECT_EQ(north.slot_changes, 0) << "ranks or files swapped on the march";
  EXPECT_EQ(north.off_ground_samples, 0);
  std::printf(
      "[march] north: worst penetration %.2f m (%.2f m once formed) over %d ticks\n",
      north.worst_penetration,
      north.worst_formed_penetration,
      north.ticks);
  EXPECT_LE(north.worst_formed_penetration, k_touching)
      << "troops overlapped while the column marched formed";
  expect_formed_on_slots(units, 2.0F);
  EXPECT_TRUE(formation->is_formed());
  EXPECT_LT((formation->anchor - formation->destination).length(), 1.3F);

  deploy(units, QVector3D(30.0F, 0.0F, 0.0F), 90.0F, ArmyFormationIntent::Column);
  MarchRecord east = north;
  east.worst_penetration = 0.0F;
  east.worst_formed_penetration = 0.0F;
  run_for(100.0, units, east);

  EXPECT_EQ(east.slot_changes, 0) << "the wheel reshuffled the column";
  EXPECT_EQ(east.off_ground_samples, 0);
  std::printf(
      "[march] east: worst penetration %.2f m (%.2f m once formed) over %d ticks\n",
      east.worst_penetration,
      east.worst_formed_penetration,
      east.ticks - north.ticks);
  EXPECT_LE(east.worst_formed_penetration, k_touching);
  expect_formed_on_slots(units, 2.0F);
  formation = formation_of(units.front());
  ASSERT_NE(formation, nullptr);
  EXPECT_TRUE(formation->is_formed())
      << Game::Formation::phase_to_string(formation->phase);
}

TEST_F(ArmyFormationMarchTest, AFormedColumnKeepsItsShapeThroughoutAWheel) {
  open({});
  auto const units = army(QVector3D(0.0F, 0.0F, -30.0F));
  deploy(units, QVector3D(0.0F, 0.0F, 0.0F), 0.0F, ArmyFormationIntent::Column);
  MarchRecord setup;
  run_for(70.0, units, setup);
  ASSERT_TRUE(formation_of(units.front())->is_formed());

  deploy(units, QVector3D(25.0F, 0.0F, 0.0F), 90.0F, ArmyFormationIntent::Column);
  MarchRecord wheel;
  run_for(100.0, units, wheel);
  EXPECT_EQ(wheel.slot_changes, 0);
  EXPECT_EQ(wheel.off_ground_samples, 0);
  EXPECT_LE(wheel.worst_penetration, k_touching);
  EXPECT_LT(wheel.worst_slot_error, 3.0F);
  for (auto const id : units) {
    EXPECT_LT(wheel.yaw_travel[id], 110.0F)
        << "troop " << id << " spun during a 90 degree wheel";
    EXPECT_LE(wheel.last_order[id] - wheel.first_order[id], 1U)
        << "slot tracking repeatedly restarted troop " << id << "'s order";
  }
  expect_formed_on_slots(units, 0.5F);
  EXPECT_TRUE(formation_of(units.front())->is_formed());
}

TEST_F(ArmyFormationMarchTest, AStraightMarchDoesNotBacktrackOrTurnBetweenSlotUpdates) {
  open({});
  auto const units = army(QVector3D(0.0F, 0.0F, -30.0F));
  deploy(units, QVector3D(), 0.0F, ArmyFormationIntent::Column);
  MarchRecord setup;
  run_for(70.0, units, setup);
  ASSERT_TRUE(formation_of(units.front())->is_formed());

  deploy(units, QVector3D(0.0F, 0.0F, 15.0F), 0.0F, ArmyFormationIntent::Column);
  MarchRecord march;
  run_for(25.0, units, march);
  EXPECT_LE(march.worst_penetration, k_touching);
  EXPECT_LT(march.worst_slot_error, 1.5F);

  EXPECT_LT(march.backwards_distance, 0.05F * static_cast<float>(units.size()));
  EXPECT_EQ(march.slot_changes, 0);
  for (auto const id : units) {
    EXPECT_LT(march.yaw_travel[id], 2.0F) << "troop " << id;
    EXPECT_EQ(march.last_order[id], march.first_order[id]);
  }
  ASSERT_TRUE(formation_of(units.front())->is_formed());
  const auto anchor = formation_of(units.front())->anchor;
  EXPECT_LT(std::hypot(anchor.x(), anchor.z() - 15.0F), 0.1F);
  expect_formed_on_slots(units, 0.1F);
}

TEST_F(ArmyFormationMarchTest, ALineCrossesABridgeInOrderAndReformsBeyondIt) {
  Game::Map::MapDefinition map;
  map.rivers.push_back(
      {QVector3D(-58.0F, 0.0F, 0.0F), QVector3D(58.0F, 0.0F, 0.0F), 8.0F});
  map.bridges.push_back(
      {QVector3D(0.0F, 0.0F, -7.0F), QVector3D(0.0F, 0.0F, 7.0F), 6.0F, 0.6F});
  open(map);
  auto const units = army(QVector3D(0.0F, 0.0F, -34.0F));

  Game::Command::DeployFormation order;
  order.units = units;
  order.anchor = QVector3D(0.0F, 0.0F, 30.0F);
  order.facing = 0.0F;
  order.intent = ArmyFormationIntent::Line;
  order.spacing = 1.5F;
  order.options.movement_policy = MovementPolicy::MaintainFormation;
  Game::Command::submit(m_session->world(),
                        Game::Command::Source::LocalPlayer,
                        k_owner,
                        std::move(order));

  MarchRecord crossing;
  run_for(90.0, units, crossing);

  EXPECT_EQ(crossing.slot_changes, 0);
  EXPECT_EQ(crossing.off_ground_samples, 0) << "a troop stood in the river";
  std::printf(
      "[march] bridge: worst penetration %.2f m (%.2f m once formed) over %d ticks\n",
      crossing.worst_penetration,
      crossing.worst_formed_penetration,
      crossing.ticks);
  EXPECT_LE(crossing.worst_formed_penetration, k_touching);
  for (auto const id : units) {
    EXPECT_GT(position_of(id).z(), 6.0F) << "troop " << id << " never crossed";
  }
  const auto* formation = formation_of(units.front());
  ASSERT_NE(formation, nullptr);
  EXPECT_FALSE(formation->compressed) << "the line did not reform past the bridge";
  expect_formed_on_slots(units, 2.0F);
}
