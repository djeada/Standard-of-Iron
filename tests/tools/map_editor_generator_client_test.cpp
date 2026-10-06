#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include "tools/map_editor/generator_client.h"

namespace {

namespace Gen = MapEditor::Generator;

const QByteArray k_describe = R"({
  "version": 1,
  "stages": [
    {"id": "terrain", "label": "Terrain", "forced_by": []},
    {"id": "water", "label": "Water", "forced_by": []},
    {"id": "settlements", "label": "Settlements", "forced_by": []},
    {"id": "roads", "label": "Roads", "forced_by": ["settlements"]},
    {"id": "forests", "label": "Forests", "forced_by": ["settlements", "roads"]},
    {"id": "landmarks", "label": "Landmarks", "forced_by": ["settlements", "roads"]},
    {"id": "dressing", "label": "Dressing",
     "forced_by": ["terrain", "water", "settlements", "roads", "landmarks"]},
    {"id": "spawns", "label": "Spawns", "forced_by": ["settlements"]}
  ],
  "parameters": [
    {"key": "general.biome", "stage": "general", "label": "Biome", "type": "choice",
     "choices": [{"id": "temperate", "label": "Temperate"}, "arid"],
     "default": "temperate"},
    {"key": "terrain.relief", "stage": "terrain", "label": "Relief", "type": "float",
     "min": 0.0, "max": 1.0, "step": 0.05, "default": 0.5, "help": "How hilly."},
    {"key": "water.rivers", "stage": "water", "label": "Rivers", "type": "int",
     "min": 0, "max": 3, "default": 1},
    {"key": "tactical.symmetric", "stage": "tactical", "label": "Mirror",
     "type": "bool", "default": false},
    {"key": "broken.type", "stage": "terrain", "label": "Broken", "type": "matrix"}
  ],
  "presets": [
    {"id": "open_plains", "label": "Open Plains", "description": "Flat.",
     "width": 650, "height": 650, "parameters": {"water.rivers": 0}},
    {"id": "river_crossing", "label": "River Crossing", "width": 800, "height": 700,
     "parameters": {"water.rivers": 2, "terrain.relief": 0.25}}
  ]
})";

const QByteArray k_report = R"({
  "version": 1,
  "ok": false,
  "request": {"seed": 7, "preset": "river_crossing", "parameters": {}},
  "stages": [
    {"id": "terrain", "status": "locked", "seed": 1932, "seconds": 0.0},
    {"id": "roads", "status": "done", "seed": 88123, "seconds": 2.25,
     "forced_by": "settlements", "notes": ["road 01: 778 -> 785"]}
  ],
  "checks": [
    {"id": "roads.connected", "status": "pass", "message": "connected road graph"},
    {"id": "tactical.flank_diversity", "status": "warn", "message": "low flank diversity",
     "objects": [{"kind": "road", "index": 3, "x": 120.0, "z": 44.5}]},
    {"id": "settlements.reachable", "status": "fail", "message": "east town unreachable",
     "objects": [{"kind": "settlement"}]}
  ],
  "metrics": {"bridges": 2, "formation_area_ratio": 0.41}
})";

TEST(MapEditorGeneratorClientTest, ParsesTheDescribeSchema) {
  QString error;
  const auto schema = Gen::parse_schema(k_describe, &error);
  ASSERT_TRUE(schema.has_value()) << error.toStdString();

  ASSERT_EQ(schema->stages.size(), 8);
  EXPECT_EQ(schema->stage(QStringLiteral("roads"))->forced_by,
            QStringList{QStringLiteral("settlements")});
  ASSERT_EQ(schema->parameters.size(), 4) << "an unknown parameter type is skipped";
  const auto* relief = schema->parameter(QStringLiteral("terrain.relief"));
  ASSERT_NE(relief, nullptr);
  EXPECT_EQ(relief->type, Gen::ParameterType::Float);
  EXPECT_DOUBLE_EQ(relief->step, 0.05);
  EXPECT_EQ(relief->help, QStringLiteral("How hilly."));
  const auto* biome = schema->parameter(QStringLiteral("general.biome"));
  ASSERT_EQ(biome->choices.size(), 2);
  EXPECT_EQ(biome->choices[1].id, QStringLiteral("arid"));
  ASSERT_EQ(schema->presets.size(), 2);
  EXPECT_EQ(schema->preset(QStringLiteral("river_crossing"))->width, 800);
  EXPECT_EQ(schema->parameter_groups(),
            (QStringList{QStringLiteral("general"),
                         QStringLiteral("terrain"),
                         QStringLiteral("water"),
                         QStringLiteral("tactical")}));
}

TEST(MapEditorGeneratorClientTest, RejectsAnotherSchemaVersionAndJunk) {
  QString error;
  EXPECT_FALSE(Gen::parse_schema(R"({"version": 2})", &error).has_value());
  EXPECT_TRUE(error.contains(QStringLiteral("version")));
  EXPECT_FALSE(Gen::parse_schema("not json", &error).has_value());
}

