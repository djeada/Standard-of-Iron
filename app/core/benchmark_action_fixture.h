#pragma once

#include <QJsonDocument>
#include <QString>
#include <QStringList>

#include <optional>
#include <vector>

namespace App::Core {

struct BenchmarkAction {
  double at_seconds = 0.0;
  QString action;
  double x = 0.5;
  double y = 0.5;
  QString argument;
};

struct BenchmarkActionFixture {
  int version = 0;
  QString name;
  double loop_seconds = 0.0;
  QStringList required_coverage;
  std::vector<BenchmarkAction> actions;

  [[nodiscard]] auto duration_seconds() const -> double {
    return actions.empty() ? 0.0 : actions.back().at_seconds;
  }
};

[[nodiscard]] auto known_benchmark_actions() -> const QStringList&;

[[nodiscard]] auto
parse_benchmark_action_fixture(const QJsonDocument& document,
                               QString* error) -> std::optional<BenchmarkActionFixture>;

[[nodiscard]] auto load_benchmark_action_fixture(const QString& path, QString* error)
    -> std::optional<BenchmarkActionFixture>;

[[nodiscard]] auto
actions_between(const BenchmarkActionFixture& fixture,
                double previous_seconds,
                double current_seconds) -> std::vector<BenchmarkAction>;

} // namespace App::Core
