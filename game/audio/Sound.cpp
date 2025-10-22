#include "Sound.h"
#include "MiniaudioBackend.h"
#include <QDebug>
#include <QFileInfo>
#include <QCryptographicHash>

Sound::Sound(const std::string &filePath, MiniaudioBackend* backend)
    : QObject(nullptr)
    , m_filepath(filePath)
    , m_backend(backend)
    , m_loaded(false)
    , m_volume(1.0f) {
  
  // Generate a unique track ID from the file path
  QByteArray hash = QCryptographicHash::hash(
    QByteArray::fromStdString(filePath), 
    QCryptographicHash::Md5
  );
  m_trackId = "sound_" + QString(hash.toHex());
  
  QFileInfo fi(QString::fromStdString(m_filepath));
  if (!fi.exists()) {
    qWarning() << "Sound: File does not exist:" << fi.absoluteFilePath();
    return;
  }

  // If backend is available, predecode the sound
  if (m_backend) {
    m_loaded = m_backend->predecode(m_trackId, fi.absoluteFilePath());
    if (m_loaded) {
      qDebug() << "Sound: Loaded" << fi.absoluteFilePath();
    }
  }
}

Sound::~Sound() {
  // Sound data stays in backend cache
}

void Sound::setBackend(MiniaudioBackend* backend) {
  if (m_backend == backend) return;
  
  m_backend = backend;
  
  // Reload if we have a new backend
  if (m_backend && !m_loaded) {
    QFileInfo fi(QString::fromStdString(m_filepath));
    if (fi.exists()) {
      m_loaded = m_backend->predecode(m_trackId, fi.absoluteFilePath());
    }
  }
}

bool Sound::isLoaded() const {
  return m_loaded.load();
}

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

void Sound::stop() {
  // Note: Individual sound effect stopping not implemented yet
  // Sound effects play to completion or can be stopped by clearing all
}

void Sound::setVolume(float volume) {
  m_volume = volume;
  // Note: Volume change for already-playing sounds not implemented
  // This will affect next playback
}