TEST(MapEditorGeneratorClientTest,
     PresetValuesOverrideDefaultsAndRequestOverridesBoth) {
  const auto schema = Gen::parse_schema(k_describe);
  ASSERT_TRUE(schema.has_value());
  Gen::Request request;
  request.preset = QStringLiteral("river_crossing");
  request.parameters[QStringLiteral("water.rivers")] = 3;

  const QJsonObject resolved = Gen::resolved_parameters(*schema, request);
  EXPECT_EQ(resolved.value(QStringLiteral("general.biome")).toString(),
            QStringLiteral("temperate"));
  EXPECT_DOUBLE_EQ(resolved.value(QStringLiteral("terrain.relief")).toDouble(), 0.25);
  EXPECT_EQ(resolved.value(QStringLiteral("water.rivers")).toInt(), 3);
}

TEST(MapEditorGeneratorClientTest, BuildsTheContractCommandLine) {
  Gen::Request request;
  request.seed = 73948291;
  request.preset = QStringLiteral("river_crossing");
  request.width = 650;
  request.height = 600;
  request.parameters[QStringLiteral("water.rivers")] = 2;
  request.parameters[QStringLiteral("terrain.relief")] = 0.35;
  request.parameters[QStringLiteral("tactical.symmetric")] = true;
  request.locks = {QStringLiteral("terrain"), QStringLiteral("water")};

  Gen::Invocation invocation;
  invocation.script = QStringLiteral("/repo/scripts/soi-mapgen.py");
  invocation.output_path = QStringLiteral("/tmp/out.json");
  invocation.report_path = QStringLiteral("/tmp/report.json");

  const QStringList fresh = Gen::build_arguments(request, invocation);
  EXPECT_EQ(fresh,
            (QStringList{QStringLiteral("/repo/scripts/soi-mapgen.py"),
                         QStringLiteral("--preset"),
                         QStringLiteral("river_crossing"),
                         QStringLiteral("--seed"),
                         QStringLiteral("73948291"),
                         QStringLiteral("--width"),
                         QStringLiteral("650"),
                         QStringLiteral("--height"),
                         QStringLiteral("600"),
                         QStringLiteral("--set"),
                         QStringLiteral("tactical.symmetric=true"),
                         QStringLiteral("--set"),
                         QStringLiteral("terrain.relief=0.35"),
                         QStringLiteral("--set"),
                         QStringLiteral("water.rivers=2"),
                         QStringLiteral("--output"),
                         QStringLiteral("/tmp/out.json"),
                         QStringLiteral("--report"),
                         QStringLiteral("/tmp/report.json"),
                         QStringLiteral("--progress")}))
      << "--lock without --base is an error in the contract, so it is left out";

  invocation.base_path = QStringLiteral("/tmp/base.json");
  const QStringList reroll = Gen::build_arguments(request, invocation);
  const int base = static_cast<int>(reroll.indexOf(QStringLiteral("--base")));
  ASSERT_GE(base, 0);
  EXPECT_EQ(reroll[base + 1], QStringLiteral("/tmp/base.json"));
  EXPECT_EQ(reroll[base + 2], QStringLiteral("--lock"));
  EXPECT_EQ(reroll[base + 3], QStringLiteral("terrain,water"));
}

TEST(MapEditorGeneratorClientTest, UnlockedUpstreamForcesItsDependentsTransitively) {
  const auto schema = Gen::parse_schema(k_describe);
  ASSERT_TRUE(schema.has_value());

  const QStringList everything_but_settlements = {QStringLiteral("terrain"),
                                                  QStringLiteral("water"),
                                                  QStringLiteral("roads"),
                                                  QStringLiteral("forests"),
                                                  QStringLiteral("landmarks"),
                                                  QStringLiteral("dressing"),
                                                  QStringLiteral("spawns")};
  const auto forced = Gen::forced_stages(*schema, everything_but_settlements);
  EXPECT_EQ(forced.value(QStringLiteral("roads")),
            QStringList{QStringLiteral("settlements")});
  EXPECT_TRUE(forced.contains(QStringLiteral("forests")));
  EXPECT_TRUE(forced.contains(QStringLiteral("dressing")));
  EXPECT_TRUE(forced.contains(QStringLiteral("spawns")));
  EXPECT_FALSE(forced.contains(QStringLiteral("terrain")));
  EXPECT_EQ(Gen::effective_locks(*schema, everything_but_settlements),
            (QStringList{QStringLiteral("terrain"), QStringLiteral("water")}));

  const QStringList dressing_only = {QStringLiteral("dressing")};
  EXPECT_TRUE(Gen::effective_locks(*schema, dressing_only).isEmpty())
      << "dressing is built from terrain, which is unlocked";
}

