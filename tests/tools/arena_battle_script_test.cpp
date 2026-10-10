#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <vector>

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/units/spawn_type.h"
#include "render/mist_volume_builder.h"
#include "tools/arena/arena_scenario.h"
#include "tools/arena/arena_weather.h"
#include "tools/arena/battle_script.h"
#include "tools/arena/battle_script_headless.h"

namespace {

using Arena::BattleScript::CompileResult;
using Arena::BattleScript::LoadOptions;

auto parse(const char* json) -> QJsonObject {
  return QJsonDocument::fromJson(QByteArray(json)).object();
}

auto fifteen_per_unit() -> LoadOptions {
  LoadOptions options;
  options.individuals_per_unit = [](Game::Units::TroopType) {
    return 15;
  };
  return options;
}

constexpr const char* k_minimal = R"({
  "schema": "soi.battle_script/1",
  "id": "test_battle",
  "title": "Test",
  "scale": 0.1,
  "duration": 30,
  "terrain": { "grid_extent": 128 },
  "armies": [
    { "id": "red", "nation": "roman_republic", "facing": 0,
      "groups": [
        { "id": "red_line", "troop": "swordsman", "historical": 3000,
          "deployment": { "shape": "line", "ranks": 2, "center": [0, -20] } },
        { "id": "red_reserve", "troop": "spearman", "units": 4,
          "deployment": { "anchor": { "behind": "red_line", "gap": 10 } } }
      ],
      "commanders": [
        { "id": "red_general", "catalog_id": "roman_veteran_consul", "todo": "ignored", "with": "red_line" }
      ] },
    { "id": "blue", "nation": "carthage", "facing": 180,
      "groups": [
        { "id": "blue_line", "troop": "swordsman", "nation": "gauls", "historical": 1500,
          "deployment": { "shape": "crescent", "ranks": 1, "bulge": 6, "center": [0, 20] } },
        { "id": "blue_ambush", "troop": "horse_swordsman", "units": 2, "ambush": true,
          "deployment": { "center": [40, 0] } }
      ] }
  ],
  "phases": [
    { "id": "advance", "trigger": { "type": "time", "at": 1 },
      "actions": [ { "type": "attack", "group": "red_line", "target": "blue_line" } ] },
    { "id": "spring", "event": "trap_sprung", "trigger": { "type": "phase", "phase": "advance", "delay": 2 },
      "actions": [ { "type": "reveal", "group": "blue_ambush", "target": "red_line" },
                   { "type": "weather", "duration": 5, "fog_density": 0.2 } ] }
  ]
})";

auto group_named(const Arena::ArenaScenarioDefinition& scenario,
                 const QString& name) -> const Arena::ArenaScenarioGroup* {
  for (const auto& group : scenario.groups) {
    if (group.name == name) {
      return &group;
    }
  }
  return nullptr;
}

auto has_error_at(const CompileResult& result, const QString& path) -> bool {
  return std::any_of(result.errors.begin(),
                     result.errors.end(),
                     [&](const auto& error) { return error.path == path; });
}

auto error_text(const CompileResult& result) -> std::string {
  return Arena::BattleScript::format_diagnostics(result).toStdString();
}

auto make_entity_host(Engine::Core::World& world) -> Arena::ArenaScenarioHost {
  Arena::ArenaScenarioHost host;
  host.spawn_unit = [&world](const Arena::ArenaScenarioGroup& group,
                             const QVector3D& position) {
    auto* entity = world.create_entity();
    entity->add_component<Engine::Core::TransformComponent>(
        position.x(), position.y(), position.z());
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    unit->owner_id = group.owner_id;
    unit->spawn_type = Game::Units::spawn_typeFromTroopType(group.troop_type);
    unit->health = 100;
    unit->max_health = 100;
    entity->add_component<Engine::Core::MovementComponent>();
    entity->add_component<Engine::Core::AttackComponent>();
    return entity->get_id();
  };
  host.find_unit = [](Engine::Core::EntityID) -> Game::Units::Unit* {
    return nullptr;
  };
  host.set_camera = [](const auto&, const auto&) {
  };
  host.set_force_full_creature_lod = [](bool) {
  };
  return host;
}

