#include "gl_view.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QOpenGLContext>
#include <QOpenGLDebugLogger>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QOpenGLFunctions>
#include <QQuickWindow>
#include <QStringList>
#include <QSurfaceFormat>
#include <qglobal.h>
#include <qobject.h>
#include <qopenglcontext.h>
#include <qopenglframebufferobject.h>
#include <qpointer.h>
#include <qquickframebufferobject.h>
#include <qtmetamacros.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <deque>
#include <exception>
#include <numeric>
#include <unordered_map>
#include <utility>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "../render/draw_cmd_traits.h"
#include "../render/graphics_settings.h"
#include "../render/i_render_backend.h"
#include "../render/profiling/combat_animation_diagnostics.h"
#include "../render/profiling/frame_continuity_analyzer.h"
#include "../render/profiling/frame_profile.h"
#include "app/core/game_engine.h"
#include "commander_portrait_scenes.h"
#include "game/core/nav_profile.h"
#include "render/profiling/allocation_tracker.h"
#include "render/profiling/asset_counters.h"
#include "render/profiling/performance_report.h"
#include "utils/percentile.h"

namespace {
constexpr double k_runtime_benchmark_warmup_seconds = 2.0;
constexpr std::size_t k_runtime_benchmark_min_frames = 30;
constexpr std::uint64_t k_visibility_churn_window_frames = 120U;
constexpr std::uint32_t k_visibility_churn_threshold = 4U;

auto average_ms(const std::vector<double>& samples) -> double {
  return Utils::Stats::mean(samples);
}

auto distribution_json(const std::vector<double>& samples) -> QJsonObject {
  const Utils::Stats::Distribution spread = Utils::Stats::distribution_of(samples);
  return QJsonObject{{QStringLiteral("average_ms"), spread.average},
                     {QStringLiteral("p50_ms"), spread.p50},
                     {QStringLiteral("p95_ms"), spread.p95},
                     {QStringLiteral("p99_ms"), spread.p99},
                     {QStringLiteral("max_ms"), spread.maximum}};
}
} // namespace

struct GLView::GLRenderer::RuntimeContinuityProbe {
  struct CapturedFrame {
    std::uint64_t index{0U};
    QImage image;
  };

  struct SoldierState {
    Render::Profiling::SoldierCullReason reason{
        Render::Profiling::SoldierCullReason::None};
    std::uint64_t last_seen_frame{0U};
    std::uint64_t window_start_frame{0U};
    std::uint32_t transitions_in_window{0U};
    bool churn_reported{false};
  };

  Render::Profiling::FrameContinuityAnalyzer framebuffer_analyzer;
  std::deque<CapturedFrame> recent_frames;
  std::unordered_map<std::uint64_t, SoldierState> soldiers;
  QStringList issues;
  std::uint64_t frame_index{0U};
  std::uint64_t soldier_visibility_transitions{0U};
  std::uint64_t soldier_visibility_churn{0U};
  std::uint64_t ultra_lod_culls{0U};
};

GLView::GLView() {
  setMirrorVertically(true);
}

auto GLView::createRenderer() const -> QQuickFramebufferObject::Renderer* {

  QOpenGLContext* ctx = QOpenGLContext::currentContext();
  if ((ctx == nullptr) || !ctx->isValid()) {
    qCritical() << "GLView::createRenderer() - No valid OpenGL context";
    qCritical() << "Running in software rendering mode - 3D view not available";
    return nullptr;
  }

  const auto fmt = ctx->format();
  const auto version = fmt.version();
  if (version.first < 3 || (version.first == 3 && version.second < 3)) {
    qWarning() << "GLView::createRenderer() - OpenGL" << version.first << "."
               << version.second
               << "detected; at least 3.3 required. Falling back to "
                  "ShaderQuality::None (software backend). Launch with "
                  "--force-software to silence this warning.";
    Render::GraphicsSettings::instance().set_backend_kind(Render::ShaderQuality::None);
  } else {
    qInfo() << "GLView::createRenderer() - OpenGL" << version.first << "."
            << version.second << "context OK";
  }

  return new GLRenderer(const_cast<GLView*>(this), m_engine);
}

auto GLView::engine() const -> QObject* {
  return m_engine;
}

void GLView::set_engine(QObject* eng) {
  if (m_engine == eng) {
    return;
  }
  m_engine = qobject_cast<GameEngine*>(eng);
  emit engine_changed();
  update();
}

void GLView::notify_renderer_ready() {
  if (m_renderer_ready) {
    return;
  }
  m_renderer_ready = true;
  qInfo() << "GLView: gameplay renderer produced its first frame";
  emit renderer_ready();
}

