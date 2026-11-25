#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

#include <gtest/gtest.h>

#include "app/bootstrap/startup_supervisor.h"

#ifndef Q_OS_WIN
#include <csignal>
#include <sys/types.h>
#endif

namespace {

using namespace App::Bootstrap;

auto read_file(const QString& path) -> QString {
  QFile file(path);
  return file.open(QIODevice::ReadOnly | QIODevice::Text)
             ? QString::fromUtf8(file.readAll())
             : QString();
}

auto count_lines(const QString& path) -> int {
  return read_file(path).split(QLatin1Char('\n'), Qt::SkipEmptyParts).size();
}

#ifndef Q_OS_WIN

struct Harness {
  QTemporaryDir dir;
  SupervisorConfig config;
  QStringList failures;
  QString launches;
  QString pids;

  // `native` and `mesa` are shell fragments; they see $SOI_STARTUP_STATUS_FILE
  // exactly as the real child does.
  Harness(const QString& native,
          const QString& mesa,
          const QString& mesa_required_file = QString()) {
    launches = dir.filePath(QStringLiteral("launches.txt"));
    pids = dir.filePath(QStringLiteral("pids.txt"));
    const QString script =
        QStringLiteral("echo $SOI_STARTUP_ATTEMPT >> '%1'; echo $$ >> '%2'; "
                       "if [ \"$SOI_STARTUP_ATTEMPT\" = native ]; then %3; else %4; fi")
            .arg(launches, pids, native, mesa);
    config.program = QStringLiteral("/bin/sh");
    config.arguments = {QStringLiteral("-c"), script};
    config.attempts = {AttemptPlan{.backend = QStringLiteral("native")},
                       AttemptPlan{.backend = QStringLiteral("mesa"),
                                   .required_file = mesa_required_file}};
    config.ready_timeout = std::chrono::milliseconds(700);
    config.log_path = dir.filePath(QStringLiteral("startup.log"));
    config.scratch_dir = dir.filePath(QStringLiteral("scratch"));
    config.show_failure = [this](const QString& text) {
      failures << text;
    };
  }

