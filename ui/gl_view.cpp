#include "gl_view.h"

#if defined(SOI_ENABLE_RUNTIME_TRACING)
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#endif
#include <QMetaObject>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QQuickWindow>
#include <QSurfaceFormat>
#include <qglobal.h>
#include <qobject.h>
#include <qopenglcontext.h>
#include <qopenglframebufferobject.h>
#include <qpointer.h>
#include <qquickframebufferobject.h>
#include <qtmetamacros.h>

#include <exception>
#if defined(SOI_ENABLE_RUNTIME_TRACING)
#include <algorithm>
#include <deque>
#include <numeric>
#include <unordered_map>
#endif
#include <utility>

#include "../app/core/game_engine.h"
#include "../render/graphics_settings.h"
#include "../render/i_render_backend.h"
#if defined(SOI_ENABLE_RUNTIME_TRACING)
#include "../render/profiling/combat_animation_diagnostics.h"
#include "../render/profiling/frame_continuity_analyzer.h"
#include "../render/profiling/frame_profile.h"
#endif

#if defined(SOI_ENABLE_RUNTIME_TRACING)
namespace {
constexpr double k_runtime_benchmark_warmup_seconds = 2.0;
constexpr std::uint64_t k_visibility_churn_window_frames = 120U;
constexpr std::uint32_t k_visibility_churn_threshold = 4U;

auto percentile_ms(std::vector<double> samples, double percentile) -> double {
  if (samples.empty()) {
    return 0.0;
  }
  std::sort(samples.begin(), samples.end());
  const auto index = static_cast<std::size_t>(
      std::clamp(percentile * static_cast<double>(samples.size() - 1U),
                 0.0,
                 static_cast<double>(samples.size() - 1U)));
  return samples[index];
}

auto average_ms(const std::vector<double>& samples) -> double {
  if (samples.empty()) {
    return 0.0;
  }
  return std::accumulate(samples.begin(), samples.end(), 0.0) /
         static_cast<double>(samples.size());
}
} // namespace
#endif

#if defined(SOI_ENABLE_RUNTIME_TRACING)
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
#endif

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
    auto& gfx = Render::GraphicsSettings::instance();
    const_cast<Render::GraphicsFeatures&>(gfx.features()).shader_quality =
        Render::ShaderQuality::None;
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

GLView::GLRenderer::GLRenderer(QPointer<GLView> view, QPointer<GameEngine> engine)
    : m_view(std::move(view))
    , m_engine(std::move(engine)) {
#if defined(SOI_ENABLE_RUNTIME_TRACING)
  bool valid = false;
  m_benchmark_seconds =
      qEnvironmentVariable("SOI_RUNTIME_BENCHMARK_SECONDS").toDouble(&valid);
  if (!valid || m_benchmark_seconds <= 0.0) {
    m_benchmark_seconds = 0.0;
  }
  m_benchmark_output = qEnvironmentVariable("SOI_RUNTIME_BENCHMARK_OUTPUT");
  if (m_benchmark_seconds > 0.0) {
    m_continuity_probe = std::make_unique<RuntimeContinuityProbe>();
    Render::Profiling::CombatAnimationDiagnostics::instance().set_enabled(true);
  }
#endif
}

GLView::GLRenderer::~GLRenderer() {
#if defined(SOI_ENABLE_RUNTIME_TRACING)
  if (m_continuity_probe != nullptr) {
    Render::Profiling::CombatAnimationDiagnostics::instance().set_enabled(false);
  }
#endif
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
    m_engine->ensure_initialized();
    if (!m_engine->renderer_initialized()) {
      qCritical() << "GLRenderer::render() - gameplay renderer initialization failed";
      return;
    }

    auto now = std::chrono::steady_clock::now();
    float dt = 1.0F / 60.0F;
    if (m_last_frame_time.time_since_epoch().count() != 0) {
      dt = std::chrono::duration<float>(now - m_last_frame_time).count();
      dt = std::min(dt, 0.1F);
    }
    m_last_frame_time = now;

#if defined(SOI_ENABLE_RUNTIME_TRACING)
    auto const frame_work_start = std::chrono::steady_clock::now();
    m_engine->update(dt);
    auto const update_end = std::chrono::steady_clock::now();
    m_engine->render(m_size.width(), m_size.height());
    auto const render_end = std::chrono::steady_clock::now();

    observe_runtime_continuity();
    observe_runtime_benchmark(
        frame_work_start,
        std::chrono::duration<double, std::milli>(update_end - frame_work_start)
            .count(),
        std::chrono::duration<double, std::milli>(render_end - update_end).count());
#else
    m_engine->update(dt);
    m_engine->render(m_size.width(), m_size.height());
#endif

    if (!m_ready_reported && m_view != nullptr) {
      m_ready_reported = true;
      QMetaObject::invokeMethod(m_view, "notify_renderer_ready", Qt::QueuedConnection);
    }
  } catch (const std::exception& e) {
    qCritical() << "GLRenderer::render() exception:" << e.what();
    return;
  } catch (...) {
    qCritical() << "GLRenderer::render() unknown exception";
    return;
  }

  update();
}

