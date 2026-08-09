#pragma once

#include <QDebug>
#include <QSettings>
#include <QString>

#include <algorithm>
#include <cmath>
#include <optional>

#include "../../game/audio/audio_constants.h"
#include "../../game/audio/audio_settings.h"
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
inline constexpr char kUiScaleKey[] = "ui/scale";
inline constexpr char kUiReducedMotionKey[] = "ui/reduced_motion";
inline constexpr char kUiHighContrastKey[] = "ui/high_contrast";
inline constexpr char kUiColorVisionKey[] = "ui/color_vision_mode";
inline constexpr char kUiKeyboardFocusKey[] = "ui/always_show_focus";
inline constexpr char kUiTeamPatternsKey[] = "ui/team_patterns";
inline constexpr char kUiEdgeScrollKey[] = "ui/edge_scroll_enabled";
inline constexpr char kUiEdgeScrollSensitivityKey[] = "ui/edge_scroll_sensitivity";
inline constexpr char kUiCameraMotionKey[] = "ui/camera_motion_scale";
inline constexpr char kUiDamageNumbersKey[] = "ui/damage_numbers";
inline constexpr char kUiScreenEffectsKey[] = "ui/screen_effect_intensity";
inline constexpr char kInputBindingsGroup[] = "input/bindings";

inline constexpr int kDefaultAutosaveSlotCount = 3;
inline constexpr int kMinAutosaveSlotCount = 1;
inline constexpr int kMaxAutosaveSlotCount = 10;
inline constexpr int kDefaultAutosaveIntervalMinutes = 5;
inline constexpr int kMaxAutosaveIntervalMinutes = 60;

inline constexpr double kDefaultUiScale = 1.0;
inline constexpr double kMinUiScale = 0.75;
inline constexpr double kMaxUiScale = 2.0;

inline constexpr double kDefaultEdgeScrollSensitivity = 1.0;
inline constexpr double kMinEdgeScrollSensitivity = 0.25;
inline constexpr double kMaxEdgeScrollSensitivity = 2.0;

inline constexpr double kDefaultCameraMotionScale = 1.0;
inline constexpr double kDefaultScreenEffectIntensity = 1.0;

using AudioVolumes = Game::Audio::Settings::Volumes;

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
    Render::GraphicsSettings::instance().set_quality(
        Render::k_default_graphics_quality);
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

inline auto load_audio_volumes() -> AudioVolumes {
  return Game::Audio::Settings::load_volumes();
}

inline void save_master_volume(float volume) {
  Game::Audio::Settings::save_master_volume(volume);
}

inline void save_sound_volume(float volume) {
  Game::Audio::Settings::save_sound_volume(volume);
}

inline void save_music_volume(float volume) {
  Game::Audio::Settings::save_music_volume(volume);
}

inline void save_voice_volume(float volume) {
  Game::Audio::Settings::save_voice_volume(volume);
}

