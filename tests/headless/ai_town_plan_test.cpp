#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <deque>
#include <gtest/gtest.h>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "game/core/component_gameplay.h"
#include "game/core/component_structures.h"
#include "game/core/event_manager.h"
#include "game/core/world.h"
#include "game/formation/army_formation_registry.h"
#include "game/map/map_definition.h"
#include "game/map/map_transformer.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/systems/ai_system.h"
#include "game/systems/ai_system/ai_commander_doctrine.h"
#include "game/systems/ai_system/ai_doctrine_catalog.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/default_content.h"
#include "game/systems/formation_combat_geometry.h"
#include "game/systems/nation_registry.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/runtime_system_registry.h"
#include "game/systems/troop_count_registry.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"

namespace {

using Engine::Core::EntityID;
using Engine::Core::UnitComponent;
using Game::Session::SessionContext;

constexpr int k_map_size = 128;
constexpr int k_owner = 2;
constexpr int k_town_grid = 64;
constexpr int k_opening_builders = 5;

struct Standing {
  std::string type;
  float x = 0.0F;
  float z = 0.0F;
  float rotation_y = 0.0F;
  float span = 0.0F;
};

struct TownCensus {
  int barracks = 0;
  int homes = 0;
  int farms = 0;
  int towers = 0;
  int walls = 0;
  int gates = 0;
  int markets = 0;
  int engines = 0;

  [[nodiscard]] auto total() const -> int {
    return barracks + homes + farms + towers + walls + markets + engines;
  }

  [[nodiscard]] auto signature() const -> std::string {
    return std::to_string(barracks) + "/" + std::to_string(homes) + "/" +
           std::to_string(farms) + "/" + std::to_string(towers) + "/" +
           std::to_string(walls) + "/" + std::to_string(markets);
  }
};

struct ArmyCensus {
  int foot = 0;
  int missile = 0;
  int horse = 0;
  int engines = 0;

  [[nodiscard]] auto total() const -> int { return foot + missile + horse + engines; }
};

class AiTownPlanTest : public ::testing::Test {
protected:
  void SetUp() override {
    reset_shared_world_state();

    ASSERT_TRUE(Game::Systems::AI::load_default_ai_doctrine_catalog())
        << "the shipped assets/data/ai files did not load";
    Game::Systems::NavGrid::initialize(k_map_size, k_map_size);
    m_factory = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*m_factory);
    Game::Map::MapTransformer::setFactoryRegistry(m_factory);
  }

  void TearDown() override {
    m_scope.reset();
    m_session.reset();
    Game::Map::MapTransformer::setFactoryRegistry(nullptr);
    reset_shared_world_state();
  }

  static void reset_shared_world_state() {
    Engine::Core::EventManager::instance().clear_all_subscriptions();
    Game::Map::TerrainService::instance().clear();
    Game::Formation::ArmyFormationRegistry::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Systems::TroopCountRegistry::instance().clear();
    Game::Systems::PlayerResourceRegistry::instance().clear();
    Game::Systems::FormationCombat::invalidate_layout_cache();
  }

  auto settle(Game::Units::SpawnType commander,
              Game::Systems::NationID nation) -> SessionContext& {
    m_session = std::make_unique<SessionContext>();
    auto& session = *m_session;
    session.world().set_presentation_enabled(false);
    m_scope = std::make_unique<Game::Session::ScopedSession>(session);
    Game::Map::MapTransformer::setFactoryRegistry(m_factory);
    Game::Systems::NavGrid::initialize(k_map_size, k_map_size);

    session.owners().register_owner_with_id(
        k_owner, Game::Systems::OwnerType::AI, "settler");
    session.owners().set_owner_team(k_owner, 1);

    Game::Systems::initialize_default_content(session.nations());
    session.nations().set_player_nation(k_owner, nation);

    Game::Map::MapDefinition map_definition;
    map_definition.grid.width = k_map_size;
    map_definition.grid.height = k_map_size;
    map_definition.grid.tile_size = 1.0F;
    scatter_resources(map_definition, k_town_grid, k_town_grid);
    session.terrain().initialize(map_definition);

    Game::Systems::register_runtime_systems(session.world());

    auto& economy = session.economy();
    economy.ensure_owner(k_owner);
    economy.set(k_owner, Game::Systems::ResourceType::Gold, 600);
    economy.set(k_owner, Game::Systems::ResourceType::Food, 300);
    economy.set(k_owner, Game::Systems::ResourceType::Wood, 400);
    economy.set(k_owner, Game::Systems::ResourceType::Stone, 300);
    economy.set(k_owner, Game::Systems::ResourceType::Iron, 200);

    spawn(
        session, Game::Units::SpawnType::Barracks, world_of(k_town_grid, k_town_grid));
    spawn(session, commander, world_of(k_town_grid + 2, k_town_grid + 2));
    for (int index = 0; index < k_opening_builders; ++index) {
      spawn(session,
            Game::Units::SpawnType::Builder,
            world_of(k_town_grid + 4, k_town_grid - 4 + index * 2));
    }
    if (const auto bearing = enemy_bearing_degrees(); bearing.has_value()) {

      constexpr int k_enemy = 3;
      constexpr float k_enemy_distance = 54.0F;
      session.owners().register_owner_with_id(
          k_enemy, Game::Systems::OwnerType::Player, "rival");
      session.owners().set_owner_team(k_enemy, 2);
      session.nations().set_player_nation(k_enemy, nation);
      economy.ensure_owner(k_enemy);
      const float radians = *bearing * 3.14159265F / 180.0F;
      const QVector3D centre = world_of(k_town_grid, k_town_grid);
      spawn(session,
            Game::Units::SpawnType::Barracks,
            QVector3D(centre.x() + std::cos(radians) * k_enemy_distance,
                      0.0F,
                      centre.z() + std::sin(radians) * k_enemy_distance),
            k_enemy);
    }

    if (auto* ai = session.world().get_system<Game::Systems::AISystem>()) {
      ai->reinitialize();
      auto profile =
          Game::Systems::AI::doctrine_profile_for_owner(session.world(), k_owner);
      EXPECT_TRUE(profile.has_value()) << "the commander has no authored doctrine";
      if (profile.has_value()) {
        ai->set_ai_profile(k_owner, *profile);
      }
    }
    return session;
  }

