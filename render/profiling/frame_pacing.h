#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include <array>
#include <cmath>
#include <vector>

#include "frame_profile.h"

namespace Render::Profiling {

struct PacingSample {
  double interval_ms{0};
  double cpu_ms{0};
  double gpu_ms{0};
  double upload_bytes{0};
  std::array<std::uint64_t, static_cast<std::size_t>(Phase::_Count)> phase_us{};
  std::uint64_t asset_work{0};
  double render_elapsed_ms{0};
};

struct PacingBudget {
  double cpu_p95_ms{12};
  double gpu_p95_ms{12};
  double upload_max_bytes{8 * 1024 * 1024};
  double interval_p95_ms{18};
  double interval_p99_ms{25};
  double hitch_ms{33.34};
  double spike_max_ms{50};
  double hitches_per_minute{2};
  std::size_t max_cluster_frames{1};
};

inline auto pacing_budget(const QString& preset) -> PacingBudget {
  PacingBudget budget;
  if (preset == QStringLiteral("low")) {
    budget.cpu_p95_ms = 10;
    budget.gpu_p95_ms = 10;
    budget.upload_max_bytes = 2 * 1024 * 1024;
  } else if (preset == QStringLiteral("medium")) {
    budget.upload_max_bytes = 4 * 1024 * 1024;
  } else if (preset == QStringLiteral("ultra")) {
    budget.cpu_p95_ms = 16.67;
  }
  return budget;
}

class FramePacing {
public:
  void reserve(std::size_t frames) { m_samples.reserve(frames); }
  void reset() { m_samples.clear(); }
  void observe(const PacingSample& sample) { m_samples.push_back(sample); }

