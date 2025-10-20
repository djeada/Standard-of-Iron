#include "Sound.h"
#include <QSoundEffect>
#include <QUrl>

Sound::Sound(const std::string &filePath) : filepath(filePath), loaded(false) {
  soundEffect = std::make_unique<QSoundEffect>();
  soundEffect->setSource(QUrl::fromLocalFile(QString::fromStdString(filePath)));

  loaded = (soundEffect->status() == QSoundEffect::Ready ||
            soundEffect->status() == QSoundEffect::Loading);
}

Sound::~Sound() {
  if (soundEffect) {
    soundEffect->stop();
  }
}

bool Sound::isLoaded() const { return loaded; }

void Sound::play(float volume, bool loop) {
  if (!soundEffect || !loaded) {
    return;
  }

  soundEffect->setVolume(volume);
  soundEffect->setLoopCount(loop ? QSoundEffect::Infinite : 1);
  soundEffect->play();
}

void Sound::stop() {
  if (soundEffect) {
    soundEffect->stop();
  }
}

void Sound::setVolume(float volume) {
  if (soundEffect) {
    soundEffect->setVolume(volume);
  }
}
