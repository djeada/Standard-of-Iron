#pragma once

#include <QPointer>
#include <QQuickFramebufferObject>

class GameEngine;

class GLView : public QQuickFramebufferObject {
  Q_OBJECT
public:
  GLView();

  [[nodiscard]] auto createRenderer() const -> Renderer * override;

  Q_PROPERTY(QObject *engine READ engine WRITE set_engine NOTIFY engine_changed)
  [[nodiscard]] auto engine() const -> QObject *;
  void set_engine(QObject *eng);

signals:
  void engine_changed();

private:
  QPointer<GameEngine> m_engine;

  class GLRenderer : public QQuickFramebufferObject::Renderer {
  public:
    explicit GLRenderer(QPointer<GameEngine> engine);
    void render() override;
    auto createFramebufferObject(const QSize &size)
        -> QOpenGLFramebufferObject * override;
    void synchronize(QQuickFramebufferObject *item) override;

  private:
    QPointer<GameEngine> m_engine;
    QSize m_size;
  };
};