  static void scatter_resources(Game::Map::MapDefinition& map, int grid_x, int grid_z) {
    const auto add = [&map](Game::Map::WorldProp::Type type, int x, int z) {
      if (x < 2 || z < 2 || x >= k_map_size - 2 || z >= k_map_size - 2) {
        return;
      }
      Game::Map::WorldProp prop;
      prop.type = type;
      prop.x = static_cast<float>(x);
      prop.z = static_cast<float>(z);
      map.world_props.push_back(prop);
    };
    for (int ring = 0; ring < 10; ++ring) {
      const int offset = 26 + ring * 2;
      for (int lane = -3; lane <= 3; ++lane) {
        const int shift = lane * 3;
        add(Game::Map::WorldProp::Type::OliveTree, grid_x + offset, grid_z + shift);
        add(Game::Map::WorldProp::Type::PineTree, grid_x - offset, grid_z + shift);
        add(Game::Map::WorldProp::Type::Boulder, grid_x + shift, grid_z + offset);
        add(Game::Map::WorldProp::Type::IronOre, grid_x + shift, grid_z - offset);
      }
    }
  }

  auto spawn(SessionContext& session,
             Game::Units::SpawnType type,
             QVector3D position,
             int owner = k_owner) -> EntityID {
    Game::Units::SpawnParams params;
    params.position = position;
    params.player_id = owner;
    params.spawn_type = type;
    params.ai_controlled = true;
    params.is_initial_spawn = true;
    params.max_population = 280;
    params.enables_production = true;
    const auto* nation = session.nations().get_nation_for_player(owner);
    params.nation_id =
        nation != nullptr ? nation->id : Game::Systems::NationID::RomanRepublic;
    auto unit = m_factory->create(type, session.world(), params);
    return unit ? unit->id() : 0;
  }

  static auto world_of(int grid_x, int grid_z) -> QVector3D {
    return Game::Systems::NavGrid::grid_to_world(Game::Systems::Point(grid_x, grid_z));
  }

  static auto enemy_bearing_degrees() -> std::optional<float> {
    if (!qEnvironmentVariableIsSet("SOI_TOWN_ENEMY_BEARING")) {
      return std::nullopt;
    }
    bool ok = false;
    const float bearing = qEnvironmentVariable("SOI_TOWN_ENEMY_BEARING").toFloat(&ok);
    return ok ? std::optional<float>(bearing) : std::nullopt;
  }

  static void run_minutes(SessionContext& session, double minutes) {
    const bool timeline = qEnvironmentVariableIsSet("SOI_TOWN_MAP");
    for (int minute = 1; minute <= static_cast<int>(std::ceil(minutes)); ++minute) {
      run_for(session, std::min(60.0, minutes * 60.0 - (minute - 1) * 60.0));
      if (!timeline) {
        continue;
      }
      const auto census = census_of(standing_town(session));
      auto& economy = session.economy();
      std::printf("   t=%2dmin walls %3d gates %d towers %d homes %2d farms %d "
                  "barracks %d | wood %4d stone %3d\n",
                  minute,
                  census.walls - census.gates,
                  census.gates,
                  census.towers,
                  census.homes,
                  census.farms,
                  census.barracks,
                  economy.get(k_owner, Game::Systems::ResourceType::Wood),
                  economy.get(k_owner, Game::Systems::ResourceType::Stone));
      std::fflush(stdout);
    }
  }