auto marker(const char* name,
            Arena::ScenarioTrigger trigger) -> Arena::ArenaScenarioStep {
  Arena::ArenaScenarioStep step;
  step.name = QString::fromLatin1(name);
  step.event = QStringLiteral("phase:%1").arg(QString::fromLatin1(name));
  step.trigger = std::move(trigger);
  step.command = Arena::ScenarioCommandKind::Marker;
  return step;
}

auto event_time(const Arena::ArenaScenarioRunner& runner,
                const char* name) -> std::optional<float> {
  for (const auto& event : runner.events()) {
    if (event.name == QString::fromLatin1(name)) {
      return event.time_seconds;
    }
  }
  return std::nullopt;
}

} // namespace

TEST(BattleScriptTest, CompilesArmiesCommandersAndPhases) {
  const auto result =
      Arena::BattleScript::compile(parse(k_minimal), fifteen_per_unit());
  ASSERT_TRUE(result.ok()) << error_text(result);
  const auto& scenario = *result.scenario;
  EXPECT_EQ(scenario.id, QStringLiteral("test_battle"));

  const auto* line = group_named(scenario, QStringLiteral("red_line"));
  ASSERT_NE(line, nullptr);
  EXPECT_EQ(line->count, 20) << "3000 historical x 0.1 = 300 soldiers = 20 units of 15";
  EXPECT_EQ(line->positions.size(), 20U);
  EXPECT_TRUE(line->keep_troop_speed);
  EXPECT_EQ(line->owner_id, 2);

  const auto* reserve = group_named(scenario, QStringLiteral("red_reserve"));
  ASSERT_NE(reserve, nullptr);
  EXPECT_EQ(reserve->count, 4);
  EXPECT_LT(reserve->origin.z(), line->origin.z() - 10.0F) << "the reserve sits behind";

  const auto* general = group_named(scenario, QStringLiteral("red_general"));
  ASSERT_NE(general, nullptr);
  EXPECT_EQ(general->troop_type, Game::Units::TroopType::RomanVeteranConsul);
  EXPECT_EQ(general->count, 1);

  const auto* gauls = group_named(scenario, QStringLiteral("blue_line"));
  ASSERT_NE(gauls, nullptr);
  EXPECT_EQ(gauls->nation_id, Game::Systems::NationID::Gauls);
  EXPECT_EQ(gauls->owner_id, 3);

  const auto* ambush = group_named(scenario, QStringLiteral("blue_ambush"));
  ASSERT_NE(ambush, nullptr);
  EXPECT_FALSE(ambush->spawn_at_start);

  ASSERT_EQ(result.phases.size(), 2U);
  EXPECT_EQ(result.phases[1].event, QStringLiteral("phase:trap_sprung"));
  EXPECT_EQ(scenario.owner_teams.size(), 2U);
  EXPECT_EQ(scenario.battle_sides.size(), 2U);
  EXPECT_EQ(scenario.weather_script.changes.size(), 1U);
  EXPECT_TRUE(Arena::validate_scenario(scenario).empty());
}

TEST(BattleScriptTest, CrescentBulgesTowardTheEnemy) {
  const auto result =
      Arena::BattleScript::compile(parse(k_minimal), fifteen_per_unit());
  ASSERT_TRUE(result.ok()) << error_text(result);
  const auto* gauls = group_named(*result.scenario, QStringLiteral("blue_line"));
  ASSERT_NE(gauls, nullptr);
  auto centre = gauls->positions.front();
  auto wing = gauls->positions.front();
  for (const auto& position : gauls->positions) {
    if (std::abs(position.x()) < std::abs(centre.x())) {
      centre = position;
    }
    if (std::abs(position.x()) > std::abs(wing.x())) {
      wing = position;
    }
  }
  EXPECT_LT(centre.z(), wing.z() - 4.0F)
      << "facing 180 the enemy is toward -z, so the centre leads the wings";
}