inline void save_ambience_volume(float volume) {
  Game::Audio::Settings::save_ambience_volume(volume);
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

inline auto is_supported_color_vision_mode(const QString& mode) -> bool {
  return mode == QLatin1String("none") || mode == QLatin1String("protanopia") ||
         mode == QLatin1String("deuteranopia") || mode == QLatin1String("tritanopia");
}

inline auto load_ui_scale() -> double {
  auto settings = open();
  const QVariant value = settings.value(QString::fromLatin1(kUiScaleKey));
  if (!value.isValid()) {
    return kDefaultUiScale;
  }

  bool ok = false;
  const double stored = value.toDouble(&ok);

  if (!ok || !(stored >= kMinUiScale * 0.5) || !(stored <= kMaxUiScale * 2.0)) {
    qWarning() << "Ignoring invalid saved UI scale:" << value;
    return kDefaultUiScale;
  }

  return std::clamp(stored, kMinUiScale, kMaxUiScale);
}

inline void save_ui_scale(double scale) {
  if (!(scale >= kMinUiScale * 0.5) || !(scale <= kMaxUiScale * 2.0)) {
    qWarning() << "Refusing to save out-of-range UI scale:" << scale;
    return;
  }

  auto settings = open();
  settings.setValue(QString::fromLatin1(kUiScaleKey),
                    std::clamp(scale, kMinUiScale, kMaxUiScale));
  settings.sync();
}

inline auto load_ui_reduced_motion() -> bool {
  auto settings = open();
  return settings.value(QString::fromLatin1(kUiReducedMotionKey), false).toBool();
}

inline void save_ui_reduced_motion(bool enabled) {
  auto settings = open();
  settings.setValue(QString::fromLatin1(kUiReducedMotionKey), enabled);
  settings.sync();
}

inline auto load_ui_high_contrast() -> bool {
  auto settings = open();
  return settings.value(QString::fromLatin1(kUiHighContrastKey), false).toBool();
}

inline void save_ui_high_contrast(bool enabled) {
  auto settings = open();
  settings.setValue(QString::fromLatin1(kUiHighContrastKey), enabled);
  settings.sync();
}

inline auto load_ui_color_vision_mode() -> QString {
  auto settings = open();
  const QString stored = settings.value(QString::fromLatin1(kUiColorVisionKey))
                             .toString()
                             .trimmed()
                             .toLower();
  if (stored.isEmpty()) {
    return QStringLiteral("none");
  }
  if (!is_supported_color_vision_mode(stored)) {
    qWarning() << "Ignoring unknown saved color vision mode:" << stored;
    return QStringLiteral("none");
  }
  return stored;
}

inline void save_ui_color_vision_mode(const QString& mode) {
  const QString normalized = mode.trimmed().toLower();
  if (!is_supported_color_vision_mode(normalized)) {
    qWarning() << "Refusing to save unknown color vision mode:" << mode;
    return;
  }

  auto settings = open();
  settings.setValue(QString::fromLatin1(kUiColorVisionKey), normalized);
  settings.sync();
}

inline auto load_ui_always_show_focus() -> bool {
  auto settings = open();
  return settings.value(QString::fromLatin1(kUiKeyboardFocusKey), false).toBool();
}

inline void save_ui_always_show_focus(bool enabled) {
  auto settings = open();
  settings.setValue(QString::fromLatin1(kUiKeyboardFocusKey), enabled);
  settings.sync();
}

namespace Detail {

inline auto load_bounded_double(const char* key,
                                double fallback,
                                double minimum,
                                double maximum) -> double {
  auto settings = open();
  const QVariant value = settings.value(QString::fromLatin1(key));
  if (!value.isValid()) {
    return fallback;
  }

  bool ok = false;
  const double stored = value.toDouble(&ok);
  if (!ok || !(stored >= minimum) || !(stored <= maximum)) {
    qWarning() << "Ignoring out-of-range setting" << key << value;
    return fallback;
  }
  return stored;
}

inline void
save_bounded_double(const char* key, double value, double minimum, double maximum) {
  auto settings = open();
  settings.setValue(QString::fromLatin1(key), std::clamp(value, minimum, maximum));
  settings.sync();
}

inline auto load_bool(const char* key, bool fallback) -> bool {
  auto settings = open();
  return settings.value(QString::fromLatin1(key), fallback).toBool();
}

inline void save_bool(const char* key, bool value) {
  auto settings = open();
  settings.setValue(QString::fromLatin1(key), value);
  settings.sync();
}

} // namespace Detail

inline auto load_ui_team_patterns() -> bool {
  return Detail::load_bool(kUiTeamPatternsKey, false);
}

inline void save_ui_team_patterns(bool enabled) {
  Detail::save_bool(kUiTeamPatternsKey, enabled);
}

inline auto load_ui_edge_scroll_enabled() -> bool {
  return Detail::load_bool(kUiEdgeScrollKey, true);
}

inline void save_ui_edge_scroll_enabled(bool enabled) {
  Detail::save_bool(kUiEdgeScrollKey, enabled);
}

inline auto load_ui_edge_scroll_sensitivity() -> double {
  return Detail::load_bounded_double(kUiEdgeScrollSensitivityKey,
                                     kDefaultEdgeScrollSensitivity,
                                     kMinEdgeScrollSensitivity,
                                     kMaxEdgeScrollSensitivity);
}

inline void save_ui_edge_scroll_sensitivity(double sensitivity) {
  Detail::save_bounded_double(kUiEdgeScrollSensitivityKey,
                              sensitivity,
                              kMinEdgeScrollSensitivity,
                              kMaxEdgeScrollSensitivity);
}

inline auto load_ui_camera_motion_scale() -> double {
  return Detail::load_bounded_double(
      kUiCameraMotionKey, kDefaultCameraMotionScale, 0.0, 1.0);
}

inline void save_ui_camera_motion_scale(double scale) {
  Detail::save_bounded_double(kUiCameraMotionKey, scale, 0.0, 1.0);
}

inline auto load_ui_damage_numbers() -> bool {
  return Detail::load_bool(kUiDamageNumbersKey, true);
}

inline void save_ui_damage_numbers(bool enabled) {
  Detail::save_bool(kUiDamageNumbersKey, enabled);
}

inline auto load_ui_screen_effect_intensity() -> double {
  return Detail::load_bounded_double(
      kUiScreenEffectsKey, kDefaultScreenEffectIntensity, 0.0, 1.0);
}

inline void save_ui_screen_effect_intensity(double intensity) {
  Detail::save_bounded_double(kUiScreenEffectsKey, intensity, 0.0, 1.0);
}

inline auto load_input_binding(const QString& action_id) -> QString {
  auto settings = open();
  settings.beginGroup(QString::fromLatin1(kInputBindingsGroup));
  const QString stored = settings.value(action_id).toString();
  settings.endGroup();
  return stored;
}

inline void save_input_binding(const QString& action_id, const QString& shortcut) {
  auto settings = open();
  settings.beginGroup(QString::fromLatin1(kInputBindingsGroup));
  if (shortcut.isEmpty()) {
    settings.remove(action_id);
  } else {
    settings.setValue(action_id, shortcut);
  }
  settings.endGroup();
  settings.sync();
}

inline void clear_input_bindings() {
  auto settings = open();
  settings.beginGroup(QString::fromLatin1(kInputBindingsGroup));
  settings.remove(QString());
  settings.endGroup();
  settings.sync();
}

} // namespace App::Core::UserSettings
