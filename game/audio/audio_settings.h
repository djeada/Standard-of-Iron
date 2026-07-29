#pragma once

#include <QDebug>
#include <QSettings>
#include <QString>

#include <algorithm>
#include <cmath>

#include "audio_constants.h"

namespace Game::Audio::Settings {

inline constexpr char k_organization[] = "djeada";
inline constexpr char k_application[] = "StandardOfIron";
inline constexpr char k_master_volume_key[] = "audio/master_volume";
inline constexpr char k_sound_volume_key[] = "audio/sound_volume";
inline constexpr char k_music_volume_key[] = "audio/music_volume";
inline constexpr char k_voice_volume_key[] = "audio/voice_volume";
inline constexpr char k_ambience_volume_key[] = "audio/ambience_volume";

struct Volumes {
  float master{AudioConstants::DEFAULT_VOLUME};
  float sound{AudioConstants::DEFAULT_VOLUME};
  float music{AudioConstants::DEFAULT_VOLUME};
  float voice{AudioConstants::DEFAULT_VOLUME};
  float ambience{AudioConstants::DEFAULT_VOLUME};
};

inline auto open() -> QSettings {
  return QSettings(QSettings::IniFormat,
                   QSettings::UserScope,
                   QString::fromLatin1(k_organization),
                   QString::fromLatin1(k_application));
}

inline auto load_volume(const char* key, const char* label) -> float {
  auto settings = open();
  const QVariant value = settings.value(QString::fromLatin1(key));
  if (!value.isValid()) {
    return AudioConstants::DEFAULT_VOLUME;
  }

  bool ok = false;
  const float stored = value.toFloat(&ok);
  if (!ok || !std::isfinite(stored)) {
    qWarning() << "Ignoring invalid saved audio volume for" << label << ":" << value;
    return AudioConstants::DEFAULT_VOLUME;
  }

  const float clamped =
      std::clamp(stored, AudioConstants::MIN_VOLUME, AudioConstants::MAX_VOLUME);
  if (clamped != stored) {
    qWarning() << "Clamping saved audio volume for" << label << "from" << stored << "to"
               << clamped;
  }
  return clamped;
}

inline auto load_volumes() -> Volumes {
  return {
      .master = load_volume(k_master_volume_key, "master"),
      .sound = load_volume(k_sound_volume_key, "sound"),
      .music = load_volume(k_music_volume_key, "music"),
      .voice = load_volume(k_voice_volume_key, "voice"),
      .ambience = load_volume(k_ambience_volume_key, "ambience"),
  };
}

inline void save_volume(const char* key, float volume, const char* label) {
  if (!std::isfinite(volume)) {
    qWarning() << "Refusing to save non-finite audio volume for" << label << ":"
               << volume;
    return;
  }

  const float clamped =
      std::clamp(volume, AudioConstants::MIN_VOLUME, AudioConstants::MAX_VOLUME);
  if (clamped != volume) {
    qWarning() << "Clamping saved audio volume for" << label << "from" << volume << "to"
               << clamped;
  }

  auto settings = open();
  settings.setValue(QString::fromLatin1(key), clamped);
  settings.sync();
}

inline void save_master_volume(float volume) {
  save_volume(k_master_volume_key, volume, "master");
}

inline void save_sound_volume(float volume) {
  save_volume(k_sound_volume_key, volume, "sound");
}

inline void save_music_volume(float volume) {
  save_volume(k_music_volume_key, volume, "music");
}

inline void save_voice_volume(float volume) {
  save_volume(k_voice_volume_key, volume, "voice");
}

inline void save_ambience_volume(float volume) {
  save_volume(k_ambience_volume_key, volume, "ambience");
}

} // namespace Game::Audio::Settings