TEST(BattleScriptTest, ScaleOverrideScalesCountsAndGeometry) {
  auto options = fifteen_per_unit();
  options.scale_override = 0.05F;
  const auto result = Arena::BattleScript::compile(parse(k_minimal), options);
  ASSERT_TRUE(result.ok()) << error_text(result);
  EXPECT_FLOAT_EQ(result.scale, 0.05F);
  EXPECT_NEAR(result.geometry_factor, std::sqrt(0.5F), 1.0e-5F);
  const auto* line = group_named(*result.scenario, QStringLiteral("red_line"));
  ASSERT_NE(line, nullptr);
  EXPECT_EQ(line->count, 10);
  EXPECT_NEAR(line->origin.z(), -20.0F * std::sqrt(0.5F), 0.5F);
  const auto* reserve = group_named(*result.scenario, QStringLiteral("red_reserve"));
  EXPECT_EQ(reserve->count, 4) << "explicit game units are not scaled";
}

TEST(BattleScriptTest, MultiLineLanesLeaveAlignedCorridors) {
  auto root = parse(k_minimal);
  auto armies = root.value(QStringLiteral("armies")).toArray();
  auto red = armies.at(0).toObject();
  auto groups = red.value(QStringLiteral("groups")).toArray();
  auto line = groups.at(0).toObject();
  line.insert(QStringLiteral("deployment"),
              parse(R"({"shape": "multi_line", "lines": 3, "ranks": 1, "lanes": 2,
                        "lane_width": 20, "line_gap": 10, "center": [0, -20]})"));
  line.insert(QStringLiteral("units"), 18);
  line.remove(QStringLiteral("historical"));
  groups.replace(0, line);
  red.insert(QStringLiteral("groups"), groups);
  armies.replace(0, red);
  root.insert(QStringLiteral("armies"), armies);

  const auto result = Arena::BattleScript::compile(root, fifteen_per_unit());
  ASSERT_TRUE(result.ok()) << error_text(result);
  const auto* group = group_named(*result.scenario, QStringLiteral("red_line"));
  ASSERT_NE(group, nullptr);
  std::vector<float> xs;
  std::vector<float> zs;
  for (const auto& position : group->positions) {
    xs.push_back(position.x());
    if (std::find_if(zs.begin(), zs.end(), [&](float z) {
          return std::abs(z - position.z()) < 0.5F;
        }) == zs.end()) {
      zs.push_back(position.z());
    }
  }
  EXPECT_EQ(zs.size(), 3U) << "three lines";
  std::sort(xs.begin(), xs.end());
  float widest_gap = 0.0F;
  for (std::size_t i = 1; i < xs.size(); ++i) {
    widest_gap = std::max(widest_gap, xs[i] - xs[i - 1]);
  }
  EXPECT_GT(widest_gap, 20.0F) << "the lanes stay open through every line";
}

TEST(BattleScriptTest, ValidationReportsJsonPaths) {
  auto root = parse(k_minimal);
  auto armies = root.value(QStringLiteral("armies")).toArray();
  auto red = armies.at(0).toObject();
  auto groups = red.value(QStringLiteral("groups")).toArray();
  auto line = groups.at(0).toObject();
  line.insert(QStringLiteral("troop"), QStringLiteral("hoplite"));
  line.insert(QStringLiteral("formation"), QStringLiteral("testudo_of_doom"));
  line.insert(QStringLiteral("colour"), QStringLiteral("red"));
  groups.replace(0, line);
  red.insert(QStringLiteral("groups"), groups);
  auto commanders = red.value(QStringLiteral("commanders")).toArray();
  auto general = commanders.at(0).toObject();
  general.insert(QStringLiteral("catalog_id"), QStringLiteral("not_a_commander"));
  commanders.replace(0, general);
  red.insert(QStringLiteral("commanders"), commanders);
  armies.replace(0, red);
  root.insert(QStringLiteral("armies"), armies);

  auto phases = root.value(QStringLiteral("phases")).toArray();
  auto spring = phases.at(1).toObject();
  spring.insert(QStringLiteral("trigger"),
                parse(R"({"type": "phase", "phase": "nowhere"})"));
  auto actions = spring.value(QStringLiteral("actions")).toArray();
  actions.append(parse(R"({"type": "reveal", "group": "red_line"})"));
  actions.append(parse(R"({"type": "wheel", "group": "ghosts", "degrees": 90})"));
  spring.insert(QStringLiteral("actions"), actions);
  phases.replace(1, spring);
  root.insert(QStringLiteral("phases"), phases);

  const auto result = Arena::BattleScript::compile(root, fifteen_per_unit());
  EXPECT_FALSE(result.ok());
  const std::string text = error_text(result);
  EXPECT_TRUE(has_error_at(result, QStringLiteral("$.armies[0].groups[0].troop")))
      << text;
  EXPECT_TRUE(has_error_at(result, QStringLiteral("$.armies[0].groups[0].formation")))
      << text;
  EXPECT_TRUE(has_error_at(result, QStringLiteral("$.armies[0].groups[0].colour")))
      << text;
  EXPECT_TRUE(
      has_error_at(result, QStringLiteral("$.armies[0].commanders[0].catalog_id")))
      << text;
  EXPECT_TRUE(has_error_at(result, QStringLiteral("$.phases[1].trigger.phase")))
      << text;
  EXPECT_TRUE(has_error_at(result, QStringLiteral("$.phases[1].actions[2]"))) << text;
  EXPECT_TRUE(has_error_at(result, QStringLiteral("$.phases[1].actions[3].group")))
      << text;
  EXPECT_NE(text.find("roman_veteran_consul"), std::string::npos)
      << "an unknown commander lists the catalog";
  EXPECT_NE(text.find("faction_default"), std::string::npos)
      << "an unknown formation lists the formations the game has";
}

