#pragma once

#include <QObject>

namespace App::Models {

class AudioSystemProxy : public QObject {
  Q_OBJECT

public:
  explicit AudioSystemProxy(QObject *parent = nullptr);
  ~AudioSystemProxy() override = default;

  Q_INVOKABLE static void setMasterVolume(float volume);
  Q_INVOKABLE static void setMusicVolume(float volume);
  Q_INVOKABLE static void setSoundVolume(float volume);
  Q_INVOKABLE static void setVoiceVolume(float volume);

  Q_INVOKABLE [[nodiscard]] static float getMasterVolume();
  Q_INVOKABLE [[nodiscard]] static float getMusicVolume();
  Q_INVOKABLE [[nodiscard]] static float getSoundVolume();
  Q_INVOKABLE [[nodiscard]] static float getVoiceVolume();
};

} 
