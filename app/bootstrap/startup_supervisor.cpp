#include "app/bootstrap/startup_supervisor.h"

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTextStream>

#include <atomic>

namespace App::Bootstrap {

namespace {

constexpr int k_poll_ms = 50;
constexpr int k_kill_wait_ms = 10000;
constexpr int k_max_tee_lines = 4000;

std::atomic_bool g_ready_reported{false};
std::atomic_int g_tee_lines{0};

auto env_path(const char* name) -> QString {
  return QString::fromLocal8Bit(qgetenv(name));
}

void append_line(const QString& path, const QString& line) {
  if (path.isEmpty()) {
    return;
  }
  QFile file(path);
  if (!file.open(QIODevice::Append | QIODevice::Text)) {
    return;
  }
  file.write(line.toUtf8());
  file.write("\n");
}

auto read_text(const QString& path) -> QString {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return {};
  }
  return QString::fromUtf8(file.readAll());
}

class SupervisorLog {
public:
  explicit SupervisorLog(const QString& path)
      : m_path(path) {
    if (m_path.isEmpty()) {
      return;
    }
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QFile file(m_path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
      file.write("# Standard of Iron startup log\n");
    }
  }

  void line(const QString& text) const {
    append_line(m_path,
                QDateTime::currentDateTime().toString(Qt::ISODateWithMs) +
                    QStringLiteral(" ") + text);
  }

  void block(const QString& title, const QString& body) const {
    if (body.trimmed().isEmpty()) {
      return;
    }
    line(QStringLiteral("--- %1 ---").arg(title));
    append_line(m_path, body.trimmed());
    line(QStringLiteral("--- end %1 ---").arg(title));
  }

private:
  QString m_path;
};

auto classify(const QProcess& process,
              const StatusSnapshot& status,
              AttemptReport& report) -> void {
  report.last_stage = status.last_stage;
  report.crashed = process.exitStatus() == QProcess::CrashExit;
  report.exit_code = process.exitCode();
  if (!status.failure_message.isEmpty()) {
    report.detail = status.failure_message;
    if (!status.failure_stage.isEmpty()) {
      report.last_stage = status.failure_stage;
    }
  }
}

auto run_attempt(const SupervisorConfig& config,
                 const AttemptPlan& plan,
                 int index,
                 const SupervisorLog& log,
                 int& launch_exit_code) -> AttemptReport {
  AttemptReport report;
  report.backend = plan.backend;

  if (!plan.required_file.isEmpty() && !QFileInfo::exists(plan.required_file)) {
    report.skipped = true;
    report.detail =
        QStringLiteral("required file is missing: %1").arg(plan.required_file);
    log.line(QStringLiteral("attempt %1 backend=%2 skipped: %3")
                 .arg(index)
                 .arg(plan.backend, report.detail));
    return report;
  }

  QDir().mkpath(config.scratch_dir);
  const QString stem = QStringLiteral("%1/attempt_%2_%3")
                           .arg(config.scratch_dir)
                           .arg(QDateTime::currentMSecsSinceEpoch())
                           .arg(index);
  const QString status_path = stem + QStringLiteral(".status");
  const QString attempt_log_path = stem + QStringLiteral(".log");
  QFile::remove(status_path);
  QFile::remove(attempt_log_path);

  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  for (const QString& name : plan.unset_env) {
    env.remove(name);
  }
  for (const auto& [name, value] : plan.set_env) {
    env.insert(name, value);
  }
  env.insert(QString::fromLatin1(k_env_startup_attempt), plan.backend);
  env.insert(QString::fromLatin1(k_env_startup_status_file), status_path);
  env.insert(QString::fromLatin1(k_env_startup_log_file), attempt_log_path);

  QProcess process;
  process.setProcessEnvironment(env);
  process.setProcessChannelMode(QProcess::ForwardedChannels);
  process.setProgram(config.program);
  process.setArguments(config.arguments);
  log.line(
      QStringLiteral("attempt %1 backend=%2 starting").arg(index).arg(plan.backend));
  process.start();
  if (!process.waitForStarted(15000)) {
    report.detail = QStringLiteral("could not start: %1").arg(process.errorString());
    log.line(QStringLiteral("attempt %1 backend=%2 failed: %3")
                 .arg(index)
                 .arg(plan.backend, report.detail));
    return report;
  }
  report.started = true;

  QElapsedTimer waited;
  waited.start();
  bool running_after_ready = false;
  while (process.state() != QProcess::NotRunning) {
    if (process.waitForFinished(k_poll_ms)) {
      break;
    }
    const StatusSnapshot status = parse_status_text(read_text(status_path));
    if (status.ready && !running_after_ready) {
      running_after_ready = true;
      report.ready = true;
      report.last_stage = status.last_stage;
      log.line(QStringLiteral("attempt %1 backend=%2 ready after %3 ms")
                   .arg(index)
                   .arg(plan.backend)
                   .arg(waited.elapsed()));
      log.block(QStringLiteral("attempt %1 log").arg(index),
                read_text(attempt_log_path));
      // The launch worked; from here the supervisor only waits for the exit.
      process.waitForFinished(-1);
      break;
    }
    if (!running_after_ready && waited.elapsed() >= config.ready_timeout.count()) {
      report.timed_out = true;
      process.kill();
      process.waitForFinished(k_kill_wait_ms);
      break;
    }
  }

  const StatusSnapshot status = parse_status_text(read_text(status_path));
  report.ready = report.ready || status.ready;
  classify(process, status, report);
  if (report.timed_out) {
    report.crashed = false;
    report.exit_code = -1;
  }
  if (!running_after_ready) {
    log.block(QStringLiteral("attempt %1 log").arg(index), read_text(attempt_log_path));
  }
  log.line(QStringLiteral("attempt %1 backend=%2 %3")
               .arg(index)
               .arg(plan.backend, describe_attempt(report)));
  if (report.ready) {
    launch_exit_code = process.exitCode();
    if (process.exitStatus() == QProcess::CrashExit) {
      launch_exit_code = k_exit_supervisor_failed;
    }
  }
  QFile::remove(status_path);
  QFile::remove(attempt_log_path);
  return report;
}

} // namespace