TEST(BattleScriptTest, KnownFormationNamesComeFromTheFormationSystem) {
  const auto names = Arena::BattleScript::known_formation_names();
  EXPECT_TRUE(names.contains(QStringLiteral("line")));
  for (const auto& name : names) {
    EXPECT_TRUE(Arena::BattleScript::resolve_formation(name).has_value())
        << name.toStdString();
  }
  EXPECT_NE(Arena::BattleScript::resolve_commander(
                QStringLiteral("carthage_sword_commander")),
            nullptr);
}

TEST(BattleScriptTest, ShippedBattleScriptsCompile) {
  for (const char* file : {"tools/arena/battles/cannae.json",
                           "tools/arena/battles/trasimene.json",
                           "tools/arena/battles/trebia.json",
                           "tools/arena/battles/zama.json"}) {
    ASSERT_TRUE(QFileInfo::exists(QString::fromLatin1(file)))
        << file << " (run arena_tests from the repository root)";
    const auto result = Arena::BattleScript::load_file(QString::fromLatin1(file));
    EXPECT_TRUE(result.ok()) << file << "\n" << error_text(result);
  }
  const auto cannae =
      Arena::BattleScript::load_file(QStringLiteral("tools/arena/battles/cannae.json"));
  ASSERT_TRUE(cannae.ok());
  QStringList events;
  for (const auto& phase : cannae.phases) {
    events.push_back(phase.event);
  }
  for (const char* expected : {"phase:roman_advance",
                               "phase:centre_yields",
                               "phase:libyans_wheel",
                               "phase:rear_attack",
                               "phase:encirclement"}) {
    EXPECT_TRUE(events.contains(QString::fromLatin1(expected))) << expected;
  }
}

TEST(BattleScriptTest, TrebiaFordIsAnExtensionPointWarning) {
  const auto result =
      Arena::BattleScript::load_file(QStringLiteral("tools/arena/battles/trebia.json"));
  ASSERT_TRUE(result.ok()) << error_text(result);
  EXPECT_TRUE(std::any_of(result.warnings.begin(), result.warnings.end(), [](auto& w) {
    return w.path == QStringLiteral("$.terrain.fords");
  }));
}

