#include "profiling_hud.h"

namespace Render::Profiling {

ProfilingHud::ProfilingHud(QObject *parent) : QObject(parent) {
  m_timer.setInterval(250);
  m_timer.setSingleShot(false);
  QObject::connect(&m_timer, &QTimer::timeout, this, &ProfilingHud::refresh);
  m_timer.start();
  refresh();
}

void ProfilingHud::set_enabled(bool on) {
  if (global_profile().enabled == on) {
    return;
  }
  global_profile().enabled = on;
  if (!on) {
    global_profile().reset();
  }
  emit enabled_changed();
  refresh();
}

void ProfilingHud::refresh() {
  QString const next = QString::fromStdString(format_overlay(global_profile()));
  bool const changed = next != m_overlay;
  m_overlay = next;
  if (changed) {
    emit overlay_changed();
  }
}

} // namespace Render::Profiling
