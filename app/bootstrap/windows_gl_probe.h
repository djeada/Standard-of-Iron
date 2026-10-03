#pragma once

#include <QtGlobal>

#ifdef Q_OS_WIN

#include <QString>

namespace App::Bootstrap {

struct NativeOpenGLProbeResult {
  bool supported = false;
  bool generic_software = false;
  bool used_core_context = false;
  int requested_major = 0;
  int requested_minor = 0;
  int major = 0;
  int minor = 0;
  QString vendor = QStringLiteral("<unknown>");
  QString renderer = QStringLiteral("<unknown>");
  QString version = QStringLiteral("<unknown>");
};

auto test_native_opengl() -> NativeOpenGLProbeResult;
auto software_requested_from_argv(int argc, char* argv[]) -> bool;
void install_opengl_crash_handler();
auto opengl_crash_detected() -> bool;

} // namespace App::Bootstrap

#endif
