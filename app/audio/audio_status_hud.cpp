#include "app/audio/audio_status_hud.h"

#include <qglobal.h>

#include "game/audio/cue_trace.h"

namespace App::Audio {

AudioStatusHud::AudioStatusHud(QObject* parent)
    : QObject(parent) {
  if (qEnvironmentVariableIntValue("SOI_AUDIO_HUD") != 0) {
    m_enabled = true;
  }

  m_timer.setInterval(250);
  m_timer.setSingleShot(false);
  QObject::connect(&m_timer, &QTimer::timeout, this, &AudioStatusHud::refresh);
  m_timer.start();
  refresh();
}

void AudioStatusHud::set_enabled(bool on) {
  if (m_enabled == on) {
    return;
  }
  m_enabled = on;
  emit enabled_changed();
  refresh();
}

void AudioStatusHud::refresh() {
  if (!m_enabled) {
    return;
  }

  const QString next = QString::fromStdString(Game::Audio::format_status_overlay());
  if (next == m_overlay) {
    return;
  }
  m_overlay = next;
  emit overlay_changed();
}

} // namespace App::Audio
