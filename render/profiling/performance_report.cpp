#include "performance_report.h"

#include <QJsonArray>
#include <QString>

#include "asset_counters.h"
#include "game/core/nav_profile.h"

namespace Render::Profiling {

namespace {

auto counter_key(std::string_view name) -> QString {
  return QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size()));
}

void note_failure(QJsonArray& failures,
                  const QString& name,
                  double measured,
                  double budget) {
  failures.append(QStringLiteral("%1 was %2 (budget %3)")
                      .arg(name)
                      .arg(measured, 0, 'f', 3)
                      .arg(budget, 0, 'f', 3));
}

auto check(QJsonObject& checks,
           QJsonArray& failures,
           const QString& name,
           double measured,
           double budget) -> bool {
  const bool passed = measured <= budget;
  checks.insert(name,
                QJsonObject{{QStringLiteral("measured"), measured},
                            {QStringLiteral("budget"), budget},
                            {QStringLiteral("passed"), passed}});
  if (!passed) {
    note_failure(failures, name, measured, budget);
  }
  return passed;
}

} // namespace

auto PerformanceBudget::scale_gate(double frame_p95_ms) noexcept -> PerformanceBudget {
  PerformanceBudget budget;
  budget.frame_p50_ms = frame_p95_ms;
  budget.frame_p95_ms = frame_p95_ms;
  budget.frame_p99_ms = frame_p95_ms * 1.2;
  budget.frame_max_ms = frame_p95_ms * 1.5;
  return budget;
}

auto asset_counters_json() -> QJsonObject {
  const AssetCounters& counters = asset_counters();
  QJsonObject totals;
  QJsonObject post_load;
  for (std::size_t i = 0; i < AssetCounters::k_count; ++i) {
    const auto counter = static_cast<AssetCounter>(i);
    const QString key = counter_key(asset_counter_name(counter));
    totals.insert(key, static_cast<qint64>(counters.total(counter)));
    post_load.insert(key, static_cast<qint64>(counters.since_barrier(counter)));
  }
  QJsonObject out{
      {QStringLiteral("load_barrier_marked"), counters.load_barrier_marked()},
      {QStringLiteral("total"), totals}};
  if (counters.load_barrier_marked()) {
    out.insert(QStringLiteral("after_load_barrier"), post_load);
    out.insert(QStringLiteral("post_load_asset_work"),
               static_cast<qint64>(counters.post_barrier_asset_work()));
  }
  return out;
}

auto navigation_counters_json() -> QJsonObject {
  const Engine::Core::NavProfile& profile = Engine::Core::nav_profile();
  const auto time = profile.tick_time_ms();
  QJsonObject per_tick;
  QJsonObject totals;
  for (std::size_t i = 0; i < Engine::Core::NavProfile::k_count; ++i) {
    const auto counter = static_cast<Engine::Core::NavCounter>(i);
    const QString key = counter_key(Engine::Core::nav_counter_name(counter));
    per_tick.insert(key, profile.per_tick_average(counter));
    totals.insert(key, static_cast<qint64>(profile.total(counter)));
  }
  return QJsonObject{{QStringLiteral("enabled"), profile.enabled()},
                     {QStringLiteral("ticks"), static_cast<qint64>(profile.ticks())},
                     {QStringLiteral("tick_ms"),
                      QJsonObject{{QStringLiteral("average_ms"), time.average},
                                  {QStringLiteral("p50_ms"), time.p50},
                                  {QStringLiteral("p95_ms"), time.p95},
                                  {QStringLiteral("p99_ms"), time.p99},
                                  {QStringLiteral("max_ms"), time.maximum}}},
                     {QStringLiteral("max_queue_age_ticks"),
                      static_cast<qint64>(profile.max_queue_age_ticks())},
                     {QStringLiteral("per_tick_average"), per_tick},
                     {QStringLiteral("total"), totals}};
}

