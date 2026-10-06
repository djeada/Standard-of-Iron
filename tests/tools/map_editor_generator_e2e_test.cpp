#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include "tools/map_editor/generator_client.h"
#include "tools/map_editor/map_data.h"

namespace {

namespace Gen = MapEditor::Generator;

auto read_all(const QString& path) -> QByteArray {
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

TEST(MapEditorGeneratorEndToEndTest, TheEditorClientDrivesTheRealGenerator) {
  if (qEnvironmentVariable("SOI_MAPGEN_E2E") != QLatin1String("1")) {
    GTEST_SKIP() << "set SOI_MAPGEN_E2E=1 to run scripts/soi-mapgen.py for real";
  }
  const QString root = Gen::find_repository_root({QDir::currentPath()});
  const QString script = Gen::script_path(root);
  const auto python = Gen::find_python();
  ASSERT_FALSE(script.isEmpty()) << "run from the repository root";
  ASSERT_TRUE(python.has_value());

  QProcess describe;
  describe.setWorkingDirectory(root);
  describe.start(python->program,
                 python->prefix_arguments +
                     QStringList{script, QStringLiteral("--describe")});
  ASSERT_TRUE(describe.waitForFinished(120000));
  QString error;
  const auto schema = Gen::parse_schema(describe.readAllStandardOutput(), &error);
  ASSERT_TRUE(schema.has_value()) << error.toStdString();
  ASSERT_FALSE(schema->presets.isEmpty());

  QTemporaryDir temp;
  ASSERT_TRUE(temp.isValid());
  Gen::Request request;
  request.preset =
      qEnvironmentVariable("SOI_MAPGEN_E2E_PRESET", schema->presets.front().id);
  request.seed = 7;
  const Gen::PresetSpec* preset = schema->preset(request.preset);
  ASSERT_NE(preset, nullptr);
  request.width = preset->width;
  request.height = preset->height;
  request.parameters = Gen::resolved_parameters(*schema, request);

  Gen::Invocation invocation;
  invocation.script = script;
  invocation.output_path = temp.filePath(QStringLiteral("candidate.json"));
  invocation.report_path = temp.filePath(QStringLiteral("report.json"));

  QProcess run;
  run.setWorkingDirectory(root);
  run.start(python->program,
            python->prefix_arguments + Gen::build_arguments(request, invocation));
  ASSERT_TRUE(run.waitForFinished(900000)) << "generation took longer than 15 minutes";
  const QByteArray stderr_bytes = run.readAllStandardError();
  ASSERT_LT(run.exitCode(), 2) << stderr_bytes.right(4000).toStdString();

  Gen::ProgressParser progress;
  progress.feed(run.readAllStandardOutput());
  progress.finish();
  const auto events = progress.take_events();
  EXPECT_EQ(progress.ignored_lines(), 0) << "--progress must keep stdout to JSON lines";
  ASSERT_FALSE(events.isEmpty());
  EXPECT_EQ(events.back().kind, Gen::ProgressEvent::Kind::Done);
  int stages_done = 0;
  for (const auto& event : events) {
    stages_done += event.kind == Gen::ProgressEvent::Kind::Stage &&
                           event.status == QLatin1String("done")
                       ? 1
                       : 0;
  }
  EXPECT_GE(stages_done, schema->stages.size());

  const auto report = Gen::parse_report(read_all(invocation.report_path), &error);
  ASSERT_TRUE(report.has_value()) << error.toStdString();
  EXPECT_EQ(report->ok, run.exitCode() == 0);
  EXPECT_FALSE(report->checks.isEmpty());

  MapEditor::MapData document;
  ASSERT_TRUE(document.replace_document(
      read_all(invocation.output_path), QStringLiteral("Generate"), &error))
      << error.toStdString();
  EXPECT_EQ(document.generation().value(QStringLiteral("seed")).toInt(), 7);
  EXPECT_EQ(document.grid().width, request.width);
  EXPECT_FALSE(document.terrain_elements().isEmpty());
  EXPECT_FALSE(document.structures().isEmpty());

  Gen::Request reroll = request;
  reroll.seed = 8;
  reroll.locks = Gen::effective_locks(
      *schema, {QStringLiteral("terrain"), QStringLiteral("water")});
  const QString base = temp.filePath(QStringLiteral("base.json"));
  QFile base_file(base);
  ASSERT_TRUE(base_file.open(QIODevice::WriteOnly));
  base_file.write(document.to_json_bytes());
  base_file.close();
  invocation.base_path = base;
  invocation.output_path = temp.filePath(QStringLiteral("reroll.json"));
  invocation.report_path = temp.filePath(QStringLiteral("reroll.report.json"));
  QProcess second;
  second.setWorkingDirectory(root);
  second.start(python->program,
               python->prefix_arguments + Gen::build_arguments(reroll, invocation));
  ASSERT_TRUE(second.waitForFinished(900000));
  ASSERT_LT(second.exitCode(), 2)
      << second.readAllStandardError().right(4000).toStdString();

  MapEditor::MapData rerolled;
  ASSERT_TRUE(rerolled.load_from_bytes(read_all(invocation.output_path), &error));
  ASSERT_EQ(rerolled.terrain_elements().size(), document.terrain_elements().size());
  for (int i = 0; i < document.terrain_elements().size(); ++i) {
    EXPECT_FLOAT_EQ(rerolled.terrain_elements()[i].x, document.terrain_elements()[i].x)
        << "locked terrain moved";
  }
}

} // namespace
