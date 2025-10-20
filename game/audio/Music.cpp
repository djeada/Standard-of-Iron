#include "Music.h"
#include <QMediaPlayer>
#include <QUrl>

Music::Music(const std::string &filePath) : filepath(filePath), loaded(false) {
  player = std::make_unique<QMediaPlayer>();
  player->setMedia(QUrl::fromLocalFile(QString::fromStdString(filePath)));

  loaded = (player->mediaStatus() != QMediaPlayer::InvalidMedia &&
            player->mediaStatus() != QMediaPlayer::NoMedia);
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

  player->setVolume(static_cast<int>(volume * 100));

  if (loop) {
    QObject::connect(player.get(), &QMediaPlayer::mediaStatusChanged,
                     [this](QMediaPlayer::MediaStatus status) {
                       if (status == QMediaPlayer::EndOfMedia) {
                         player->play();
                       }
                     });
  }

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
  if (player && player->state() == QMediaPlayer::PausedState) {
    player->play();
  }
}

void Music::setVolume(float volume) {
  if (player) {
    player->setVolume(static_cast<int>(volume * 100));
  }
}
