#pragma once

#include <QObject>

namespace App {
namespace Models {

class AudioSystemProxy : public QObject {
  Q_OBJECT
public:
  explicit AudioSystemProxy(QObject *parent = nullptr);
  ~AudioSystemProxy() override = default;

  Q_INVOKABLE void setMasterVolume(float volume);
  Q_INVOKABLE void setMusicVolume(float volume);
  Q_INVOKABLE void setSoundVolume(float volume);
  Q_INVOKABLE void setVoiceVolume(float volume);

  Q_INVOKABLE float getMasterVolume() const;
  Q_INVOKABLE float getMusicVolume() const;
  Q_INVOKABLE float getSoundVolume() const;
  Q_INVOKABLE float getVoiceVolume() const;
};

} 
} 