TEST(BattlePhaseTriggerTest, TimePhaseAndDelayTriggersFireInOrder) {
  Engine::Core::World world;
  Arena::ArenaScenarioDefinition scenario;
  scenario.id = QStringLiteral("phase_triggers");
  scenario.duration_seconds = 6.0F;
  scenario.groups = {{QStringLiteral("a"),
                      Game::Units::TroopType::Swordsman,
                      Game::Systems::NationID::RomanRepublic,
                      2,
                      2,
                      0,
                      QVector3D(-5.0F, 0.0F, 0.0F)},
                     {QStringLiteral("b"),
                      Game::Units::TroopType::Swordsman,
                      Game::Systems::NationID::Carthage,
                      3,
                      1,
                      0,
                      QVector3D(5.0F, 0.0F, 0.0F)}};
  Arena::ScenarioTrigger at_one{Arena::ScenarioTriggerKind::AtTime, 1.0F};
  Arena::ScenarioTrigger after_first;
  after_first.kind = Arena::ScenarioTriggerKind::StepExecuted;
  after_first.step = QStringLiteral("first");
  after_first.time_seconds = 1.5F;
  Arena::ScenarioTrigger gated{Arena::ScenarioTriggerKind::AtTime, 0.0F};
  gated.after_step = QStringLiteral("second");
  Arena::ScenarioTrigger never_but_fallback;
  never_but_fallback.kind = Arena::ScenarioTriggerKind::GroupDestroyed;
  never_but_fallback.group = QStringLiteral("b");
  never_but_fallback.fallback_seconds = 4.0F;
  scenario.steps = {marker("first", at_one),
                    marker("second", after_first),
                    marker("gated", gated),
                    marker("fallback", never_but_fallback)};
  scenario.expectations = {
      {Arena::ArenaExpectationKind::GroupExists, QStringLiteral("a")}};
  ASSERT_TRUE(Arena::validate_scenario(scenario).empty());

  Arena::ArenaScenarioRunner runner(world, make_entity_host(world), scenario);
  ASSERT_TRUE(runner.start());
  while (!runner.finished()) {
    runner.update(0.1F);
  }
  ASSERT_TRUE(event_time(runner, "phase:first").has_value());
  EXPECT_NEAR(*event_time(runner, "phase:first"), 1.0F, 0.11F);
  EXPECT_NEAR(*event_time(runner, "phase:second"), 2.5F, 0.21F);
  EXPECT_NEAR(
      *event_time(runner, "phase:gated"), *event_time(runner, "phase:second"), 0.11F)
      << "a gated trigger fires as soon as its gate opens";
  EXPECT_NEAR(*event_time(runner, "phase:fallback"), 4.0F, 0.11F);
}

TEST(BattlePhaseTriggerTest, StrengthAreaAndContactTriggers) {
  Engine::Core::World world;
  Arena::ArenaScenarioDefinition scenario;
  scenario.id = QStringLiteral("strength_triggers");
  scenario.duration_seconds = 5.0F;
  scenario.groups = {{QStringLiteral("a"),
                      Game::Units::TroopType::Swordsman,
                      Game::Systems::NationID::RomanRepublic,
                      2,
                      2,
                      0,
                      QVector3D(-20.0F, 0.0F, 0.0F),
                      QVector3D(2.0F, 0.0F, 0.0F)},
                     {QStringLiteral("b"),
                      Game::Units::TroopType::Swordsman,
                      Game::Systems::NationID::Carthage,
                      3,
                      1,
                      0,
                      QVector3D(20.0F, 0.0F, 0.0F)}};
  Arena::ScenarioTrigger weakened;
  weakened.kind = Arena::ScenarioTriggerKind::GroupStrengthBelow;
  weakened.group = QStringLiteral("a");
  weakened.threshold = 0.6F;
  Arena::ScenarioTrigger arrived;
  arrived.kind = Arena::ScenarioTriggerKind::GroupEnteredArea;
  arrived.group = QStringLiteral("b");
  arrived.position = QVector3D(0.0F, 0.0F, 0.0F);
  arrived.distance = 3.0F;
  Arena::ScenarioTrigger touching;
  touching.kind = Arena::ScenarioTriggerKind::FirstContact;
  touching.group = QStringLiteral("a");
  touching.target_group = QStringLiteral("b");
  touching.distance = 3.0F;
  scenario.steps = {marker("weakened", weakened),
                    marker("arrived", arrived),
                    marker("touching", touching)};
  scenario.expectations = {
      {Arena::ArenaExpectationKind::GroupExists, QStringLiteral("a")}};
  ASSERT_TRUE(Arena::validate_scenario(scenario).empty());

  Arena::ArenaScenarioRunner runner(world, make_entity_host(world), scenario);
  ASSERT_TRUE(runner.start());
  runner.update(0.1F);
  EXPECT_TRUE(runner.events().empty());

  const auto& a = runner.group_entities(QStringLiteral("a"));
  ASSERT_EQ(a.size(), 2U);
  world.try_get<Engine::Core::UnitComponent>(a[0])->health = 10;
  runner.update(0.1F);
  EXPECT_TRUE(event_time(runner, "phase:weakened").has_value())
      << "110 of 200 health is below 60%";

  const auto b = runner.group_entities(QStringLiteral("b")).front();
  auto* transform = world.try_get<Engine::Core::TransformComponent>(b);
  transform->position.x = 1.0F;
  runner.update(0.1F);
  EXPECT_TRUE(event_time(runner, "phase:arrived").has_value());
  EXPECT_FALSE(event_time(runner, "phase:touching").has_value());

  transform->position.x = -18.0F;
  runner.update(0.1F);
  EXPECT_TRUE(event_time(runner, "phase:touching").has_value());
}

