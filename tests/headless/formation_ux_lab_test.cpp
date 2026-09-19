#include <QDir>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "game/command/command.h"
#include "game/command/command_queue.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/formation/army_formation_planner.h"
#include "game/formation/army_formation_registry.h"
#include "game/formation/army_formation_service.h"
#include "game/game_config.h"
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
using Game::Formation::ArmyFormationIntent;
using Game::Session::SessionContext;
using Game::Systems::NavGrid;
using Game::Units::SpawnType;

constexpr int k_owner = 1;
constexpr float k_pi = 3.14159265F;

struct UxScore {
  std::string label;
  float ideal_seconds{0.0F};
  float formed_seconds{-1.0F};
  float worst_final_error{0.0F};
  float worst_facing_error{0.0F};
  int stalls{0};
  float worst_penetration{0.0F};
  float penetration_seconds{0.0F};
  float heading_travel{0.0F};
  int off_ground{0};
  bool valid_order{false};

  [[nodiscard]] auto efficiency() const -> float {
    return formed_seconds > 0.0F ? ideal_seconds / formed_seconds : 0.0F;
  }

  void print() const {
    std::printf(
        "[ux] %-44s order=%s ideal=%5.1fs formed=%6.1fs eff=%4.2f "
        "final_err=%5.2fm facing_err=%5.1f stalls=%3d body_overlap=%4.2fm/%5.1fs "
        "turning=%6.0fdeg off_ground=%d\n",
        label.c_str(),
        valid_order ? "ok" : "REJECTED",
        ideal_seconds,
        formed_seconds,
        efficiency(),
        worst_final_error,
        worst_facing_error,
        stalls,
        worst_penetration,
        penetration_seconds,
        heading_travel,
        off_ground);
  }
};

struct Troop {
  SpawnType type;
  QVector3D position;
};

class FormationUxLab : public ::testing::Test {
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

  void open_match(Game::Map::MapDefinition map) {
    m_scope.reset();
    m_session.reset();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
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
    NavGrid::get_pathfinder()->update_navigation_grid();
  }

  void open_field(int size = 160) {
    Game::Map::MapDefinition map;
    map.grid.width = size;
    map.grid.height = size;
    map.grid.tile_size = 1.0F;
    map.coordSystem = Game::Map::CoordSystem::World;
    map.biome.procedural_boulders_enabled = false;
    map.biome.procedural_iron_ore_enabled = false;
    map.biome.procedural_trees_enabled = false;
    open_match(map);
  }

  auto real_map(const char* file) -> bool {
    Game::Map::MapDefinition map;
    QString error;
    if (!Game::Map::MapLoader::load_from_json_file(
            QDir(QStringLiteral("assets/maps")).filePath(QString::fromLatin1(file)),
            map,
            &error)) {
      ADD_FAILURE() << error.toStdString();
      return false;
    }
    open_match(map);
    return true;
  }

  auto spawn(SpawnType type, const QVector3D& position, float yaw = 0.0F) -> EntityID {
    Game::Units::SpawnParams params;
    params.position = position;
    params.player_id = k_owner;
    params.spawn_type = type;
    params.rotation_y = yaw;
    params.nation_id = Game::Systems::NationID::RomanRepublic;
    auto unit = m_factory->create(type, m_session->world(), params);
    return unit ? unit->id() : 0;
  }

  // A realistic mixed selection standing as a loose crowd around `centre`.
  auto
  army(const QVector3D& centre, int count, float yaw = 0.0F) -> std::vector<EntityID> {
    std::vector<SpawnType> const kinds{SpawnType::Swordsman,
                                       SpawnType::Spearman,
                                       SpawnType::Swordsman,
                                       SpawnType::Archer,
                                       SpawnType::Spearman,
                                       SpawnType::MountedSwordsman};
    std::vector<EntityID> units;
    int const per_row =
        std::max(1, static_cast<int>(std::ceil(std::sqrt(count * 1.5F))));
    for (int i = 0; i < count; ++i) {
      float const x = (static_cast<float>(i % per_row) - (per_row - 1) * 0.5F) * 9.0F;
      float const z = -static_cast<float>(i / per_row) * 7.0F;
      float const jitter_x = std::sin(static_cast<float>(i) * 12.9898F) * 2.0F;
      float const jitter_z = std::cos(static_cast<float>(i) * 78.233F) * 2.0F;
      auto const kind = kinds[static_cast<std::size_t>(i) % kinds.size()];
      auto const id =
          spawn(kind, centre + QVector3D(x + jitter_x, 0.0F, z + jitter_z), yaw);
      if (id != 0U) {
        units.push_back(id);
      }
    }
    return units;
  }

