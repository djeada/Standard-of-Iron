#include "Music.h"
#include <QAudioOutput>
#include <QCoreApplication>
#include <QMediaPlayer>
#include <QMetaObject>
#include <QThread>
#include <QTimer>
#include <QUrl>

Music::Music(const std::string &file_path)
    : file_path(file_path), loaded(false), audio_output(nullptr),
      main_thread(nullptr), playing(false), marked_for_deletion(false) {

  if (!QCoreApplication::instance()) {
    return;
  }

  main_thread = QCoreApplication::instance()->thread();
  player = new QMediaPlayer();

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  audio_output = new QAudioOutput(player);
  player->setAudioOutput(audio_output);

  QObject::connect(player, &QMediaPlayer::errorOccurred,
                   [file_path = this->file_path](QMediaPlayer::Error error,
                                                 const QString &desc) {
                     qWarning() << "QMediaPlayer error for"
                                << QString::fromStdString(file_path)
                                << "- Error code:" << static_cast<int>(error)
                                << "Message:" << desc;
                   });

  QObject::connect(
      player, &QMediaPlayer::mediaStatusChanged,
      [file_path = this->file_path, this](QMediaPlayer::MediaStatus status) {
        qDebug() << "Media status for" << QString::fromStdString(file_path)
                 << ":" << static_cast<int>(status);
        if (status == QMediaPlayer::EndOfMedia) {
          playing = false;
        }
      });

  QObject::connect(
      player, &QMediaPlayer::playbackStateChanged,
      [file_path = this->file_path](QMediaPlayer::PlaybackState state) {
        qDebug() << "Playback state for" << QString::fromStdString(file_path)
                 << ":" << static_cast<int>(state);
      });

  player->setSource(QUrl::fromLocalFile(QString::fromStdString(file_path)));
  loaded = (player->error() == QMediaPlayer::NoError);
#else
  player->setMedia(QUrl::fromLocalFile(QString::fromStdString(file_path)));
  loaded = (player->mediaStatus() != QMediaPlayer::InvalidMedia &&
            player->mediaStatus() != QMediaPlayer::NoMedia);
#endif
}

Music::~Music() { cleanup_player(); }

void Music::cleanup_player() {
  if (!player || marked_for_deletion) {
    return;
  }

  marked_for_deletion = true;
  QMediaPlayer *raw_player = player.data();

  if (!raw_player) {
    return;
  }

  if (QCoreApplication::instance() && main_thread) {
    raw_player->deleteLater();
  }
}

bool Music::is_loaded() const { return loaded && !marked_for_deletion; }

void Music::play(float volume, bool loop) {
  if (!player || !loaded || marked_for_deletion) {
    return;
  }

  QPointer<QMediaPlayer> player_ptr = player;
  QAudioOutput *output = audio_output;

  if (!player_ptr || !QCoreApplication::instance()) {
    return;
  }

  QMetaObject::invokeMethod(
      QCoreApplication::instance(),
      [player_ptr, output, volume, loop, this]() {
        if (!player_ptr) {
          return;
        }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)

        bool is_currently_playing =
            (player_ptr->playbackState() == QMediaPlayer::PlayingState);

        if (output) {
          output->setVolume(volume);
        }
        player_ptr->setLoops(loop ? QMediaPlayer::Infinite : 1);

        if (!is_currently_playing) {
          qDebug() << "Starting playback for"
                   << QString::fromStdString(file_path);
          playing = true;
          player_ptr->play();
        } else {
          qDebug() << "Already playing" << QString::fromStdString(file_path)
                   << "- updating volume only";
        }
#else
        player_ptr->setVolume(static_cast<int>(volume * 100));
        playing = true;
        player_ptr->play();
#endif
      },
      Qt::QueuedConnection);
}

void Music::stop() {
  if (!player || marked_for_deletion) {
    return;
  }

  playing = false;
  QPointer<QMediaPlayer> player_ptr = player;
  if (!player_ptr || !QCoreApplication::instance()) {
    return;
  }

  QMetaObject::invokeMethod(
      QCoreApplication::instance(),
      [player_ptr]() {
        if (player_ptr) {
          player_ptr->stop();
        }
      },
      Qt::QueuedConnection);
}

void Music::pause() {
  if (!player || marked_for_deletion) {
    return;
  }

  QPointer<QMediaPlayer> player_ptr = player;
  if (!player_ptr || !QCoreApplication::instance()) {
    return;
  }

  QMetaObject::invokeMethod(
      QCoreApplication::instance(),
      [player_ptr]() {
        if (player_ptr) {
          player_ptr->pause();
        }
      },
      Qt::QueuedConnection);
}

void Music::resume() {
  if (!player || marked_for_deletion) {
    return;
  }

  QPointer<QMediaPlayer> player_ptr = player;
  if (!player_ptr || !QCoreApplication::instance()) {
    return;
  }

  QMetaObject::invokeMethod(
      QCoreApplication::instance(),
      [player_ptr]() {
        if (!player_ptr) {
          return;
        }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        if (player_ptr->playbackState() == QMediaPlayer::PausedState) {
          player_ptr->play();
        }
#else
        if (player_ptr->state() == QMediaPlayer::PausedState) {
          player_ptr->play();
        }
#endif
      },
      Qt::QueuedConnection);
}

void Music::set_volume(float volume) {
  if (!player || marked_for_deletion) {
    return;
  }

  QPointer<QMediaPlayer> player_ptr = player;
  if (!player_ptr || !QCoreApplication::instance()) {
    return;
  }

  QMetaObject::invokeMethod(
      QCoreApplication::instance(),
      [player_ptr, volume]() {
        if (!player_ptr) {
          return;
        }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        if (player_ptr->audioOutput()) {
          player_ptr->audioOutput()->setVolume(volume);
        }
#else
        player_ptr->setVolume(static_cast<int>(volume * 100));
#endif
      },
      Qt::QueuedConnection);
}

void Music::fade_out() {
  static constexpr int FADE_OUT_DELAY_MS = 50;

  if (!player || marked_for_deletion) {
    return;
  }

  QPointer<QMediaPlayer> player_ptr = player;
  if (!player_ptr || !QCoreApplication::instance()) {
    return;
  }

  QMetaObject::invokeMethod(
      QCoreApplication::instance(),
      [player_ptr, this]() {
        if (!player_ptr) {
          return;
        }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        if (player_ptr->audioOutput()) {
          player_ptr->audioOutput()->setVolume(0.0F);
        }

        QTimer::singleShot(FADE_OUT_DELAY_MS, [player_ptr, this]() {
          if (player_ptr &&
              player_ptr->playbackState() == QMediaPlayer::PlayingState) {
            qDebug() << "Fading out and pausing"
                     << QString::fromStdString(file_path);
            player_ptr->pause();
            playing = false;
          }
        });
#else
        player_ptr->setVolume(0);
        player_ptr->pause();
        playing = false;
#endif
      },
      Qt::QueuedConnection);
}
