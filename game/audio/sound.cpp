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
             Game::Audio::Mastering::Material material,
             MiniaudioBackend* backend)
    : QObject(nullptr)
    , m_file_path(file_path)
    , m_material(material)
    , m_backend(backend)
    , m_registered(false)
    , m_volume(Sound::DEFAULT_VOLUME) {

  m_track_id = "sound_" + QString::fromStdString(resource_id);

  QFileInfo const file_info(QString::fromStdString(m_file_path));
  if (!file_info.exists()) {
    qWarning() << "Sound: File does not exist:" << file_info.absoluteFilePath();
    return;
  }

  if (m_backend != nullptr) {
    m_registered =
        m_backend->request_track(m_track_id, file_info.absoluteFilePath(), m_material);
  }
}

Sound::~Sound() = default;

void Sound::set_backend(MiniaudioBackend* backend) {
  if (m_backend == backend) {
    return;
  }

  m_backend = backend;

  if ((m_backend != nullptr) && !m_registered) {
    QFileInfo const file_info(QString::fromStdString(m_file_path));
    if (file_info.exists()) {
      m_registered = m_backend->request_track(
          m_track_id, file_info.absoluteFilePath(), m_material);
    }
  }
}

auto Sound::is_registered() const -> bool {
  return m_registered.load();
}

auto Sound::is_playing() const -> bool {
  if ((m_backend == nullptr) || !m_registered) {
    return false;
  }

  return m_backend->is_sound_active(m_track_id);
}

void Sound::play(float volume, bool loop) {
  if ((m_backend == nullptr) || !m_registered) {
    qWarning() << "Sound: Cannot play - backend unavailable or asset not registered";
    return;
  }

  m_volume = volume;
  m_backend->play_sound(m_track_id, volume, loop);
}

void Sound::stop() {
  if ((m_backend == nullptr) || !m_registered) {
    return;
  }

  m_backend->stop_sound(m_track_id);
}

void Sound::set_volume(float volume) {
  m_volume = volume;
}

void Sound::set_playing_volume(float volume, int fade_ms) {
  m_volume = volume;
  if (m_backend != nullptr) {
    m_backend->set_sound_volume(m_track_id, volume, fade_ms);
  }
}