  auto position_of(EntityID id) -> QVector3D {
    const auto* transform =
        m_session->world().try_get<Engine::Core::TransformComponent>(id);
    return transform == nullptr
               ? QVector3D()
               : QVector3D(transform->position.x, 0.0F, transform->position.z);
  }

  auto yaw_of(EntityID id) -> float {
    const auto* transform =
        m_session->world().try_get<Engine::Core::TransformComponent>(id);
    return transform == nullptr ? 0.0F : transform->rotation.y;
  }

  auto speed_of(EntityID id) -> float {
    const auto* unit = m_session->world().try_get<Engine::Core::UnitComponent>(id);
    return unit == nullptr ? 1.0F : std::max(0.1F, unit->speed);
  }

  // The player's formation placement: exactly what ArmyFormationController sends.
  auto deploy(const std::vector<EntityID>& units,
              const QVector3D& anchor,
              float facing,
              ArmyFormationIntent intent,
              float frontage = 0.0F) -> bool {
    Game::Formation::ArmyFormationRequest request;
    request.members = units;
    request.anchor = anchor;
    request.facing = facing;
    request.frontage = frontage;
    request.intent = intent;
    request.spacing = Game::GameConfig::instance().gameplay().formation_spacing_default;
    auto const preview =
        Game::Formation::ArmyFormationService::preview(m_session->world(), request);
    m_shown.assign(preview.positions.begin(), preview.positions.end());
    m_shown_facing = preview.facing_angles;
    if (!preview.valid) {
      return false;
    }
    Game::Command::DeployFormation order;
    order.units = units;
    order.anchor = anchor;
    order.facing = facing;
    order.frontage = frontage;
    order.intent = intent;
    order.spacing = request.spacing;
    Game::Command::submit(m_session->world(),
                          Game::Command::Source::LocalPlayer,
                          k_owner,
                          std::move(order));
    return true;
  }

  // The player's plain right-click move of a selection.
  auto move(const std::vector<EntityID>& units, const QVector3D& target) -> bool {
    auto const plan = Game::Systems::CommandService::plan_ground_move(
        m_session->world(), units, target);
    m_shown = plan.target_positions();
    m_shown_facing = plan.facing_angles();
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
    return plan.matches_members(units);
  }

  auto half_extent(EntityID id) -> std::pair<float, float> {
    auto const members = Game::Formation::ArmyFormationPlanner::collect_members(
        m_session->world(), {id});
    if (members.empty()) {
      return {0.5F, 0.5F};
    }
    return {members.front().half_width, members.front().half_depth};
  }

  // Soldiers of two different troops standing inside each other, as the
  // player sees them: the deepest body interpenetration this tick.
  auto soldier_overlap(const std::vector<EntityID>& units) -> float {
    struct Body {
      std::vector<QVector3D> points;
      QVector3D centre;
      float reach{0.0F};
      float radius{0.3F};
    };
    std::vector<Body> bodies;
    for (auto const id : units) {
      auto* entity = m_session->world().get_entity(id);
      if (entity == nullptr) {
        continue;
      }
      Body body;
      auto const layout = Game::Systems::FormationCombat::resolve_layout(*entity);
      body.radius = std::min(0.35F, layout.body_radius);
      body.centre = position_of(id);
      for (const auto& anchor :
           Game::Systems::FormationCombat::soldier_spatial_anchors(*entity, layout)) {
        QVector3D const point(anchor.world_x, 0.0F, anchor.world_z);
        body.points.push_back(point);
        body.reach = std::max(body.reach, (point - body.centre).length());
      }
      bodies.push_back(std::move(body));
    }
    float worst = 0.0F;
    for (std::size_t i = 0; i < bodies.size(); ++i) {
      for (std::size_t j = i + 1; j < bodies.size(); ++j) {
        const auto& a = bodies[i];
        const auto& b = bodies[j];
        float const contact = a.radius + b.radius;
        if ((a.centre - b.centre).length() > a.reach + b.reach + contact) {
          continue;
        }
        for (const auto& p : a.points) {
          for (const auto& q : b.points) {
            worst = std::max(worst, contact - (p - q).length());
          }
        }
      }
    }
    return worst;
  }

