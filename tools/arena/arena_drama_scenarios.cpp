#include "arena_drama_scenarios.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>

#include <utility>

#include "arena_scenario_builders.h"
#include "game/systems/nation_id.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"

namespace Arena::Scenarios {
namespace {

using namespace builders;

using Command = ScenarioCommandKind;
using Nation = Game::Systems::NationID;
using Trigger = ScenarioTriggerKind;
using Troop = Game::Units::TroopType;

constexpr int k_roman_owner = 2;
constexpr int k_punic_owner = 1;
constexpr int k_dead_owner = 3;

auto stage_directory() -> QString {
  auto const env = QProcessEnvironment::systemEnvironment().value(
      QStringLiteral("SOI_DRAMA_STAGES"));
  return env.isEmpty() ? QStringLiteral("tools/arena/promos/dead_company/stages") : env;
}

auto point(const QJsonValue& value) -> QVector3D {
  auto const array = value.toArray();
  return {static_cast<float>(array.at(0).toDouble()),
          0.0F,
          static_cast<float>(array.at(1).toDouble())};
}

auto owner_for(Nation nation) -> int {
  if (nation == Nation::Carthage) {
    return k_punic_owner;
  }
  if (nation == Nation::IronSepulcher) {
    return k_dead_owner;
  }
  return k_roman_owner;
}

auto renderer_for(const QString& look) -> QString {
  if (look.contains(QLatin1Char('/'))) {
    return look;
  }
  if (look == QLatin1String("bostar")) {
    return QStringLiteral("troops/carthage/dead_company/bostar");
  }
  if (look == QLatin1String("skeleton_legionary")) {
    return QStringLiteral("troops/iron_sepulcher/skeleton_legionary");
  }
  return QStringLiteral("troops/roman/dead_company/") + look;
}

auto parse_nation(const QJsonObject& obj) -> Nation {
  Nation nation = Nation::RomanRepublic;
  Game::Systems::try_parse_nation_id(
      obj.value("nation").toString(QStringLiteral("roman")), nation);
  return nation;
}

auto parse_actor(const QJsonObject& obj) -> ArenaScenarioGroup {
  ArenaScenarioGroup group;
  group.name = obj.value("name").toString();
  Troop troop = Troop::Swordsman;
  Game::Units::try_parse_troop_type(
      obj.value("troop").toString(QStringLiteral("swordsman")), troop);
  group.nation_id = parse_nation(obj);
  group.troop_type = troop;
  group.owner_id = owner_for(group.nation_id);
  group.count = obj.value("count").toInt(1);
  group.individuals_per_unit = obj.value("individuals").toInt(1);
  group.origin = point(obj.value("at")) - QVector3D(0.5F, 0.0F, 0.5F);
  group.spacing = obj.contains("spacing") ? point(obj.value("spacing"))
                                          : QVector3D(0.0F, 0.0F, 0.0F);
  group.facing_degrees = static_cast<float>(obj.value("facing").toDouble());
  group.attacks_disabled = true;
  if (obj.contains("look")) {
    group.renderer_override = renderer_for(obj.value("look").toString());
  }
  for (QJsonValue const move : obj.value("routine").toArray()) {
    group.showcase_routine.push_back(move.toString());
  }
  group.showcase_start_delay =
      static_cast<float>(obj.value("routine_delay").toDouble());
  group.showcase_loop = obj.value("routine_loop").toBool(true);
  group.showcase_loop_from = obj.value("routine_loop_from").toInt(0);
  return group;
}

auto parse_building(const QJsonObject& obj) -> ArenaScenarioGroup {
  Game::Units::SpawnType type = Game::Units::SpawnType::Home;
  Game::Units::try_parse_spawn_type(obj.value("type").toString(), type);
  Nation const nation = parse_nation(obj);
  return building(obj.value("name").toString(),
                  type,
                  nation,
                  owner_for(nation),
                  obj.value("count").toInt(1),
                  point(obj.value("at")),
                  obj.contains("spacing") ? point(obj.value("spacing"))
                                          : QVector3D(4.5F, 0.0F, 0.0F),
                  static_cast<float>(obj.value("facing").toDouble()));
}

auto parse_step(const QJsonObject& obj) -> ArenaScenarioStep {
  ArenaScenarioStep step;
  float const time = static_cast<float>(obj.value("t").toDouble());
  QString const action = obj.value("do").toString();
  step.group = obj.value("who").toString();
  step.name =
      QStringLiteral("%1_%2_%3").arg(step.group, action, QString::number(time, 'f', 2));
  step.trigger = {Trigger::AtTime, time, {}, {}, 0.0F};
  if (action == QLatin1String("walk")) {
    step.command = Command::Move;
    step.destination = point(obj.value("to"));
  } else if (action == QLatin1String("run")) {
    step.command = Command::Run;
    step.destination = point(obj.value("to"));
  } else if (action == QLatin1String("turn")) {
    step.command = Command::Wheel;
    step.angle_degrees = static_cast<float>(obj.value("degrees").toDouble());
    step.pivot_side = QStringLiteral("center");
  } else if (action == QLatin1String("face")) {
    step.command = Command::Wheel;
    step.pivot_side = QStringLiteral("face");
    step.target_group = obj.value("toward").toString();
    step.destination = point(obj.value("to"));
  } else if (action == QLatin1String("kill")) {
    step.command = Command::ApplyDamage;
    step.value = 100000;
    if (obj.contains("stay")) {
      step.corpse_hold_seconds = static_cast<float>(obj.value("stay").toDouble());
    }
  } else if (action == QLatin1String("stop")) {
    step.command = Command::Stop;
  } else {
    step.command = Command::Hold;
  }
  return step;
}

void add_rampart(ArenaScenarioDefinition& s, const QJsonObject& obj) {
  float const extent = static_cast<float>(obj.value("extent").toDouble(20.0));
  constexpr float k_gate_clearance = 4.0F;
  auto const run =
      [&](const QString& name, float first, float last, bool along_x, float fixed) {
        int const count = static_cast<int>((last - first) / 2.0F) + 1;
        float const center = (first + last) * 0.5F;
        s.groups.push_back(building(
            name,
            Game::Units::SpawnType::WallSegment,
            Nation::RomanRepublic,
            k_roman_owner,
            count,
            along_x ? QVector3D(center, 0.0F, fixed) : QVector3D(fixed, 0.0F, center),
            along_x ? QVector3D(2.0F, 0.0F, 0.0F) : QVector3D(0.0F, 0.0F, 2.0F),
            along_x ? 0.0F : 90.0F));
      };
  for (float const sign : {-1.0F, 1.0F}) {
    QString const half = sign < 0.0F ? QStringLiteral("a") : QStringLiteral("b");
    float const first = sign < 0.0F ? -extent + 2.0F : k_gate_clearance;
    float const last = sign < 0.0F ? -k_gate_clearance : extent - 2.0F;
    run(QStringLiteral("rampart_n") + half, first, last, true, -extent);
    run(QStringLiteral("rampart_s") + half, first, last, true, extent);
    run(QStringLiteral("rampart_w") + half, first, last, false, -extent);
    run(QStringLiteral("rampart_e") + half, first, last, false, extent);
  }
  struct Gate {
    const char* name{nullptr};
    QVector3D at;
    float facing{0.0F};
  };
  for (auto const& gate : {Gate{"gate_n", {0.0F, 0.0F, -extent}, 0.0F},
                           Gate{"gate_s", {0.0F, 0.0F, extent}, 0.0F},
                           Gate{"gate_w", {-extent, 0.0F, 0.0F}, 90.0F},
                           Gate{"gate_e", {extent, 0.0F, 0.0F}, 90.0F}}) {
    s.groups.push_back(building(QString::fromLatin1(gate.name),
                                Game::Units::SpawnType::WallGate,
                                Nation::RomanRepublic,
                                k_roman_owner,
                                1,
                                gate.at,
                                {},
                                gate.facing));
  }
  for (float const x : {-extent, extent}) {
    for (float const z : {-extent, extent}) {
      s.groups.push_back(building(QStringLiteral("tower_%1%2")
                                      .arg(QLatin1Char(x < 0.0F ? 'w' : 'e'))
                                      .arg(QLatin1Char(z < 0.0F ? 'n' : 's')),
                                  Game::Units::SpawnType::DefenseTower,
                                  Nation::RomanRepublic,
                                  k_roman_owner,
                                  1,
                                  {x, 0.0F, z}));
    }
  }
}

auto road_style(const QString& style) -> const char* {
  if (style == QLatin1String("stone")) {
    return "stone";
  }
  if (style == QLatin1String("rough")) {
    return "rough";
  }
  return "default";
}

void apply_layer(ArenaScenarioDefinition& s, const QJsonObject& obj) {
  for (auto const& value : obj.value("props").toArray()) {
    auto const prop = value.toObject();
    QByteArray const type = prop.value("type").toString().toLatin1();
    auto patch_value = patch(type.constData(),
                             prop.value("count").toInt(1),
                             point(prop.value("at")),
                             prop.contains("spacing") ? point(prop.value("spacing"))
                                                      : QVector3D(2.5F, 0.0F, 0.0F),
                             static_cast<float>(prop.value("scale").toDouble(1.0)));
    patch_value.exact = true;
    patch_value.yaw_spread = static_cast<float>(prop.value("yaw_spread").toDouble());
    s.resource_patches.push_back(std::move(patch_value));
  }
  for (auto const& value : obj.value("buildings").toArray()) {
    s.groups.push_back(parse_building(value.toObject()));
  }
  for (auto const& value : obj.value("roads").toArray()) {
    auto const road = value.toObject();
    s.roads.push_back(street(point(road.value("from")),
                             point(road.value("to")),
                             static_cast<float>(road.value("width").toDouble(3.0)),
                             road_style(road.value("style").toString())));
  }
  for (auto const& value : obj.value("rivers").toArray()) {
    auto const river = value.toObject();
    s.rivers.push_back(Game::Map::RiverSegment{
        point(river.value("from")),
        point(river.value("to")),
        static_cast<float>(river.value("width").toDouble(2.0))});
  }
  for (auto const& value : obj.value("bridges").toArray()) {
    auto const bridge = value.toObject();
    s.bridges.push_back(
        Game::Map::Bridge{point(bridge.value("from")),
                          point(bridge.value("to")),
                          static_cast<float>(bridge.value("width").toDouble(2.4)),
                          static_cast<float>(bridge.value("height").toDouble(0.35))});
  }
  if (obj.contains("rampart")) {
    add_rampart(s, obj.value("rampart").toObject());
  }
  for (auto const& value : obj.value("actors").toArray()) {
    s.groups.push_back(parse_actor(value.toObject()));
  }
  auto const gestures = obj.value("gestures").toObject();
  for (auto& group : s.groups) {
    auto const timed = gestures.value(group.name).toArray();
    if (timed.isEmpty() || !group.showcase_routine.isEmpty()) {
      continue;
    }
    float clock = 0.0F;
    for (auto const& entry : timed) {
      auto const cue = entry.toArray();
      float const at = static_cast<float>(cue.at(0).toDouble());
      float const length = static_cast<float>(cue.at(2).toDouble());
      if (at > clock + 0.01F) {
        group.showcase_routine.push_back(
            QStringLiteral("wait:%1").arg(QString::number(at - clock, 'f', 3)));
        clock = at;
      }
      group.showcase_routine.push_back(QStringLiteral("%1:%2").arg(
          cue.at(1).toString(), QString::number(length, 'f', 3)));
      clock += length;
    }
    group.showcase_loop = false;
  }
  for (auto const& value : obj.value("steps").toArray()) {
    s.steps.push_back(parse_step(value.toObject()));
  }
}

auto read_json(const QString& path) -> QJsonObject {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }
  return QJsonDocument::fromJson(file.readAll()).object();
}

auto parse_stage(const QDir& dir,
                 const QString& file,
                 const QJsonObject& obj) -> ArenaScenarioDefinition {
  auto s = definition(obj.value("id").toString(),
                      obj.value("label").toString(),
                      QStringLiteral("A staged drama scene from %1.").arg(file),
                      static_cast<float>(obj.value("duration").toDouble(60.0)),
                      {18.0F, 30.0F, 30.0F});
  s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
  s.select_spawned_units = false;
  s.suppress_spawn_anchor = true;
  s.suppress_ui_overlays = true;
  s.suppress_combat_dust = true;
  s.force_full_creature_lod = true;
  s.collect_animation_diagnostics = false;
  s.graphics_quality = Render::GraphicsQuality::Ultra;
  s.arena_floor_half_extent = static_cast<float>(obj.value("extent").toDouble(48.0));
  s.ground_type = obj.value("ground").toString(QStringLiteral("grass_dry"));
  s.terrain_seed_override = obj.value("seed").toInt(216);
  s.suppress_procedural_props = obj.value("bare").toBool(false);
  s.environment.start_time = static_cast<float>(obj.value("hour").toDouble(12.0));
  s.environment.time_mode = Game::Map::TimeMode::Locked;
  s.environment.lighting_profile =
      obj.value("lighting_profile").toString(QStringLiteral("mediterranean_summer"));
  s.environment.fog_density_override =
      static_cast<float>(obj.value("fog").toDouble(-1.0));
  s.environment.exposure_override =
      static_cast<float>(obj.value("exposure").toDouble(-1.0));
  s.owner_teams = {{.owner_id = k_roman_owner, .team_id = 1},
                   {.owner_id = k_punic_owner, .team_id = 1},
                   {.owner_id = k_dead_owner, .team_id = 1}};
  for (auto const& include : obj.value("include").toArray()) {
    apply_layer(s, read_json(dir.filePath(include.toString())));
  }
  auto layer = obj;
  auto const cues = read_json(dir.filePath(QString(file).replace(
      QStringLiteral(".stage.json"), QStringLiteral(".gestures.json"))));
  if (!cues.isEmpty()) {
    layer.insert(QStringLiteral("gestures"), cues);
  }
  apply_layer(s, layer);
  for (auto const& group : s.groups) {
    if (group.spawn_type.has_value()) {
      continue;
    }
    ArenaExpectation present;
    present.kind = ArenaExpectationKind::GroupExists;
    present.group = group.name;
    s.expectations.push_back(std::move(present));
  }
  return s;
}

} // namespace

auto build_drama_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;
  QDir const dir(stage_directory());
  for (auto const& name :
       dir.entryList({QStringLiteral("*.stage.json")}, QDir::Files, QDir::Name)) {
    auto const obj = read_json(dir.filePath(name));
    if (obj.value("id").toString().isEmpty()) {
      continue;
    }
    result.push_back(parse_stage(dir, name, obj));
  }
  return result;
}

} // namespace Arena::Scenarios