  [[nodiscard]] auto report(const QString& preset) const -> QJsonObject {
    const auto budget = pacing_budget(preset);
    std::vector<double> intervals, cpu, gpu;
    QJsonArray failures, clusters;
    double elapsed_ms = 0, upload_max = 0;
    std::uint64_t asset_work = 0;
    std::size_t hitches = 0, longest = 0, cluster_size = 0, cluster_start = 0;
    std::size_t gpu_timed_frames = 0;
    double cluster_max = 0;
    QJsonObject worst_evidence;
    auto finish_cluster = [&] {
      if (cluster_size == 0) {
        return;
      }
      longest = std::max(longest, cluster_size);
      clusters.append(QJsonObject{{"first_frame", static_cast<qint64>(cluster_start)},
                                  {"frames", static_cast<qint64>(cluster_size)},
                                  {"max_ms", cluster_max},
                                  {"worst_frame_evidence", worst_evidence}});
      cluster_size = 0;
      cluster_max = 0;
    };
    for (std::size_t i = 0; i < m_samples.size(); ++i) {
      const auto& sample = m_samples[i];
      if (!std::isfinite(sample.interval_ms) || sample.interval_ms <= 0 ||
          !std::isfinite(sample.cpu_ms) || sample.cpu_ms < 0 ||
          !std::isfinite(sample.gpu_ms) || sample.gpu_ms < 0 ||
          !std::isfinite(sample.upload_bytes) || sample.upload_bytes < 0 ||
          !std::isfinite(sample.render_elapsed_ms) || sample.render_elapsed_ms < 0) {
        failures.append(QStringLiteral("invalid frame sample"));
        finish_cluster();
        continue;
      }
      intervals.push_back(sample.interval_ms);
      cpu.push_back(sample.cpu_ms);
      gpu.push_back(sample.gpu_ms);
      gpu_timed_frames += sample.gpu_ms > 0 ? 1 : 0;
      elapsed_ms += sample.interval_ms;
      upload_max = std::max(upload_max, sample.upload_bytes);
      asset_work += sample.asset_work;
      if (sample.interval_ms > budget.hitch_ms) {
        ++hitches;
        if (cluster_size++ == 0) {
          cluster_start = i;
        }
        if (sample.interval_ms > cluster_max) {
          cluster_max = sample.interval_ms;
          QJsonObject phases;
          std::size_t dominant = static_cast<std::size_t>(Phase::Frame);
          for (std::size_t p = 0; p < sample.phase_us.size(); ++p) {

            if (p == static_cast<std::size_t>(Phase::Simulation) ||
                p == static_cast<std::size_t>(Phase::Present)) {
              continue;
            }
            phases.insert(QString::fromLatin1(phase_name(static_cast<Phase>(p))),
                          static_cast<double>(sample.phase_us[p]) / 1000.0);
            if (sample.phase_us[p] > sample.phase_us[dominant]) {
              dominant = p;
            }
          }
          worst_evidence = QJsonObject{
              {"frame", static_cast<qint64>(i)},
              {"cpu_ms", sample.cpu_ms},
              {"render_elapsed_ms", sample.render_elapsed_ms},
              {"gpu_ms_delayed", sample.gpu_ms},
              {"upload_bytes", sample.upload_bytes},
              {"asset_work", static_cast<qint64>(sample.asset_work)},
              {"phase_ms", phases},
              {"largest_cpu_phase",
               sample.phase_us[dominant] == 0
                   ? QStringLiteral("unattributed")
                   : QString::fromLatin1(phase_name(static_cast<Phase>(dominant)))}};
        }
      } else {
        finish_cluster();
      }
    }
    finish_cluster();
    const auto spread = Utils::Stats::distribution_of(intervals);
    QJsonObject checks;
    auto check = [&](const QString& name, double value, double limit) {
      checks.insert(name,
                    QJsonObject{{"measured", value},
                                {"budget", limit},
                                {"passed", value <= limit}});
      if (value > limit) {
        failures.append(name + QStringLiteral(" exceeded budget"));
      }
    };
    check("interval_p95_ms", spread.p95, budget.interval_p95_ms);
    check("interval_p99_ms", spread.p99, budget.interval_p99_ms);
    check("interval_max_ms", spread.maximum, budget.spike_max_ms);
    check("cpu_p95_ms", Utils::Stats::distribution_of(cpu).p95, budget.cpu_p95_ms);
    check("gpu_p95_ms", Utils::Stats::distribution_of(gpu).p95, budget.gpu_p95_ms);
    check("upload_max_bytes", upload_max, budget.upload_max_bytes);
    check("post_playable_asset_work", static_cast<double>(asset_work), 0);
    check("hitches_per_minute",
          elapsed_ms > 0 ? hitches * 60000.0 / elapsed_ms : 0,
          budget.hitches_per_minute);
    check("longest_cluster_frames",
          static_cast<double>(longest),
          static_cast<double>(budget.max_cluster_frames));
    if (intervals.size() < 120 || elapsed_ms < 30000) {
      failures.append(QStringLiteral("requires at least 120 frames and 30 seconds"));
    }
    check("missing_gpu_sample_fraction",
          intervals.empty() ? 1.0
                            : 1.0 - static_cast<double>(gpu_timed_frames) /
                                        static_cast<double>(intervals.size()),
          0.1);
    if (preset != "low" && preset != "medium" && preset != "high" &&
        preset != "ultra") {
      failures.append(QStringLiteral("unknown graphics preset"));
    }
    return QJsonObject{{"passed", failures.isEmpty()},
                       {"preset", preset},
                       {"frames", static_cast<qint64>(intervals.size())},
                       {"elapsed_seconds", elapsed_ms / 1000.0},
                       {"interval_p50_ms", spread.p50},
                       {"hitch_threshold_ms", budget.hitch_ms},
                       {"hitch_frames", static_cast<qint64>(hitches)},
                       {"clusters", clusters},
                       {"checks", checks},
                       {"failures", failures}};
  }

private:
  std::vector<PacingSample> m_samples;
};

} // namespace Render::Profiling