  static auto castle_is_selected(const char* name) -> bool {
    if (!qEnvironmentVariableIsSet("SOI_TOWN_ONLY")) {
      return true;
    }
    return qEnvironmentVariable("SOI_TOWN_ONLY") == QString::fromLatin1(name);
  }

  struct Enclosure {
    bool has_ring = false;
    bool closed = false;
    int interior_cells = 0;
    float breach_x = 0.0F;
    float breach_z = 0.0F;
  };

  static auto gate_blocks(const Standing& gate, float x, float z) -> bool {
    constexpr float k_along = Engine::Core::GateComponent::k_structure_half_span;
    constexpr float k_across = Engine::Core::GateComponent::k_cross_half_extent;
    const float dx = x - gate.x;
    const float dz = z - gate.z;
    return Engine::Core::GateComponent::spans_x_axis(gate.rotation_y)
               ? std::abs(dx) <= k_along && std::abs(dz) <= k_across
               : std::abs(dz) <= k_along && std::abs(dx) <= k_across;
  }

  static auto wall_reach_of(const Game::Systems::AI::TownPlan* plan) -> float {
    float reach = 0.0F;
    if (plan == nullptr) {
      return reach;
    }
    for (const auto& step : plan->steps) {
      if (step.building == "wall_segment" || step.building == "wall_gate") {
        reach = std::max(reach, std::hypot(step.x, step.z));
      }
    }
    return reach;
  }

  static auto enclosure_of(const std::vector<Standing>& town,
                           float anchor_x,
                           float anchor_z,
                           const Game::Systems::AI::TownPlan* plan) -> Enclosure {
    Enclosure result;
    const float reach = wall_reach_of(plan);
    if (reach <= 0.0F) {
      return result;
    }
    result.has_ring = true;
    std::vector<const Standing*> gates;
    for (const auto& building : town) {
      if (building.type == "wall_gate") {
        gates.push_back(&building);
      }
    }
    const auto blocked = [&](const Game::Systems::Point& cell) {
      if (!Game::Systems::NavGrid::is_grid_walkable(cell)) {
        return true;
      }
      const QVector3D at = Game::Systems::NavGrid::grid_to_world(cell);
      return std::any_of(gates.begin(), gates.end(), [&](const Standing* gate) {
        return gate_blocks(*gate, at.x(), at.z());
      });
    };
    constexpr float k_escape_margin = 5.0F;
    const float escape = reach + k_escape_margin;
    const auto anchor = Game::Systems::NavGrid::world_to_grid(anchor_x, anchor_z);
    auto start = anchor;
    bool found = false;
    constexpr int k_anchor_search = 8;
    for (int radius = 0; radius <= k_anchor_search && !found; ++radius) {
      for (int dx = -radius; dx <= radius && !found; ++dx) {
        for (int dz = -radius; dz <= radius && !found; ++dz) {
          if (std::max(std::abs(dx), std::abs(dz)) != radius) {
            continue;
          }
          const Game::Systems::Point cell{anchor.x + dx, anchor.y + dz};
          if (cell.x >= 0 && cell.y >= 0 && cell.x < k_map_size &&
              cell.y < k_map_size && !blocked(cell)) {
            start = cell;
            found = true;
          }
        }
      }
    }
    if (!found) {
      return result;
    }
    std::set<std::pair<int, int>> seen{{start.x, start.y}};
    std::deque<Game::Systems::Point> frontier{start};
    result.closed = true;
    while (!frontier.empty()) {
      const auto cell = frontier.front();
      frontier.pop_front();
      const QVector3D at = Game::Systems::NavGrid::grid_to_world(cell);
      if (std::hypot(at.x() - anchor_x, at.z() - anchor_z) > escape) {
        result.closed = false;
        result.breach_x = at.x();
        result.breach_z = at.z();
        break;
      }
      ++result.interior_cells;
      for (const auto [dx, dz] :
           {std::pair{1, 0}, std::pair{-1, 0}, std::pair{0, 1}, std::pair{0, -1}}) {
        const Game::Systems::Point next{cell.x + dx, cell.y + dz};
        if (next.x < 0 || next.y < 0 || next.x >= k_map_size || next.y >= k_map_size) {
          continue;
        }
        if (!seen.insert({next.x, next.y}).second || blocked(next)) {
          continue;
        }
        frontier.push_back(next);
      }
    }
    return result;
  }

