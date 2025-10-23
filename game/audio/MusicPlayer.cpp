#include "MusicPlayer.h"
#include "MiniaudioBackend.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QMetaObject>
#include <QTimer>
#include <QtGlobal>

using namespace Game::Audio;

static inline void requireGuiThread(const char *where) {
  if (!QCoreApplication::instance() ||
      QThread::currentThread() != QCoreApplication::instance()->thread()) {
    qFatal("%s must be called on the GUI thread", where);
  }
}

MusicPlayer &MusicPlayer::getInstance() {
  static MusicPlayer instance;
  return instance;
}

MusicPlayer::MusicPlayer() : QObject(nullptr) {}
MusicPlayer::~MusicPlayer() { shutdown(); }

bool MusicPlayer::initialize(int musicChannels) {
  if (m_initialized)
    return true;
  if (!QCoreApplication::instance()) {
    qWarning() << "MusicPlayer: no Q(Gui)Application instance";
    return false;
  }
  ensureOnGuiThread("MusicPlayer::initialize");

  m_channelCount = std::max(1, musicChannels);
  m_backend = new MiniaudioBackend(this);
  if (!m_backend->initialize(48000, 2, m_channelCount)) {
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
  if (!m_initialized)
    return;

  if (QCoreApplication::instance() &&
      QThread::currentThread() != QCoreApplication::instance()->thread()) {
    QMetaObject::invokeMethod(
        this, [this]() { shutdown(); }, Qt::BlockingQueuedConnection);
    return;
  }

  ensureOnGuiThread("MusicPlayer::shutdown");
  if (m_backend) {
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

  if (QCoreApplication::instance() &&
      QThread::currentThread() != QCoreApplication::instance()->thread()) {
    QMetaObject::invokeMethod(
        this, [this, trackId, filePath]() { registerTrack(trackId, filePath); },
        Qt::QueuedConnection);
    return;
  }
  ensureOnGuiThread("MusicPlayer::registerTrack");

  QFileInfo fi(QString::fromStdString(filePath));
  if (!fi.exists()) {
    qWarning() << "MusicPlayer: Missing asset"
               << QString::fromStdString(trackId) << "->"
               << fi.absoluteFilePath();
    return;
  }
  m_tracks[trackId] = fi.absoluteFilePath();

  if (m_backend) {
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
  play(id, v, loop, m_defaultChannel, 250);
}
void MusicPlayer::stop() { stop(m_defaultChannel, 150); }
void MusicPlayer::pause() { pause(m_defaultChannel); }
void MusicPlayer::resume() { resume(m_defaultChannel); }
void MusicPlayer::setVolume(float v) { setVolume(m_defaultChannel, v, 0); }

int MusicPlayer::play(const std::string &id, float vol, bool loop, int channel,
                      int fadeMs) {
  if (!m_initialized || !m_backend) {
    qWarning() << "MusicPlayer not initialized";
    return -1;
  }
  int result = -1;
  if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
    QMetaObject::invokeMethod(
        this,
        [this, id, vol, loop, channel, fadeMs, &result]() mutable {
          int ch = channel < 0 ? findFreeChannel()
                               : std::min(channel, m_channelCount - 1);
          play_gui(id, vol, loop, ch, fadeMs);
          result = ch;
        },
        Qt::BlockingQueuedConnection);
    return result;
  }
  int ch =
      channel < 0 ? findFreeChannel() : std::min(channel, m_channelCount - 1);
  play_gui(id, vol, loop, ch, fadeMs);
  return ch;
}

void MusicPlayer::stop(int ch, int ms) {
  if (!m_initialized || !m_backend)
    return;
  if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
    QMetaObject::invokeMethod(
        this, [this, ch, ms]() { stop_gui(ch, ms); }, Qt::QueuedConnection);
    return;
  }
  stop_gui(ch, ms);
}
void MusicPlayer::pause(int ch) {
  if (!m_initialized || !m_backend)
    return;
  if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
    QMetaObject::invokeMethod(
        this, [this, ch]() { pause_gui(ch); }, Qt::QueuedConnection);
    return;
  }
  pause_gui(ch);
}
void MusicPlayer::resume(int ch) {
  if (!m_initialized || !m_backend)
    return;
  if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
    QMetaObject::invokeMethod(
        this, [this, ch]() { resume_gui(ch); }, Qt::QueuedConnection);
    return;
  }
  resume_gui(ch);
}
void MusicPlayer::setVolume(int ch, float v, int ms) {
  if (!m_initialized || !m_backend)
    return;
  if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
    QMetaObject::invokeMethod(
        this, [this, ch, v, ms]() { setVolume_gui(ch, v, ms); },
        Qt::QueuedConnection);
    return;
  }
  setVolume_gui(ch, v, ms);
}
void MusicPlayer::stopAll(int ms) {
  if (!m_initialized || !m_backend)
    return;
  if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
    QMetaObject::invokeMethod(
        this, [this, ms]() { stopAll_gui(ms); }, Qt::QueuedConnection);
    return;
  }
  stopAll_gui(ms);
}
void MusicPlayer::setMasterVolume(float v, int ms) {
  if (!m_initialized || !m_backend)
    return;
  if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
    QMetaObject::invokeMethod(
        this, [this, v, ms]() { setMasterVolume_gui(v, ms); },
        Qt::QueuedConnection);
    return;
  }
  setMasterVolume_gui(v, ms);
}

bool MusicPlayer::isPlaying() const {
  return m_backend && m_backend->anyChannelPlaying();
}
bool MusicPlayer::isPlaying(int ch) const {
  return m_backend && m_backend->channelPlaying(ch);
}

void MusicPlayer::ensureOnGuiThread(const char *where) const {
  requireGuiThread(where);
}
int MusicPlayer::findFreeChannel() const {
  if (!m_backend)
    return 0;
  for (int i = 0; i < m_channelCount; ++i)
    if (!m_backend->channelPlaying(i))
      return i;
  return 0;
}

void MusicPlayer::play_gui(const std::string &id, float vol, bool loop, int ch,
                           int fadeMs) {
  if (!m_backend)
    return;
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
  m_backend->setVolume(ch, v, ms);
}
void MusicPlayer::setMasterVolume_gui(float v, int ms) {
  m_backend->setMasterVolume(v, ms);
}
void MusicPlayer::stopAll_gui(int ms) { m_backend->stopAll(ms); }