  auto measure(const std::string& label,
               const std::vector<EntityID>& units,
               bool valid_order,
               double seconds) -> UxScore {
    UxScore score;
    score.label = label;
    score.valid_order = valid_order;
    std::vector<QVector3D> start;
    for (std::size_t i = 0; i < units.size(); ++i) {
      start.push_back(position_of(units[i]));
      if (i < m_shown.size()) {
        score.ideal_seconds =
            std::max(score.ideal_seconds,
                     (m_shown[i] - start.back()).length() / speed_of(units[i]));
      }
    }
    m_dump = nullptr;
    if (const char* dir = std::getenv("SOI_UX_DUMP")) {
      std::string name = label;
      std::replace(name.begin(), name.end(), ' ', '_');
      m_dump = std::fopen((std::string(dir) + "/" + name + ".csv").c_str(), "w");
      if (m_dump != nullptr) {
        auto* nav = NavGrid::get_pathfinder();
        float min_x = 1e9F, max_x = -1e9F, min_z = 1e9F, max_z = -1e9F;
        for (auto const id : units) {
          auto const p = position_of(id);
          min_x = std::min(min_x, p.x());
          max_x = std::max(max_x, p.x());
          min_z = std::min(min_z, p.z());
          max_z = std::max(max_z, p.z());
        }
        for (const auto& p : m_shown) {
          min_x = std::min(min_x, p.x());
          max_x = std::max(max_x, p.x());
          min_z = std::min(min_z, p.z());
          max_z = std::max(max_z, p.z());
        }
        for (float x = min_x - 25.0F; x <= max_x + 25.0F; x += 1.0F) {
          for (float z = min_z - 25.0F; z <= max_z + 25.0F; z += 1.0F) {
            if (nav != nullptr && !nav->is_world_position_walkable(
                                      QVector3D(x, 0.0F, z),
                                      Game::Systems::Pathfinding::Passability::Heavy)) {
              std::fprintf(m_dump,
                           "o,0,0,%d,%d\n",
                           static_cast<int>(x * 100.0F),
                           static_cast<int>(z * 100.0F));
            }
          }
        }
        for (std::size_t i = 0; i < m_shown.size(); ++i) {
          std::fprintf(m_dump,
                       "p,0,%zu,%d,%d\n",
                       i,
                       static_cast<int>(std::lround(m_shown[i].x() * 100.0F)),
                       static_cast<int>(std::lround(m_shown[i].z() * 100.0F)));
        }
      }
    }
    std::vector<float> last_yaw(units.size());
    std::vector<bool> moving(units.size(), false);
    std::vector<QVector3D> last_position = start;
    for (std::size_t i = 0; i < units.size(); ++i) {
      last_yaw[i] = yaw_of(units[i]);
    }
    auto* pathfinder = NavGrid::get_pathfinder();
    double const step = m_session->clock().tick_seconds();
    int tick = 0;
    float formed_since = -1.0F;
    for (double elapsed = 0.0; elapsed < seconds - 1e-9; elapsed += step) {
      m_session->clock().advance(step);
      while (m_session->clock().consume_tick()) {
        m_session->world().update(static_cast<float>(step));
        ++tick;
        float const now = static_cast<float>(tick * step);
        bool all_formed = true;
        float tick_penetration = 0.0F;
        for (std::size_t i = 0; i < units.size(); ++i) {
          auto const position = position_of(units[i]);
          float const speed =
              (position - last_position[i]).length() / static_cast<float>(step);
          last_position[i] = position;
          float const remaining =
              i < m_shown.size() ? (m_shown[i] - position).length() : 0.0F;
          if (speed > 0.5F * speed_of(units[i])) {
            moving[i] = true;
          } else if (moving[i] && speed < 0.15F * speed_of(units[i]) &&
                     remaining > 2.0F) {
            ++score.stalls;
            moving[i] = false;
          }
          float const yaw = yaw_of(units[i]);
          score.heading_travel += std::abs(std::remainder(yaw - last_yaw[i], 360.0F));
          last_yaw[i] = yaw;
          float const facing_error =
              i < m_shown_facing.size()
                  ? std::abs(std::remainder(yaw - m_shown_facing[i], 360.0F))
                  : 0.0F;
          if (remaining > 1.5F || speed > 0.2F || facing_error > 15.0F) {
            all_formed = false;
          }
          auto const cell = pathfinder->world_to_grid(position.x(), position.z());
          if (!pathfinder->is_walkable(cell.x, cell.y)) {
            ++score.off_ground;
          }
        }
        tick_penetration = soldier_overlap(units);
        if (tick == 2 && std::getenv("SOI_UX_DEBUG") != nullptr) {
          auto& registry =
              Game::Formation::ArmyFormationRegistry::for_world(m_session->world());
          if (const auto* group = registry.find(registry.group_of(units.front()))) {
            std::printf("[ux-debug] %s group maintain=%d morph=%d has_dest=%d\n",
                        label.c_str(),
                        static_cast<int>(group->maintains_formation()),
                        static_cast<int>(group->morph.active),
                        static_cast<int>(group->has_destination));
          }
        }
        if (m_dump != nullptr) {
          for (std::size_t i = 0; i < units.size(); ++i) {
            auto const position = position_of(units[i]);
            std::fprintf(m_dump,
                         "y,%d,%zu,%d,%d,%d\n",
                         static_cast<int>(std::lround(now * 100.0F)),
                         i,
                         static_cast<int>(std::lround(yaw_of(units[i]) * 10.0F)),
                         static_cast<int>(std::lround(position.x() * 100.0F)),
                         static_cast<int>(std::lround(position.z() * 100.0F)));
          }
        }
        if (m_dump != nullptr && tick % 15 == 0) {
          for (std::size_t i = 0; i < units.size(); ++i) {
            auto* entity = m_session->world().get_entity(units[i]);
            if (entity == nullptr) {
              continue;
            }
            for (const auto& anchor :
                 Game::Systems::FormationCombat::soldier_spatial_anchors(*entity)) {
              std::fprintf(m_dump,
                           "s,%d,%zu,%d,%d\n",
                           static_cast<int>(std::lround(now * 100.0F)),
                           i,
                           static_cast<int>(std::lround(anchor.world_x * 100.0F)),
                           static_cast<int>(std::lround(anchor.world_z * 100.0F)));
            }
          }
        }
        score.worst_penetration = std::max(score.worst_penetration, tick_penetration);
        if (tick_penetration > 0.0F) {
          score.penetration_seconds += static_cast<float>(step);
        }
        if (all_formed) {
          if (formed_since < 0.0F) {
            formed_since = now;
          }
        } else {
          formed_since = -1.0F;
        }
      }
    }
    score.formed_seconds = formed_since;
    if (m_dump != nullptr) {
      std::fclose(m_dump);
      m_dump = nullptr;
    }
    for (std::size_t i = 0; i < units.size() && i < m_shown.size(); ++i) {
      score.worst_final_error = std::max(score.worst_final_error,
                                         (position_of(units[i]) - m_shown[i]).length());
      if (i < m_shown_facing.size()) {
        score.worst_facing_error = std::max(
            score.worst_facing_error,
            std::abs(std::remainder(yaw_of(units[i]) - m_shown_facing[i], 360.0F)));
      }
    }
    if (std::getenv("SOI_UX_DEBUG") != nullptr) {
      auto& registry =
          Game::Formation::ArmyFormationRegistry::for_world(m_session->world());
      for (std::size_t i = 0; i < units.size(); ++i) {
        auto* entity = m_session->world().get_entity(units[i]);
        const auto* group = registry.find(registry.group_of(units[i]));
        const auto* slot = group != nullptr ? group->find_slot_for(units[i]) : nullptr;
        float const yaw = yaw_of(units[i]) * k_pi / 180.0F;
        QVector3D const lateral(std::cos(yaw), 0.0F, -std::sin(yaw));
        QVector3D const depth(std::sin(yaw), 0.0F, std::cos(yaw));
        float reach_x = 0.0F;
        float reach_z = 0.0F;
        auto const centre = position_of(units[i]);
        for (const auto& a :
             Game::Systems::FormationCombat::soldier_spatial_anchors(*entity)) {
          QVector3D const o = QVector3D(a.world_x, 0.0F, a.world_z) - centre;
          reach_x = std::max(reach_x, std::abs(QVector3D::dotProduct(o, lateral)));
          reach_z = std::max(reach_z, std::abs(QVector3D::dotProduct(o, depth)));
        }
        const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
        std::printf("[ux-debug] %zu type=%d yaw=%.0f planned hw=%.2f hd=%.2f actual "
                    "reach x=%.2f z=%.2f "
                    "files_override=%d local=(%.1f,%.1f)\n",
                    i,
                    unit ? static_cast<int>(unit->spawn_type) : -1,
                    yaw_of(units[i]),
                    slot ? slot->half_width : -1.0F,
                    slot ? slot->half_depth : -1.0F,
                    reach_x,
                    reach_z,
                    unit ? unit->formation_files_override : -1,
                    slot ? slot->local_offset.x() : 0.0F,
                    slot ? slot->local_offset.z() : 0.0F);
      }
    }
    score.print();
    return score;
  }

  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory;
  std::unique_ptr<SessionContext> m_session;
  std::unique_ptr<Game::Session::ScopedSession> m_scope;
  std::vector<QVector3D> m_shown;
  std::vector<float> m_shown_facing;
  std::FILE* m_dump{nullptr};
};