  static void
  draw_nav_map(const std::vector<Standing>& town, float anchor_x, float anchor_z) {
    if (!qEnvironmentVariableIsSet("SOI_TOWN_MAP")) {
      return;
    }
    constexpr int k_half = 34;
    const auto origin = Game::Systems::NavGrid::world_to_grid(anchor_x, anchor_z);
    std::printf("   nav map, 1 m per cell, %d m each way of the anchor; # blocked "
                ". open\n",
                k_half);
    for (int dz = -k_half; dz <= k_half; ++dz) {
      std::string row;
      for (int dx = -k_half; dx <= k_half; ++dx) {
        const Game::Systems::Point cell{origin.x + dx, origin.y + dz};
        char glyph = ' ';
        if (cell.x >= 0 && cell.y >= 0 && cell.x < k_map_size && cell.y < k_map_size) {
          glyph = Game::Systems::NavGrid::is_grid_walkable(cell) ? '.' : '#';
        }
        const QVector3D at = Game::Systems::NavGrid::grid_to_world(cell);
        for (const auto& building : town) {
          if (std::abs(building.x - at.x()) > 0.5F ||
              std::abs(building.z - at.z()) > 0.5F) {
            continue;
          }
          if (building.type == "wall_gate") {
            glyph = 'G';
          } else if (building.type == "defense_tower") {
            glyph = 'T';
          } else if (building.type == "barracks") {
            glyph = 'B';
          } else if (building.type == "home") {
            glyph = 'h';
          } else if (building.type == "farm") {
            glyph = 'f';
          } else if (building.type == "marketplace") {
            glyph = 'M';
          }
        }
        row.push_back(glyph);
      }
      std::printf("   |%s|\n", row.c_str());
    }
  }

  static void run_for(SessionContext& session, double seconds) {
    const double step = session.clock().tick_seconds();
    for (double elapsed = 0.0; elapsed < seconds; elapsed += step) {
      session.clock().advance(step);
      while (session.clock().consume_tick()) {
        session.world().update(static_cast<float>(step));
      }
    }
  }

  static auto standing_town(SessionContext& session) -> std::vector<Standing> {
    std::vector<Standing> town;
    for (auto [id, unit] : session.world().view<UnitComponent>()) {
      if (unit.owner_id != k_owner || unit.health <= 0 ||
          !Game::Units::is_building_spawn(unit.spawn_type)) {
        continue;
      }
      const auto* transform =
          session.world().try_get<Engine::Core::TransformComponent>(id);
      if (transform == nullptr) {
        continue;
      }
      town.push_back(Standing{.type = Game::Units::spawn_typeToString(unit.spawn_type),
                              .x = transform->position.x,
                              .z = transform->position.z,
                              .rotation_y = transform->rotation.y});
    }
    return town;
  }

  static auto census_of(const std::vector<Standing>& town) -> TownCensus {
    TownCensus census;
    for (const auto& building : town) {
      if (building.type == "barracks") {
        ++census.barracks;
      } else if (building.type == "home") {
        ++census.homes;
      } else if (building.type == "farm") {
        ++census.farms;
      } else if (building.type == "defense_tower") {
        ++census.towers;
      } else if (building.type == "wall_segment" || building.type == "wall_gate") {
        ++census.walls;
        if (building.type == "wall_gate") {
          ++census.gates;
        }
      } else if (building.type == "marketplace") {
        ++census.markets;
      } else if (building.type == "catapult" || building.type == "ballista") {
        ++census.engines;
      }
    }
    return census;
  }

  static auto army_of(SessionContext& session) -> ArmyCensus {
    ArmyCensus army;
    const auto* nation = session.nations().get_nation_for_player(k_owner);
    for (auto [id, unit] : session.world().view<UnitComponent>()) {
      if (unit.owner_id != k_owner || unit.health <= 0 ||
          Game::Units::is_building_spawn(unit.spawn_type) ||
          unit.spawn_type == Game::Units::SpawnType::Builder ||
          unit.spawn_type == Game::Units::SpawnType::Civilian) {
        continue;
      }
      const auto troop = Game::Units::spawn_typeToTroopType(unit.spawn_type);
      if (troop.has_value() && Game::Units::is_commander_troop(*troop)) {
        continue;
      }
      if (unit.spawn_type == Game::Units::SpawnType::Catapult ||
          unit.spawn_type == Game::Units::SpawnType::Ballista) {
        ++army.engines;
      } else if (Game::Units::is_cavalry(unit.spawn_type)) {
        ++army.horse;
      } else if (troop.has_value() && nation != nullptr &&
                 nation->is_ranged_unit(*troop)) {
        ++army.missile;
      } else {
        ++army.foot;
      }
    }
    return army;
  }

