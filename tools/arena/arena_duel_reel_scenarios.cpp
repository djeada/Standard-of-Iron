#include "arena_duel_reel_scenarios.h"

#include <QString>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>
#include <utility>
#include <vector>

#include "arena_scenario_builders.h"
#include "arena_scenarios.h"
#include "game/map/map_definition.h"
#include "game/map/map_loader.h"
#include "utils/resource_utils.h"

namespace Arena::Scenarios {
namespace {

using namespace builders;

using Command = ScenarioCommandKind;
using Expect = ArenaExpectationKind;
using Nation = Game::Systems::NationID;
using Troop = Game::Units::TroopType;

constexpr float k_face_off_distance = 15.0F;
constexpr float k_duel_begins_at = 5.6F;

struct Duelist {
  const char* name;
  Troop troop;
  int health;
  const char* opening_move;
  float opening_delay;
};

struct Stage {
  const char* id;
  const char* label;
  const char* description;
  const char* map;
  QVector3D grid_centre;
  float axis_degrees;
  float hour;
  Duelist roman;
  Duelist punic;
  Troop roman_escort;
  Troop punic_escort;
  int escort_files;
  float escort_distance;
};

constexpr int k_punic_owner = 1;
constexpr int k_roman_owner = 2;

auto duelist_group(const Duelist& duelist,
                   Nation nation,
                   int owner,
                   QVector3D position,
                   float facing) -> ArenaScenarioGroup {
  auto g = nation_group(
      QString::fromLatin1(duelist.name), duelist.troop, nation, owner, 1, position, 1);
  g.facing_degrees = facing;
  g.health_override = g.max_health_override = duelist.health;
  g.showcase_routine = {QString::fromLatin1(duelist.opening_move)};
  g.showcase_start_delay = duelist.opening_delay;
  g.showcase_loop = false;
  return g;
}

auto escort_line(const QString& name,
                 Troop troop,
                 Nation nation,
                 int owner,
                 int files,
                 QVector3D line_centre,
                 QVector3D across,
                 float facing) -> ArenaScenarioGroup {
  constexpr float k_file_spacing = 3.4F;
  QVector3D const origin =
      line_centre - across * (k_file_spacing * 0.5F * static_cast<float>(files - 1));
  auto g = nation_group(
      name, troop, nation, owner, files, origin, 8, across * k_file_spacing);
  g.facing_degrees = facing;
  return g;
}

auto stage(const Stage& spec) -> std::optional<ArenaScenarioDefinition> {
  Game::Map::MapDefinition map;
  QString error;
  QString const map_path = QString::fromLatin1(spec.map);
  if (!Game::Map::MapLoader::load_from_json_file(
          Utils::Resources::resolve_resource_path(map_path), map, &error)) {
    qWarning() << "Arena: cannot read duel reel map" << map_path << ":" << error;
    return std::nullopt;
  }

  auto s = definition(QString::fromLatin1(spec.id),
                      QString::fromLatin1(spec.label),
                      QString::fromLatin1(spec.description),
                      60.0F,
                      {16.0F, 16.0F, spec.axis_degrees + 90.0F});
  s.campaign_map_path = map_path;
  s.terrain_grid_extent = map.grid.width;
  s.arena_floor_half_extent = static_cast<float>(map.grid.width) * 0.5F;
  s.environment = map.environment;
  s.environment.start_time = spec.hour;
  s.environment.time_mode = Game::Map::TimeMode::Locked;
  s.select_spawned_units = false;
  s.suppress_spawn_anchor = true;
  s.suppress_ui_overlays = true;
  s.suppress_procedural_props = true;
  s.force_full_creature_lod = true;
  s.graphics_quality = Render::GraphicsQuality::Ultra;

  float const tile = std::max(0.0001F, map.grid.tile_size);
  QVector3D const centre(
      (spec.grid_centre.x() - (static_cast<float>(map.grid.width) * 0.5F - 0.5F)) *
          tile,
      0.0F,
      (spec.grid_centre.z() - (static_cast<float>(map.grid.height) * 0.5F - 0.5F)) *
          tile);
  float const axis_radians = spec.axis_degrees * std::numbers::pi_v<float> / 180.0F;
  QVector3D const axis(std::sin(axis_radians), 0.0F, std::cos(axis_radians));
  QVector3D const across(axis.z(), 0.0F, -axis.x());
  s.camera_focus = centre + QVector3D(0.0F, 1.0F, 0.0F);

  QString const roman = QString::fromLatin1(spec.roman.name);
  QString const punic = QString::fromLatin1(spec.punic.name);
  float const half = k_face_off_distance * 0.5F;
  float const roman_facing = spec.axis_degrees;
  float const punic_facing = spec.axis_degrees + 180.0F;
  s.groups = {
      duelist_group(spec.roman,
                    Nation::RomanRepublic,
                    k_roman_owner,
                    centre - axis * half,
                    roman_facing),
      duelist_group(spec.punic,
                    Nation::Carthage,
                    k_punic_owner,
                    centre + axis * half,
                    punic_facing),
  };
  if (spec.escort_files > 0) {
    s.groups.push_back(escort_line(QStringLiteral("roman_escort"),
                                   spec.roman_escort,
                                   Nation::RomanRepublic,
                                   k_roman_owner,
                                   spec.escort_files,
                                   centre - axis * spec.escort_distance,
                                   across,
                                   roman_facing));
    s.groups.push_back(escort_line(QStringLiteral("punic_escort"),
                                   spec.punic_escort,
                                   Nation::Carthage,
                                   k_punic_owner,
                                   spec.escort_files,
                                   centre + axis * spec.escort_distance,
                                   across,
                                   punic_facing));
    s.steps.push_back(at(0.2F, Command::Hold, QStringLiteral("roman_escort")));
    s.steps.push_back(at(0.2F, Command::Hold, QStringLiteral("punic_escort")));
  }
  s.steps.push_back(at(k_duel_begins_at, Command::Attack, roman, punic));
  s.steps.push_back(at(k_duel_begins_at, Command::Attack, punic, roman));

  for (auto const& name : {roman, punic}) {
    s.expectations.push_back(expectation(Expect::GroupIsRendered, name));
    s.expectations.push_back(expectation(Expect::AttackAnimationObserved, name));
    s.expectations.push_back(expectation(Expect::HitReactionObserved, name));
  }
  s.expectations.push_back(expectation(Expect::AttackHasVisibleContact, roman, punic));
  s.expectations.push_back(expectation(Expect::AttackHasVisibleContact, punic, roman));
  return s;
}

auto old_enemies() -> std::optional<ArenaScenarioDefinition> {
  auto s = stage({
      .id = k_duel_reel_old_enemies_id,
      .label = "Duel Reel: Old Enemies",
      .description = "Scipio and Hannibal meet again on the statue avenue of the "
                     "Zama sanctuary at golden hour, each with a line of his own "
                     "men watching. Hannibal taunts, Scipio answers with his "
                     "blade, then the duel director takes over: charge, clash, "
                     "parried strings, evasions, a launcher juggle and knockback "
                     "finishers.",
      .map = ":/assets/maps/map_battle_zama.json",
      .grid_centre = {137.0F, 0.0F, 466.0F},
      .axis_degrees = 270.0F,
      .hour = 17.3F,
      .roman = {"scipio", Troop::RomanVeteranConsul, 5200, "sword_flourish:2.2", 0.4F},
      .punic =
          {"hannibal", Troop::CarthageSwordCommander, 1300, "taunt_cynical:3.0", 2.6F},
      .roman_escort = Troop::Swordsman,
      .punic_escort = Troop::Swordsman,
      .escort_files = 5,
      .escort_distance = 24.0F,
  });
  return s;
}

auto alpine_pass() -> std::optional<ArenaScenarioDefinition> {
  auto s = stage({
      .id = k_duel_reel_alpine_pass_id,
      .label = "Duel Reel: The Alpine Pass",
      .description = "Fabius bars the pass shrine with his spear and Hannibal "
                     "comes down through the snow with a sword. Spear reach "
                     "against sword speed among the ruins.",
      .map = ":/assets/maps/map_crossing_alps.json",
      .grid_centre = {321.0F, 0.0F, 446.0F},
      .axis_degrees = 90.0F,
      .hour = 8.6F,
      .roman =
          {"fabius", Troop::RomanLegionOrganizer, 1700, "taunt_dismissive:2.6", 0.4F},
      .punic =
          {"hannibal", Troop::CarthageSwordCommander, 5200, "sword_flourish:2.2", 3.0F},
      .roman_escort = Troop::Spearman,
      .punic_escort = Troop::Swordsman,
      .escort_files = 4,
      .escort_distance = 22.0F,
  });
  if (s.has_value()) {
    s->weather.snow = 0.5F;
    s->precipitation.enabled = true;
    s->precipitation.type = Game::Map::WeatherType::Snow;
    s->precipitation.intensity = 0.5F;
    s->precipitation.wind_strength = 0.35F;
    s->precipitation.wind_direction_deg = 70.0F;
  }
  return s;
}

auto night_raid() -> std::optional<ArenaScenarioDefinition> {
  auto s = stage({
      .id = k_duel_reel_night_raid_id,
      .label = "Duel Reel: Night Raid",
      .description = "Scipio comes up to the west gate of the Punic camp on the "
                     "Trebia after dark and Hanno walks out past the watch fires "
                     "to stop him. Sword against spear by firelight.",
      .map = ":/assets/maps/map_battle_trebia.json",
      .grid_centre = {267.0F, 0.0F, 515.0F},
      .axis_degrees = 0.0F,
      .hour = 20.4F,
      .roman = {"scipio", Troop::RomanVeteranConsul, 5200, "sword_flourish:2.2", 3.0F},
      .punic =
          {"hanno", Troop::CarthageSpearCommander, 5200, "taunt_dismissive:2.6", 0.4F},
      .roman_escort = Troop::Swordsman,
      .punic_escort = Troop::Spearman,
      .escort_files = 0,
      .escort_distance = 22.0F,
  });
  if (s.has_value()) {
    s->environment.exposure_override = 1.7F;
  }
  return s;
}

} // namespace

auto build_duel_reel_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;
  for (auto build : {&old_enemies, &alpine_pass, &night_raid}) {
    if (auto scenario = build()) {
      result.push_back(std::move(*scenario));
    }
  }
  return result;
}

} // namespace Arena::Scenarios
