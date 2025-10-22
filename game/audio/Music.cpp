#include "Music.h"
#include <QAudioOutput>
#include <QCoreApplication>
#include <QMediaPlayer>
#include <QMetaObject>
#include <QThread>
#include <QUrl>

Music::Music(const std::string &filePath)
    : filepath(filePath), loaded(false), audioOutput(nullptr),
      mainThread(nullptr), playing(false) {

  if (!QCoreApplication::instance()) {
    return;
  }

  mainThread = QCoreApplication::instance()->thread();

  player = std::make_unique<QMediaPlayer>();

  if (mainThread && QThread::currentThread() != mainThread) {
    player->moveToThread(mainThread);
  }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  audioOutput = new QAudioOutput(player.get());
  player->setAudioOutput(audioOutput);

  player->setSource(QUrl::fromLocalFile(QString::fromStdString(filePath)));
  loaded = (player->error() == QMediaPlayer::NoError);
#else
  player->setMedia(QUrl::fromLocalFile(QString::fromStdString(filePath)));
  loaded = (player->mediaStatus() != QMediaPlayer::InvalidMedia &&
            player->mediaStatus() != QMediaPlayer::NoMedia);
#endif
}

Music::~Music() { cleanupPlayer(); }

void Music::cleanupPlayer() {
  if (!player) {
    return;
  }

  if (!mainThread || QThread::currentThread() == mainThread) {
    player->stop();
    player.reset();
  } else {
    QMediaPlayer *rawPlayer = player.release();
    if (QCoreApplication::instance()) {
      QMetaObject::invokeMethod(
          QCoreApplication::instance(),
          [rawPlayer]() {
            if (rawPlayer) {
              rawPlayer->stop();
              delete rawPlayer;
            }
          },
          Qt::QueuedConnection);
    } else {
      delete rawPlayer;
    }
  }
}

bool Music::isLoaded() const { return loaded; }

void Music::play(float volume, bool loop) {
  if (!player || !loaded) {
    return;
  }

  // Already playing, just update volume
  if (playing) {
    setVolume(volume);
    return;
  }

  playing = true;
  QMediaPlayer *p = player.get();

  QMetaObject::invokeMethod(
      p,
      [p, volume, loop, this]() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        // Audio output already set in constructor, just update volume and play
        if (audioOutput) {
          audioOutput->setVolume(volume);
        }
        p->setLoops(loop ? QMediaPlayer::Infinite : 1);
#else
        p->setVolume(static_cast<int>(volume * 100));
        if (loop) {
          QObject::connect(p, &QMediaPlayer::mediaStatusChanged,
                           [p](QMediaPlayer::MediaStatus status) {
                             if (status == QMediaPlayer::EndOfMedia) {
                               p->play();
                             }
                           });
        }
#endif
        p->play();
      },
      Qt::QueuedConnection);
}

void Music::stop() {
  if (!player) {
    return;
  }

  playing = false;
  QMediaPlayer *p = player.get();
  QMetaObject::invokeMethod(p, [p]() { p->stop(); }, Qt::QueuedConnection);
}

void Music::pause() {
  if (!player) {
    return;
  }

  QMediaPlayer *p = player.get();
  QMetaObject::invokeMethod(p, [p]() { p->pause(); }, Qt::QueuedConnection);
}

void Music::resume() {
  if (!player) {
    return;
  }

  QMediaPlayer *p = player.get();
  QMetaObject::invokeMethod(
      p,
      [p]() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        if (p->playbackState() == QMediaPlayer::PausedState) {
          p->play();
        }
#else
        if (p->state() == QMediaPlayer::PausedState) {
          p->play();
        }
#endif
      },
      Qt::QueuedConnection);
}

void Music::setVolume(float volume) {
  if (!player) {
    return;
  }

  QMediaPlayer *p = player.get();
  QMetaObject::invokeMethod(
      p,
      [p, volume]() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        if (p->audioOutput()) {
          p->audioOutput()->setVolume(volume);
        }
#else
        p->setVolume(static_cast<int>(volume * 100));
#endif
      },
      Qt::QueuedConnection);
}
