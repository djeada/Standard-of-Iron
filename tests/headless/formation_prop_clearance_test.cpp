#include <QDir>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "game/command/command.h"
#include "game/command/command_queue.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/map/map_loader.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/command_service.h"
#include "game/systems/default_content.h"
#include "game/systems/formation_combat_geometry.h"
#include "game/systems/nation_registry.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/pathfinding.h"
#include "game/systems/runtime_system_registry.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"

namespace {

using Engine::Core::EntityID;
using Game::Session::SessionContext;
using Game::Systems::NavGrid;
using Game::Units::SpawnType;

constexpr int k_owner = 1;
constexpr float k_soldier_radius = 0.25F;

struct PropHit {
  float soldier_seconds{0.0F};
  float worst_depth{0.0F};
  int soldiers_sampled{0};
};

class FormationPropClearance : public ::testing::Test {
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
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
  }

  auto open_map(const char* file) -> bool {
    Game::Map::MapDefinition map;
    QString error;
    if (!Game::Map::MapLoader::load_from_json_file(
            QDir(QStringLiteral("assets/maps")).filePath(QString::fromLatin1(file)),
            map,
            &error)) {
      ADD_FAILURE() << error.toStdString();
      return false;
    }
    map.structures.clear();
    map.spawns.clear();
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
    NavGrid::get_pathfinder()->update_navigation_grid();
    return true;
  }

  struct SolidProp {
    Game::Map::WorldProp prop;
    float x{0.0F};
    float z{0.0F};
  };

  auto solid_props() -> std::vector<SolidProp> {
    std::vector<SolidProp> out;
    auto& terrain = m_session->terrain();
    for (const auto& prop : terrain.world_props()) {
      if (!Game::Map::is_solid_world_prop_type(prop.type)) {
        continue;
      }
      auto const [x, z] = terrain.world_prop_world_xz(prop);
      out.push_back({prop, x, z});
    }
    return out;
  }

  auto densest_prop_patch(const std::vector<SolidProp>& props,
                          float half) -> QVector3D {
    QVector3D best;
    int best_count = -1;
    for (const auto& candidate : props) {
      if (std::abs(candidate.x) > 230.0F || std::abs(candidate.z) > 230.0F) {
        continue;
      }
      int count = 0;
      for (const auto& other : props) {
        if (std::abs(other.x - candidate.x) < half &&
            std::abs(other.z - candidate.z) < half) {
          ++count;
        }
      }
      if (count > best_count) {
        best_count = count;
        best = QVector3D(candidate.x, 0.0F, candidate.z);
      }
    }
    return best;
  }

  auto spawn(SpawnType type, const QVector3D& position) -> EntityID {
    Game::Units::SpawnParams params;
    params.position = NavGrid::snap_to_walkable_ground(position);
    params.player_id = k_owner;
    params.spawn_type = type;
    params.nation_id = Game::Systems::NationID::Carthage;
    auto unit = m_factory->create(type, m_session->world(), params);
    return unit ? unit->id() : 0;
  }

  auto march(const std::vector<EntityID>& units, const QVector3D& target) {
    auto const plan = Game::Systems::CommandService::plan_ground_move(
        m_session->world(), units, target);
    Game::Command::Move order;
    order.units = units;
    order.targets = plan.target_positions();
    order.facing_angles = plan.facing_angles();
    order.kind = Game::Systems::MoveOrderKind::PlayerMove;
    order.preserve_formation_mode = plan.preserve_formation_mode;
    Game::Command::submit(m_session->world(),
                          Game::Command::Source::LocalPlayer,
                          k_owner,
                          std::move(order));
  }

  auto measure(const std::vector<EntityID>& units,
               const std::vector<SolidProp>& props,
               double seconds) -> PropHit {
    PropHit hit;
    double const step = m_session->clock().tick_seconds();
    for (double elapsed = 0.0; elapsed < seconds; elapsed += step) {
      m_session->clock().advance(step);
      while (m_session->clock().consume_tick()) {
        m_session->world().update(static_cast<float>(step));
        for (auto const id : units) {
          auto* entity = m_session->world().get_entity(id);
          if (entity == nullptr) {
            continue;
          }
          for (const auto& anchor :
               Game::Systems::FormationCombat::soldier_spatial_anchors(*entity)) {
            ++hit.soldiers_sampled;
            float deepest = 0.0F;
            for (const auto& solid : props) {
              if (std::abs(solid.x - anchor.world_x) > 6.0F ||
                  std::abs(solid.z - anchor.world_z) > 6.0F) {
                continue;
              }
              deepest =
                  std::max(deepest,
                           Game::Map::world_prop_overlap_depth(solid.prop.type,
                                                               solid.prop.scale,
                                                               solid.x,
                                                               solid.z,
                                                               solid.prop.rotation,
                                                               anchor.world_x,
                                                               anchor.world_z,
                                                               k_soldier_radius));
            }
            if (deepest > 0.05F) {
              hit.soldier_seconds += static_cast<float>(step);
              hit.worst_depth = std::max(hit.worst_depth, deepest);
            }
          }
        }
      }
    }
    return hit;
  }

  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory;
  std::unique_ptr<SessionContext> m_session;
  std::unique_ptr<Game::Session::ScopedSession> m_scope;
};

TEST_F(FormationPropClearance, SoldiersMarchAroundSolidProps) {
  ASSERT_TRUE(open_map("map_battle_ticino.json"));
  auto const props = solid_props();
  ASSERT_FALSE(props.empty());
  QVector3D const patch = densest_prop_patch(props, 18.0F);

  std::vector<EntityID> units;
  std::vector<SpawnType> const kinds{SpawnType::Spearman,
                                     SpawnType::Swordsman,
                                     SpawnType::Swordsman,
                                     SpawnType::Spearman};
  for (int i = 0; i < 4; ++i) {
    auto const id = spawn(kinds[static_cast<std::size_t>(i)],
                          patch + QVector3D(-15.0F + i * 10.0F, 0.0F, -40.0F));
    if (id != 0U) {
      units.push_back(id);
    }
  }
  ASSERT_EQ(units.size(), 4U);
  QVector3D const target = patch + QVector3D(0.0F, 0.0F, 40.0F);
  march(units, target);

  auto const hit = measure(units, props, 45.0);
  float farthest = 0.0F;
  for (auto const id : units) {
    auto const* transform =
        m_session->world().try_get<Engine::Core::TransformComponent>(id);
    ASSERT_NE(transform, nullptr);
    farthest = std::max(farthest,
                        std::hypot(transform->position.x - target.x(),
                                   transform->position.z - target.z()));
  }
  std::printf("[prop-clearance] farthest unit from the ordered spot: %.1fm\n",
              farthest);
  EXPECT_LT(farthest, 25.0F) << "the army did not get through the props";
  std::printf("[prop-clearance] patch=(%.1f, %.1f) soldiers_in_props=%.1f "
              "soldier-seconds worst_depth=%.2fm samples=%d\n",
              patch.x(),
              patch.z(),
              hit.soldier_seconds,
              hit.worst_depth,
              hit.soldiers_sampled);
  EXPECT_LT(hit.worst_depth, 0.45F);
  EXPECT_LT(hit.soldier_seconds, 4.0F);
}

} // namespace