  auto run() -> int { return run_supervised_startup(config); }
  [[nodiscard]] auto log() const -> QString { return read_file(config.log_path); }
  [[nodiscard]] auto launch_count() const -> int { return count_lines(launches); }
  [[nodiscard]] auto launched_backends() const -> QStringList {
    return read_file(launches).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
  }
  [[nodiscard]] auto leftover_processes() const -> int {
    int alive = 0;
    for (const QString& pid :
         read_file(pids).split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
      if (::kill(static_cast<pid_t>(pid.toLongLong()), 0) == 0) {
        ++alive;
      }
    }
    return alive;
  }
};

constexpr const char* k_ready_then_exit3 =
    "echo 'stage qml' >> $SOI_STARTUP_STATUS_FILE; echo ready >> "
    "$SOI_STARTUP_STATUS_FILE; sleep 0.3; exit 3";
constexpr const char* k_ready_then_exit0 =
    "echo 'stage qml' >> $SOI_STARTUP_STATUS_FILE; echo ready >> "
    "$SOI_STARTUP_STATUS_FILE; sleep 0.3; exit 0";
constexpr const char* k_probe_rejected =
    "echo 'stage probe' >> $SOI_STARTUP_STATUS_FILE; echo 'failed probe: GDI Generic' "
    ">> $SOI_STARTUP_STATUS_FILE; exit 20";
constexpr const char* k_crash_in_probe =
    "echo 'stage probe' >> $SOI_STARTUP_STATUS_FILE; kill -SEGV $$";
constexpr const char* k_hang_at_scene_graph =
    "echo 'stage scene_graph' >> $SOI_STARTUP_STATUS_FILE; sleep 30";
constexpr const char* k_qt_failure =
    "echo 'stage scene_graph' >> $SOI_STARTUP_STATUS_FILE; echo 'failed scene_graph: "
    "no context' >> $SOI_STARTUP_STATUS_FILE; exit 3";

TEST(StartupSupervisor, HealthyNativeLaunchNeverTouchesMesa) {
  Harness harness(k_ready_then_exit0, k_ready_then_exit0);
  EXPECT_EQ(harness.run(), 0);
  EXPECT_EQ(harness.launched_backends(), QStringList{QStringLiteral("native")});
  EXPECT_TRUE(harness.failures.isEmpty());
  EXPECT_TRUE(harness.log().contains(QStringLiteral("backend=native ready")));
}

TEST(StartupSupervisor, ExitCodeAfterReadinessIsPassedThroughWithoutRetry) {
  Harness harness(k_ready_then_exit3, k_ready_then_exit0);
  EXPECT_EQ(harness.run(), 3);
  EXPECT_EQ(harness.launch_count(), 1);
  EXPECT_TRUE(harness.failures.isEmpty());
}

TEST(StartupSupervisor, RejectedProbeGetsOneFreshMesaAttempt) {
  Harness harness(k_probe_rejected, k_ready_then_exit0);
  EXPECT_EQ(harness.run(), 0);
  EXPECT_EQ(harness.launched_backends(),
            (QStringList{QStringLiteral("native"), QStringLiteral("mesa")}));
  const QString log = harness.log();
  EXPECT_TRUE(
      log.contains(QStringLiteral("native OpenGL was rejected at stage 'probe'")))
      << log.toStdString();
  EXPECT_TRUE(log.contains(QStringLiteral("GDI Generic")));
  EXPECT_TRUE(log.contains(QStringLiteral("backend=mesa ready")));
  EXPECT_TRUE(harness.failures.isEmpty());
}

TEST(StartupSupervisor, DriverCrashDuringProbeFallsBackToMesa) {
  Harness harness(k_crash_in_probe, k_ready_then_exit0);
  EXPECT_EQ(harness.run(), 0);
  EXPECT_EQ(harness.launch_count(), 2);
  EXPECT_TRUE(harness.log().contains(QStringLiteral("crashed at stage 'probe'")));
}

TEST(StartupSupervisor, PassingProbeFollowedByQtFailureFallsBackToMesa) {
  Harness harness(k_qt_failure, k_ready_then_exit0);
  EXPECT_EQ(harness.run(), 0);
  EXPECT_EQ(harness.launch_count(), 2);
  EXPECT_TRUE(harness.log().contains(QStringLiteral("no context")));
}

TEST(StartupSupervisor, StalledAttemptIsKilledAndLeavesNoProcessBehind) {
  Harness harness(k_hang_at_scene_graph, k_ready_then_exit0);
  EXPECT_EQ(harness.run(), 0);
  EXPECT_EQ(harness.launch_count(), 2);
  EXPECT_TRUE(harness.log().contains(
      QStringLiteral("did not become ready in time; stopped at stage 'scene_graph'")));
  EXPECT_EQ(harness.leftover_processes(), 0);
}

TEST(StartupSupervisor, BothAttemptsFailingShowsOneDiagnosticAndStops) {
  Harness harness(k_probe_rejected, k_hang_at_scene_graph);
  EXPECT_NE(harness.run(), 0);
  EXPECT_EQ(harness.launch_count(), 2);
  ASSERT_EQ(harness.failures.size(), 1);
  const QString& text = harness.failures.front();
  EXPECT_TRUE(text.contains(QStringLiteral("Graphics driver OpenGL")));
  EXPECT_TRUE(text.contains(QStringLiteral("Bundled software OpenGL")));
  EXPECT_TRUE(text.contains(harness.config.log_path));
  EXPECT_EQ(harness.leftover_processes(), 0);
}

TEST(StartupSupervisor, MissingMesaRuntimeIsReportedWithoutLaunchingIt) {
  Harness harness(k_probe_rejected,
                  k_ready_then_exit0,
                  QStringLiteral("/nonexistent/opengl32sw.dll"));
  EXPECT_NE(harness.run(), 0);
  EXPECT_EQ(harness.launched_backends(), QStringList{QStringLiteral("native")});
  ASSERT_EQ(harness.failures.size(), 1);
  EXPECT_TRUE(harness.failures.front().contains(QStringLiteral("opengl32sw.dll")));
}

TEST(StartupSupervisor, ProgramThatCannotStartIsAFailedAttempt) {
  Harness harness(k_ready_then_exit0, k_ready_then_exit0);
  harness.config.program = QStringLiteral("/nonexistent/standard_of_iron");
  EXPECT_NE(harness.run(), 0);
  EXPECT_EQ(harness.failures.size(), 1);
}

TEST(StartupSupervisor, CleanExitBeforeReadinessIsNotRetried) {
  Harness harness(QStringLiteral("exit 0"), k_ready_then_exit0);
  EXPECT_EQ(harness.run(), 0);
  EXPECT_EQ(harness.launch_count(), 1);
  EXPECT_TRUE(harness.failures.isEmpty());
}

#endif

TEST(StartupSupervisor, StatusTextKeepsLastStageAndFailure) {
  const auto status = parse_status_text(
      QStringLiteral("stage probe\nstage qml\nfailed scene_graph: no ctx: bad\n"));
  EXPECT_FALSE(status.ready);
  EXPECT_EQ(status.last_stage, QStringLiteral("qml"));
  EXPECT_EQ(status.failure_stage, QStringLiteral("scene_graph"));
  EXPECT_EQ(status.failure_message, QStringLiteral("no ctx: bad"));
  EXPECT_TRUE(parse_status_text(QStringLiteral("stage a\nready\n")).ready);
}

TEST(StartupSupervisor, ChildReportingIsInertOutsideASupervisedLaunch) {
  qunsetenv(k_env_startup_status_file);
  qunsetenv(k_env_startup_attempt);
  EXPECT_FALSE(supervised_child());
  report_startup_stage(QStringLiteral("probe"));
  report_startup_failure(QStringLiteral("probe"), QStringLiteral("x"));
  EXPECT_FALSE(startup_ready_reported());
}

} // namespace
