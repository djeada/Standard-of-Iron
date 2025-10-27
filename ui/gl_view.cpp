#include "gl_view.h"
#include "../app/core/game_engine.h"

#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QQuickWindow>
#include <qobject.h>
#include <qopenglframebufferobject.h>
#include <qpointer.h>
#include <qquickframebufferobject.h>
#include <qtmetamacros.h>
#include <utility>

GLView::GLView() { setMirrorVertically(true); }

auto GLView::createRenderer() const -> QQuickFramebufferObject::Renderer * {
  return new GLRenderer(m_engine);
}

auto GLView::engine() const -> QObject * { return m_engine; }

void GLView::setEngine(QObject *eng) {
  if (m_engine == eng) {
    return;
  }
  m_engine = qobject_cast<GameEngine *>(eng);
  emit engineChanged();
  update();
}

GLView::GLRenderer::GLRenderer(QPointer<GameEngine> engine)
    : m_engine(std::move(std::move(engine))) {}

void GLView::GLRenderer::render() {
  if (m_engine == nullptr) {
    return;
  }
  m_engine->ensureInitialized();
  m_engine->update(1.0F / 60.0F);
  m_engine->render(m_size.width(), m_size.height());
  update();
}

auto GLView::GLRenderer::createFramebufferObject(const QSize &size)
    -> QOpenGLFramebufferObject * {
  m_size = size;
  QOpenGLFramebufferObjectFormat fmt;
  fmt.setAttachment(QOpenGLFramebufferObject::Depth);
  fmt.setSamples(0);
  return new QOpenGLFramebufferObject(size, fmt);
}

void GLView::GLRenderer::synchronize(QQuickFramebufferObject *item) {
  auto *view = dynamic_cast<GLView *>(item);
  m_engine = qobject_cast<GameEngine *>(view->engine());
}