namespace {

struct FrameGate {
  GameEngine* engine = nullptr;

  ~FrameGate() {
    if (engine != nullptr) {
      engine->end_render_frame();
    }
  }
};

auto render_thread_cpu_ms() -> double {
#if defined(_WIN32)
  FILETIME creation{};
  FILETIME exit{};
  FILETIME kernel{};
  FILETIME user{};
  if (GetThreadTimes(GetCurrentThread(), &creation, &exit, &kernel, &user) == 0) {
    return 0.0;
  }
  auto hundred_ns = [](const FILETIME& value) -> std::uint64_t {
    return (static_cast<std::uint64_t>(value.dwHighDateTime) << 32U) |
           static_cast<std::uint64_t>(value.dwLowDateTime);
  };
  return static_cast<double>(hundred_ns(kernel) + hundred_ns(user)) / 1.0e4;
#else
  timespec now{};
  if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &now) != 0) {
    return 0.0;
  }
  return static_cast<double>(now.tv_sec) * 1000.0 +
         static_cast<double>(now.tv_nsec) / 1.0e6;
#endif
}

} // namespace

GLView::GLRenderer::GLRenderer(QPointer<GLView> view, QPointer<GameEngine> engine)
    : m_view(std::move(view))
    , m_engine(std::move(engine)) {
  bool valid = false;
  m_benchmark_seconds =
      qEnvironmentVariable("SOI_RUNTIME_BENCHMARK_SECONDS").toDouble(&valid);
  if (!valid || m_benchmark_seconds <= 0.0) {
    m_benchmark_seconds = 0.0;
  }
  m_benchmark_output = qEnvironmentVariable("SOI_RUNTIME_BENCHMARK_OUTPUT");
  if (m_benchmark_seconds > 0.0) {
    Render::Profiling::global_profile().enabled = true;
    Engine::Core::nav_profile().set_enabled(true);
    m_benchmark_created_time = std::chrono::steady_clock::now();
  }
  UI::CommanderPortraitScenes::instance().add_reference();
  if (qEnvironmentVariableIntValue("SOI_RUNTIME_CONTINUITY") != 0) {
    m_continuity_probe = std::make_unique<RuntimeContinuityProbe>();
    Render::Profiling::CombatAnimationDiagnostics::instance().set_enabled(true);
  }
}

GLView::GLRenderer::~GLRenderer() {
  if (m_engine != nullptr) {
    m_engine->stop_simulation_thread();
    m_engine->cleanup_opengl_resources();
  }
  if (m_continuity_probe != nullptr) {
    Render::Profiling::CombatAnimationDiagnostics::instance().set_enabled(false);
  }
  UI::CommanderPortraitScenes::instance().release_reference();
}