auto degrees_toward(const QVector3D& from, const QVector3D& to) -> float {
  return std::atan2(to.x() - from.x(), to.z() - from.z()) * 180.0F / k_pi;
}

} // namespace

TEST_F(FormationUxLab, OpenGround) {
  if (std::getenv("SOI_UX_LAB") == nullptr) {
    GTEST_SKIP() << "set SOI_UX_LAB=1 to run the open-ground formation lab";
  }
  struct Case {
    const char* name;
    ArmyFormationIntent intent;
    QVector3D target;
    float facing;
    bool plain_move;
  };
  std::vector<Case> const cases{
      {"line ahead", ArmyFormationIntent::Line, {0, 0, 40}, 0.0F, false},
      {"column ahead", ArmyFormationIntent::Column, {0, 0, 40}, 0.0F, false},
      {"default ahead", ArmyFormationIntent::FactionDefault, {0, 0, 40}, 0.0F, false},
      {"line wheel right", ArmyFormationIntent::Line, {40, 0, 0}, 90.0F, false},
      {"column wheel right", ArmyFormationIntent::Column, {40, 0, 0}, 90.0F, false},
      {"line about face", ArmyFormationIntent::Line, {0, 0, -40}, 180.0F, false},
      {"column about face", ArmyFormationIntent::Column, {0, 0, -40}, 180.0F, false},
      {"line in place turn", ArmyFormationIntent::Line, {0, 0, 0}, 90.0F, false},
      {"plain move ahead", ArmyFormationIntent::FactionDefault, {0, 0, 40}, 0.0F, true},
      {"plain move sideways",
       ArmyFormationIntent::FactionDefault,
       {40, 0, 0},
       0.0F,
       true},
  };
  int const count = std::getenv("SOI_UX_COUNT") != nullptr
                        ? std::atoi(std::getenv("SOI_UX_COUNT"))
                        : 8;
  for (const auto& c : cases) {
    open_field();
    auto const units = army(QVector3D(0, 0, 0), count);
    ASSERT_EQ(static_cast<int>(units.size()), count);
    bool const valid = c.plain_move ? move(units, c.target)
                                    : deploy(units, c.target, c.facing, c.intent);
    measure(c.name, units, valid, 60.0);
  }
}

