#include "promo_runner.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "arena_scenario.h"
#include "arena_viewport.h"
#include "render/humanoid/render_stats.h"
#include "video_encoder.h"

namespace Arena::Promo {
namespace {

constexpr float k_scenario_tail_seconds = 2.0F;
constexpr int k_pass_watchdog_ms = 1'800'000;
constexpr int k_pass_warmup_frames = 3;
constexpr int k_first_frame_min_peak = 8;
constexpr int k_max_black_frames_skipped = 45;

auto brightest_sample(const QImage& frame) -> int {
  if (frame.isNull()) {
    return 0;
  }
  const QImage probe =
      frame.scaled(64, 64, Qt::IgnoreAspectRatio, Qt::FastTransformation)
          .convertToFormat(QImage::Format_Grayscale8);
  int peak = 0;
  for (int y = 0; y < probe.height(); ++y) {
    const uchar* line = probe.constScanLine(y);
    for (int x = 0; x < probe.width(); ++x) {
      peak = std::max(peak, int(line[x]));
    }
  }
  return peak;
}

struct ShotResult {
  QString name;
  QString scenario;
  QString clip_path;
  QString poster_path;
  int frames{0};
  float scene_duration{0.0F};
  float clip_duration{0.0F};
};

class Recorder {
public:
  Recorder(ArenaViewport& viewport, const Spec& spec, RunOptions options)
      : m_viewport(viewport)
      , m_spec(spec)
      , m_options(std::move(options))
      , m_passes(plan_passes(spec))
      , m_results(spec.shots.size()) {}

  auto start(QString* error) -> bool {
    if (!VideoEncoder::ffmpeg_available()) {
      if (error != nullptr) {
        *error = QStringLiteral("ffmpeg is required for --promo-spec but was not "
                                "found on PATH");
      }
      return false;
    }
    if (!QDir().mkpath(m_options.output_directory)) {
      if (error != nullptr) {
        *error = QStringLiteral("could not create promo output directory %1")
                     .arg(m_options.output_directory);
      }
      return false;
    }

    m_viewport.set_promo_mode(true);
    m_viewport.set_capture_resolution(m_spec.width * m_spec.supersample,
                                      m_spec.height * m_spec.supersample);
    m_viewport.set_capture_sink([this](const QImage& frame) { on_frame(frame); });
    m_viewport.set_frame_hook([this](float scenario_time) { on_tick(scenario_time); });
    QTimer::singleShot(250, [this]() { begin_next_pass(); });
    return true;
  }

  [[nodiscard]] auto failed() const noexcept -> bool { return m_failed; }

private:
  [[nodiscard]] auto current_shot() const -> const Shot& {
    return m_spec.shots[m_passes[m_pass_index].shots[m_slot_index]];
  }

  [[nodiscard]] auto current_shot_index() const -> std::size_t {
    return m_passes[m_pass_index].shots[m_slot_index];
  }

  void begin_next_pass() {
    if (m_pass_index >= m_passes.size()) {
      finish_run();
      return;
    }
    const CapturePass& pass = m_passes[m_pass_index];
    if (Arena::Scenarios::find_definition(pass.scenario) == nullptr) {
      qCritical().noquote() << QStringLiteral("Promo pass names unknown scenario '%1'")
                                   .arg(pass.scenario);
      m_failed = true;
      ++m_pass_index;
      QTimer::singleShot(0, [this]() { begin_next_pass(); });
      return;
    }

    float last_end = 0.0F;
    for (std::size_t index : pass.shots) {
      const Shot& shot = m_spec.shots[index];
      last_end = std::max(last_end, shot.start_seconds + shot.duration_seconds);
    }

    m_slot_index = 0;
    m_pass_active = true;
    m_pass_frames = 0;
    m_viewport.set_terrain_seed(pass.seed);
    m_viewport.set_batch_fixed_step(idle_step());
    m_viewport.set_scenario_duration_override(last_end + k_scenario_tail_seconds);
    m_viewport.set_capture_active(false);
    m_viewport.clear_cinematic_view();
    m_viewport.load_scenario(pass.scenario);

    qInfo().noquote() << QStringLiteral("Promo pass %1/%2: %3, %4 shot(s) across "
                                        "%5 s of scenario")
                             .arg(m_pass_index + 1U)
                             .arg(m_passes.size())
                             .arg(pass.scenario)
                             .arg(pass.shots.size())
                             .arg(QString::number(last_end, 'f', 1));

    const std::size_t guarded_pass = m_pass_index;
    QTimer::singleShot(k_pass_watchdog_ms, [this, guarded_pass]() {
      if (m_pass_active && m_pass_index == guarded_pass) {
        qCritical().noquote() << QStringLiteral("Promo pass over scenario '%1' "
                                                "exceeded its watchdog")
                                     .arg(m_passes[guarded_pass].scenario);
        m_failed = true;
        end_shot();
        end_pass();
      }
    });
    begin_shot();
  }