auto parse_status_text(const QString& text) -> StatusSnapshot {
  StatusSnapshot snapshot;
  const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
  for (const QString& raw : lines) {
    const QString line = raw.trimmed();
    if (line == QLatin1String("ready")) {
      snapshot.ready = true;
    } else if (line.startsWith(QLatin1String("stage "))) {
      snapshot.last_stage = line.mid(6).trimmed();
    } else if (line.startsWith(QLatin1String("failed "))) {
      const QString rest = line.mid(7);
      const qsizetype split = rest.indexOf(QLatin1Char(':'));
      snapshot.failure_stage = (split < 0 ? rest : rest.left(split)).trimmed();
      snapshot.failure_message = split < 0 ? QString() : rest.mid(split + 1).trimmed();
      if (snapshot.failure_message.isEmpty()) {
        snapshot.failure_message = QStringLiteral("failed");
      }
    }
  }
  return snapshot;
}

auto describe_attempt(const AttemptReport& report) -> QString {
  if (report.skipped) {
    return QStringLiteral("not attempted (%1)").arg(report.detail);
  }
  if (!report.started) {
    return QStringLiteral("did not start (%1)").arg(report.detail);
  }
  const QString stage =
      report.last_stage.isEmpty() ? QStringLiteral("startup") : report.last_stage;
  QString text;
  if (report.ready) {
    text = QStringLiteral("reached the first gameplay frame, then exited with code %1")
               .arg(report.exit_code);
  } else if (report.timed_out) {
    text = QStringLiteral("did not become ready in time; stopped at stage '%1'")
               .arg(stage);
  } else if (report.crashed) {
    text = QStringLiteral("crashed at stage '%1'").arg(stage);
  } else if (report.exit_code == k_exit_probe_rejected) {
    text = QStringLiteral("native OpenGL was rejected at stage '%1'").arg(stage);
  } else {
    text = QStringLiteral("failed at stage '%1' (exit code %2)")
               .arg(stage)
               .arg(report.exit_code);
  }
  if (!report.detail.isEmpty()) {
    text += QStringLiteral(": ") + report.detail;
  }
  return text;
}