TEST(BattleWeatherTest, FogBankKeysInterpolateAndChangesBlend) {
  Arena::ArenaWeatherScript script;
  Arena::ArenaFogBank bank;
  bank.id = QStringLiteral("lake_fog");
  bank.start = QVector3D(-10.0F, 0.0F, 0.0F);
  bank.end = QVector3D(10.0F, 0.0F, 0.0F);
  bank.density = 0.9F;
  bank.ceiling = 8.0F;
  bank.keys = {{10.0F, 0.5F}, {20.0F, 0.1F, std::nullopt, 2.0F}};
  script.fog_banks = {bank};
  Arena::ArenaWeatherChange dawn;
  dawn.duration_seconds = 10.0F;
  dawn.hour = 7.0F;
  dawn.rain = 0.6F;
  Arena::ArenaFogBankChange lift;
  lift.id = QStringLiteral("lake_fog");
  lift.density = 0.0F;
  dawn.fog_banks = {lift};
  script.changes = {dawn};
  std::vector<Arena::ArenaTimedWeatherChange> schedule = {{30.0F, 0}};

  Arena::ArenaWeatherBase base;
  base.hour = 5.0F;
  base.fog_density = 0.04F;

  const auto start = Arena::evaluate_weather(script, schedule, 0.0F, base);
  ASSERT_EQ(start.fog_banks.size(), 1U);
  EXPECT_FLOAT_EQ(start.fog_banks[0].density, 0.9F);
  EXPECT_FALSE(start.hour_scripted);

  const auto mid = Arena::evaluate_weather(script, schedule, 5.0F, base);
  EXPECT_NEAR(mid.fog_banks[0].density, 0.7F, 1.0e-4F);
  const auto later = Arena::evaluate_weather(script, schedule, 15.0F, base);
  EXPECT_NEAR(later.fog_banks[0].density, 0.3F, 1.0e-4F);
  EXPECT_NEAR(later.fog_banks[0].ceiling, 5.0F, 1.0e-4F);

  const auto blending = Arena::evaluate_weather(script, schedule, 35.0F, base);
  EXPECT_TRUE(blending.hour_scripted);
  EXPECT_NEAR(blending.hour, 6.0F, 1.0e-4F) << "smoothstep is half way at the midpoint";
  EXPECT_NEAR(blending.rain, 0.3F, 1.0e-4F);
  EXPECT_NEAR(blending.fog_banks[0].density, 0.05F, 1.0e-4F);
  const auto done = Arena::evaluate_weather(script, schedule, 60.0F, base);
  EXPECT_NEAR(done.hour, 7.0F, 1.0e-4F);
  EXPECT_NEAR(done.fog_banks[0].density, 0.0F, 1.0e-4F);
  EXPECT_FALSE(done.fog_density_scripted);

  const auto volumes = Arena::fog_bank_mist_volumes(
      start.fog_banks, QVector3D(), Render::MistSurfaceHeight{});
  ASSERT_EQ(volumes.size(), 1U);
  EXPECT_FLOAT_EQ(volumes[0].ceiling, 8.0F);
  EXPECT_TRUE(Arena::fog_bank_mist_volumes(done.fog_banks, QVector3D(), {}).empty())
      << "a lifted bank leaves no mist behind";
}

