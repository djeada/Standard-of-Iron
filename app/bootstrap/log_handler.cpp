#include "app/bootstrap/log_handler.h"

#include <QByteArray>
#include <QDebug>
#include <QMutex>
#include <QMutexLocker>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "app/bootstrap/startup_supervisor.h"

namespace App::Bootstrap {

namespace {

// Enough to hold the last few seconds of a busy frame, small enough that a
// bundle stays attachable. The tail is what a bug report needs; the beginning of
// a session is reproducible by starting the game again.
constexpr int k_max_recent_lines = 2000;

QMutex g_recent_mutex;
QStringList g_recent_lines;

void record_recent_line(const QString& line) {
  QMutexLocker const lock(&g_recent_mutex);
  g_recent_lines.append(line);
  if (g_recent_lines.size() > k_max_recent_lines) {
    g_recent_lines.remove(0, g_recent_lines.size() - k_max_recent_lines);
  }
}

auto severity_label(QtMsgType type) -> const char* {
  switch (type) {
  case QtDebugMsg:
    return "[DEBUG]";
  case QtInfoMsg:
    return "[INFO]";
  case QtWarningMsg:
    return "[WARNING]";
  case QtCriticalMsg:
    return "[CRITICAL]";
  case QtFatalMsg:
    return "[FATAL]";
  }
  return "[LOG]";
}

// The line the console prints, minus the bits the console adds outside the
// message (hints, recovery advice): a bundle reader gets what the game said, not
// what the handler told the user to try.
auto bundle_line(QtMsgType type,
                 const QMessageLogContext& context,
                 const QString& msg) -> QString {
  QString line = QString::fromLatin1(severity_label(type)) + QLatin1Char(' ') + msg;
  if (context.file != nullptr) {
    line += QStringLiteral(" (%1:%2)")
                .arg(QString::fromUtf8(context.file))
                .arg(context.line);
  }
  return line;
}

void log_message_handler(QtMsgType type,
                         const QMessageLogContext& context,
                         const QString& msg) {

  App::Bootstrap::tee_startup_log(msg);
  record_recent_line(bundle_line(type, context, msg));
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

auto recent_log_lines(int max_lines) -> QStringList {
  if (max_lines <= 0) {
    return {};
  }
  QMutexLocker const lock(&g_recent_mutex);
  const int first = std::max(0, static_cast<int>(g_recent_lines.size()) - max_lines);
  return g_recent_lines.mid(first);
}

void clear_recent_log_lines() {
  QMutexLocker const lock(&g_recent_mutex);
  g_recent_lines.clear();
}

} // namespace App::Bootstrap
