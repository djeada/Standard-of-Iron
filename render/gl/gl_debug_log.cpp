#include "gl_debug_log.h"

#include <QByteArray>
#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLDebugLogger>
#include <QOpenGLDebugMessage>
#include <QString>

namespace Render::GL {

namespace {
const char* const k_logger_name = "soi_gl_debug_logger";
}

void install_gl_debug_logger() {
  if (!qEnvironmentVariableIsSet("SOI_GL_DEBUG")) {
    return;
  }

  QOpenGLContext* ctx = QOpenGLContext::currentContext();
  if (ctx == nullptr) {
    return;
  }
  if (ctx->findChild<QOpenGLDebugLogger*>(QString::fromLatin1(k_logger_name)) !=
      nullptr) {
    return;
  }
  if (!ctx->hasExtension(QByteArrayLiteral("GL_KHR_debug"))) {
    qWarning() << "SOI_GL_DEBUG requested but GL_KHR_debug is unavailable";
    return;
  }

  auto* logger = new QOpenGLDebugLogger(ctx);
  logger->setObjectName(QString::fromLatin1(k_logger_name));
  if (!logger->initialize()) {
    qWarning() << "SOI_GL_DEBUG requested but the debug logger failed to initialize";
    delete logger;
    return;
  }

  QObject::connect(logger,
                   &QOpenGLDebugLogger::messageLogged,
                   logger,
                   [](const QOpenGLDebugMessage& message) {
                     if (message.severity() ==
                         QOpenGLDebugMessage::NotificationSeverity) {
                       return;
                     }
                     qWarning().noquote() << "GL debug:" << message.message();
                   });
  logger->startLogging(QOpenGLDebugLogger::SynchronousLogging);
  qInfo() << "SOI_GL_DEBUG: synchronous OpenGL debug output enabled";
}

} // namespace Render::GL
