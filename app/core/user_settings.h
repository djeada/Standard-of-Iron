#pragma once

#include <QDebug>
#include <QSettings>
#include <QString>

#include <algorithm>
#include <cmath>
#include <optional>

#include "../../game/audio/audio_constants.h"
#include "../../render/graphics_settings.h"

namespace App::Core::UserSettings {

inline constexpr char kOrganization[] = "djeada";
inline constexpr char kApplication[] = "StandardOfIron";
inline constexpr char kGraphicsQualityKey[] = "graphics/quality_level";
inline constexpr char kLanguageKey[] = "ui/language";
inline constexpr char kMasterVolumeKey[] = "audio/master_volume";
inline constexpr char kSoundVolumeKey[] = "audio/sound_volume";
inline constexpr char kMusicVolumeKey[] = "audio/music_volume";
inline constexpr char kVoiceVolumeKey[] = "audio/voice_volume";
inline constexpr char kAmbienceVolumeKey[] = "audio/ambience_volume";
inline constexpr char kAutosaveSlotCountKey[] = "saves/autosave_slot_count";
inline constexpr char kAutosaveIntervalKey[] = "saves/autosave_interval_minutes";

inline constexpr int kDefaultAutosaveSlotCount = 3;
inline constexpr int kMinAutosaveSlotCount = 1;
inline constexpr int kMaxAutosaveSlotCount = 10;
inline constexpr int kDefaultAutosaveIntervalMinutes = 5;
inline constexpr int kMaxAutosaveIntervalMinutes = 60;

struct AudioVolumes {
  float master{AudioConstants::DEFAULT_VOLUME};
  float sound{AudioConstants::DEFAULT_VOLUME};
  float music{AudioConstants::DEFAULT_VOLUME};
  float voice{AudioConstants::DEFAULT_VOLUME};
  float ambience{AudioConstants::DEFAULT_VOLUME};
};

inline auto open() -> QSettings {
  return QSettings(QSettings::IniFormat,
                   QSettings::UserScope,
                   QString::fromLatin1(kOrganization),
                   QString::fromLatin1(kApplication));
}

inline void clear() {
  auto settings = open();
  settings.clear();
  settings.sync();
}

inline auto load_graphics_quality_level() -> std::optional<int> {
  auto settings = open();
  const QVariant value = settings.value(QString::fromLatin1(kGraphicsQualityKey));
  if (!value.isValid()) {
    return std::nullopt;
  }

  bool ok = false;
  const int level = value.toInt(&ok);
  if (!ok || level < 0 || level > 3) {
    qWarning() << "Ignoring invalid saved graphics quality level:" << value;
    return std::nullopt;
  }

  return level;
}

inline void apply_saved_graphics_quality() {
  const auto level = load_graphics_quality_level();
  if (!level.has_value()) {
    return;
  }

  Render::GraphicsSettings::instance().set_quality(
      static_cast<Render::GraphicsQuality>(*level));
}

inline void save_graphics_quality_level(int level) {
  if (level < 0 || level > 3) {
    qWarning() << "Refusing to save invalid graphics quality level:" << level;
    return;
  }

  auto settings = open();
  settings.setValue(QString::fromLatin1(kGraphicsQualityKey), level);
  settings.sync();
}

inline auto load_language() -> std::optional<QString> {
  auto settings = open();
  const QString language =
      settings.value(QString::fromLatin1(kLanguageKey)).toString().trimmed();
  if (language.isEmpty()) {
    return std::nullopt;
  }

  return language;
}

inline void save_language(const QString& language) {
  const QString normalized = language.trimmed();
  if (normalized.isEmpty()) {
    qWarning() << "Refusing to save empty language code";
    return;
  }

  auto settings = open();
  settings.setValue(QString::fromLatin1(kLanguageKey), normalized);
  settings.sync();
}

inline auto load_audio_volume(const char* key, const char* label) -> float {
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

inline auto load_audio_volumes() -> AudioVolumes {
  return {
      .master = load_audio_volume(kMasterVolumeKey, "master"),
      .sound = load_audio_volume(kSoundVolumeKey, "sound"),
      .music = load_audio_volume(kMusicVolumeKey, "music"),
      .voice = load_audio_volume(kVoiceVolumeKey, "voice"),
      .ambience = load_audio_volume(kAmbienceVolumeKey, "ambience"),
  };
}

inline void save_audio_volume(const char* key, float volume, const char* label) {
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
  save_audio_volume(kMasterVolumeKey, volume, "master");
}

inline void save_sound_volume(float volume) {
  save_audio_volume(kSoundVolumeKey, volume, "sound");
}

inline void save_music_volume(float volume) {
  save_audio_volume(kMusicVolumeKey, volume, "music");
}

inline void save_voice_volume(float volume) {
  save_audio_volume(kVoiceVolumeKey, volume, "voice");
}

inline void save_ambience_volume(float volume) {
  save_audio_volume(kAmbienceVolumeKey, volume, "ambience");
}

inline auto load_autosave_slot_count() -> int {
  auto settings = open();
  const int stored =
      settings
          .value(QString::fromLatin1(kAutosaveSlotCountKey), kDefaultAutosaveSlotCount)
          .toInt();
  return std::clamp(stored, kMinAutosaveSlotCount, kMaxAutosaveSlotCount);
}

inline void save_autosave_slot_count(int count) {
  auto settings = open();
  settings.setValue(QString::fromLatin1(kAutosaveSlotCountKey),
                    std::clamp(count, kMinAutosaveSlotCount, kMaxAutosaveSlotCount));
  settings.sync();
}

inline auto load_autosave_interval_minutes() -> int {
  auto settings = open();
  const int stored = settings
                         .value(QString::fromLatin1(kAutosaveIntervalKey),
                                kDefaultAutosaveIntervalMinutes)
                         .toInt();
  return std::clamp(stored, 0, kMaxAutosaveIntervalMinutes);
}

inline void save_autosave_interval_minutes(int minutes) {
  auto settings = open();
  settings.setValue(QString::fromLatin1(kAutosaveIntervalKey),
                    std::clamp(minutes, 0, kMaxAutosaveIntervalMinutes));
  settings.sync();
}

} // namespace App::Core::UserSettings