TEST(BattleWeatherTest, SharedMistBuilderMatchesTheSkirmishRules) {
  std::vector<Game::Map::FogZone> zones(1);
  zones[0].x = 4.0F;
  zones[0].width = 10.0F;
  zones[0].height = 6.0F;
  zones[0].density = 0.5F;
  std::vector<Game::Map::Lake> lakes(1);
  lakes[0].center = QVector3D(30.0F, 0.0F, 0.0F);
  lakes[0].width = 40.0F;
  lakes[0].depth = 20.0F;
  std::vector<Game::Map::RiverSegment> rivers(2);
  rivers[0].start = QVector3D(0.0F, 0.0F, 0.0F);
  rivers[0].end = QVector3D(10.0F, 0.0F, 0.0F);
  rivers[1].start = QVector3D(10.0F, 0.0F, 0.0F);
  rivers[1].end = QVector3D(20.0F, 0.0F, 0.0F);
  rivers[0].width = rivers[1].width = 4.0F;

  const auto volumes = Render::build_mist_volumes(
      {.fog_zones = &zones, .rivers = &rivers, .lakes = &lakes},
      [](float, float) { return 1.5F; });
  ASSERT_EQ(volumes.size(), 3U) << "one miasma, one simplified river run, one lake";
  EXPECT_EQ(volumes[0].kind, Render::MistVolume::Kind::Miasma);
  EXPECT_NEAR(volumes[0].strength, 0.30F + 0.5F * 0.55F, 1.0e-5F);
  EXPECT_FLOAT_EQ(volumes[0].radius, 5.0F);
  EXPECT_EQ(volumes[1].kind, Render::MistVolume::Kind::WaterMist);
  EXPECT_FLOAT_EQ(volumes[1].end.x(), 20.0F) << "collinear river segments merge";
  EXPECT_FLOAT_EQ(volumes[2].radius, 20.0F);
  EXPECT_FLOAT_EQ(volumes[2].strength, Render::k_water_mist_strength);
  EXPECT_FLOAT_EQ(volumes[2].start.y(), 1.5F);

  std::vector<Render::MistVolume> many(30);
  const auto merged = Render::merge_mist_volumes(volumes, many);
  EXPECT_EQ(merged.size(), static_cast<std::size_t>(Render::k_max_mist_volumes));
  EXPECT_EQ(merged[0].kind, Render::MistVolume::Kind::Miasma)
      << "priority volumes first";
}

TEST(BattleScriptDeterminismTest, CannaePlaysThroughItsPhasesIdenticallyTwice) {
  LoadOptions options;
  options.scale_override = 0.01F;
  auto compiled = Arena::BattleScript::load_file(
      QStringLiteral("tools/arena/battles/cannae.json"), options);
  ASSERT_TRUE(compiled.ok()) << error_text(compiled);

  Arena::Headless::Options headless;
  headless.duration_override = 45.0F;
  headless.digest_interval_seconds = 0.5F;
  const auto first = Arena::Headless::run(*compiled.scenario, headless);
  ASSERT_TRUE(first.started) << first.error.toStdString();
  const auto second = Arena::Headless::run(*compiled.scenario, headless);
  ASSERT_TRUE(second.started) << second.error.toStdString();

  EXPECT_GT(first.spawned_units, 50);
  EXPECT_GE(first.digests.size(), 80U);
  const auto verdict = Arena::Headless::compare(first, second);
  EXPECT_TRUE(verdict.deterministic) << verdict.detail.toStdString();

  QStringList fired;
  for (const auto& event : first.events) {
    fired.push_back(event.name);
  }
  EXPECT_TRUE(fired.contains(QStringLiteral("phase:skirmish")))
      << fired.join(',').toStdString();
  EXPECT_TRUE(fired.contains(QStringLiteral("phase:cavalry_clash")))
      << fired.join(',').toStdString();
  EXPECT_TRUE(fired.contains(QStringLiteral("phase:screens_withdraw")))
      << fired.join(',').toStdString();
}