  static void draw_town(const char* name,
                        const std::vector<Standing>& town,
                        const TownCensus& census,
                        const ArmyCensus& army,
                        const std::vector<Standing>& soldiers = {}) {
    if (!qEnvironmentVariableIsSet("SOI_TOWN_MAP")) {
      return;
    }
    constexpr int k_span = 46;
    constexpr int k_rows = 23;
    std::vector<std::string> canvas(k_rows,
                                    std::string(static_cast<std::size_t>(k_span), ' '));
    const float centre = static_cast<float>(k_town_grid) - (k_map_size / 2.0F);
    for (const auto& building : town) {
      const int gx = static_cast<int>((building.x - centre) / 2.2F) + (k_span / 2);
      const int gz = static_cast<int>((building.z - centre) / 4.0F) + (k_rows / 2);
      if (gx < 0 || gx >= k_span || gz < 0 || gz >= k_rows) {
        continue;
      }
      char glyph = '?';
      if (building.type == "barracks") {
        glyph = 'B';
      } else if (building.type == "home") {
        glyph = 'h';
      } else if (building.type == "farm") {
        glyph = 'f';
      } else if (building.type == "defense_tower") {
        glyph = 'T';
      } else if (building.type == "wall_segment" || building.type == "wall_gate") {
        glyph = '#';
      } else if (building.type == "marketplace") {
        glyph = 'M';
      } else if (building.type == "catapult" || building.type == "ballista") {
        glyph = 'C';
      }
      canvas[static_cast<std::size_t>(gz)][static_cast<std::size_t>(gx)] = glyph;
    }
    std::printf("\n== %s  bar %d home %d farm %d tower %d wall %d market %d engine %d"
                " | foot %d bow %d horse %d engine %d\n",
                name,
                census.barracks,
                census.homes,
                census.farms,
                census.towers,
                census.walls,
                census.markets,
                census.engines,
                army.foot,
                army.missile,
                army.horse,
                army.engines);
    for (const auto& soldier : soldiers) {
      const int gx = static_cast<int>((soldier.x - centre) / 2.2F) + (k_span / 2);
      const int gz = static_cast<int>((soldier.z - centre) / 4.0F) + (k_rows / 2);
      if (gx < 0 || gx >= k_span || gz < 0 || gz >= k_rows) {
        continue;
      }
      auto& cell = canvas[static_cast<std::size_t>(gz)][static_cast<std::size_t>(gx)];
      cell = cell == ' ' ? 's' : cell;
    }
    for (const auto& row : canvas) {
      std::printf("|%s|\n", row.c_str());
    }
  }

  struct Settlement {
    TownCensus census;
    ArmyCensus army;
    Enclosure enclosure;
    int wall_runs = 0;

    float fortification_coverage = 0.0F;
    Game::Systems::AI::AIContext::StationReport stations;
  };

  static auto soldiers_of(SessionContext& session) -> std::vector<Standing> {
    std::vector<Standing> soldiers;
    for (auto [id, unit] : session.world().view<UnitComponent>()) {
      if (unit.owner_id != k_owner || unit.health <= 0 ||
          Game::Units::is_building_spawn(unit.spawn_type) ||
          unit.spawn_type == Game::Units::SpawnType::Builder ||
          unit.spawn_type == Game::Units::SpawnType::Civilian) {
        continue;
      }
      const auto* transform =
          session.world().try_get<Engine::Core::TransformComponent>(id);
      if (transform == nullptr) {
        continue;
      }
      soldiers.push_back(Standing{
          .type = "soldier", .x = transform->position.x, .z = transform->position.z});
    }
    return soldiers;
  }

  static auto wall_runs(const std::vector<Standing>& town) -> int {
    std::vector<Standing> line;
    for (const auto& building : town) {
      if (building.type == "wall_segment" || building.type == "wall_gate") {
        line.push_back(building);
      }
    }

    constexpr float k_in_the_line = 3.0F;
    for (const auto& prop : Game::Map::TerrainService::instance().world_props()) {
      if (!Game::Map::is_solid_world_prop_type(prop.type)) {
        continue;
      }
      const QVector3D at =
          Game::Map::TerrainService::instance().world_prop_world_position(prop);
      const bool in_the_line =
          std::any_of(line.begin(), line.end(), [&](const Standing& piece) {
            return piece.type != "prop" &&
                   std::hypot(piece.x - at.x(), piece.z - at.z()) <= k_in_the_line;
          });
      if (in_the_line) {
        line.push_back(Standing{
            .type = "prop",
            .x = at.x(),
            .z = at.z(),
            .span = Game::Map::world_prop_ground_radius(prop.type, prop.scale)});
      }
    }
    std::vector<const Standing*> pieces;
    pieces.reserve(line.size());
    for (const auto& piece : line) {
      pieces.push_back(&piece);
    }
    std::vector<int> parent(pieces.size());
    for (std::size_t i = 0; i < parent.size(); ++i) {
      parent[i] = static_cast<int>(i);
    }
    const auto find = [&](int i) {
      while (parent[static_cast<std::size_t>(i)] != i) {
        i = parent[static_cast<std::size_t>(i)];
      }
      return i;
    };
    const auto half_span = [](const Standing& piece) {
      if (piece.type == "wall_gate") {
        return Engine::Core::GateComponent::k_structure_half_span;
      }
      return piece.type == "prop" ? piece.span : 1.0F;
    };
    constexpr float k_touch_slack = 0.4F;
    for (std::size_t i = 0; i < pieces.size(); ++i) {
      for (std::size_t j = i + 1; j < pieces.size(); ++j) {
        const float reach =
            half_span(*pieces[i]) + half_span(*pieces[j]) + k_touch_slack;
        if (std::hypot(pieces[i]->x - pieces[j]->x, pieces[i]->z - pieces[j]->z) <=
            reach) {
          parent[static_cast<std::size_t>(find(static_cast<int>(i)))] =
              find(static_cast<int>(j));
        }
      }
    }
    std::map<int, std::vector<const Standing*>> members;
    for (std::size_t i = 0; i < pieces.size(); ++i) {
      members[find(static_cast<int>(i))].push_back(pieces[i]);
    }
    if (qEnvironmentVariableIsSet("SOI_TOWN_MAP")) {
      for (const auto& [root, run] : members) {
        std::printf("   run of %zu: first %s at %.1f,%.1f last %s at %.1f,%.1f\n",
                    run.size(),
                    run.front()->type.c_str(),
                    static_cast<double>(run.front()->x),
                    static_cast<double>(run.front()->z),
                    run.back()->type.c_str(),
                    static_cast<double>(run.back()->x),
                    static_cast<double>(run.back()->z));
      }
    }
    return static_cast<int>(members.size());
  }