void GLView::GLRenderer::render() {
  if (m_engine == nullptr) {
    qWarning() << "GLRenderer::render() - engine is null";
    return;
  }

  QOpenGLContext* ctx = QOpenGLContext::currentContext();
  if ((ctx == nullptr) || !ctx->isValid()) {
    qCritical() << "GLRenderer::render() - OpenGL context lost";
    return;
  }

  try {
    if (!m_gl_debug_checked) {
      m_gl_debug_checked = true;
      if (qEnvironmentVariableIntValue("SOI_GL_DEBUG") != 0 &&
          ctx->hasExtension(QByteArrayLiteral("GL_KHR_debug"))) {
        m_gl_debug_logger = std::make_unique<QOpenGLDebugLogger>();
        if (m_gl_debug_logger->initialize()) {
          QObject::connect(
              m_gl_debug_logger.get(),
              &QOpenGLDebugLogger::messageLogged,
              m_gl_debug_logger.get(),
              [](const QOpenGLDebugMessage& message) {
                static QHash<GLuint, int> seen;
                const int count = ++seen[message.id()];
                if (count <= 3 || (count % 1000) == 0) {
                  qWarning().noquote() << "GLDEBUG#" << message.id() << "x" << count
                                       << message.message();
                }
              },
              Qt::DirectConnection);
          m_gl_debug_logger->startLogging(QOpenGLDebugLogger::SynchronousLogging);
          qInfo() << "GLDEBUG logging started";
        } else {
          qWarning() << "GLDEBUG: logger failed to initialize";
          m_gl_debug_logger.reset();
        }
      }
    }

    if (!m_engine->try_begin_render_frame()) {

      update();
      return;
    }
    const FrameGate frame_gate{m_engine};

    m_engine->ensure_initialized();
    if (!m_engine->renderer_initialized()) {
      qCritical() << "GLRenderer::render() - gameplay renderer initialization failed";
      return;
    }
    if (!m_engine->simulation_thread_running()) {
      m_engine->start_simulation_thread();
    }

    auto const frame_work_start = std::chrono::steady_clock::now();
    const double thread_cpu_start_ms = render_thread_cpu_ms();

    auto& profile = Render::Profiling::global_profile();
    profile.begin_frame();
    if (m_last_render_end.time_since_epoch().count() != 0) {
      profile.add_phase_us(Render::Profiling::Phase::Present,
                           static_cast<std::uint64_t>(
                               std::chrono::duration_cast<std::chrono::microseconds>(
                                   frame_work_start - m_last_render_end)
                                   .count()));
    }

    const std::uint64_t simulation_us = m_engine->take_simulation_tick_us();
    profile.add_phase_us(Render::Profiling::Phase::Simulation, simulation_us);

    float dt = 1.0F / 60.0F;
    if (m_last_frame_time.time_since_epoch().count() != 0) {
      dt = std::chrono::duration<float>(frame_work_start - m_last_frame_time).count();
      dt = std::min(dt, 0.1F);
    }
    m_last_frame_time = frame_work_start;
    {
      Render::Profiling::PhaseScope const frame_scope(&profile,
                                                      Render::Profiling::Phase::Frame);
      m_engine->update_presentation(dt);
    }
    m_engine->render(m_size.width(), m_size.height());
    auto const render_end = std::chrono::steady_clock::now();
    m_last_render_end = render_end;
    const double thread_cpu_end_ms = render_thread_cpu_ms();

    observe_runtime_continuity();
    observe_runtime_benchmark(
        frame_work_start,
        static_cast<double>(simulation_us) / 1000.0,
        std::chrono::duration<double, std::milli>(render_end - frame_work_start)
            .count(),
        thread_cpu_end_ms - thread_cpu_start_ms);

    if (m_engine->consume_screenshot_request()) {
      if (auto* fbo = framebufferObject()) {
        m_engine->submit_frame_image(fbo->toImage());
      }
    }

    if (!m_ready_reported && m_view != nullptr) {
      m_ready_reported = true;
      QMetaObject::invokeMethod(m_view, "notify_renderer_ready", Qt::QueuedConnection);
    }

    warm_commander_portraits();
  } catch (const std::exception& e) {
    qCritical() << "GLRenderer::render() exception:" << e.what();
    return;
  } catch (...) {
    qCritical() << "GLRenderer::render() unknown exception";
    return;
  }

  update();
}

void GLView::GLRenderer::warm_commander_portraits() {
  if (m_pending_commander_speakers.isEmpty()) {
    return;
  }
  UI::CommanderPortraitScenes::instance().warm(m_pending_commander_speakers);
  m_pending_commander_speakers.clear();
}

