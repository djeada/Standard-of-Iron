#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

namespace MapEditor::Generator {

inline constexpr int k_schema_version = 1;

enum class ParameterType {
  Float,
  Int,
  Bool,
  Choice
};

struct ParameterChoice {
  QString id;
  QString label;
};

struct ParameterSpec {
  QString key;
  QString stage;
  QString label;
  QString help;
  ParameterType type = ParameterType::Float;
  double min = 0.0;
  double max = 1.0;
  double step = 0.0;
  QJsonValue default_value;
  QVector<ParameterChoice> choices;
};

struct StageSpec {
  QString id;
  QString label;
  QStringList forced_by;
};

struct PresetSpec {
  QString id;
  QString label;
  QString description;
  int width = 650;
  int height = 650;
  QJsonObject parameters;
};

struct Schema {
  int version = 0;
  QVector<StageSpec> stages;
  QVector<ParameterSpec> parameters;
  QVector<PresetSpec> presets;

  [[nodiscard]] auto stage(const QString& id) const -> const StageSpec*;
  [[nodiscard]] auto preset(const QString& id) const -> const PresetSpec*;
  [[nodiscard]] auto parameter(const QString& key) const -> const ParameterSpec*;
  [[nodiscard]] auto
  parameters_for(const QString& stage) const -> QVector<const ParameterSpec*>;
  [[nodiscard]] auto parameter_groups() const -> QStringList;
};

[[nodiscard]] auto parse_schema(const QByteArray& json,
                                QString* error = nullptr) -> std::optional<Schema>;

struct Request {
  qint64 seed = 1;
  QString preset;
  int width = 650;
  int height = 650;
  QJsonObject parameters;
  QStringList locks;
};

[[nodiscard]] auto request_to_json(const Request& request) -> QJsonObject;
[[nodiscard]] auto request_from_json(const QJsonObject& json) -> Request;
[[nodiscard]] auto resolved_parameters(const Schema& schema,
                                       const Request& request) -> QJsonObject;

struct Invocation {
  QString script;
  QString base_path;
  QString output_path;
  QString report_path;
  QString request_path;
  bool progress = true;
};

[[nodiscard]] auto build_arguments(const Request& request,
                                   const Invocation& invocation) -> QStringList;

[[nodiscard]] auto forced_stages(const Schema& schema, const QStringList& locks)
    -> QMap<QString, QStringList>;
[[nodiscard]] auto effective_locks(const Schema& schema,
                                   const QStringList& locks) -> QStringList;

struct ProgressEvent {
  enum class Kind {
    Stage,
    Done,
    Error
  };
  Kind kind = Kind::Stage;
  QString stage;
  QString status;
  double seconds = -1.0;
  int attempt = 1;
  bool ok = false;
  QString output;
  QString report;
  QString message;
};

[[nodiscard]] auto
parse_progress_line(const QByteArray& line) -> std::optional<ProgressEvent>;

class ProgressParser {
public:
  void feed(const QByteArray& chunk);
  void finish();
  [[nodiscard]] auto take_events() -> QVector<ProgressEvent>;
  [[nodiscard]] auto ignored_lines() const -> int { return m_ignored; }

private:
  void consume_line(const QByteArray& line);

  QByteArray m_buffer;
  QVector<ProgressEvent> m_events;
  int m_ignored = 0;
};

struct ReportObject {
  QString kind;
  int index = -1;
  double x = 0.0;
  double z = 0.0;
  bool has_position = false;
};

enum class CheckStatus {
  Pass,
  Warn,
  Fail
};

struct ReportCheck {
  QString id;
  CheckStatus status = CheckStatus::Pass;
  QString message;
  QVector<ReportObject> objects;
};

struct ReportStage {
  QString id;
  QString status;
  qint64 seed = 0;
  double seconds = 0.0;
  QString forced_by;
  QStringList notes;
};

struct Report {
  int version = 0;
  bool ok = false;
  QJsonObject request;
  QVector<ReportStage> stages;
  QVector<ReportCheck> checks;
  QJsonObject metrics;

  [[nodiscard]] auto count(CheckStatus status) const -> int;
};

[[nodiscard]] auto parse_report(const QByteArray& json,
                                QString* error = nullptr) -> std::optional<Report>;

[[nodiscard]] auto
find_repository_root(const QStringList& start_directories) -> QString;
[[nodiscard]] auto script_path(const QString& repository_root) -> QString;

struct PythonCommand {
  QString program;
  QStringList prefix_arguments;
};

[[nodiscard]] auto find_python() -> std::optional<PythonCommand>;

} // namespace MapEditor::Generator
