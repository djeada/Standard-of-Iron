#pragma once

class QOpenGLDebugLogger;

#include <QPointer>
#include <QQuickFramebufferObject>
#include <QString>
#include <QStringList>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "render/profiling/frame_profile.h"

class GameEngine;

class GLView : public QQuickFramebufferObject {
  Q_OBJECT
public:
  GLView();

  [[nodiscard]] auto createRenderer() const -> Renderer* override;

  Q_PROPERTY(QObject* engine READ engine WRITE set_engine NOTIFY engine_changed)
  Q_PROPERTY(bool rendererReady READ is_renderer_ready NOTIFY renderer_ready)
  [[nodiscard]] auto engine() const -> QObject*;
  void set_engine(QObject* eng);
  [[nodiscard]] auto is_renderer_ready() const noexcept -> bool {
    return m_renderer_ready;
  }

signals:
  void engine_changed();
  void renderer_ready();

private slots:
  void notify_renderer_ready();

private:
  QPointer<GameEngine> m_engine;
  bool m_renderer_ready = false;

  class GLRenderer : public QQuickFramebufferObject::Renderer {
  public:
    explicit GLRenderer(QPointer<GLView> view, QPointer<GameEngine> engine);
    ~GLRenderer() override;
    void render() override;
    auto
    createFramebufferObject(const QSize& size) -> QOpenGLFramebufferObject* override;
    void synchronize(QQuickFramebufferObject* item) override;

  private:
    QPointer<GLView> m_view;
    QPointer<GameEngine> m_engine;
    bool m_ready_reported = false;
    QStringList m_pending_commander_speakers;
    QSize m_size;
    int m_fbo_msaa_samples = -1;
    std::uint32_t m_fbo_graphics_generation = 0;
    std::chrono::steady_clock::time_point m_last_frame_time{};
    std::chrono::steady_clock::time_point m_last_render_end{};
    struct RuntimeContinuityProbe;
    std::chrono::steady_clock::time_point m_benchmark_ready_time{};
    std::chrono::steady_clock::time_point m_benchmark_previous_frame_time{};
    double m_benchmark_seconds = 0.0;
    QString m_benchmark_output;
    bool m_benchmark_complete = false;
    std::vector<double> m_benchmark_frame_work_ms;
    std::vector<double> m_benchmark_update_ms;
    std::vector<double> m_benchmark_render_ms;
    std::vector<double> m_benchmark_thread_cpu_ms;
    std::unique_ptr<QOpenGLDebugLogger> m_gl_debug_logger;
    bool m_gl_debug_checked = false;
    std::vector<double> m_benchmark_gpu_shadow_ms;
    std::vector<double> m_benchmark_gpu_color_ms;
    std::vector<double> m_benchmark_gpu_wait_ms;
    std::vector<double> m_benchmark_wall_interval_ms;
    std::uint64_t m_benchmark_draw_calls = 0;
    std::uint64_t m_benchmark_visible_soldiers = 0;
    std::array<std::uint64_t,
               static_cast<std::size_t>(Render::Profiling::Phase::_Count)>
        m_benchmark_phase_us{};
    std::uint64_t m_benchmark_world_us = 0;
    std::uint64_t m_benchmark_visibility_us = 0;
    std::uint64_t m_benchmark_minimap_us = 0;
    std::uint64_t m_benchmark_weather_us = 0;
    std::uint64_t m_benchmark_victory_us = 0;
    std::uint64_t m_benchmark_view_model_us = 0;
    std::unique_ptr<RuntimeContinuityProbe> m_continuity_probe;

    void warm_commander_portraits();
    void observe_runtime_continuity();
    void observe_runtime_benchmark(std::chrono::steady_clock::time_point frame_start,
                                   double update_ms,
                                   double render_ms,
                                   double thread_cpu_ms);
    void finish_runtime_benchmark();
  };
};