TEST(MapEditorGeneratorClientTest, ProgressParserToleratesSplitLinesAndJunk) {
  Gen::ProgressParser parser;
  parser.feed(R"({"event": "stage", "stage": "water", "sta)");
  EXPECT_TRUE(parser.take_events().isEmpty());
  parser.feed("tus\": \"running\"}\nwarning: not json\n\n");
  parser.feed(
      R"({"event": "stage", "stage": "water", "status": "done", "seconds": 1.4})"
      "\n"
      R"({"event": "mystery"})"
      "\n"
      R"({"event": "done", "ok": true, "output": "/o.json", "report": "/r.json"})");
  parser.finish();

  const auto events = parser.take_events();
  ASSERT_EQ(events.size(), 3);
  EXPECT_EQ(events[0].status, QStringLiteral("running"));
  EXPECT_DOUBLE_EQ(events[1].seconds, 1.4);
  EXPECT_EQ(events[2].kind, Gen::ProgressEvent::Kind::Done);
  EXPECT_TRUE(events[2].ok);
  EXPECT_EQ(events[2].output, QStringLiteral("/o.json"));
  EXPECT_EQ(parser.ignored_lines(), 2);
}

TEST(MapEditorGeneratorClientTest, ProgressCarriesAttemptsAndErrors) {
  Gen::ProgressParser parser;
  parser.feed(
      R"({"event": "stage", "stage": "terrain", "status": "running", "attempt": 2})"
      "\n"
      R"({"event": "stage", "stage": "placement", "status": "done"})"
      "\n"
      R"({"event": "error", "message": "unknown preset 'nope'"})"
      "\n");
  parser.finish();

  const auto events = parser.take_events();
  ASSERT_EQ(events.size(), 3);
  EXPECT_EQ(events[0].attempt, 2);
  EXPECT_EQ(events[1].attempt, 1);
  EXPECT_EQ(events[1].stage, QStringLiteral("placement"));
  EXPECT_EQ(events[2].kind, Gen::ProgressEvent::Kind::Error);
  EXPECT_EQ(events[2].message, QStringLiteral("unknown preset 'nope'"));
}

TEST(MapEditorGeneratorClientTest, ParsesTheReport) {
  QString error;
  const auto report = Gen::parse_report(k_report, &error);
  ASSERT_TRUE(report.has_value()) << error.toStdString();
  EXPECT_FALSE(report->ok);
  ASSERT_EQ(report->stages.size(), 2);
  EXPECT_EQ(report->stages[1].forced_by, QStringLiteral("settlements"));
  EXPECT_EQ(report->stages[1].notes.size(), 1);
  EXPECT_EQ(report->count(Gen::CheckStatus::Pass), 1);
  EXPECT_EQ(report->count(Gen::CheckStatus::Warn), 1);
  EXPECT_EQ(report->count(Gen::CheckStatus::Fail), 1);
  const auto& warn = report->checks[1];
  ASSERT_EQ(warn.objects.size(), 1);
  EXPECT_TRUE(warn.objects[0].has_position);
  EXPECT_DOUBLE_EQ(warn.objects[0].z, 44.5);
  EXPECT_FALSE(report->checks[2].objects[0].has_position);
  EXPECT_EQ(report->metrics.value(QStringLiteral("bridges")).toInt(), 2);
}

TEST(MapEditorGeneratorClientTest, RequestRoundTripsAndReadsAMapGenerationObject) {
  Gen::Request request;
  request.seed = 9007199254740;
  request.preset = QStringLiteral("siege_town");
  request.width = 800;
  request.parameters[QStringLiteral("water.rivers")] = 1;
  request.locks = {QStringLiteral("terrain")};

  const Gen::Request back = Gen::request_from_json(Gen::request_to_json(request));
  EXPECT_EQ(back.seed, request.seed);
  EXPECT_EQ(back.preset, request.preset);
  EXPECT_EQ(back.width, 800);
  EXPECT_EQ(back.parameters, request.parameters);
  EXPECT_EQ(back.locks, request.locks);
}

TEST(MapEditorGeneratorClientTest, FindsTheRepositoryAndScriptFromANestedDirectory) {
  QTemporaryDir temp;
  ASSERT_TRUE(temp.isValid());
  QDir root(temp.path());
  ASSERT_TRUE(root.mkpath(QStringLiteral("assets")));
  ASSERT_TRUE(root.mkpath(QStringLiteral("scripts")));
  ASSERT_TRUE(root.mkpath(QStringLiteral("build/bin")));
  QFile(root.filePath(QStringLiteral("CMakeLists.txt"))).open(QIODevice::WriteOnly);

  EXPECT_EQ(Gen::find_repository_root({root.filePath(QStringLiteral("build/bin"))}),
            root.absolutePath());
  EXPECT_TRUE(Gen::script_path(root.absolutePath()).isEmpty());
  QFile(root.filePath(QStringLiteral("scripts/soi-mapgen.py")))
      .open(QIODevice::WriteOnly);
  EXPECT_EQ(Gen::script_path(root.absolutePath()),
            root.filePath(QStringLiteral("scripts/soi-mapgen.py")));
}

} // namespace