void GLView::GLRenderer::observe_runtime_continuity() {
  if (m_continuity_probe == nullptr || m_engine == nullptr || m_engine->is_loading() ||
      !m_engine->match_setup()->is_mission_match() || m_size.isEmpty()) {
    return;
  }

  auto* context = QOpenGLContext::currentContext();
  auto* functions = context != nullptr ? context->functions() : nullptr;
  if (functions == nullptr) {
    return;
  }

  auto& probe = *m_continuity_probe;
  ++probe.frame_index;

  QImage frame(m_size, QImage::Format_RGBA8888);
  GLint previous_pack_alignment = 4;
  functions->glGetIntegerv(GL_PACK_ALIGNMENT, &previous_pack_alignment);
  functions->glPixelStorei(GL_PACK_ALIGNMENT, 1);
  functions->glReadPixels(
      0, 0, m_size.width(), m_size.height(), GL_RGBA, GL_UNSIGNED_BYTE, frame.bits());
  functions->glPixelStorei(GL_PACK_ALIGNMENT, previous_pack_alignment);

  probe.recent_frames.push_back(
      {probe.framebuffer_analyzer.observed_frames() + 1U, frame});
  while (probe.recent_frames.size() > 4U) {
    probe.recent_frames.pop_front();
  }

  if (const auto issue = probe.framebuffer_analyzer.observe(frame); issue.has_value()) {
    probe.issues.push_back(issue->message());
    qWarning().noquote() << "SOI runtime continuity:" << issue->message();

    if (!m_benchmark_output.isEmpty()) {
      const QFileInfo output_info(m_benchmark_output);
      QDir().mkpath(output_info.absolutePath());
      for (const auto& captured : probe.recent_frames) {
        if (captured.index + 1U < issue->bright_frame ||
            captured.index > issue->recovery_frame) {
          continue;
        }
        const QString image_path =
            m_benchmark_output + QStringLiteral(".frame-%1.png").arg(captured.index);
        captured.image.mirrored().save(image_path);
      }
    }
  }

  const auto& diagnostic_units =
      Render::Profiling::CombatAnimationDiagnostics::instance().units();
  for (const auto& [entity_id, unit] : diagnostic_units) {
    for (const auto& soldier : unit.soldiers) {
      if (soldier.visual_state == Render::Profiling::SoldierVisualState::Dying ||
          soldier.visual_state == Render::Profiling::SoldierVisualState::Dead) {
        continue;
      }

      const std::uint64_t key =
          (static_cast<std::uint64_t>(entity_id) << 32U) |
          static_cast<std::uint32_t>(std::max(0, soldier.soldier_index));
      auto [found, inserted] = probe.soldiers.try_emplace(key);
      auto& state = found->second;
      if (inserted) {
        state.reason = soldier.cull_reason;
        state.last_seen_frame = probe.frame_index;
        state.window_start_frame = probe.frame_index;
      } else if (state.last_seen_frame + 1U == probe.frame_index &&
                 state.reason != soldier.cull_reason) {
        const bool visibility_transition =
            (state.reason == Render::Profiling::SoldierCullReason::None) !=
            (soldier.cull_reason == Render::Profiling::SoldierCullReason::None);
        if (visibility_transition) {
          ++probe.soldier_visibility_transitions;
          if (probe.frame_index - state.window_start_frame >
              k_visibility_churn_window_frames) {
            state.window_start_frame = probe.frame_index;
            state.transitions_in_window = 0U;
            state.churn_reported = false;
          }
          ++state.transitions_in_window;
          if (!state.churn_reported &&
              state.transitions_in_window >= k_visibility_churn_threshold) {
            state.churn_reported = true;
            ++probe.soldier_visibility_churn;
            const QString message =
                QStringLiteral("soldier visibility churn: entity=%1 soldier=%2 "
                               "transitions=%3 in %4 frames")
                    .arg(entity_id)
                    .arg(soldier.soldier_index)
                    .arg(state.transitions_in_window)
                    .arg(k_visibility_churn_window_frames);
            probe.issues.push_back(message);
            qWarning().noquote() << "SOI runtime continuity:" << message;
          }
        }
      }

      if (soldier.cull_reason == Render::Profiling::SoldierCullReason::Distance) {
        ++probe.ultra_lod_culls;
      }
      state.reason = soldier.cull_reason;
      state.last_seen_frame = probe.frame_index;
    }
  }
}

void GLView::GLRenderer::reset_runtime_benchmark_samples() {
  m_benchmark_ready_time = {};
  m_benchmark_render_ms.clear();
  m_benchmark_update_ms.clear();
  m_benchmark_thread_cpu_ms.clear();
  m_benchmark_wall_interval_ms.clear();
  m_benchmark_gpu_shadow_ms.clear();
  m_benchmark_gpu_color_ms.clear();
  m_benchmark_gpu_wait_ms.clear();
  m_benchmark_phase_us.fill(0);
  m_benchmark_draw_calls = 0;
  m_benchmark_visible_soldiers = 0;
  m_benchmark_soldiers_lod_full = 0;
  m_benchmark_soldiers_lod_minimal = 0;
  m_benchmark_draw_cmd_counts.fill(0);
  m_benchmark_snapshot_cache_bytes = 0;
  m_benchmark_prepared_batches = 0;
  m_benchmark_instanced_batches = 0;
  m_benchmark_rigged_commands = 0;
  m_benchmark_rigged_instanced_draws = 0;
  m_benchmark_rigged_instanced_instances = 0;
  m_benchmark_rigged_single_draws = 0;
  m_benchmark_triangles_by_type.fill(0);
  m_benchmark_world_us = 0;
  m_benchmark_visibility_us = 0;
  m_benchmark_minimap_us = 0;
  m_benchmark_weather_us = 0;
  m_benchmark_victory_us = 0;
  m_benchmark_view_model_us = 0;
  m_benchmark_loading_seconds = 0.0;
  m_benchmark_render_allocations = 0;
  m_benchmark_render_allocated_bytes = 0;
  m_benchmark_post_load_work_seen = 0;
  m_benchmark_frames_with_post_load_work = 0;
  m_benchmark_first_post_load_frame = -1;
  m_benchmark_last_post_load_frame = -1;
}

