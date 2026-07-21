#pragma once

#include <QPointer>
#include <QQuickFramebufferObject>

#include <chrono>

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
  };
};
