#include "benchmark_action_fixture.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>

#include <algorithm>
#include <cmath>

#include "game/core/presentation_coverage.h"

namespace App::Core {

namespace {

constexpr int k_supported_version = 1;
constexpr double k_min_loop_seconds = 1.0;

auto action_names() -> QStringList {
  return {QStringLiteral("select_all"),
          QStringLiteral("select_at"),
          QStringLiteral("select_by_type"),
          QStringLiteral("move_to"),
          QStringLiteral("attack_at"),
          QStringLiteral("guard_at"),
          QStringLiteral("patrol_at"),
          QStringLiteral("hover_at"),
          QStringLiteral("stop"),
          QStringLiteral("hold"),
          QStringLiteral("run"),
          QStringLiteral("guard"),
          QStringLiteral("build_panel"),
          QStringLiteral("production_panel"),
          QStringLiteral("recruit"),
          QStringLiteral("set_rally")};
}

auto normalized(const QJsonValue& value, double fallback) -> double {
  if (!value.isDouble()) {
    return fallback;
  }
  return std::clamp(value.toDouble(), 0.0, 1.0);
}

} // namespace

auto known_benchmark_actions() -> const QStringList& {
  static const QStringList names = action_names();
  return names;
}

auto parse_benchmark_action_fixture(const QJsonDocument& document, QString* error)
    -> std::optional<BenchmarkActionFixture> {
  const auto fail = [error](const QString& message) {
    if (error != nullptr) {
      *error = message;
    }
    return std::optional<BenchmarkActionFixture>{};
  };

  if (!document.isObject()) {
    return fail(QStringLiteral("action fixture is not a JSON object"));
  }
  const QJsonObject root = document.object();

  BenchmarkActionFixture fixture;
  fixture.version = root.value(QStringLiteral("version")).toInt(0);
  if (fixture.version != k_supported_version) {
    return fail(QStringLiteral("unsupported action fixture version %1; expected %2")
                    .arg(fixture.version)
                    .arg(k_supported_version));
  }
  fixture.name = root.value(QStringLiteral("name")).toString();
  if (fixture.name.isEmpty()) {
    return fail(QStringLiteral("action fixture has no name"));
  }
  fixture.loop_seconds = root.value(QStringLiteral("loop_seconds")).toDouble(0.0);
  if (fixture.loop_seconds != 0.0 && fixture.loop_seconds < k_min_loop_seconds) {
    return fail(QStringLiteral("loop_seconds must be 0 or at least %1")
                    .arg(k_min_loop_seconds));
  }

  const QJsonValue required = root.value(QStringLiteral("required_coverage"));
  if (!required.isArray()) {
    return fail(QStringLiteral("action fixture has no required_coverage array"));
  }
  for (const QJsonValue& entry : required.toArray()) {
    const QString name = entry.toString();
    if (name.isEmpty()) {
      return fail(QStringLiteral("required_coverage entries must be strings"));
    }
    if (Engine::Core::coverage_event_from_name(name.toStdString()) ==
        Engine::Core::CoverageEvent::_Count) {
      return fail(QStringLiteral("unknown required coverage event: %1").arg(name));
    }
    fixture.required_coverage.append(name);
  }
  if (fixture.required_coverage.isEmpty()) {
    return fail(QStringLiteral("action fixture requires at least one coverage event"));
  }

  const QJsonValue actions = root.value(QStringLiteral("actions"));
  if (!actions.isArray() || actions.toArray().isEmpty()) {
    return fail(QStringLiteral("action fixture has no actions"));
  }
  for (const QJsonValue& entry : actions.toArray()) {
    if (!entry.isObject()) {
      return fail(QStringLiteral("action entries must be objects"));
    }
    const QJsonObject object = entry.toObject();
    BenchmarkAction action;
    action.at_seconds = object.value(QStringLiteral("at")).toDouble(-1.0);
    if (!(action.at_seconds >= 0.0) || !std::isfinite(action.at_seconds)) {
      return fail(QStringLiteral("action entries need a finite non-negative \"at\""));
    }
    action.action = object.value(QStringLiteral("action")).toString();
    if (!known_benchmark_actions().contains(action.action)) {
      return fail(QStringLiteral("unknown action: %1").arg(action.action));
    }
    action.x = normalized(object.value(QStringLiteral("x")), 0.5);
    action.y = normalized(object.value(QStringLiteral("y")), 0.5);
    action.argument = object.value(QStringLiteral("argument")).toString();
    fixture.actions.push_back(action);
  }
  std::stable_sort(fixture.actions.begin(),
                   fixture.actions.end(),
                   [](const BenchmarkAction& left, const BenchmarkAction& right) {
                     return left.at_seconds < right.at_seconds;
                   });
  if (fixture.loop_seconds != 0.0 &&
      fixture.actions.back().at_seconds >= fixture.loop_seconds) {
    return fail(QStringLiteral("the last action at %1 s is not inside the %2 s loop")
                    .arg(fixture.actions.back().at_seconds)
                    .arg(fixture.loop_seconds));
  }
  return fixture;
}

auto load_benchmark_action_fixture(const QString& path, QString* error)
    -> std::optional<BenchmarkActionFixture> {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    if (error != nullptr) {
      *error = QStringLiteral("cannot open action fixture: %1").arg(path);
    }
    return {};
  }
  QJsonParseError parse_error{};
  const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parse_error);
  if (parse_error.error != QJsonParseError::NoError) {
    if (error != nullptr) {
      *error = QStringLiteral("%1: %2").arg(path, parse_error.errorString());
    }
    return {};
  }
  return parse_benchmark_action_fixture(document, error);
}

auto actions_between(const BenchmarkActionFixture& fixture,
                     double previous_seconds,
                     double current_seconds) -> std::vector<BenchmarkAction> {
  std::vector<BenchmarkAction> due;
  if (current_seconds <= previous_seconds) {
    return due;
  }
  if (fixture.loop_seconds <= 0.0) {
    for (const auto& action : fixture.actions) {
      if (action.at_seconds > previous_seconds &&
          action.at_seconds <= current_seconds) {
        due.push_back(action);
      }
    }
    return due;
  }

  const double span =
      std::min(current_seconds - previous_seconds, fixture.loop_seconds);
  const double start = std::fmod(previous_seconds, fixture.loop_seconds);
  const double end = start + span;
  for (const auto& action : fixture.actions) {
    const bool in_window = action.at_seconds > start && action.at_seconds <= end;
    const bool wrapped =
        end > fixture.loop_seconds && action.at_seconds <= end - fixture.loop_seconds;
    if (in_window || wrapped) {
      due.push_back(action);
    }
  }
  return due;
}

} // namespace App::Core
