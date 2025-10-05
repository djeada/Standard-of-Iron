#include "gl_view.h"
#include "../app/game_engine.h"

#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QQuickWindow>

GLView::GLView() {
    setMirrorVertically(true); 
}

QQuickFramebufferObject::Renderer* GLView::createRenderer() const {
    return new GLRenderer(m_engine);
}

QObject* GLView::engine() const { return m_engine; }

void GLView::setEngine(QObject* eng) {
    if (m_engine == eng) return;
    m_engine = qobject_cast<GameEngine*>(eng);
    emit engineChanged();
    update();
}

GLView::GLRenderer::GLRenderer(QPointer<GameEngine> engine)
    : m_engine(engine) {}

void GLView::GLRenderer::render() {
    if (!m_engine) return;
    m_engine->ensureInitialized();
    m_engine->update(1.0f/60.0f);
    m_engine->render(m_size.width(), m_size.height());
    update(); 
}

QOpenGLFramebufferObject* GLView::GLRenderer::createFramebufferObject(const QSize &size) {
    m_size = size;
    QOpenGLFramebufferObjectFormat fmt;
    fmt.setAttachment(QOpenGLFramebufferObject::Depth);
    fmt.setSamples(0);
    return new QOpenGLFramebufferObject(size, fmt);
}

void GLView::GLRenderer::synchronize(QQuickFramebufferObject *item) {
    auto *view = static_cast<GLView*>(item);
    m_engine = qobject_cast<GameEngine*>(view->engine());
}
