#pragma once

#include <QDebug>
#include <QSettings>
#include <QString>

#include <algorithm>
#include <cmath>
#include <optional>

#include "audio_constants.h"
#include "gameplay_mix.h"

namespace Game::Audio::Settings {

inline constexpr char k_organization[] = "djeada";
inline constexpr char k_application[] = "StandardOfIron";
inline constexpr char k_listening_preset_key[] = "audio/listening_preset";
inline constexpr char k_master_volume_key[] = "audio/master_volume";
inline constexpr char k_sound_volume_key[] = "audio/sound_volume";
inline constexpr char k_music_volume_key[] = "audio/music_volume";
inline constexpr char k_voice_volume_key[] = "audio/voice_volume";
inline constexpr char k_ambience_volume_key[] = "audio/ambience_volume";

inline constexpr float k_first_run_master_volume = 0.7F;
inline constexpr float k_first_run_sound_volume = 1.0F;
inline constexpr float k_first_run_music_volume = 0.45F;
inline constexpr float k_first_run_voice_volume = 1.0F;
inline constexpr float k_first_run_ambience_volume = 0.3F;

struct Volumes {
  float master{k_first_run_master_volume};
  float sound{k_first_run_sound_volume};
  float music{k_first_run_music_volume};
  float voice{k_first_run_voice_volume};
  float ambience{k_first_run_ambience_volume};
};

inline auto first_run_volumes() -> Volumes {
  return {};
}

inline auto open() -> QSettings {
  return QSettings(QSettings::IniFormat,
                   QSettings::UserScope,
                   QString::fromLatin1(k_organization),
                   QString::fromLatin1(k_application));
}

inline auto load_listening_preset() -> int {
  auto settings = open();
  bool ok = false;
  const int value = settings.value(k_listening_preset_key, 1).toInt(&ok);
  return ok && value >= 0 && value <= 2 ? value : 1;
}

inline void save_listening_preset(int preset) {
  auto settings = open();
  settings.setValue(k_listening_preset_key, static_cast<int>(preset_from_int(preset)));
  settings.sync();
}

namespace Detail {

inline auto read_volume(QSettings& settings,
                        const char* key,
                        const char* label) -> std::optional<float> {
  const QVariant value = settings.value(QString::fromLatin1(key));
  if (!value.isValid()) {
    return std::nullopt;
  }

  bool ok = false;
  const float stored = value.toFloat(&ok);
  if (!ok || !std::isfinite(stored)) {
    qWarning() << "Ignoring invalid saved audio volume for" << label << ":" << value;
    return std::nullopt;
  }

  const float clamped =
      std::clamp(stored, AudioConstants::MIN_VOLUME, AudioConstants::MAX_VOLUME);
  if (clamped != stored) {
    qWarning() << "Clamping saved audio volume for" << label << "from" << stored << "to"
               << clamped;
  }
  return clamped;
}

} // namespace Detail

inline auto load_volume(const char* key, const char* label) -> float {
  auto settings = open();
  return Detail::read_volume(settings, key, label)
      .value_or(AudioConstants::DEFAULT_VOLUME);
}

inline auto load_volumes() -> Volumes {
  auto settings = open();

  const auto master = Detail::read_volume(settings, k_master_volume_key, "master");
  const auto sound = Detail::read_volume(settings, k_sound_volume_key, "sound");
  const auto music = Detail::read_volume(settings, k_music_volume_key, "music");
  const auto voice = Detail::read_volume(settings, k_voice_volume_key, "voice");
  const auto ambience =
      Detail::read_volume(settings, k_ambience_volume_key, "ambience");

  const bool has_saved_preference = master.has_value() || sound.has_value() ||
                                    music.has_value() || voice.has_value() ||
                                    ambience.has_value();
  if (!has_saved_preference) {
    const Volumes defaults = first_run_volumes();
    settings.setValue(QString::fromLatin1(k_master_volume_key), defaults.master);
    settings.setValue(QString::fromLatin1(k_sound_volume_key), defaults.sound);
    settings.setValue(QString::fromLatin1(k_music_volume_key), defaults.music);
    settings.setValue(QString::fromLatin1(k_voice_volume_key), defaults.voice);
    settings.setValue(QString::fromLatin1(k_ambience_volume_key), defaults.ambience);
    settings.sync();
    return defaults;
  }

  const Volumes defaults = first_run_volumes();
  return {
      .master = master.value_or(defaults.master),
      .sound = sound.value_or(defaults.sound),
      .music = music.value_or(defaults.music),
      .voice = voice.value_or(defaults.voice),
      .ambience = ambience.value_or(defaults.ambience),
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
