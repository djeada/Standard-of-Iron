#include "gl_view.h"
#include "../app/core/game_engine.h"
#include "../render/graphics_settings.h"
#include "../render/i_render_backend.h"

#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QQuickWindow>
#include <QSurfaceFormat>
#include <exception>
#include <qglobal.h>
#include <qobject.h>
#include <qopenglcontext.h>
#include <qopenglframebufferobject.h>
#include <qpointer.h>
#include <qquickframebufferobject.h>
#include <qtmetamacros.h>
#include <utility>

GLView::GLView() { setMirrorVertically(true); }

auto GLView::createRenderer() const -> QQuickFramebufferObject::Renderer * {

  QOpenGLContext *ctx = QOpenGLContext::currentContext();
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
    auto &gfx = Render::GraphicsSettings::instance();
    const_cast<Render::GraphicsFeatures &>(gfx.features()).shader_quality =
        Render::ShaderQuality::None;
  } else {
    qInfo() << "GLView::createRenderer() - OpenGL" << version.first << "."
            << version.second << "context OK";
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

    auto now = std::chrono::steady_clock::now();
    float dt = 1.0F / 60.0F;
    if (m_last_frame_time.time_since_epoch().count() != 0) {
      dt = std::chrono::duration<float>(now - m_last_frame_time).count();
      dt = std::min(dt, 0.1F);
    }
    m_last_frame_time = now;

    m_engine->update(dt);
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