auto failure_diagnostic(const std::vector<AttemptReport>& reports,
                        const QString& log_path) -> QString {
  QStringList lines;
  lines << QStringLiteral("Standard of Iron could not start its OpenGL renderer.")
        << QString();
  for (const AttemptReport& report : reports) {
    lines << QStringLiteral("- %1 OpenGL: %2")
                 .arg(report.backend == QLatin1String("mesa")
                          ? QStringLiteral("Bundled software")
                          : QStringLiteral("Graphics driver"),
                      describe_attempt(report));
  }
  lines << QString() << QStringLiteral("What to try:")
        << QStringLiteral("1. Update the graphics driver from the GPU manufacturer.")
        << QStringLiteral(
               "2. Run run_debug.cmd in the game folder and attach runlog.txt to a bug "
               "report.");
  if (!log_path.isEmpty()) {
    lines << QStringLiteral("3. Attach the startup log: %1").arg(log_path);
  }
  return lines.join(QLatin1Char('\n'));
}

auto run_supervised_startup(const SupervisorConfig& config) -> int {
  const SupervisorLog log(config.log_path);
  log.line(QStringLiteral("supervisor starting %1").arg(config.program));

  std::vector<AttemptReport> reports;
  int index = 0;
  for (const AttemptPlan& plan : config.attempts) {
    ++index;
    int launch_exit_code = 0;
    AttemptReport report = run_attempt(config, plan, index, log, launch_exit_code);
    const bool ready = report.ready;
    // Exiting cleanly before readiness is the child answering a request that
    // never needs a renderer, not a failed launch.
    const bool clean_exit =
        report.started && !report.timed_out && !report.crashed && report.exit_code == 0;
    reports.push_back(std::move(report));
    if (ready) {
      return launch_exit_code;
    }
    if (clean_exit) {
      return 0;
    }
  }

  const QString diagnostic = failure_diagnostic(reports, config.log_path);
  log.line(QStringLiteral("launch failed"));
  log.block(QStringLiteral("diagnostic"), diagnostic);
  if (config.show_failure) {
    config.show_failure(diagnostic);
  }
  return k_exit_supervisor_failed;
}

auto supervised_child() -> bool {
  return !env_path(k_env_startup_attempt).isEmpty() &&
         !env_path(k_env_startup_status_file).isEmpty();
}

auto supervised_attempt_name() -> QString {
  return env_path(k_env_startup_attempt);
}

void report_startup_stage(const QString& stage) {
  if (!supervised_child()) {
    return;
  }
  append_line(env_path(k_env_startup_status_file), QStringLiteral("stage ") + stage);
  tee_startup_log(QStringLiteral("[stage] ") + stage);
}

void report_startup_ready() {
  if (!supervised_child() || g_ready_reported.exchange(true)) {
    return;
  }
  append_line(env_path(k_env_startup_status_file), QStringLiteral("ready"));
  tee_startup_log(QStringLiteral("[stage] ready"));
}

void report_startup_failure(const QString& stage, const QString& message) {
  if (!supervised_child()) {
    return;
  }
  QString flat = message;
  flat.replace(QLatin1Char('\n'), QLatin1Char(' '));
  append_line(env_path(k_env_startup_status_file),
              QStringLiteral("failed %1: %2").arg(stage, flat));
  tee_startup_log(QStringLiteral("[failed] %1: %2").arg(stage, flat));
}

auto startup_ready_reported() -> bool {
  return g_ready_reported.load();
}

void tee_startup_log(const QString& line) {
  if (!supervised_child() || g_ready_reported.load() ||
      g_tee_lines.fetch_add(1) >= k_max_tee_lines) {
    return;
  }
  append_line(env_path(k_env_startup_log_file), line);
}

} // namespace App::Bootstrap