  static auto fortification_coverage(const std::vector<Standing>& town) -> float {
    const Standing* anchor = nullptr;
    for (const auto& building : town) {
      if (building.type == "barracks") {
        anchor = &building;
        break;
      }
    }
    if (anchor == nullptr) {
      return 0.0F;
    }
    std::vector<Game::Systems::AI::TownPlanOffset> offsets;
    for (const auto& building : town) {
      if (building.type == "wall_segment" || building.type == "wall_gate" ||
          building.type == "defense_tower") {
        offsets.push_back({building.x - anchor->x, building.z - anchor->z});
      }
    }
    return Game::Systems::AI::TownPlan::compass_coverage(offsets);
  }

  static auto barracks_manpower(SessionContext& session) -> int {
    int manpower = 0;
    for (auto [id, unit] : session.world().view<UnitComponent>()) {
      if (unit.owner_id != k_owner ||
          unit.spawn_type != Game::Units::SpawnType::Barracks) {
        continue;
      }
      if (const auto* production =
              session.world().try_get<Engine::Core::ProductionComponent>(id)) {
        manpower += production->manpower_available;
      }
    }
    return manpower;
  }

  auto raise_town(const char* name,
                  Game::Units::SpawnType commander,
                  Game::Systems::NationID nation,
                  double minutes) -> Settlement {
    auto& session = settle(commander, nation);
    run_minutes(session, minutes);
    const auto town = standing_town(session);
    const auto census = census_of(town);
    const auto army = army_of(session);
    Settlement settlement{.census = census, .army = army};
    settlement.fortification_coverage = fortification_coverage(town);
    settlement.wall_runs = wall_runs(town);
    if (auto* ai = session.world().get_system<Game::Systems::AISystem>()) {
      if (const auto* plan = ai->plan_for(k_owner); plan != nullptr) {
        settlement.stations = plan->station_report;
        const auto* town_plan = plan->strategy_config.doctrine != nullptr
                                    ? plan->strategy_config.doctrine->town_plan
                                    : nullptr;
        settlement.enclosure =
            enclosure_of(town, plan->base_pos_x, plan->base_pos_z, town_plan);
        if (qEnvironmentVariableIsSet("SOI_TOWN_MAP")) {
          std::printf("   wall runs: %d\n", settlement.wall_runs);
          std::printf(
              "   enclosure: %s (%d interior cells%s) facing %.2f,%.2f%s\n",
              !settlement.enclosure.has_ring ? "no ring planned"
              : settlement.enclosure.closed  ? "CLOSED"
                                             : "OPEN",
              settlement.enclosure.interior_cells,
              settlement.enclosure.closed
                  ? ""
                  : (" breach near " +
                     std::to_string(static_cast<int>(settlement.enclosure.breach_x)) +
                     "," +
                     std::to_string(static_cast<int>(settlement.enclosure.breach_z)))
                        .c_str(),
              static_cast<double>(plan->settlement_facing_x),
              static_cast<double>(plan->settlement_facing_z),
              plan->settlement_facing_locked ? " (locked)" : "");
          draw_nav_map(town, plan->base_pos_x, plan->base_pos_z);
        }
      }
    }
    if (qEnvironmentVariableIsSet("SOI_TOWN_MAP")) {
      if (auto* ai = session.world().get_system<Game::Systems::AISystem>()) {
        if (const auto* plan = ai->plan_for(k_owner); plan != nullptr) {
          const auto& stations = plan->station_report;
          std::printf("   stations: %s form %s | soldiers %d stationed %d marching %d "
                      "fighting %d at_spawn %d adrift %d | rally %.0f,%.0f%s\n",
                      plan->strategy_config.doctrine != nullptr &&
                              plan->strategy_config.doctrine->town_plan != nullptr
                          ? plan->strategy_config.doctrine->town_plan->id.c_str()
                          : "-",
                      plan->strategy_config.doctrine != nullptr &&
                              plan->strategy_config.doctrine->town_plan != nullptr
                          ? Game::Systems::AI::settlement_form_name(
                                plan->strategy_config.doctrine->town_plan->form())
                          : "-",
                      stations.combat_units,
                      stations.stationed,
                      stations.marching,
                      stations.fighting,
                      stations.at_spawn,
                      stations.adrift,
                      static_cast<double>(plan->station.x),
                      static_cast<double>(plan->station.z),
                      plan->musters_outside ? " (before the gate)" : " (in the ward)");
          std::printf("   outline: %.0f%% of the compass fortified\n",
                      static_cast<double>(settlement.fortification_coverage * 100.0F));
          const auto* doctrine = plan->strategy_config.doctrine;
          std::printf(
              "   doctrine: %s cav %.2f ranged %.2f siege %.2f | counts melee "
              "%d ranged %d horse %d siege %d\n",
              doctrine != nullptr ? doctrine->id.c_str() : "(none)",
              static_cast<double>(
                  doctrine != nullptr ? doctrine->recruitment.cavalry_share : -1.0F),
              static_cast<double>(
                  doctrine != nullptr ? doctrine->recruitment.ranged_share : -1.0F),
              static_cast<double>(
                  doctrine != nullptr ? doctrine->recruitment.siege_share : -1.0F),
              plan->melee_count,
              plan->ranged_count,
              plan->cavalry_count,
              plan->siege_count);
        }
      }
      auto& economy = session.economy();
      std::printf("   purse: food %d wood %d stone %d iron %d gold %d | manpower %d\n",
                  economy.get(k_owner, Game::Systems::ResourceType::Food),
                  economy.get(k_owner, Game::Systems::ResourceType::Wood),
                  economy.get(k_owner, Game::Systems::ResourceType::Stone),
                  economy.get(k_owner, Game::Systems::ResourceType::Iron),
                  economy.get(k_owner, Game::Systems::ResourceType::Gold),
                  barracks_manpower(session));
    }
    draw_town(name, town, census, army, soldiers_of(session));
    std::fflush(stdout);
    return settlement;
  }

  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory;
  std::unique_ptr<SessionContext> m_session;
  std::unique_ptr<Game::Session::ScopedSession> m_scope;
};

