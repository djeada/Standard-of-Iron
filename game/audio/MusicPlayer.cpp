#include "MusicPlayer.h"
#include "AudioConstants.h"
#include "MiniaudioBackend.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QMetaObject>
#include <QTimer>
#include <QtGlobal>
#include <algorithm>
#include <qcoreapplication.h>
#include <qfileinfo.h>
#include <qglobal.h>
#include <qhashfunctions.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qobjectdefs.h>
#include <qthread.h>
#include <string>

using namespace Game::Audio;

static inline void requireGuiThread(const char *where) {
  if ((QCoreApplication::instance() == nullptr) ||
      QThread::currentThread() != QCoreApplication::instance()->thread()) {
    qFatal("%s must be called on the GUI thread", where);
  }
}

auto MusicPlayer::getInstance() -> MusicPlayer & {
  static MusicPlayer instance;
  return instance;
}

MusicPlayer::MusicPlayer() : QObject(nullptr) {}
MusicPlayer::~MusicPlayer() { shutdown(); }

auto MusicPlayer::initialize(int musicChannels) -> bool {
  static constexpr int MIN_CHANNELS = 1;

  if (m_initialized) {
    return true;
  }
  if (QCoreApplication::instance() == nullptr) {
    qWarning() << "MusicPlayer: no Q(Gui)Application instance";
    return false;
  }
  ensureOnGuiThread("MusicPlayer::initialize");

  m_channelCount = std::max(MIN_CHANNELS, musicChannels);
  m_backend = new MiniaudioBackend(this);
  if (!m_backend->initialize(AudioConstants::DEFAULT_SAMPLE_RATE,
                             AudioConstants::DEFAULT_OUTPUT_CHANNELS,
                             m_channelCount)) {
    qWarning() << "MusicPlayer: backend init failed";
    m_backend->deleteLater();
    m_backend = nullptr;
    return false;
  }

  m_initialized = true;
  qInfo() << "MusicPlayer initialized (miniaudio backend) channels:"
          << m_channelCount;
  return true;
}

void MusicPlayer::shutdown() {
  if (!m_initialized) {
    return;
  }

  if ((QCoreApplication::instance() != nullptr) &&
      QThread::currentThread() != QCoreApplication::instance()->thread()) {
    QMetaObject::invokeMethod(
        this, [this]() { shutdown(); }, Qt::BlockingQueuedConnection);
    return;
  }

  ensureOnGuiThread("MusicPlayer::shutdown");
  if (m_backend != nullptr) {
    m_backend->shutdown();
    m_backend->deleteLater();
    m_backend = nullptr;
  }
  m_tracks.clear();
  m_channelCount = 0;
  m_initialized = false;
}

void MusicPlayer::registerTrack(const std::string &trackId,
                                const std::string &filePath) {

  if ((QCoreApplication::instance() != nullptr) &&
      QThread::currentThread() != QCoreApplication::instance()->thread()) {
    QMetaObject::invokeMethod(
        this, [this, trackId, filePath]() { registerTrack(trackId, filePath); },
        Qt::QueuedConnection);
    return;
  }
  ensureOnGuiThread("MusicPlayer::registerTrack");

  QFileInfo const fi(QString::fromStdString(filePath));
  if (!fi.exists()) {
    qWarning() << "MusicPlayer: Missing asset"
               << QString::fromStdString(trackId) << "->"
               << fi.absoluteFilePath();
    return;
  }
  m_tracks[trackId] = fi.absoluteFilePath();

  if (m_backend != nullptr) {
    if (!m_backend->predecode(QString::fromStdString(trackId),
                              fi.absoluteFilePath())) {
      qWarning() << "MusicPlayer: predecode failed for"
                 << fi.absoluteFilePath();
    } else {
      qDebug() << "MusicPlayer: predecoded" << fi.absoluteFilePath();
    }
  }
}

void MusicPlayer::play(const std::string &id, float v, bool loop) {
  play(id, v, loop, m_defaultChannel, AudioConstants::DEFAULT_FADE_IN_MS);
}
void MusicPlayer::stop() {
  stop(m_defaultChannel, AudioConstants::DEFAULT_FADE_OUT_MS);
}
void MusicPlayer::pause() { pause(m_defaultChannel); }
void MusicPlayer::resume() { resume(m_defaultChannel); }
void MusicPlayer::setVolume(float v) {
  setVolume(m_defaultChannel, v, AudioConstants::NO_FADE_MS);
}

