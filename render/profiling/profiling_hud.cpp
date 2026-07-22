#include "profiling_hud.h"

namespace Render::Profiling {

ProfilingHud::ProfilingHud(QObject* parent)
    : QObject(parent) {
#if defined(SOI_ENABLE_RUNTIME_TRACING)
  m_timer.setInterval(250);
  m_timer.setSingleShot(false);
  QObject::connect(&m_timer, &QTimer::timeout, this, &ProfilingHud::refresh);
  m_timer.start();
  refresh();
#endif
}

void ProfilingHud::set_enabled(bool on) {
#if defined(SOI_ENABLE_RUNTIME_TRACING)
  if (global_profile().enabled == on) {
    return;
  }
  global_profile().enabled = on;
  if (!on) {
    global_profile().reset();
  }
  emit enabled_changed();
  refresh();
#else
  (void)on;
#endif
}

void ProfilingHud::refresh() {
#if defined(SOI_ENABLE_RUNTIME_TRACING)
  QString const next = QString::fromStdString(format_overlay(global_profile()));
  bool const changed = next != m_overlay;
  m_overlay = next;
  if (changed) {
    emit overlay_changed();
  }
#endif
}

} // namespace Render::Profiling
