#pragma once

#include <QObject>

namespace App::Models {

class AudioSystemProxy : public QObject {
  Q_OBJECT

public:
  explicit AudioSystemProxy(QObject* parent = nullptr);
  ~AudioSystemProxy() override = default;

  Q_INVOKABLE void set_master_volume(float volume);
  Q_INVOKABLE void set_music_volume(float volume);
  Q_INVOKABLE void set_sound_volume(float volume);
  Q_INVOKABLE void set_voice_volume(float volume);
  Q_INVOKABLE void set_ambience_volume(float volume);
  Q_INVOKABLE void play_ui_hover();
  Q_INVOKABLE void play_ui_click();

  Q_INVOKABLE [[nodiscard]] float get_master_volume();
  Q_INVOKABLE [[nodiscard]] float get_music_volume();
  Q_INVOKABLE [[nodiscard]] float get_sound_volume();
  Q_INVOKABLE [[nodiscard]] float get_voice_volume();
  Q_INVOKABLE [[nodiscard]] float get_ambience_volume();
};

} // namespace App::Models