  [[nodiscard]] auto idle_step() const -> float {
    return 1.0F / static_cast<float>(m_spec.fps);
  }

  void begin_shot() {
    if (!m_pass_active) {
      return;
    }
    if (m_slot_index >= m_passes[m_pass_index].shots.size()) {
      end_pass();
      return;
    }
    const Shot& shot = current_shot();
    m_clip_path =
        QDir(m_options.output_directory)
            .filePath(QStringLiteral("%1_%2.mp4")
                          .arg(current_shot_index() + 1U, 2, 10, QLatin1Char('0'))
                          .arg(shot.name));
    QString encoder_error;
    m_encoder = std::make_unique<VideoEncoder>();
    if (!m_encoder->open(
            m_clip_path, m_spec.width, m_spec.height, m_spec.fps, &encoder_error)) {
      qCritical().noquote()
          << QStringLiteral("Promo shot '%1': %2").arg(shot.name, encoder_error);
      m_failed = true;
      m_encoder.reset();
      ++m_slot_index;
      begin_shot();
      return;
    }

    m_step_seconds = 1.0F / (static_cast<float>(m_spec.fps) * shot.slow_motion);
    m_target_frames = std::max(
        1, static_cast<int>(std::lround(shot.duration_seconds / m_step_seconds)));
    m_frames_written = 0;
    m_black_frames_skipped = 0;
    m_focus_valid = false;
    m_logged_framing = false;
    m_shot_active = true;
    m_shot_armed = false;
    m_last_frame.reset();
    m_viewport.set_batch_fixed_step(idle_step());

    qInfo().noquote() << QStringLiteral("  shot %1: %2 (%3 frames at %4x%5, from "
                                        "%6 s)")
                             .arg(current_shot_index() + 1U)
                             .arg(shot.name)
                             .arg(m_target_frames)
                             .arg(m_spec.width)
                             .arg(m_spec.height)
                             .arg(QString::number(shot.start_seconds, 'f', 1));
  }

  void on_tick(float scenario_time) {
    ++m_pass_frames;
    if (!m_shot_active) {
      return;
    }
    const Shot& shot = current_shot();
    const float shot_time = std::max(0.0F, scenario_time - shot.start_seconds);

    QVector3D target;
    if (shot.gameplay_camera) {

      m_viewport.clear_cinematic_view();
    } else {
      const QVector3D focus = resolve_focus(shot);
      Pose pose = evaluate(shot.keys, shot_time);
      target = focus + shot.focus.offset + QVector3D(0.0F, pose.height, 0.0F);
      if (shot.shake > 0.0F) {
        target += shake_offset(m_frames_written, shot.shake);
      }
      m_viewport.set_cinematic_view(
          target, pose.distance, pose.pitch, pose.yaw, pose.fov, pose.roll);
    }

    const bool in_window =
        scenario_time >= shot.start_seconds && m_pass_frames > k_pass_warmup_frames;
    if (in_window && !m_shot_armed) {
      m_shot_armed = true;
      m_viewport.set_batch_fixed_step(m_step_seconds);
      m_viewport.set_capture_active(false);
      return;
    }
    const bool recording = in_window && m_frames_written < m_target_frames;
    m_viewport.set_capture_active(recording);

    if (recording && !m_logged_framing) {
      m_logged_framing = true;
      const auto& stats = Render::GL::get_humanoid_render_stats();
      qInfo().noquote() << QStringLiteral(
                               "  focus (%1, %2, %3) %4; soldiers %5/%6 drawn, culled "
                               "frustum %7 fog %8 lod %9 temporal %10")
                               .arg(QString::number(target.x(), 'f', 1),
                                    QString::number(target.y(), 'f', 1),
                                    QString::number(target.z(), 'f', 1),
                                    m_focus_valid ? QStringLiteral("tracked")
                                                  : QStringLiteral("UNRESOLVED"))
                               .arg(stats.soldiers_rendered)
                               .arg(stats.soldiers_total)
                               .arg(stats.soldiers_skipped_frustum)
                               .arg(stats.soldiers_skipped_fog)
                               .arg(stats.soldiers_skipped_lod)
                               .arg(stats.soldiers_skipped_temporal);
    }

    if (!recording && m_frames_written >= m_target_frames) {
      end_shot();
      advance_within_pass();
      return;
    }

    if (m_viewport.active_scenario_finished() && m_frames_written < m_target_frames) {
      qWarning().noquote() << QStringLiteral(
                                  "Promo shot '%1' ended early with %2 of %3 frames")
                                  .arg(shot.name)
                                  .arg(m_frames_written)
                                  .arg(m_target_frames);
      end_shot();
      end_pass();
    }
  }

