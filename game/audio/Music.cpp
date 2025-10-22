#include "Music.h"
#include <QAudioOutput>
#include <QCoreApplication>
#include <QMediaPlayer>
#include <QMetaObject>
#include <QThread>
#include <QTimer>
#include <QUrl>

Music::Music(const std::string &filePath)
    : filepath(filePath), loaded(false), audioOutput(nullptr),
      mainThread(nullptr), playing(false), markedForDeletion(false) {

  if (!QCoreApplication::instance()) {
    return;
  }

  mainThread = QCoreApplication::instance()->thread();
  player = new QMediaPlayer();

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  audioOutput = new QAudioOutput(player);
  player->setAudioOutput(audioOutput);

  QObject::connect(player, &QMediaPlayer::errorOccurred,
                   [filepath = this->filepath](QMediaPlayer::Error error,
                                               const QString &desc) {
                     qWarning() << "QMediaPlayer error for"
                                << QString::fromStdString(filepath)
                                << "- Error code:" << static_cast<int>(error)
                                << "Message:" << desc;
                   });

  QObject::connect(
      player, &QMediaPlayer::mediaStatusChanged,
      [filepath = this->filepath, this](QMediaPlayer::MediaStatus status) {
        qDebug() << "Media status for" << QString::fromStdString(filepath)
                 << ":" << static_cast<int>(status);
        if (status == QMediaPlayer::EndOfMedia) {
          playing = false;
        }
      });

  QObject::connect(
      player, &QMediaPlayer::playbackStateChanged,
      [filepath = this->filepath](QMediaPlayer::PlaybackState state) {
        qDebug() << "Playback state for" << QString::fromStdString(filepath)
                 << ":" << static_cast<int>(state);
      });

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
  if (!player || markedForDeletion) {
    return;
  }

  markedForDeletion = true;
  QMediaPlayer *rawPlayer = player.data();

  if (!rawPlayer) {
    return;
  }

  if (QCoreApplication::instance() && mainThread) {
    rawPlayer->deleteLater();
  }
}

bool Music::isLoaded() const { return loaded && !markedForDeletion; }

void Music::play(float volume, bool loop) {
  if (!player || !loaded || markedForDeletion) {
    return;
  }

  QPointer<QMediaPlayer> p = player;
  QAudioOutput *output = audioOutput;

  if (!p || !QCoreApplication::instance()) {
    return;
  }

  QMetaObject::invokeMethod(
      QCoreApplication::instance(),
      [p, output, volume, loop, this]() {
        if (!p) {
          return;
        }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)

        bool isCurrentlyPlaying =
            (p->playbackState() == QMediaPlayer::PlayingState);

        if (output) {
          output->setVolume(volume);
        }
        p->setLoops(loop ? QMediaPlayer::Infinite : 1);

        if (!isCurrentlyPlaying) {
          qDebug() << "Starting playback for"
                   << QString::fromStdString(filepath);
          playing = true;
          p->play();
        } else {
          qDebug() << "Already playing" << QString::fromStdString(filepath)
                   << "- updating volume only";
        }
#else
        p->setVolume(static_cast<int>(volume * 100));
        playing = true;
        p->play();
#endif
      },
      Qt::QueuedConnection);
}

void Music::stop() {
  if (!player || markedForDeletion) {
    return;
  }

  playing = false;
  QPointer<QMediaPlayer> p = player;
  if (!p || !QCoreApplication::instance()) {
    return;
  }

  QMetaObject::invokeMethod(
      QCoreApplication::instance(),
      [p]() {
        if (p) {
          p->stop();
        }
      },
      Qt::QueuedConnection);
}

void Music::pause() {
  if (!player || markedForDeletion) {
    return;
  }

  QPointer<QMediaPlayer> p = player;
  if (!p || !QCoreApplication::instance()) {
    return;
  }

  QMetaObject::invokeMethod(
      QCoreApplication::instance(),
      [p]() {
        if (p) {
          p->pause();
        }
      },
      Qt::QueuedConnection);
}

void Music::resume() {
  if (!player || markedForDeletion) {
    return;
  }

  QPointer<QMediaPlayer> p = player;
  if (!p || !QCoreApplication::instance()) {
    return;
  }

  QMetaObject::invokeMethod(
      QCoreApplication::instance(),
      [p]() {
        if (!p) {
          return;
        }
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
  if (!player || markedForDeletion) {
    return;
  }

  QPointer<QMediaPlayer> p = player;
  if (!p || !QCoreApplication::instance()) {
    return;
  }

  QMetaObject::invokeMethod(
      QCoreApplication::instance(),
      [p, volume]() {
        if (!p) {
          return;
        }
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

void Music::fadeOut() {
  if (!player || markedForDeletion) {
    return;
  }

  QPointer<QMediaPlayer> p = player;
  if (!p || !QCoreApplication::instance()) {
    return;
  }

  QMetaObject::invokeMethod(
      QCoreApplication::instance(),
      [p, this]() {
        if (!p) {
          return;
        }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        if (p->audioOutput()) {
          p->audioOutput()->setVolume(0.0f);
        }

        QTimer::singleShot(50, [p, this]() {
          if (p && p->playbackState() == QMediaPlayer::PlayingState) {
            qDebug() << "Fading out and pausing"
                     << QString::fromStdString(filepath);
            p->pause();
            playing = false;
          }
        });
#else
        p->setVolume(0);
        p->pause();
        playing = false;
#endif
      },
      Qt::QueuedConnection);
}
