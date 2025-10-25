#include "Sound.h"
#include "MiniaudioBackend.h"
#include <QCryptographicHash>
#include <QDebug>
#include <QFileInfo>

Sound::Sound(const std::string &filePath, MiniaudioBackend *backend)
    : QObject(nullptr), m_filepath(filePath), m_backend(backend),
      m_loaded(false), m_volume(1.0f) {

  QByteArray hash = QCryptographicHash::hash(
      QByteArray::fromStdString(filePath), QCryptographicHash::Md5);
  m_trackId = "sound_" + QString(hash.toHex());

  QFileInfo fi(QString::fromStdString(m_filepath));
  if (!fi.exists()) {
    qWarning() << "Sound: File does not exist:" << fi.absoluteFilePath();
    return;
  }

  if (m_backend) {
    m_loaded = m_backend->predecode(m_trackId, fi.absoluteFilePath());
    if (m_loaded) {
      qDebug() << "Sound: Loaded" << fi.absoluteFilePath();
    }
  }
}

Sound::~Sound() {}

void Sound::setBackend(MiniaudioBackend *backend) {
  if (m_backend == backend) {
    return;
  }

  m_backend = backend;

  if (m_backend && !m_loaded) {
    QFileInfo fi(QString::fromStdString(m_filepath));
    if (fi.exists()) {
      m_loaded = m_backend->predecode(m_trackId, fi.absoluteFilePath());
    }
  }
}

bool Sound::isLoaded() const { return m_loaded.load(); }

void Sound::play(float volume, bool loop) {
  if (!m_backend || !m_loaded) {
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