#if defined(SOI_ENABLE_RUNTIME_TRACING)
void GLView::GLRenderer::observe_runtime_continuity() {
  if (m_continuity_probe == nullptr || m_engine == nullptr || m_engine->is_loading() ||
      !m_engine->is_campaign_mission() || m_size.isEmpty()) {
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

  probe.recent_frames.push_back({probe.framebuffer_analyzer.observed_frames() + 1U,
                                 frame});
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

      if (soldier.cull_reason == Render::Profiling::SoldierCullReason::Billboard ||
          soldier.cull_reason == Render::Profiling::SoldierCullReason::Temporal) {
        ++probe.ultra_lod_culls;
      }
      state.reason = soldier.cull_reason;
      state.last_seen_frame = probe.frame_index;
    }
  }
}

void GLView::GLRenderer::observe_runtime_benchmark(
    std::chrono::steady_clock::time_point frame_start,
    double update_ms,
    double render_ms) {
  if (m_benchmark_seconds <= 0.0 || m_benchmark_complete || m_engine == nullptr ||
      m_engine->is_loading() || !m_engine->is_campaign_mission()) {
    return;
  }

  if (m_benchmark_ready_time.time_since_epoch().count() == 0) {
    m_benchmark_ready_time = frame_start;
    m_benchmark_previous_frame_time = frame_start;
    return;
  }

  const double ready_seconds =
      std::chrono::duration<double>(frame_start - m_benchmark_ready_time).count();
  if (ready_seconds < k_runtime_benchmark_warmup_seconds) {
    m_benchmark_previous_frame_time = frame_start;
    return;
  }

  m_benchmark_frame_work_ms.push_back(update_ms + render_ms);
  m_benchmark_update_ms.push_back(update_ms);
  m_benchmark_render_ms.push_back(render_ms);
  m_benchmark_wall_interval_ms.push_back(
      std::chrono::duration<double, std::milli>(frame_start -
                                                m_benchmark_previous_frame_time)
          .count());
  m_benchmark_previous_frame_time = frame_start;

  auto const& profile = Render::Profiling::global_profile();
  m_benchmark_draw_calls += profile.draw_calls;
  m_benchmark_visible_soldiers += profile.visible_soldiers;

  if (ready_seconds >= k_runtime_benchmark_warmup_seconds + m_benchmark_seconds) {
    finish_runtime_benchmark();
  }
}

void GLView::GLRenderer::finish_runtime_benchmark() {
  if (m_benchmark_complete) {
    return;
  }
  m_benchmark_complete = true;

  const double average_work = average_ms(m_benchmark_frame_work_ms);
  const double average_wall = average_ms(m_benchmark_wall_interval_ms);
  const double sample_count = static_cast<double>(m_benchmark_frame_work_ms.size());
  QJsonObject report{
      {QStringLiteral("graphics_preset"), QStringLiteral("ultra")},
      {QStringLiteral("measured_seconds"), m_benchmark_seconds},
      {QStringLiteral("frames"), static_cast<qint64>(m_benchmark_frame_work_ms.size())},
      {QStringLiteral("cpu_work_ms_average"), average_work},
      {QStringLiteral("cpu_work_ms_p95"),
       percentile_ms(m_benchmark_frame_work_ms, 0.95)},
      {QStringLiteral("cpu_work_fps"),
       average_work > 0.0 ? 1000.0 / average_work : 0.0},
      {QStringLiteral("wall_interval_ms_average"), average_wall},
      {QStringLiteral("presented_fps"),
       average_wall > 0.0 ? 1000.0 / average_wall : 0.0},
      {QStringLiteral("update_ms_average"), average_ms(m_benchmark_update_ms)},
      {QStringLiteral("render_ms_average"), average_ms(m_benchmark_render_ms)},
      {QStringLiteral("draw_calls_average"),
       sample_count > 0.0 ? static_cast<double>(m_benchmark_draw_calls) / sample_count
                          : 0.0},
      {QStringLiteral("visible_soldiers_average"),
       sample_count > 0.0
           ? static_cast<double>(m_benchmark_visible_soldiers) / sample_count
           : 0.0}};

  QJsonArray continuity_issues;
  if (m_continuity_probe != nullptr) {
    for (const auto& issue : m_continuity_probe->issues) {
      continuity_issues.push_back(issue);
    }
    report.insert(QStringLiteral("frame_continuity_samples"),
                  static_cast<qint64>(
                      m_continuity_probe->framebuffer_analyzer.observed_frames()));
    report.insert(QStringLiteral("soldier_visibility_transitions"),
                  static_cast<qint64>(
                      m_continuity_probe->soldier_visibility_transitions));
    report.insert(QStringLiteral("soldier_visibility_churn"),
                  static_cast<qint64>(m_continuity_probe->soldier_visibility_churn));
    report.insert(QStringLiteral("ultra_lod_culls"),
                  static_cast<qint64>(m_continuity_probe->ultra_lod_culls));
  }
  report.insert(QStringLiteral("continuity_issues"), continuity_issues);
  report.insert(QStringLiteral("continuity_passed"), continuity_issues.isEmpty());

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
#endif

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
  fmt.setSamples(0);
  return new QOpenGLFramebufferObject(size, fmt);
}

void GLView::GLRenderer::synchronize(QQuickFramebufferObject* item) {
  auto* view = dynamic_cast<GLView*>(item);
  m_engine = qobject_cast<GameEngine*>(view->engine());
  if (m_engine != nullptr) {
    m_engine->set_input_viewport_size(view->width(), view->height());
  }
}
