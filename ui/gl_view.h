#pragma once

#include <QQuickFramebufferObject>
#include <QPointer>

class GameEngine;

class GLView : public QQuickFramebufferObject {
    Q_OBJECT
public:
    GLView();

    Renderer* createRenderer() const override;

    Q_PROPERTY(QObject* engine READ engine WRITE setEngine NOTIFY engineChanged)
    QObject* engine() const;
    void setEngine(QObject* eng);

signals:
    void engineChanged();

private:
    QPointer<GameEngine> m_engine;

    class GLRenderer : public QQuickFramebufferObject::Renderer {
    public:
        explicit GLRenderer(QPointer<GameEngine> engine);
        void render() override;
        QOpenGLFramebufferObject* createFramebufferObject(const QSize &size) override;
        void synchronize(QQuickFramebufferObject *item) override;
    private:
        QPointer<GameEngine> m_engine;
        QSize m_size;
    };
};