auto MusicPlayer::play(const std::string &id, float vol, bool loop, int channel,
                       int fadeMs) -> int {
  if (!m_initialized || (m_backend == nullptr)) {
    qWarning() << "MusicPlayer not initialized";
    return -1;
  }
  int result = -1;
  if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
    QMetaObject::invokeMethod(
        this,
        [this, id, vol, loop, channel, fadeMs, &result]() mutable {
          int const ch = channel < 0 ? findFreeChannel()
                                     : std::min(channel, m_channelCount - 1);
          play_gui(id, vol, loop, ch, fadeMs);
          result = ch;
        },
        Qt::BlockingQueuedConnection);
    return result;
  }
  int const ch =
      channel < 0 ? findFreeChannel() : std::min(channel, m_channelCount - 1);
  play_gui(id, vol, loop, ch, fadeMs);
  return ch;
}

void MusicPlayer::stop(int ch, int ms) {
  if (!m_initialized || (m_backend == nullptr)) {
    return;
  }
  if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
    QMetaObject::invokeMethod(
        this, [this, ch, ms]() { stop_gui(ch, ms); }, Qt::QueuedConnection);
    return;
  }
  stop_gui(ch, ms);
}
void MusicPlayer::pause(int ch) {
  if (!m_initialized || (m_backend == nullptr)) {
    return;
  }
  if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
    QMetaObject::invokeMethod(
        this, [this, ch]() { pause_gui(ch); }, Qt::QueuedConnection);
    return;
  }
  pause_gui(ch);
}
void MusicPlayer::resume(int ch) {
  if (!m_initialized || (m_backend == nullptr)) {
    return;
  }
  if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
    QMetaObject::invokeMethod(
        this, [this, ch]() { resume_gui(ch); }, Qt::QueuedConnection);
    return;
  }
  resume_gui(ch);
}
void MusicPlayer::setVolume(int ch, float v, int ms) {
  if (!m_initialized || (m_backend == nullptr)) {
    return;
  }
  if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
    QMetaObject::invokeMethod(
        this, [this, ch, v, ms]() { setVolume_gui(ch, v, ms); },
        Qt::QueuedConnection);
    return;
  }
  setVolume_gui(ch, v, ms);
}
void MusicPlayer::stopAll(int ms) {
  if (!m_initialized || (m_backend == nullptr)) {
    return;
  }
  if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
    QMetaObject::invokeMethod(
        this, [this, ms]() { stopAll_gui(ms); }, Qt::QueuedConnection);
    return;
  }
  stopAll_gui(ms);
}
void MusicPlayer::setMasterVolume(float v, int ms) {
  if (!m_initialized || (m_backend == nullptr)) {
    return;
  }
  if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
    QMetaObject::invokeMethod(
        this, [this, v, ms]() { setMasterVolume_gui(v, ms); },
        Qt::QueuedConnection);
    return;
  }
  setMasterVolume_gui(v, ms);
}

auto MusicPlayer::isPlaying() const -> bool {
  return (m_backend != nullptr) && m_backend->any_channel_playing();
}
auto MusicPlayer::isPlaying(int ch) const -> bool {
  return (m_backend != nullptr) && m_backend->channel_playing(ch);
}

void MusicPlayer::ensureOnGuiThread(const char *where) {
  requireGuiThread(where);
}
auto MusicPlayer::findFreeChannel() const -> int {
  if (m_backend == nullptr) {
    return 0;
  }
  for (int i = 0; i < m_channelCount; ++i) {
    if (!m_backend->channel_playing(i)) {
      return i;
    }
  }
  return 0;
}

void MusicPlayer::play_gui(const std::string &id, float vol, bool loop, int ch,
                           int fadeMs) {
  if (m_backend == nullptr) {
    return;
  }
  auto it = m_tracks.find(id);
  if (it == m_tracks.end()) {
    qWarning() << "Unknown trackId:" << QString::fromStdString(id);
    return;
  }
  m_backend->play(ch, QString::fromStdString(id), vol, loop, fadeMs);
}
void MusicPlayer::stop_gui(int ch, int ms) { m_backend->stop(ch, ms); }
void MusicPlayer::pause_gui(int ch) { m_backend->pause(ch); }
void MusicPlayer::resume_gui(int ch) { m_backend->resume(ch); }
void MusicPlayer::setVolume_gui(int ch, float v, int ms) {
  m_backend->set_volume(ch, v, ms);
}
void MusicPlayer::setMasterVolume_gui(float v, int ms) {
  m_backend->set_master_volume(v, ms);
}
void MusicPlayer::stopAll_gui(int ms) { m_backend->stop_all(ms); }