namespace {

struct Leg {
  QVector3D from;
  QVector3D to;
  float route{0.0F};
};

auto area_is_open(Game::Systems::Pathfinding& pathfinder,
                  const QVector3D& centre,
                  float half_extent) -> bool {
  for (int i = -3; i <= 3; ++i) {
    for (int j = -3; j <= 3; ++j) {
      QVector3D const p(centre.x() + half_extent * static_cast<float>(i) / 3.0F,
                        0.0F,
                        centre.z() + half_extent * static_cast<float>(j) / 3.0F);
      if (!pathfinder.is_world_position_walkable(
              p, Game::Systems::Pathfinding::Passability::Light)) {
        return false;
      }
    }
  }
  return true;
}

// Legs where the ground matters: open ground for the army at both ends, a
// path between them, and no straight line (a river, a hill, a wood or a
// settlement is in the way).
auto find_legs(int grid_width, int grid_height, int wanted) -> std::vector<Leg> {
  std::vector<Leg> legs;
  auto* pathfinder = NavGrid::get_pathfinder();
  if (pathfinder == nullptr) {
    return legs;
  }
  constexpr float k_leg = 50.0F;
  constexpr float k_half_area = 12.0F;
  constexpr float k_max_detour = 1.8F;
  int const step = std::max(12, std::min(grid_width, grid_height) / 14);
  for (int gx = step; gx < grid_width - step && static_cast<int>(legs.size()) < wanted;
       gx += step) {
    for (int gz = step;
         gz < grid_height - step && static_cast<int>(legs.size()) < wanted;
         gz += step) {
      QVector3D const from = NavGrid::grid_to_world(Game::Systems::Point(gx, gz));
      if (!area_is_open(*pathfinder, from, k_half_area)) {
        continue;
      }
      for (int d = 0; d < 8; ++d) {
        float const angle = static_cast<float>(d) * k_pi / 4.0F;
        QVector3D const to(from.x() + std::sin(angle) * k_leg,
                           0.0F,
                           from.z() + std::cos(angle) * k_leg);
        if (!area_is_open(*pathfinder, to, k_half_area)) {
          continue;
        }
        if (pathfinder->is_world_segment_walkable(
                from, to, Game::Systems::Pathfinding::Passability::Heavy)) {
          continue;
        }
        auto const path =
            pathfinder->find_path(NavGrid::world_to_grid(from.x(), from.z()),
                                  NavGrid::world_to_grid(to.x(), to.z()),
                                  Game::Systems::Pathfinding::Passability::Heavy);
        if (path.size() < 2U) {
          continue;
        }
        float route = 0.0F;
        for (std::size_t k = 1; k < path.size(); ++k) {
          route += std::hypot(static_cast<float>(path[k].x - path[k - 1].x),
                              static_cast<float>(path[k].y - path[k - 1].y));
        }
        if (route > k_leg * k_max_detour) {
          continue;
        }
        bool distinct = true;
        for (const auto& leg : legs) {
          distinct = distinct && (leg.from - from).length() > 80.0F;
        }
        if (distinct) {
          legs.push_back({from, to, route});
          break;
        }
      }
    }
  }
  return legs;
}

} // namespace

