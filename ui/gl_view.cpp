#include "gl_view.h"
#include "../app/core/game_engine.h"

#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QQuickWindow>
#include <exception>
#include <qglobal.h>
#include <qobject.h>
#include <qopenglcontext.h>
#include <qopenglframebufferobject.h>
#include <qpointer.h>
#include <qquickframebufferobject.h>
#include <qtmetamacros.h>
#include <utility>

GLView::GLView() {
  setMirrorVertically(true);

  QOpenGLContext *ctx = QOpenGLContext::currentContext();
  if (ctx == nullptr) {
    qWarning() << "GLView: No OpenGL context available";
    qWarning() << "GLView: 3D rendering will not work in software mode";
    qWarning() << "GLView: Try running without QT_QUICK_BACKEND=software for "
                  "full functionality";
  }
}

auto GLView::createRenderer() const -> QQuickFramebufferObject::Renderer * {

  QOpenGLContext *ctx = QOpenGLContext::currentContext();
  if ((ctx == nullptr) || !ctx->isValid()) {
    qCritical() << "GLView::createRenderer() - No valid OpenGL context";
    qCritical() << "Running in software rendering mode - 3D view not available";
    return nullptr;
  }

  return new GLRenderer(m_engine);
}

auto GLView::engine() const -> QObject * { return m_engine; }

void GLView::set_engine(QObject *eng) {
  if (m_engine == eng) {
    return;
  }
  m_engine = qobject_cast<GameEngine *>(eng);
  emit engine_changed();
  update();
}

GLView::GLRenderer::GLRenderer(QPointer<GameEngine> engine)
    : m_engine(std::move(std::move(engine))) {}

void GLView::GLRenderer::render() {
  if (m_engine == nullptr) {
    qWarning() << "GLRenderer::render() - engine is null";
    return;
  }

  QOpenGLContext *ctx = QOpenGLContext::currentContext();
  if ((ctx == nullptr) || !ctx->isValid()) {
    qCritical() << "GLRenderer::render() - OpenGL context lost";
    return;
  }

  try {
    m_engine->ensure_initialized();
    m_engine->update(1.0F / 60.0F);
    m_engine->render(m_size.width(), m_size.height());
  } catch (const std::exception &e) {
    qCritical() << "GLRenderer::render() exception:" << e.what();
    return;
  } catch (...) {
    qCritical() << "GLRenderer::render() unknown exception";
    return;
  }

  update();
}

auto GLView::GLRenderer::createFramebufferObject(const QSize &size)
    -> QOpenGLFramebufferObject * {
  m_size = size;

  QOpenGLContext *ctx = QOpenGLContext::currentContext();
  if ((ctx == nullptr) || !ctx->isValid()) {
    qCritical()
        << "GLRenderer::createFramebufferObject() - No valid OpenGL context";
    return nullptr;
  }

  QOpenGLFramebufferObjectFormat fmt;
  fmt.setAttachment(QOpenGLFramebufferObject::Depth);
  fmt.setSamples(0);
  return new QOpenGLFramebufferObject(size, fmt);
}

void GLView::GLRenderer::synchronize(QQuickFramebufferObject *item) {
  auto *view = dynamic_cast<GLView *>(item);
  m_engine = qobject_cast<GameEngine *>(view->engine());
}