void GLView::GLRenderer::observe_runtime_benchmark(
    std::chrono::steady_clock::time_point frame_start,
    double update_ms,
    double render_ms,
    double thread_cpu_ms) {
  if (m_benchmark_seconds <= 0.0 || m_benchmark_complete || m_engine == nullptr ||
      !m_engine->simulation_thread_running()) {
    return;
  }

  if (m_engine->is_loading()) {
    reset_runtime_benchmark_samples();
    return;
  }

  if (m_benchmark_ready_time.time_since_epoch().count() == 0) {
    m_benchmark_ready_time = frame_start;
    m_benchmark_previous_frame_time = frame_start;
    m_benchmark_loading_seconds =
        m_benchmark_created_time.time_since_epoch().count() == 0
            ? 0.0
            : std::chrono::duration<double>(frame_start - m_benchmark_created_time)
                  .count();
    Render::Profiling::reset_thread_allocations();
    return;
  }

  const double ready_seconds =
      std::chrono::duration<double>(frame_start - m_benchmark_ready_time).count();
  if (ready_seconds < k_runtime_benchmark_warmup_seconds) {
    m_benchmark_previous_frame_time = frame_start;
    return;
  }

  m_benchmark_render_ms.push_back(render_ms);
  m_benchmark_update_ms.push_back(update_ms);
  m_benchmark_thread_cpu_ms.push_back(thread_cpu_ms);
  m_benchmark_render_allocations = Render::Profiling::thread_allocation_count();
  m_benchmark_render_allocated_bytes = Render::Profiling::thread_allocated_bytes();

  const std::uint64_t post_load_work =
      Render::Profiling::asset_counters().post_barrier_asset_work();
  if (post_load_work > m_benchmark_post_load_work_seen) {
    m_benchmark_post_load_work_seen = post_load_work;
    const auto frame = static_cast<std::int64_t>(m_benchmark_render_ms.size()) - 1;
    if (m_benchmark_first_post_load_frame < 0) {
      m_benchmark_first_post_load_frame = frame;
    }
    m_benchmark_last_post_load_frame = frame;
    ++m_benchmark_frames_with_post_load_work;
  }
  m_benchmark_wall_interval_ms.push_back(
      std::chrono::duration<double, std::milli>(frame_start -
                                                m_benchmark_previous_frame_time)
          .count());
  m_benchmark_previous_frame_time = frame_start;

  auto const& profile = Render::Profiling::global_profile();
  m_benchmark_draw_calls += profile.draw_calls;
  m_benchmark_visible_soldiers += profile.visible_soldiers;
  m_benchmark_soldiers_lod_full += profile.soldiers_lod_full;
  m_benchmark_soldiers_lod_minimal += profile.soldiers_lod_minimal;
  for (std::size_t i = 0; i < profile.draw_cmd_counts.size(); ++i) {
    m_benchmark_draw_cmd_counts[i] += profile.draw_cmd_counts[i];
  }
  m_benchmark_snapshot_cache_bytes =
      std::max(m_benchmark_snapshot_cache_bytes, profile.snapshot_cache_bytes);
  m_benchmark_prepared_batches += profile.prepared_batches;
  m_benchmark_instanced_batches += profile.instanced_batches;
  m_benchmark_rigged_commands += profile.rigged_commands;
  m_benchmark_rigged_instanced_draws += profile.rigged_instanced_draws;
  m_benchmark_rigged_instanced_instances += profile.rigged_instanced_instances;
  m_benchmark_rigged_single_draws += profile.rigged_single_draws;
  for (std::size_t i = 0; i < m_benchmark_triangles_by_type.size(); ++i) {
    m_benchmark_triangles_by_type[i] += profile.triangles_by_type[i];
  }
  m_benchmark_gpu_shadow_ms.push_back(profile.gpu_shadow_ms);
  m_benchmark_gpu_color_ms.push_back(profile.gpu_color_ms);
  m_benchmark_gpu_wait_ms.push_back(profile.gpu_wait_ms);
  for (std::size_t i = 0; i < profile.phase_us.size(); ++i) {
    m_benchmark_phase_us[i] += profile.phase_us[i];
  }
  m_benchmark_world_us += profile.world_update_us;
  m_benchmark_visibility_us += profile.visibility_update_us;
  m_benchmark_minimap_us += profile.minimap_update_us;
  m_benchmark_weather_us += profile.weather_lighting_us;
  m_benchmark_victory_us += profile.victory_update_us;
  m_benchmark_view_model_us += profile.view_model_sync_us;

  if (ready_seconds >= k_runtime_benchmark_warmup_seconds + m_benchmark_seconds) {
    finish_runtime_benchmark();
  }
}