void expect_a_working_town(const char* name, const TownCensus& census) {
  EXPECT_GE(census.barracks, 1) << name << " never raised a barracks";
  EXPECT_GE(census.homes, 2) << name << " never raised its homes";
  EXPECT_GE(census.farms, 1) << name << " never broke ground on a field";
  EXPECT_GE(census.total(), 8)
      << name << " raised only " << census.total() << " buildings from an empty field";
}

TEST_F(AiTownPlanTest, EveryCommanderRaisesItsOwnTownFromAnEmptyField) {
  struct Settler {
    const char* name;
    Game::Units::SpawnType commander;
    Game::Systems::NationID nation;
  };

  const Settler settlers[] = {
      {"fabius",
       Game::Units::SpawnType::RomanLegionOrganizer,
       Game::Systems::NationID::RomanRepublic},
      {"scipio",
       Game::Units::SpawnType::RomanVeteranConsul,
       Game::Systems::NationID::RomanRepublic},
      {"marcellus",
       Game::Units::SpawnType::RomanFieldCommander,
       Game::Systems::NationID::RomanRepublic},
      {"hanno",
       Game::Units::SpawnType::CarthageSpearCommander,
       Game::Systems::NationID::Carthage},
      {"hasdrubal",
       Game::Units::SpawnType::CarthageBowCommander,
       Game::Systems::NationID::Carthage},
      {"hannibal",
       Game::Units::SpawnType::CarthageSwordCommander,
       Game::Systems::NationID::Carthage},
  };

  std::map<std::string, Settlement> towns;
  for (const auto& settler : settlers) {
    const auto settlement =
        raise_town(settler.name, settler.commander, settler.nation, 26.0);
    expect_a_working_town(settler.name, settlement.census);
    towns.emplace(settler.name, settlement);
    TearDown();
    SetUp();
  }

  std::map<std::string, std::string> by_shape;
  for (const auto& [name, settlement] : towns) {
    const auto signature = settlement.census.signature();
    const auto twin = by_shape.find(signature);
    EXPECT_EQ(twin, by_shape.end())
        << name << " and " << (twin != by_shape.end() ? twin->second : std::string{})
        << " raised the same town (" << signature
        << "); every commander is supposed to build its own";
    by_shape.emplace(signature, name);
  }

  for (const char* castle : {"fabius", "scipio", "hanno", "hannibal"}) {
    const auto& census = towns.at(castle).census;
    const int fortifications = census.walls + census.towers;
    EXPECT_GE(fortifications, 8) << castle << " raised almost no fortification";

    EXPECT_LE(towns.at(castle).wall_runs, std::max(3, census.walls / 8))
        << castle << " stands as " << towns.at(castle).wall_runs << " separate runs of "
        << census.walls
        << " wall pieces; a half-built ring is one wall growing out from its "
           "gate, not posts with gaps between them";
  }

  for (const auto& [name, settlement] : towns) {
    const auto& stations = settlement.stations;
    if (stations.combat_units < 4) {
      continue;
    }
    EXPECT_LE(stations.at_spawn, 1) << name << " left " << stations.at_spawn
                                    << " soldiers standing by its barracks";
    EXPECT_GE((stations.stationed + stations.marching + stations.fighting) * 10,
              stations.combat_units * 7)
        << name << " has " << stations.adrift << " of " << stations.combat_units
        << " soldiers adrift between spawn and station";
  }

  EXPECT_EQ(towns.at("hasdrubal").census.walls, 0)
      << "the Barcid raider camp is authored without a wall; it must stay open";
  EXPECT_GE(towns.at("fabius").census.walls, 3)
      << "the Fabian bulwark is a walled castrum and must raise a frame";
  EXPECT_GE(towns.at("hannibal").census.towers, 3)
      << "the Hannibalic hexagon is authored with a tower on every corner";
  EXPECT_GE(towns.at("marcellus").census.barracks, 2)
      << "the vanguard chevron is authored around three barracks";
}