auto system_profiler_json(const Engine::Core::SystemProfiler& profiler) -> QJsonObject {
  QJsonArray systems;
  for (const auto& record : profiler.systems()) {
    if (record.calls == 0U) {
      continue;
    }
    const auto spread = record.distribution_us();
    systems.append(
        QJsonObject{{QStringLiteral("name"), QString::fromStdString(record.name)},
                    {QStringLiteral("calls"), static_cast<qint64>(record.calls)},
                    {QStringLiteral("average_us"), record.average_us()},
                    {QStringLiteral("p50_us"), spread.p50},
                    {QStringLiteral("p95_us"), spread.p95},
                    {QStringLiteral("p99_us"), spread.p99},
                    {QStringLiteral("peak_us"), static_cast<qint64>(record.peak_us)}});
  }
  const auto tick = profiler.tick_time_us();
  return QJsonObject{
      {QStringLiteral("ticks"), static_cast<qint64>(profiler.ticks_recorded())},
      {QStringLiteral("tick_us"),
       QJsonObject{{QStringLiteral("average_us"), tick.average},
                   {QStringLiteral("p50_us"), tick.p50},
                   {QStringLiteral("p95_us"), tick.p95},
                   {QStringLiteral("p99_us"), tick.p99},
                   {QStringLiteral("max_us"), tick.maximum}}},
      {QStringLiteral("systems"), systems}};
}

auto budget_verdict_json(const PerformanceBudget& budget,
                         const PerformanceMeasurement& measured) -> QJsonObject {
  QJsonObject checks;
  QJsonArray failures;

  check(checks,
        failures,
        QStringLiteral("frame_p50_ms"),
        measured.frame_p50_ms,
        budget.frame_p50_ms);
  check(checks,
        failures,
        QStringLiteral("frame_p95_ms"),
        measured.frame_p95_ms,
        budget.frame_p95_ms);
  check(checks,
        failures,
        QStringLiteral("frame_p99_ms"),
        measured.frame_p99_ms,
        budget.frame_p99_ms);
  check(checks,
        failures,
        QStringLiteral("frame_max_ms"),
        measured.frame_max_ms,
        budget.frame_max_ms);
  check(checks,
        failures,
        QStringLiteral("update_average_ms"),
        measured.update_average_ms,
        budget.update_average_ms);
  check(checks,
        failures,
        QStringLiteral("update_p95_ms"),
        measured.update_p95_ms,
        budget.update_p95_ms);
  check(checks,
        failures,
        QStringLiteral("render_submit_p95_ms"),
        measured.render_submit_p95_ms,
        budget.render_submit_p95_ms);

  const Engine::Core::NavProfile& nav = Engine::Core::nav_profile();
  if (nav.ticks() > 0U) {
    const auto nav_time = nav.tick_time_ms();
    check(checks,
          failures,
          QStringLiteral("navigation_average_ms"),
          nav_time.average,
          budget.navigation_average_ms);
    check(checks,
          failures,
          QStringLiteral("navigation_p95_ms"),
          nav_time.p95,
          budget.navigation_p95_ms);
  }

  if (measured.gpu_timed) {
    check(checks,
          failures,
          QStringLiteral("gpu_total_p95_ms"),
          measured.gpu_shadow_p95_ms + measured.gpu_color_p95_ms,
          budget.gpu_total_p95_ms);
  } else {
    failures.append(QStringLiteral(
        "gpu timing was unavailable; the run cannot claim a GPU budget"));
  }

  const AssetCounters& counters = asset_counters();
  const bool barrier = counters.load_barrier_marked();
  const std::uint64_t post_load_work =
      barrier ? counters.post_barrier_asset_work() : 0U;
  checks.insert(
      QStringLiteral("post_load_asset_work"),
      QJsonObject{{QStringLiteral("measured"), static_cast<qint64>(post_load_work)},
                  {QStringLiteral("budget"), 0},
                  {QStringLiteral("passed"), barrier && post_load_work == 0U}});
  if (!barrier) {
    failures.append(QStringLiteral(
        "the load barrier was never reached; post-load asset work is unmeasured"));
  } else if (post_load_work > 0U) {
    failures.append(QStringLiteral("post-load asset work occurred: %1")
                        .arg(QString::fromStdString(format_post_barrier_violations())));
  }

  if (!measured.ultra_preset) {
    failures.append(QStringLiteral("the run was not on the Ultra graphics preset"));
  }
  if (!measured.full_creature_lod) {
    failures.append(QStringLiteral("creature LOD was not forced to Full"));
  }

  return QJsonObject{{QStringLiteral("passed"), failures.isEmpty()},
                     {QStringLiteral("frames"), static_cast<qint64>(measured.frames)},
                     {QStringLiteral("checks"), checks},
                     {QStringLiteral("failures"), failures}};
}

} // namespace Render::Profiling
