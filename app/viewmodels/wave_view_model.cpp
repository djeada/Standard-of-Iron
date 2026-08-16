#include "app/viewmodels/wave_view_model.h"

namespace App::ViewModels {

WaveViewModel::WaveViewModel(QObject* parent)
    : QObject(parent) {
}

void WaveViewModel::set_status(const QVariantMap& status) {
  if (m_status == status) {
    return;
  }
  m_status = status;
  emit status_changed();
}

void WaveViewModel::clear() {
  if (m_status.isEmpty()) {
    return;
  }
  m_status.clear();
  emit status_changed();
}

auto WaveViewModel::active() const -> bool {
  return m_status.value("active", false).toBool();
}

auto WaveViewModel::total_phases() const -> int {
  return m_status.value("total", 0).toInt();
}

auto WaveViewModel::cleared_phases() const -> int {
  return m_status.value("cleared", 0).toInt();
}

auto WaveViewModel::current_phase() const -> int {
  return m_status.value("current", 0).toInt();
}

auto WaveViewModel::seconds_until_next() const -> qreal {
  return m_status.value("seconds_until_next", -1.0).toReal();
}

auto WaveViewModel::warning() const -> bool {
  return m_status.value("warning", false).toBool();
}

auto WaveViewModel::live_enemies() const -> int {
  return m_status.value("live_enemies", 0).toInt();
}

auto WaveViewModel::state() const -> QString {
  return m_status.value("state", QStringLiteral("idle")).toString();
}

auto WaveViewModel::alerts() const -> QVariantList {
  return m_status.value("alerts").toList();
}

} // namespace App::ViewModels