TEST_F(FormationUxLab, RealMaps) {
  if (std::getenv("SOI_UX_LAB") == nullptr) {
    GTEST_SKIP() << "set SOI_UX_LAB=1 to run the real-map formation lab";
  }
  std::vector<const char*> maps{"map_rivers.json",
                                "map_mountain.json",
                                "map_forest.json",
                                "map_spanish_grove.json",
                                "map_copper_canyons.json",
                                "map_amber_delta.json",
                                "map_battle_trebia.json",
                                "map_crossing_rhone.json",
                                "map_battle_cannae.json"};
  if (const char* only = std::getenv("SOI_UX_MAP")) {
    maps = {only};
  }
  int const legs_per_map =
      std::getenv("SOI_UX_LEGS") != nullptr ? std::atoi(std::getenv("SOI_UX_LEGS")) : 2;
  struct Order {
    const char* name;
    ArmyFormationIntent intent;
    bool plain_move;
  };
  std::vector<Order> const orders{{"line", ArmyFormationIntent::Line, false},
                                  {"column", ArmyFormationIntent::Column, false},
                                  {"move", ArmyFormationIntent::FactionDefault, true}};
  for (const char* map : maps) {
    Game::Map::MapDefinition definition;
    QString error;
    ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
        QDir(QStringLiteral("assets/maps")).filePath(QString::fromLatin1(map)),
        definition,
        &error))
        << error.toStdString();
    open_match(definition);
    auto const legs =
        find_legs(definition.grid.width, definition.grid.height, legs_per_map);
    if (legs.empty()) {
      std::printf("[ux] %s: no leg across obstacles found\n", map);
    }
    for (std::size_t leg_index = 0; leg_index < legs.size(); ++leg_index) {
      const auto& leg = legs[leg_index];
      for (const auto& order : orders) {
        open_match(definition);
        float const heading = degrees_toward(leg.from, leg.to);
        auto const units = army(leg.from, 8, heading);
        bool const valid = order.plain_move
                               ? move(units, leg.to)
                               : deploy(units, leg.to, heading, order.intent);
        char label[160];
        std::snprintf(label,
                      sizeof label,
                      "%s leg%zu route=%.0fm %s",
                      map,
                      leg_index,
                      leg.route,
                      order.name);
        measure(label, units, valid, 90.0);
      }
    }
  }
}

