#include "sound.h"

#include <QDebug>
#include <QFileInfo>
#include <qfileinfo.h>
#include <qglobal.h>
#include <qobject.h>
#include <qstringview.h>

#include <string>

#include "miniaudio_backend.h"

Sound::Sound(const std::string& resource_id,
             const std::string& file_path,
             MiniaudioBackend* backend)
    : QObject(nullptr)
    , m_file_path(file_path)
    , m_backend(backend)
    , m_loaded(false)
    , m_volume(Sound::DEFAULT_VOLUME) {

  m_track_id = "sound_" + QString::fromStdString(resource_id);

  QFileInfo const file_info(QString::fromStdString(m_file_path));
  if (!file_info.exists()) {
    qWarning() << "Sound: File does not exist:" << file_info.absoluteFilePath();
    return;
  }

  if (m_backend != nullptr) {
    m_loaded = m_backend->predecode(m_track_id, file_info.absoluteFilePath());
  }
}

Sound::~Sound() = default;

void Sound::set_backend(MiniaudioBackend* backend) {
  if (m_backend == backend) {
    return;
  }

  m_backend = backend;

  if ((m_backend != nullptr) && !m_loaded) {
    QFileInfo const file_info(QString::fromStdString(m_file_path));
    if (file_info.exists()) {
      m_loaded = m_backend->predecode(m_track_id, file_info.absoluteFilePath());
    }
  }
}

auto Sound::is_loaded() const -> bool {
  return m_loaded.load();
}

auto Sound::is_playing() const -> bool {
  if ((m_backend == nullptr) || !m_loaded) {
    return false;
  }

  return m_backend->is_sound_active(m_track_id);
}

void Sound::play(float volume, bool loop) {
  if ((m_backend == nullptr) || !m_loaded) {
    qWarning() << "Sound: Cannot play - backend not available or not loaded";
    return;
  }

  m_volume = volume;
  m_backend->play_sound(m_track_id, volume, loop);
}

void Sound::stop() {
  if ((m_backend == nullptr) || !m_loaded) {
    return;
  }

  m_backend->stop_sound(m_track_id);
}

void Sound::set_volume(float volume) {
  m_volume = volume;
}
