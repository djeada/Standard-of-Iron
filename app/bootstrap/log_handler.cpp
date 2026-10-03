#include "app/bootstrap/log_handler.h"

#include <QByteArray>
#include <QDebug>
#include <QString>

#include <cstdio>
#include <cstdlib>

namespace App::Bootstrap {

namespace {

void log_message_handler(QtMsgType type,
                         const QMessageLogContext& context,
                         const QString& msg) {
  QByteArray const local_msg = msg.toLocal8Bit();
  const char* file = (context.file != nullptr) ? context.file : "";
  const char* function = (context.function != nullptr) ? context.function : "";

  FILE* out = stderr;
  switch (type) {
  case QtDebugMsg:
    fprintf(out,
            "[DEBUG] %s (%s:%u, %s)\n",
            local_msg.constData(),
            file,
            context.line,
            function);
    break;
  case QtInfoMsg:
    fprintf(out, "[INFO] %s\n", local_msg.constData());
    break;
  case QtWarningMsg:
    fprintf(out,
            "[WARNING] %s (%s:%u, %s)\n",
            local_msg.constData(),
            file,
            context.line,
            function);

    if (msg.contains("OpenGL", Qt::CaseInsensitive) ||
        msg.contains("scene graph", Qt::CaseInsensitive) ||
        msg.contains("RHI", Qt::CaseInsensitive)) {
      fprintf(out,
              "[HINT] If you see crashes, try software rendering: set "
              "QT_OPENGL=software\n");
    }
    break;
  case QtCriticalMsg:
    fprintf(out,
            "[CRITICAL] %s (%s:%u, %s)\n",
            local_msg.constData(),
            file,
            context.line,
            function);
    if (msg.contains("scene graph is not using OpenGL", Qt::CaseInsensitive)) {
      fprintf(out,
              "[CRITICAL] Do not use QT_QUICK_BACKEND=software; the game "
              "requires Qt Quick's OpenGL backend\n");
    } else if (msg.contains("OpenGL", Qt::CaseInsensitive) ||
               msg.contains("scene graph", Qt::CaseInsensitive) ||
               msg.contains("RHI", Qt::CaseInsensitive) ||
               msg.contains("graphics", Qt::CaseInsensitive)) {
      fprintf(out, "[CRITICAL] Try running with software OpenGL if this persists\n");
    }
    break;
  case QtFatalMsg:
    fprintf(out,
            "[FATAL] %s (%s:%u, %s)\n",
            local_msg.constData(),
            file,
            context.line,
            function);
    fprintf(out, "[FATAL] === RECOVERY SUGGESTION ===\n");
    fprintf(out, "[FATAL] Run: run_debug_softwaregl.cmd\n");
    fprintf(out, "[FATAL] Or set: QT_OPENGL=software\n");
    abort();
  }
  fflush(out);
}

} // namespace

void install_log_message_handler() {
  qInstallMessageHandler(log_message_handler);
}

} // namespace App::Bootstrap
