#include "Music.h"
#include <QAudioOutput>
#include <QMediaPlayer>
#include <QUrl>

Music::Music(const std::string &filePath) : filepath(filePath), loaded(false) {
  player = std::make_unique<QMediaPlayer>();

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  player->setSource(QUrl::fromLocalFile(QString::fromStdString(filePath)));
  loaded = (player->error() == QMediaPlayer::NoError);
#else
  player->setMedia(QUrl::fromLocalFile(QString::fromStdString(filePath)));
  loaded = (player->mediaStatus() != QMediaPlayer::InvalidMedia &&
            player->mediaStatus() != QMediaPlayer::NoMedia);
#endif
}

Music::~Music() {
  if (player) {
    player->stop();
  }
}

bool Music::isLoaded() const { return loaded; }

void Music::play(float volume, bool loop) {
  if (!player || !loaded) {
    return;
  }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  auto audioOutput = new QAudioOutput();
  audioOutput->setVolume(volume);
  player->setAudioOutput(audioOutput);
  player->setLoops(loop ? QMediaPlayer::Infinite : 1);
#else
  player->setVolume(static_cast<int>(volume * 100));
  if (loop) {
    QObject::connect(player.get(), &QMediaPlayer::mediaStatusChanged,
                     [this](QMediaPlayer::MediaStatus status) {
                       if (status == QMediaPlayer::EndOfMedia) {
                         player->play();
                       }
                     });
  }
#endif

  player->play();
}

void Music::stop() {
  if (player) {
    player->stop();
  }
}

void Music::pause() {
  if (player) {
    player->pause();
  }
}

void Music::resume() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  if (player && player->playbackState() == QMediaPlayer::PausedState) {
    player->play();
  }
#else
  if (player && player->state() == QMediaPlayer::PausedState) {
    player->play();
  }
#endif
}

void Music::setVolume(float volume) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  if (player && player->audioOutput()) {
    player->audioOutput()->setVolume(volume);
  }
#else
  if (player) {
    player->setVolume(static_cast<int>(volume * 100));
  }
#endif
}
