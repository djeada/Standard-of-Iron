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

Sound::Sound(const std::string &filePath, MiniaudioBackend *backend)
    : QObject(nullptr), m_filepath(filePath), m_backend(backend),
      m_loaded(false), m_volume(1.0F) {

  QByteArray const hash = QCryptographicHash::hash(
      QByteArray::fromStdString(filePath), QCryptographicHash::Md5);
  m_trackId = "sound_" + QString(hash.toHex());

  QFileInfo const fi(QString::fromStdString(m_filepath));
  if (!fi.exists()) {
    qWarning() << "Sound: File does not exist:" << fi.absoluteFilePath();
    return;
  }

  if (m_backend != nullptr) {
    m_loaded = m_backend->predecode(m_trackId, fi.absoluteFilePath());
    if (m_loaded) {
      qDebug() << "Sound: Loaded" << fi.absoluteFilePath();
    }
  }
}

Sound::~Sound() = default;

void Sound::setBackend(MiniaudioBackend *backend) {
  if (m_backend == backend) {
    return;
  }

  m_backend = backend;

  if ((m_backend != nullptr) && !m_loaded) {
    QFileInfo const fi(QString::fromStdString(m_filepath));
    if (fi.exists()) {
      m_loaded = m_backend->predecode(m_trackId, fi.absoluteFilePath());
    }
  }
}

auto Sound::isLoaded() const -> bool { return m_loaded.load(); }

void Sound::play(float volume, bool loop) {
  if ((m_backend == nullptr) || !m_loaded) {
    qWarning() << "Sound: Cannot play - backend not available or not loaded";
    return;
  }

  m_volume = volume;
  m_backend->playSound(m_trackId, volume, loop);

  qDebug() << "Sound: Playing" << QString::fromStdString(m_filepath)
           << "volume:" << volume << "loop:" << loop;
}

void Sound::stop() {}

void Sound::setVolume(float volume) { m_volume = volume; }
