#pragma once

#include <QPointer>
#include <QQuickFramebufferObject>
#if defined(SOI_ENABLE_RUNTIME_TRACING)
#include <QString>
#endif

#include <chrono>
#if defined(SOI_ENABLE_RUNTIME_TRACING)
#include <cstdint>
#include <memory>
#include <vector>
#endif

class GameEngine;

class GLView : public QQuickFramebufferObject {
  Q_OBJECT
public:
  GLView();

  [[nodiscard]] auto createRenderer() const -> Renderer* override;

  Q_PROPERTY(QObject* engine READ engine WRITE set_engine NOTIFY engine_changed)
  [[nodiscard]] auto engine() const -> QObject*;
  void set_engine(QObject* eng);

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
    QSize m_size;
    std::chrono::steady_clock::time_point m_last_frame_time{};
#if defined(SOI_ENABLE_RUNTIME_TRACING)
    struct RuntimeContinuityProbe;
    std::chrono::steady_clock::time_point m_benchmark_ready_time{};
    std::chrono::steady_clock::time_point m_benchmark_previous_frame_time{};
    double m_benchmark_seconds = 0.0;
    QString m_benchmark_output;
    bool m_benchmark_complete = false;
    std::vector<double> m_benchmark_frame_work_ms;
    std::vector<double> m_benchmark_update_ms;
    std::vector<double> m_benchmark_render_ms;
    std::vector<double> m_benchmark_wall_interval_ms;
    std::uint64_t m_benchmark_draw_calls = 0;
    std::uint64_t m_benchmark_visible_soldiers = 0;
    std::unique_ptr<RuntimeContinuityProbe> m_continuity_probe;

    void observe_runtime_continuity();
    void observe_runtime_benchmark(std::chrono::steady_clock::time_point frame_start,
                                   double update_ms,
                                   double render_ms);
    void finish_runtime_benchmark();
#endif
  };
};
