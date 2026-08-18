

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include <cstdio>
#include <utility>
#include <vector>

#include "balance_fixture.h"
#include "balance_report.h"
#include "battle_simulation.h"
#include "game/session/session_context.h"

namespace {

auto resolve_default_fixture_dir() -> QString {
  const QStringList candidates{
      QDir::current().filePath(QStringLiteral("assets/balance")),
      QDir(QCoreApplication::applicationDirPath())
          .filePath(QStringLiteral("assets/balance")),
      QDir(QCoreApplication::applicationDirPath())
          .filePath(QStringLiteral("../assets/balance")),
      QDir(QCoreApplication::applicationDirPath())
          .filePath(QStringLiteral("../../assets/balance")),
  };
  for (const QString& candidate : candidates) {
    if (QDir(candidate).exists()) {
      return QDir(candidate).absolutePath();
    }
  }
  return {};
}

auto write_file(const QString& path, const QByteArray& contents) -> bool {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    QTextStream(stderr) << "cannot write " << path << ": " << file.errorString()
                        << "\n";
    return false;
  }
  file.write(contents);
  return true;
}

} // namespace

auto main(int argc, char** argv) -> int {
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QCoreApplication app(argc, argv);

  Game::Session::SessionContext session;
  Game::Session::ScopedSession const active_session(session);
  QCoreApplication::setApplicationName(QStringLiteral("balance_sim"));

  QCommandLineParser parser;
  parser.setApplicationDescription(
      QStringLiteral("Headless deterministic battle-balance simulator."));
  parser.addHelpOption();

  const QCommandLineOption fixtures_option(
      {QStringLiteral("f"), QStringLiteral("fixtures")},
      QStringLiteral("Fixture file or directory (default: assets/balance)."),
      QStringLiteral("path"));
  const QCommandLineOption filter_option(
      QStringLiteral("filter"),
      QStringLiteral("Only run fixtures whose id contains this substring."),
      QStringLiteral("substring"));
  const QCommandLineOption seeds_option(
      QStringLiteral("seeds"),
      QStringLiteral("Override the seed count declared by each fixture."),
      QStringLiteral("count"));
  const QCommandLineOption json_option(QStringLiteral("json"),
                                       QStringLiteral("Write the full report as JSON."),
                                       QStringLiteral("path"));
  const QCommandLineOption csv_option(QStringLiteral("csv"),
                                      QStringLiteral("Write per-battle rows as CSV."),
                                      QStringLiteral("path"));
  const QCommandLineOption quiet_option(
      QStringLiteral("quiet"), QStringLiteral("Suppress the human-readable report."));
  const QCommandLineOption trace_option(
      QStringLiteral("trace"),
      QStringLiteral("Run seed 0 of each selected fixture and dump per-second state "
                     "instead of the usual report."));

  parser.addOption(fixtures_option);
  parser.addOption(filter_option);
  parser.addOption(seeds_option);
  parser.addOption(json_option);
  parser.addOption(csv_option);
  parser.addOption(quiet_option);
  parser.addOption(trace_option);
  parser.process(app);

  QString fixture_path = parser.value(fixtures_option);
  if (fixture_path.isEmpty()) {
    fixture_path = resolve_default_fixture_dir();
  }
  if (fixture_path.isEmpty()) {
    QTextStream(stderr) << "no fixture path given and assets/balance not found\n";
    return 2;
  }

  std::vector<Balance::FixtureLoadError> errors;
  std::vector<Balance::Fixture> fixtures;
  if (QFileInfo(fixture_path).isDir()) {
    fixtures = Balance::load_fixture_directory(fixture_path, errors);
  } else if (auto fixture = Balance::load_fixture_file(fixture_path, errors)) {
    fixtures.push_back(std::move(*fixture));
  }

  for (const auto& error : errors) {
    QTextStream(stderr) << "fixture error [" << error.field << "]: " << error.message
                        << "\n";
  }
  if (fixtures.empty()) {
    QTextStream(stderr) << "no fixtures loaded from " << fixture_path << "\n";
    return 2;
  }

  const QString filter = parser.value(filter_option);
  const int seed_override =
      parser.isSet(seeds_option) ? parser.value(seeds_option).toInt() : 0;

  Balance::initialize_simulation_environment();

  std::vector<Balance::FixtureSummary> summaries;
  for (auto& fixture : fixtures) {
    if (!filter.isEmpty() && !fixture.id.contains(filter, Qt::CaseInsensitive)) {
      continue;
    }
    if (seed_override > 0) {
      fixture.seeds = seed_override;
    }

    if (parser.isSet(trace_option)) {
      std::vector<Balance::TraceSample> trace;
      const auto result = Balance::run_battle(fixture, 1U, false, &trace);
      QTextStream out(stdout);
      out << "== trace " << fixture.id << " -> "
          << Balance::outcome_name(result.outcome) << " @"
          << QString::number(result.elapsed_seconds, 'f', 1) << "s\n";
      out << "    t   aliveA aliveB   hpA   hpB    gap  targeted  locked  moving\n";
      for (const auto& sample : trace) {
        out << QStringLiteral("%1 %2 %3 %4 %5 %6 %7 %8 %9\n")
                   .arg(sample.time_seconds, 5, 'f', 1)
                   .arg(sample.alive_a, 7)
                   .arg(sample.alive_b, 7)
                   .arg(sample.health_a, 7, 'f', 0)
                   .arg(sample.health_b, 6, 'f', 0)
                   .arg(sample.centroid_gap, 7, 'f', 1)
                   .arg(sample.units_with_target, 8)
                   .arg(sample.units_in_melee_lock, 8)
                   .arg(sample.units_moving, 8);
      }
      continue;
    }

    std::vector<Balance::BattleResult> results;
    for (int seed = 0; seed < fixture.seeds; ++seed) {
      const auto seed_value = static_cast<std::uint32_t>(seed) * 0x9E3779B9U + 1U;
      results.push_back(Balance::run_battle(fixture, seed_value, false));
      if (fixture.mirror_sides) {
        results.push_back(Balance::run_battle(fixture, seed_value, true));
      }
    }
    summaries.push_back(Balance::summarize(fixture, std::move(results)));
  }

  if (summaries.empty()) {
    QTextStream(stderr) << "filter '" << filter << "' matched no fixtures\n";
    return 2;
  }

  if (!parser.isSet(quiet_option)) {
    QTextStream(stdout) << Balance::render_text_report(summaries);
  }
  if (parser.isSet(json_option) &&
      !write_file(parser.value(json_option), Balance::render_json_report(summaries))) {
    return 3;
  }
  if (parser.isSet(csv_option) &&
      !write_file(parser.value(csv_option), Balance::render_csv_report(summaries))) {
    return 3;
  }

  const bool all_passed =
      std::all_of(summaries.begin(), summaries.end(), [](const auto& summary) {
        return summary.passed();
      });
  return all_passed ? 0 : 1;
}
