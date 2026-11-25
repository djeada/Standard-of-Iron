#pragma once

#include <QString>
#include <QStringList>

#include <chrono>
#include <functional>
#include <utility>
#include <vector>

namespace App::Bootstrap {

inline constexpr char k_env_startup_attempt[] = "SOI_STARTUP_ATTEMPT";
inline constexpr char k_env_startup_status_file[] = "SOI_STARTUP_STATUS_FILE";
inline constexpr char k_env_startup_log_file[] = "SOI_STARTUP_LOG_FILE";
inline constexpr char k_env_startup_log_path[] = "SOI_STARTUP_LOG_PATH";
inline constexpr char k_env_no_supervisor[] = "SOI_NO_STARTUP_SUPERVISOR";

inline constexpr int k_exit_probe_rejected = 20;
inline constexpr int k_exit_renderer_failed = 21;
inline constexpr int k_exit_supervisor_failed = 22;

inline constexpr auto k_default_ready_timeout = std::chrono::seconds(90);

struct AttemptPlan {
  QString backend;
  std::vector<std::pair<QString, QString>> set_env;
  QStringList unset_env;

  QString required_file;
};

struct SupervisorConfig {
  QString program;
  QStringList arguments;
  std::vector<AttemptPlan> attempts;
  std::chrono::milliseconds ready_timeout = k_default_ready_timeout;
  QString log_path;
  QString scratch_dir;
  std::function<void(const QString&)> show_failure;
};

struct StatusSnapshot {
  bool ready = false;
  QString last_stage;
  QString failure_stage;
  QString failure_message;
};

struct AttemptReport {
  QString backend;
  bool started = false;
  bool ready = false;
  bool crashed = false;
  bool timed_out = false;
  bool skipped = false;
  int exit_code = 0;
  QString last_stage;
  QString detail;
};

[[nodiscard]] auto parse_status_text(const QString& text) -> StatusSnapshot;
[[nodiscard]] auto describe_attempt(const AttemptReport& report) -> QString;
[[nodiscard]] auto failure_diagnostic(const std::vector<AttemptReport>& reports,
                                      const QString& log_path) -> QString;

[[nodiscard]] auto run_supervised_startup(const SupervisorConfig& config) -> int;

[[nodiscard]] auto supervised_child() -> bool;
[[nodiscard]] auto supervised_attempt_name() -> QString;
void report_startup_stage(const QString& stage);
void report_startup_ready();
void report_startup_failure(const QString& stage, const QString& message);
[[nodiscard]] auto startup_ready_reported() -> bool;
void tee_startup_log(const QString& line);

} // namespace App::Bootstrap