  void on_frame(const QImage& frame) {
    if (!m_shot_active || m_encoder == nullptr) {
      return;
    }
    QImage output = frame;
    if (m_spec.supersample > 1) {
      output = frame.scaled(
          m_spec.width, m_spec.height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    if (m_frames_written == 0) {
      const int peak = brightest_sample(output);
      if (peak < k_first_frame_min_peak) {
        if (m_black_frames_skipped < k_max_black_frames_skipped) {
          ++m_black_frames_skipped;
          return;
        }
        qWarning().noquote() << QStringLiteral(
                                    "Promo shot '%1' is still black after %2 frames; "
                                    "recording it anyway")
                                    .arg(current_shot().name)
                                    .arg(m_black_frames_skipped);
      } else if (m_black_frames_skipped > 0) {
        qInfo().noquote() << QStringLiteral("  skipped %1 black lead-in frame(s)")
                                 .arg(m_black_frames_skipped);
      }
    }

    QString error;
    if (!m_encoder->write_frame(output, &error)) {
      qCritical().noquote() << QStringLiteral("Promo encode failed: %1").arg(error);
      m_failed = true;
      end_shot();
      end_pass();
      return;
    }
    ++m_frames_written;
    m_last_frame = output;
  }

  auto resolve_focus(const Shot& shot) -> QVector3D {
    std::optional<QVector3D> raw;
    switch (shot.focus.mode) {
    case FocusMode::Point:
      raw = shot.focus.point;
      break;
    case FocusMode::Group:
      raw = m_viewport.scenario_group_center(shot.focus.group);
      break;
    case FocusMode::GroupPair: {
      const auto first = m_viewport.scenario_group_center(shot.focus.group);
      const auto second = m_viewport.scenario_group_center(shot.focus.second_group);
      if (first.has_value() && second.has_value()) {
        raw = (*first + *second) * 0.5F;
      } else if (first.has_value()) {
        raw = first;
      } else {
        raw = second;
      }
      break;
    }
    case FocusMode::AllUnits:
      raw = m_viewport.scenario_center_of_mass();
      break;
    }

    if (!raw.has_value()) {
      return m_focus_valid ? m_smoothed_focus : QVector3D{};
    }
    if (!m_focus_valid || shot.focus.smoothing <= 0.0F) {
      m_smoothed_focus = *raw;
      m_focus_valid = true;
      return m_smoothed_focus;
    }
    const float blend =
        1.0F - std::exp(-m_step_seconds / std::max(0.01F, shot.focus.smoothing));
    m_smoothed_focus += (*raw - m_smoothed_focus) * blend;
    return m_smoothed_focus;
  }

  void end_shot() {
    if (!m_shot_active) {
      return;
    }
    m_shot_active = false;
    m_viewport.set_capture_active(false);
    m_viewport.set_batch_fixed_step(idle_step());

    const std::size_t shot_index = current_shot_index();
    const Shot& shot = m_spec.shots[shot_index];
    QString error;
    if (m_encoder != nullptr && !m_encoder->close(&error)) {
      qCritical().noquote()
          << QStringLiteral("Promo shot '%1': %2").arg(shot.name, error);
      m_failed = true;
    }
    m_encoder.reset();

    ShotResult result;
    result.name = shot.name;
    result.scenario = shot.scenario;
    result.clip_path = m_clip_path;
    result.frames = m_frames_written;
    result.scene_duration = static_cast<float>(m_frames_written) * m_step_seconds;
    result.clip_duration =
        static_cast<float>(m_frames_written) / static_cast<float>(m_spec.fps);
    if (m_options.write_posters && m_last_frame.has_value()) {
      result.poster_path =
          QDir(m_options.output_directory)
              .filePath(QStringLiteral("%1_%2.png")
                            .arg(shot_index + 1U, 2, 10, QLatin1Char('0'))
                            .arg(shot.name));
      m_last_frame->save(result.poster_path);
    }
    m_results[shot_index] = result;

    qInfo().noquote() << QStringLiteral("  wrote %1 (%2 frames, %3 s)")
                             .arg(QFileInfo(m_clip_path).fileName())
                             .arg(m_frames_written)
                             .arg(QString::number(result.clip_duration, 'f', 2));
  }

  void advance_within_pass() {
    ++m_slot_index;
    begin_shot();
  }

  void end_pass() {
    if (!m_pass_active) {
      return;
    }
    m_pass_active = false;
    ++m_pass_index;
    QTimer::singleShot(0, [this]() { begin_next_pass(); });
  }

  void finish_run() {
    m_viewport.set_capture_sink(nullptr);
    m_viewport.set_frame_hook(nullptr);
    write_manifest();
    qInfo().noquote() << QStringLiteral("Promo capture complete: %1 shot(s) in %2")
                             .arg(recorded_count())
                             .arg(QDir(m_options.output_directory).absolutePath());
    QApplication::exit(m_failed ? 1 : 0);
  }

  [[nodiscard]] auto recorded_count() const -> std::size_t {
    return static_cast<std::size_t>(
        std::count_if(m_results.begin(), m_results.end(), [](const ShotResult& result) {
          return result.frames > 0;
        }));
  }

  void write_manifest() {
    QJsonArray shots;
    for (const ShotResult& result : m_results) {
      if (result.frames <= 0) {
        continue;
      }
      shots.append(QJsonObject{
          {QStringLiteral("name"), result.name},
          {QStringLiteral("scenario"), result.scenario},
          {QStringLiteral("clip"), QFileInfo(result.clip_path).fileName()},
          {QStringLiteral("poster"),
           result.poster_path.isEmpty() ? QString{}
                                        : QFileInfo(result.poster_path).fileName()},
          {QStringLiteral("frames"), result.frames},
          {QStringLiteral("scene_seconds"), result.scene_duration},
          {QStringLiteral("clip_seconds"), result.clip_duration}});
    }
    const QJsonObject manifest{{QStringLiteral("id"), m_spec.id},
                               {QStringLiteral("title"), m_spec.title},
                               {QStringLiteral("width"), m_spec.width},
                               {QStringLiteral("height"), m_spec.height},
                               {QStringLiteral("fps"), m_spec.fps},
                               {QStringLiteral("shots"), shots}};
    QFile file(QDir(m_options.output_directory).filePath(QStringLiteral("shots.json")));
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      file.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented));
    }
  }

  ArenaViewport& m_viewport;
  const Spec& m_spec;
  RunOptions m_options;
  std::vector<CapturePass> m_passes;
  std::unique_ptr<VideoEncoder> m_encoder;
  std::vector<ShotResult> m_results;
  std::optional<QImage> m_last_frame;
  QString m_clip_path;
  QVector3D m_smoothed_focus;
  std::size_t m_pass_index{0};
  std::size_t m_slot_index{0};
  int m_target_frames{0};
  int m_frames_written{0};
  int m_pass_frames{0};
  int m_black_frames_skipped{0};
  float m_step_seconds{1.0F / 60.0F};
  bool m_pass_active{false};
  bool m_shot_active{false};
  bool m_shot_armed{false};
  bool m_focus_valid{false};
  bool m_logged_framing{false};
  bool m_failed{false};
};

} // namespace

auto run(ArenaViewport& viewport,
         const Spec& spec,
         const RunOptions& options,
         QString* error) -> int {
  Recorder recorder(viewport, spec, options);
  if (!recorder.start(error)) {
    return 2;
  }
  return QApplication::exec();
}

} // namespace Arena::Promo
