#include "Sound.h"
#include <QCoreApplication>
#include <QMetaObject>
#include <QSoundEffect>
#include <QThread>
#include <QUrl>

Sound::Sound(const std::string &filePath)
    : filepath(filePath), loaded(false), mainThread(nullptr) {

  if (!QCoreApplication::instance()) {
    return;
  }

  mainThread = QCoreApplication::instance()->thread();

  soundEffect = std::make_unique<QSoundEffect>();

  if (mainThread && QThread::currentThread() != mainThread) {
    soundEffect->moveToThread(mainThread);
  }

  soundEffect->setSource(QUrl::fromLocalFile(QString::fromStdString(filePath)));

  loaded = (soundEffect->status() == QSoundEffect::Ready ||
            soundEffect->status() == QSoundEffect::Loading);
}

Sound::~Sound() { cleanupSoundEffect(); }

void Sound::cleanupSoundEffect() {
  if (!soundEffect) {
    return;
  }

  if (!mainThread || QThread::currentThread() == mainThread) {
    soundEffect->stop();
    soundEffect.reset();
  } else {
    QSoundEffect *rawEffect = soundEffect.release();
    if (QCoreApplication::instance()) {
      QMetaObject::invokeMethod(
          QCoreApplication::instance(),
          [rawEffect]() {
            if (rawEffect) {
              rawEffect->stop();
              delete rawEffect;
            }
          },
          Qt::QueuedConnection);
    } else {
      delete rawEffect;
    }
  }
}

bool Sound::isLoaded() const { return loaded; }

void Sound::play(float volume, bool loop) {
  if (!soundEffect || !loaded) {
    return;
  }

  QSoundEffect *se = soundEffect.get();
  QMetaObject::invokeMethod(
      se,
      [se, volume, loop]() {
        se->setVolume(volume);
        se->setLoopCount(loop ? QSoundEffect::Infinite : 1);
        se->play();
      },
      Qt::QueuedConnection);
}

void Sound::stop() {
  if (!soundEffect) {
    return;
  }

  QSoundEffect *se = soundEffect.get();
  QMetaObject::invokeMethod(se, [se]() { se->stop(); }, Qt::QueuedConnection);
}

void Sound::setVolume(float volume) {
  if (!soundEffect) {
    return;
  }

  QSoundEffect *se = soundEffect.get();
  QMetaObject::invokeMethod(
      se, [se, volume]() { se->setVolume(volume); }, Qt::QueuedConnection);
}
