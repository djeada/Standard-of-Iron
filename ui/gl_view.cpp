#include "gl_view.h"
#include "../app/core/game_engine.h"

#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QOpenGLContext>
#include <QQuickWindow>
#include <qobject.h>
#include <qopenglframebufferobject.h>
#include <qpointer.h>
#include <qquickframebufferobject.h>
#include <qtmetamacros.h>
#include <utility>

GLView::GLView() { 
  setMirrorVertically(true);
  
  // Check if OpenGL is available
  QOpenGLContext* ctx = QOpenGLContext::currentContext();
  if (!ctx) {
    qWarning() << "GLView: No OpenGL context available";
    qWarning() << "GLView: 3D rendering will not work in software mode";
    qWarning() << "GLView: Try running without QT_QUICK_BACKEND=software for full functionality";
  }
}

auto GLView::createRenderer() const -> QQuickFramebufferObject::Renderer * {
  // Check if we have a valid OpenGL context
  QOpenGLContext* ctx = QOpenGLContext::currentContext();
  if (!ctx || !ctx->isValid()) {
    qCritical() << "GLView::createRenderer() - No valid OpenGL context";
    qCritical() << "Running in software rendering mode - 3D view not available";
    return nullptr;
  }
  
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
    qWarning() << "GLRenderer::render() - engine is null";
    return;
  }
  
  // Double-check OpenGL context is still valid
  QOpenGLContext* ctx = QOpenGLContext::currentContext();
  if (!ctx || !ctx->isValid()) {
    qCritical() << "GLRenderer::render() - OpenGL context lost";
    return;
  }
  
  try {
    m_engine->ensureInitialized();
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
  
  // Verify OpenGL context before creating FBO
  QOpenGLContext* ctx = QOpenGLContext::currentContext();
  if (!ctx || !ctx->isValid()) {
    qCritical() << "GLRenderer::createFramebufferObject() - No valid OpenGL context";
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