TEST_F(FormationUxLab, Silhouettes) {
  if (std::getenv("SOI_UX_LAB") == nullptr) {
    GTEST_SKIP() << "set SOI_UX_LAB=1 to print the formation silhouettes";
  }
  struct Intent {
    const char* name;
    ArmyFormationIntent intent;
  };
  std::vector<Intent> const intents{
      {"faction_default", ArmyFormationIntent::FactionDefault},
      {"line", ArmyFormationIntent::Line},
      {"column", ArmyFormationIntent::Column},
      {"defensive", ArmyFormationIntent::Defensive},
      {"assault", ArmyFormationIntent::Assault},
      {"encirclement", ArmyFormationIntent::Encirclement}};
  for (int const count : {4, 6, 8, 12, 16}) {
    open_field(220);
    auto const units = army(QVector3D(0, 0, -60), count);
    for (const auto& intent : intents) {
      Game::Formation::ArmyFormationRequest request;
      request.members = units;
      request.anchor = QVector3D(0, 0, 20);
      request.facing = 0.0F;
      request.intent = intent.intent;
      request.spacing =
          Game::GameConfig::instance().gameplay().formation_spacing_default;
      auto const plan =
          Game::Formation::ArmyFormationPlanner::plan(m_session->world(), request);
      std::printf("[shape] %-16s n=%2d %s frontage=%5.1f depth=%5.1f ranks=%d\n",
                  intent.name,
                  count,
                  plan.valid ? "" : plan.rejection_reason.c_str(),
                  plan.frontage,
                  plan.depth,
                  plan.valid ? plan.rank_count() : 0);
      if (!plan.valid) {
        continue;
      }
      // One character per troop centre on a 3 m grid, front rank on top.
      float min_x = 1e9F, max_x = -1e9F, min_z = 1e9F, max_z = -1e9F;
      for (const auto& slot : plan.slot_list) {
        min_x = std::min(min_x, slot.local_offset.x());
        max_x = std::max(max_x, slot.local_offset.x());
        min_z = std::min(min_z, slot.local_offset.z());
        max_z = std::max(max_z, slot.local_offset.z());
      }
      constexpr float k_cell = 3.0F;
      int const w = static_cast<int>((max_x - min_x) / k_cell) + 1;
      int const h = static_cast<int>((max_z - min_z) / k_cell) + 1;
      std::vector<std::string> grid(static_cast<std::size_t>(h), std::string(w, '.'));
      for (const auto& slot : plan.slot_list) {
        int const x = static_cast<int>((slot.local_offset.x() - min_x) / k_cell);
        int const z = static_cast<int>((max_z - slot.local_offset.z()) / k_cell);
        char mark = '#';
        switch (slot.role) {
        case Game::Formation::ArmyRole::Ranged:
          mark = 'r';
          break;
        case Game::Formation::ArmyRole::LeftFlank:
        case Game::Formation::ArmyRole::RightFlank:
        case Game::Formation::ArmyRole::Vanguard:
          mark = 'c';
          break;
        default:
          break;
        }
        grid[static_cast<std::size_t>(z)][static_cast<std::size_t>(x)] = mark;
      }
      for (const auto& line : grid) {
        std::printf("[shape]    %s\n", line.c_str());
      }
    }
  }
}
