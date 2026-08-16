#pragma once

#include <QDebug>
#include <QSettings>
#include <QString>

#include <algorithm>
#include <cmath>
#include <optional>

#include "audio_constants.h"

namespace Game::Audio::Settings {

inline constexpr char k_organization[] = "djeada";
inline constexpr char k_application[] = "StandardOfIron";
inline constexpr char k_master_volume_key[] = "audio/master_volume";
inline constexpr char k_sound_volume_key[] = "audio/sound_volume";
inline constexpr char k_music_volume_key[] = "audio/music_volume";
inline constexpr char k_voice_volume_key[] = "audio/voice_volume";
inline constexpr char k_ambience_volume_key[] = "audio/ambience_volume";

// Volumes multiply (master * category * cue), so a fresh install opens well
// below full scale rather than at the top of the mixer. Music sits under the
// effects bed and ambience loops continuously, so both are pulled down further.
inline constexpr float k_first_run_master_volume = 0.6F;
inline constexpr float k_first_run_sound_volume = 1.0F;
inline constexpr float k_first_run_music_volume = 0.5F;
inline constexpr float k_first_run_voice_volume = 1.0F;
inline constexpr float k_first_run_ambience_volume = 0.8F;

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

namespace Detail {

// Returns nothing when the channel has no usable stored preference.
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

// A player who has never touched the mixer gets the conservative first-run
// values; anyone with a stored preference keeps every channel they saved, and
// the pre-existing full-scale fallback for the channels they never touched.
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
    // Persist immediately: otherwise the next time the player nudges a single
    // slider the remaining channels would stop counting as a first run and
    // snap back up to full scale.
    settings.setValue(QString::fromLatin1(k_master_volume_key), defaults.master);
    settings.setValue(QString::fromLatin1(k_sound_volume_key), defaults.sound);
    settings.setValue(QString::fromLatin1(k_music_volume_key), defaults.music);
    settings.setValue(QString::fromLatin1(k_voice_volume_key), defaults.voice);
    settings.setValue(QString::fromLatin1(k_ambience_volume_key), defaults.ambience);
    settings.sync();
    return defaults;
  }

  return {
      .master = master.value_or(AudioConstants::DEFAULT_VOLUME),
      .sound = sound.value_or(AudioConstants::DEFAULT_VOLUME),
      .music = music.value_or(AudioConstants::DEFAULT_VOLUME),
      .voice = voice.value_or(AudioConstants::DEFAULT_VOLUME),
      .ambience = ambience.value_or(AudioConstants::DEFAULT_VOLUME),
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
