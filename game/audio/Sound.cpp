#include "Sound.h"
#include "MiniaudioBackend.h"
#include <QCryptographicHash>
#include <QDebug>
#include <QFileInfo>
#include <qcryptographichash.h>
#include <qfileinfo.h>
#include <qglobal.h>
#include <qobject.h>
#include <qstringview.h>
#include <string>

Sound::Sound(const std::string &file_path, MiniaudioBackend *backend)
    : QObject(nullptr), m_file_path(file_path), m_backend(backend),
      m_loaded(false), m_volume(Sound::DEFAULT_VOLUME) {

  QByteArray const hash = QCryptographicHash::hash(
      QByteArray::fromStdString(file_path), QCryptographicHash::Md5);
  m_track_id = "sound_" + QString(hash.toHex());

  QFileInfo const file_info(QString::fromStdString(m_file_path));
  if (!file_info.exists()) {
    qWarning() << "Sound: File does not exist:" << file_info.absoluteFilePath();
    return;
  }

  if (m_backend != nullptr) {
    m_loaded = m_backend->predecode(m_track_id, file_info.absoluteFilePath());
    if (m_loaded) {
      qDebug() << "Sound: Loaded" << file_info.absoluteFilePath();
    }
  }
}

Sound::~Sound() = default;

void Sound::set_backend(MiniaudioBackend *backend) {
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

auto Sound::is_loaded() const -> bool { return m_loaded.load(); }

void Sound::play(float volume, bool loop) {
  if ((m_backend == nullptr) || !m_loaded) {
    qWarning() << "Sound: Cannot play - backend not available or not loaded";
    return;
  }

  m_volume = volume;
  m_backend->play_sound(m_track_id, volume, loop);

  qDebug() << "Sound: Playing" << QString::fromStdString(m_file_path)
           << "volume:" << volume << "loop:" << loop;
}

void Sound::stop() {}

void Sound::set_volume(float volume) { m_volume = volume; }