TEST_F(AiTownPlanTest, AWalledCommanderClosesItsCircuitGivenTime) {
  struct Castle {
    const char* name;
    Game::Units::SpawnType commander;
    Game::Systems::NationID nation;
    const char* plan;
  };
  const Castle castles[] = {
      {"hannibal",
       Game::Units::SpawnType::CarthageSwordCommander,
       Game::Systems::NationID::Carthage,
       "punic_grand_camp"},
      {"fabius",
       Game::Units::SpawnType::RomanLegionOrganizer,
       Game::Systems::NationID::RomanRepublic,
       "roman_bulwark"},
      {"scipio",
       Game::Units::SpawnType::RomanVeteranConsul,
       Game::Systems::NationID::RomanRepublic,
       "roman_assault_camp"},
  };
  for (const auto& castle : castles) {
    if (!castle_is_selected(castle.name)) {
      continue;
    }
    const auto* plan = Game::Systems::AI::authored_town_plan(castle.plan);
    ASSERT_NE(plan, nullptr) << castle.plan;
    const int planned = plan->step_count("wall_segment");
    const auto settlement =
        raise_town(castle.name, castle.commander, castle.nation, 50.0);
    EXPECT_GE(settlement.census.walls * 100, planned * 50)
        << castle.name << " raised " << settlement.census.walls << " of the " << planned
        << " wall links in its blueprint after fifty minutes; a castle "
           "the economy cannot afford is a wish, not a plan";
    EXPECT_GE(settlement.census.towers, 2)
        << castle.name << " has no towers to speak of";
    EXPECT_GE(settlement.census.gates, 1)
        << castle.name << " closed its ring without a gate to march out of";
    EXPECT_TRUE(settlement.enclosure.has_ring && settlement.enclosure.closed)
        << castle.name << " has not closed its ring after fifty minutes: ground "
        << "inside the walls still connects to the field without passing a gate, "
        << "near " << settlement.enclosure.breach_x << ","
        << settlement.enclosure.breach_z
        << "; a ring with a hole in it is a fence, not a castle";
    TearDown();
    SetUp();
  }
}

TEST_F(AiTownPlanTest, ACommanderThatWantsHorseFieldsHorse) {
  const auto raiders = raise_town("hasdrubal",
                                  Game::Units::SpawnType::CarthageBowCommander,
                                  Game::Systems::NationID::Carthage,
                                  22.0);

  EXPECT_GT(raiders.army.missile, raiders.army.foot)
      << "a bow doctrine must field more missile troops than foot";
  EXPECT_GT(raiders.army.horse, 0)
      << "a doctrine with a quarter of its army mounted must be able to pay for a "
         "horse: a barracks that cannot hold the price of its own cavalry never "
         "fields any";
}

} // namespace