void GLView::GLRenderer::finish_runtime_benchmark() {
  if (m_benchmark_complete) {
    return;
  }
  m_benchmark_complete = true;

  const QString preset = QString::fromLatin1(
      Render::graphics_quality_key(Render::GraphicsSettings::instance().quality()));
  const std::size_t frame_count = m_benchmark_render_ms.size();
  if (frame_count < k_runtime_benchmark_min_frames) {
    QJsonObject const refusal{
        {QStringLiteral("valid"), false},
        {QStringLiteral("graphics_preset"), preset},
        {QStringLiteral("measured_seconds"), m_benchmark_seconds},
        {QStringLiteral("frames"), static_cast<qint64>(frame_count)},
        {QStringLiteral("minimum_frames"),
         static_cast<qint64>(k_runtime_benchmark_min_frames)},
        {QStringLiteral("error"),
         QStringLiteral("too few frames to report; the run never reached a steady "
                        "frame rate")}};
    const QByteArray refusal_json =
        QJsonDocument(refusal).toJson(QJsonDocument::Indented);
    qWarning().noquote() << "SOI_RUNTIME_BENCHMARK" << refusal_json;
    if (!m_benchmark_output.isEmpty()) {
      QFile output(m_benchmark_output);
      if (output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        output.write(refusal_json);
      }
    }
    QMetaObject::invokeMethod(
        QCoreApplication::instance(), "quit", Qt::QueuedConnection);
    return;
  }

  const Utils::Stats::Distribution render_spread =
      Utils::Stats::distribution_of(m_benchmark_render_ms);
  const double average_wall = average_ms(m_benchmark_wall_interval_ms);
  const double sample_count = static_cast<double>(frame_count);
  QJsonObject report{
      {QStringLiteral("valid"), true},
      {QStringLiteral("graphics_preset"), preset},
      {QStringLiteral("measured_seconds"), m_benchmark_seconds},
      {QStringLiteral("warmup_seconds"), k_runtime_benchmark_warmup_seconds},
      {QStringLiteral("renderer_to_first_playable_frame_seconds"),
       m_benchmark_loading_seconds},
      {QStringLiteral("frames"), static_cast<qint64>(frame_count)},
      {QStringLiteral("render_ms"), distribution_json(m_benchmark_render_ms)},
      {QStringLiteral("update_ms"), distribution_json(m_benchmark_update_ms)},
      {QStringLiteral("thread_cpu_ms"), distribution_json(m_benchmark_thread_cpu_ms)},
      {QStringLiteral("wall_interval_ms"),
       distribution_json(m_benchmark_wall_interval_ms)},
      {QStringLiteral("gpu_shadow_ms"), distribution_json(m_benchmark_gpu_shadow_ms)},
      {QStringLiteral("gpu_color_ms"), distribution_json(m_benchmark_gpu_color_ms)},
      {QStringLiteral("gpu_wait_ms"), distribution_json(m_benchmark_gpu_wait_ms)},
      {QStringLiteral("cpu_work_fps"),
       render_spread.average > 0.0 ? 1000.0 / render_spread.average : 0.0},
      {QStringLiteral("presented_fps"),
       average_wall > 0.0 ? 1000.0 / average_wall : 0.0},
      {QStringLiteral("draw_calls_average"),
       sample_count > 0.0 ? static_cast<double>(m_benchmark_draw_calls) / sample_count
                          : 0.0},
      {QStringLiteral("visible_soldiers_average"),
       sample_count > 0.0
           ? static_cast<double>(m_benchmark_visible_soldiers) / sample_count
           : 0.0},
      {QStringLiteral("soldiers_lod_full_average"),
       sample_count > 0.0
           ? static_cast<double>(m_benchmark_soldiers_lod_full) / sample_count
           : 0.0},
      {QStringLiteral("soldiers_lod_minimal_average"),
       sample_count > 0.0
           ? static_cast<double>(m_benchmark_soldiers_lod_minimal) / sample_count
           : 0.0},
      {QStringLiteral("snapshot_cache_bytes_peak"),
       static_cast<qint64>(m_benchmark_snapshot_cache_bytes)},
      {QStringLiteral("prepared_batches_average"),
       sample_count > 0.0
           ? static_cast<double>(m_benchmark_prepared_batches) / sample_count
           : 0.0},
      {QStringLiteral("instanced_batches_average"),
       sample_count > 0.0
           ? static_cast<double>(m_benchmark_instanced_batches) / sample_count
           : 0.0},
      {QStringLiteral("rigged_draws_average"),
       QJsonObject{
           {QStringLiteral("commands"),
            sample_count > 0.0
                ? static_cast<double>(m_benchmark_rigged_commands) / sample_count
                : 0.0},
           {QStringLiteral("instanced_draws"),
            sample_count > 0.0
                ? static_cast<double>(m_benchmark_rigged_instanced_draws) / sample_count
                : 0.0},
           {QStringLiteral("instanced_creatures"),
            sample_count > 0.0
                ? static_cast<double>(m_benchmark_rigged_instanced_instances) /
                      sample_count
                : 0.0},
           {QStringLiteral("single_draws"),
            sample_count > 0.0
                ? static_cast<double>(m_benchmark_rigged_single_draws) / sample_count
                : 0.0}}}};

  QJsonObject draw_cmd_average;
  for (std::size_t i = 0; i < m_benchmark_draw_cmd_counts.size(); ++i) {
    if (m_benchmark_draw_cmd_counts[i] == 0) {
      continue;
    }
    draw_cmd_average.insert(QString::fromLatin1(Render::GL::draw_cmd_type_name(i)),
                            sample_count > 0.0
                                ? static_cast<double>(m_benchmark_draw_cmd_counts[i]) /
                                      sample_count
                                : 0.0);
  }
  report.insert(QStringLiteral("draw_commands_average_by_type"), draw_cmd_average);

  QJsonObject triangle_average;
  for (std::size_t i = 0; i < m_benchmark_triangles_by_type.size(); ++i) {
    if (m_benchmark_triangles_by_type[i] == 0) {
      continue;
    }
    const bool other = i + 1 == m_benchmark_triangles_by_type.size();
    triangle_average.insert(
        other ? QStringLiteral("other")
              : QString::fromLatin1(Render::GL::draw_cmd_type_name(i)),
        sample_count > 0.0
            ? static_cast<double>(m_benchmark_triangles_by_type[i]) / sample_count
            : 0.0);
  }
  report.insert(QStringLiteral("triangles_average_by_type"), triangle_average);

  auto stage_ms = [sample_count](std::uint64_t total_us) {
    return sample_count > 0.0 ? static_cast<double>(total_us) / sample_count / 1000.0
                              : 0.0;
  };
  QJsonObject render_thread_stages{
      {QStringLiteral("world_ms_average"), stage_ms(m_benchmark_world_us)},
      {QStringLiteral("visibility_ms_average"), stage_ms(m_benchmark_visibility_us)},
      {QStringLiteral("minimap_ms_average"), stage_ms(m_benchmark_minimap_us)},
      {QStringLiteral("weather_lighting_ms_average"), stage_ms(m_benchmark_weather_us)},
      {QStringLiteral("victory_ms_average"), stage_ms(m_benchmark_victory_us)},
      {QStringLiteral("view_model_sync_ms_average"),
       stage_ms(m_benchmark_view_model_us)}};
  for (std::size_t i = 0; i < m_benchmark_phase_us.size(); ++i) {
    render_thread_stages.insert(
        QStringLiteral("phase_%1_ms_average")
            .arg(QString::fromLatin1(Render::Profiling::phase_name(
                static_cast<Render::Profiling::Phase>(i)))),
        stage_ms(m_benchmark_phase_us[i]));
  }
  report.insert(QStringLiteral("render_thread_stages"), render_thread_stages);

  Render::Profiling::count_asset(
      Render::Profiling::AssetCounter::RenderThreadAllocations,
      m_benchmark_render_allocations);
  Render::Profiling::count_asset(
      Render::Profiling::AssetCounter::RenderThreadAllocatedBytes,
      m_benchmark_render_allocated_bytes);
  report.insert(QStringLiteral("render_thread_allocations"),
                QJsonObject{{QStringLiteral("tracked"),
                             Render::Profiling::allocation_tracking_available()},
                            {QStringLiteral("count_after_loading"),
                             static_cast<qint64>(m_benchmark_render_allocations)},
                            {QStringLiteral("bytes_after_loading"),
                             static_cast<qint64>(m_benchmark_render_allocated_bytes)}});
  report.insert(
      QStringLiteral("post_load_asset_work_timing"),
      QJsonObject{{QStringLiteral("measured_frames"), static_cast<qint64>(frame_count)},
                  {QStringLiteral("frames_with_work"),
                   static_cast<qint64>(m_benchmark_frames_with_post_load_work)},
                  {QStringLiteral("first_frame"),
                   static_cast<qint64>(m_benchmark_first_post_load_frame)},
                  {QStringLiteral("last_frame"),
                   static_cast<qint64>(m_benchmark_last_post_load_frame)}});
  report.insert(QStringLiteral("asset_counters"),
                Render::Profiling::asset_counters_json());
  report.insert(QStringLiteral("navigation"),
                Render::Profiling::navigation_counters_json());
  const auto& graphics = Render::GraphicsSettings::instance();
  Render::Profiling::PerformanceMeasurement measurement;
  measurement.frames = frame_count;
  measurement.frame_p50_ms = render_spread.p50;
  measurement.frame_p95_ms = render_spread.p95;
  measurement.frame_p99_ms = render_spread.p99;
  measurement.frame_max_ms = render_spread.maximum;
  const auto update_spread = Utils::Stats::distribution_of(m_benchmark_update_ms);
  measurement.update_average_ms = update_spread.average;
  measurement.update_p95_ms = update_spread.p95;
  measurement.render_submit_p95_ms =
      Utils::Stats::distribution_of(m_benchmark_thread_cpu_ms).p95;
  const auto shadow_spread = Utils::Stats::distribution_of(m_benchmark_gpu_shadow_ms);
  const auto color_spread = Utils::Stats::distribution_of(m_benchmark_gpu_color_ms);
  measurement.gpu_shadow_p95_ms = shadow_spread.p95;
  measurement.gpu_color_p95_ms = color_spread.p95;
  measurement.gpu_timed = shadow_spread.maximum > 0.0 || color_spread.maximum > 0.0;
  measurement.ultra_preset = graphics.quality() == Render::GraphicsQuality::Ultra;
  measurement.full_creature_lod = !graphics.creature_lod_enabled();
  report.insert(QStringLiteral("budget"),
                Render::Profiling::budget_verdict_json(
                    Render::Profiling::PerformanceBudget::release_gate(), measurement));

  if (m_continuity_probe != nullptr) {
    QJsonArray continuity_issues;
    for (const auto& issue : m_continuity_probe->issues) {
      continuity_issues.push_back(issue);
    }
    report.insert(QStringLiteral("continuity_issues"), continuity_issues);
    report.insert(QStringLiteral("continuity_passed"), continuity_issues.isEmpty());
    report.insert(QStringLiteral("frame_continuity_samples"),
                  static_cast<qint64>(
                      m_continuity_probe->framebuffer_analyzer.observed_frames()));
    report.insert(
        QStringLiteral("soldier_visibility_transitions"),
        static_cast<qint64>(m_continuity_probe->soldier_visibility_transitions));
    report.insert(QStringLiteral("soldier_visibility_churn"),
                  static_cast<qint64>(m_continuity_probe->soldier_visibility_churn));
    report.insert(QStringLiteral("ultra_lod_culls"),
                  static_cast<qint64>(m_continuity_probe->ultra_lod_culls));
  }

  const QByteArray json = QJsonDocument(report).toJson(QJsonDocument::Indented);
  qInfo().noquote() << "SOI_RUNTIME_BENCHMARK" << json;
  if (!m_benchmark_output.isEmpty()) {
    QFile output(m_benchmark_output);
    if (output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      output.write(json);
    } else {
      qWarning() << "Could not write runtime benchmark:" << m_benchmark_output;
    }
  }
  QMetaObject::invokeMethod(QCoreApplication::instance(), "quit", Qt::QueuedConnection);
}

auto GLView::GLRenderer::createFramebufferObject(const QSize& size)
    -> QOpenGLFramebufferObject* {
  m_size = size;

  QOpenGLContext* ctx = QOpenGLContext::currentContext();
  if ((ctx == nullptr) || !ctx->isValid()) {
    qCritical() << "GLRenderer::createFramebufferObject() - No valid OpenGL context";
    return nullptr;
  }

  QOpenGLFramebufferObjectFormat fmt;
  fmt.setAttachment(QOpenGLFramebufferObject::Depth);
  const auto& graphics = Render::GraphicsSettings::instance();
  int const requested_samples = graphics.presentation().msaa_samples;
  m_fbo_msaa_samples = requested_samples;
  m_fbo_graphics_generation = graphics.generation();
  fmt.setSamples(requested_samples);
  auto* target = new QOpenGLFramebufferObject(size, fmt);
  if (requested_samples > 0 && !target->isValid()) {
    delete target;
    fmt.setSamples(0);
    target = new QOpenGLFramebufferObject(size, fmt);
  }
  return target;
}

void GLView::GLRenderer::synchronize(QQuickFramebufferObject* item) {
  auto* view = dynamic_cast<GLView*>(item);
  m_engine = qobject_cast<GameEngine*>(view->engine());
  if (m_engine != nullptr) {
    m_engine->set_input_viewport_size(view->width(), view->height());
  }

  if (m_engine != nullptr && !m_engine->is_loading()) {
    m_pending_commander_speakers = m_engine->commander_message_speakers();
  }

  const auto& graphics = Render::GraphicsSettings::instance();
  if (graphics.generation() != m_fbo_graphics_generation) {
    m_fbo_graphics_generation = graphics.generation();
    if (graphics.presentation().msaa_samples != m_fbo_msaa_samples) {
      invalidateFramebufferObject();
    }
  }
}
