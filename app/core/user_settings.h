#pragma once

#include <QDebug>
#include <QSettings>
#include <QString>

#include <algorithm>
#include <cmath>
#include <optional>

#include "game/audio/audio_constants.h"
#include "game/audio/audio_settings.h"
#include "game/render_bridge/camera_speeds.h"
#include "render/graphics_settings.h"

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
inline constexpr char kUiDamageNumberModeKey[] = "ui/damage_number_mode";
inline constexpr char kUiEconomyNumbersKey[] = "ui/economy_numbers";
inline constexpr char kUiScreenEffectsKey[] = "ui/screen_effect_intensity";
inline constexpr char kUiEconomyCoachKey[] = "ui/economy_coach";
inline constexpr char kUiFormationHintsKey[] = "ui/formation_hints";
inline constexpr char kUiCameraLegendSeenKey[] = "ui/camera_legend_seen";
inline constexpr char kUiTutorialCompletedKey[] = "ui/tutorial_completed";
inline constexpr char kDisplayWindowModeKey[] = "display/window_mode";
inline constexpr char kDisplayVsyncKey[] = "display/vsync";
inline constexpr char kUiShowFpsKey[] = "ui/show_fps";
inline constexpr char kCameraPanSpeedScaleKey[] = "camera/pan_speed_scale";
inline constexpr char kCameraZoomSpeedScaleKey[] = "camera/zoom_speed_scale";
inline constexpr char kCameraRotationSpeedScaleKey[] = "camera/rotation_speed_scale";
inline constexpr char kInputBindingsGroup[] = "input/bindings";
inline constexpr char kCommanderLookSensitivityXKey[] = "commander/look_sensitivity_x";
inline constexpr char kCommanderLookSensitivityYKey[] = "commander/look_sensitivity_y";
inline constexpr char kCommanderInvertLookYKey[] = "commander/invert_look_y";
inline constexpr char kCommanderCameraImpulseKey[] = "commander/camera_impulse";
inline constexpr char kCommanderHeadBobKey[] = "commander/head_bob";
inline constexpr char kCommanderFieldOfViewScaleKey[] = "commander/fov_scale";
inline constexpr char kCommanderGuardToggleKey[] = "commander/guard_is_toggle";

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

inline constexpr double kDefaultCommanderLookSensitivity = 1.0;
inline constexpr double kMinCommanderLookSensitivity = 0.25;
inline constexpr double kMaxCommanderLookSensitivity = 4.0;
inline constexpr double kDefaultCommanderFieldOfViewScale = 1.0;
inline constexpr double kMinCommanderFieldOfViewScale = 0.75;
inline constexpr double kMaxCommanderFieldOfViewScale = 1.35;
inline constexpr double kDefaultScreenEffectIntensity = 1.0;

inline constexpr char kDefaultDisplayWindowMode[] = "fullscreen";
inline constexpr bool kDefaultDisplayVsync = true;
inline constexpr bool kDefaultUiShowFps = false;
inline constexpr double kDefaultCameraSpeedScale =
    Game::Systems::CameraSpeeds::k_default_scale;
inline constexpr double kMinCameraSpeedScale = Game::Systems::CameraSpeeds::k_min_scale;
inline constexpr double kMaxCameraSpeedScale = Game::Systems::CameraSpeeds::k_max_scale;

using AudioVolumes = Game::Audio::Settings::Volumes;

inline auto default_audio_volumes() -> AudioVolumes {
  return Game::Audio::Settings::first_run_volumes();
}

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

inline auto load_string(const char* key, const QString& fallback) -> QString {
  auto settings = open();
  return settings.value(QString::fromLatin1(key), fallback).toString();
}

inline void save_string(const char* key, const QString& value) {
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

inline auto load_commander_look_sensitivity_x() -> double {
  return Detail::load_bounded_double(kCommanderLookSensitivityXKey,
                                     kDefaultCommanderLookSensitivity,
                                     kMinCommanderLookSensitivity,
                                     kMaxCommanderLookSensitivity);
}

inline void save_commander_look_sensitivity_x(double scale) {
  Detail::save_bounded_double(kCommanderLookSensitivityXKey,
                              scale,
                              kMinCommanderLookSensitivity,
                              kMaxCommanderLookSensitivity);
}

inline auto load_commander_look_sensitivity_y() -> double {
  return Detail::load_bounded_double(kCommanderLookSensitivityYKey,
                                     kDefaultCommanderLookSensitivity,
                                     kMinCommanderLookSensitivity,
                                     kMaxCommanderLookSensitivity);
}

inline void save_commander_look_sensitivity_y(double scale) {
  Detail::save_bounded_double(kCommanderLookSensitivityYKey,
                              scale,
                              kMinCommanderLookSensitivity,
                              kMaxCommanderLookSensitivity);
}

inline auto load_commander_invert_look_y() -> bool {
  return Detail::load_bool(kCommanderInvertLookYKey, false);
}

inline void save_commander_invert_look_y(bool enabled) {
  Detail::save_bool(kCommanderInvertLookYKey, enabled);
}

inline auto load_commander_camera_impulse() -> bool {
  return Detail::load_bool(kCommanderCameraImpulseKey, true);
}

inline void save_commander_camera_impulse(bool enabled) {
  Detail::save_bool(kCommanderCameraImpulseKey, enabled);
}

inline auto load_commander_head_bob() -> bool {
  return Detail::load_bool(kCommanderHeadBobKey, true);
}

inline void save_commander_head_bob(bool enabled) {
  Detail::save_bool(kCommanderHeadBobKey, enabled);
}

inline auto load_commander_field_of_view_scale() -> double {
  return Detail::load_bounded_double(kCommanderFieldOfViewScaleKey,
                                     kDefaultCommanderFieldOfViewScale,
                                     kMinCommanderFieldOfViewScale,
                                     kMaxCommanderFieldOfViewScale);
}

inline void save_commander_field_of_view_scale(double scale) {
  Detail::save_bounded_double(kCommanderFieldOfViewScaleKey,
                              scale,
                              kMinCommanderFieldOfViewScale,
                              kMaxCommanderFieldOfViewScale);
}

inline auto load_commander_guard_is_toggle() -> bool {
  return Detail::load_bool(kCommanderGuardToggleKey, false);
}

inline void save_commander_guard_is_toggle(bool enabled) {
  Detail::save_bool(kCommanderGuardToggleKey, enabled);
}

inline auto load_ui_damage_numbers() -> bool {
  return Detail::load_bool(kUiDamageNumbersKey, true);
}

inline void save_ui_damage_numbers(bool enabled) {
  Detail::save_bool(kUiDamageNumbersKey, enabled);
}

inline auto is_damage_number_mode(const QString& mode) -> bool {
  return mode == QLatin1String("off") || mode == QLatin1String("important") ||
         mode == QLatin1String("all");
}

inline auto load_ui_damage_number_mode() -> QString {
  const QString stored =
      Detail::load_string(kUiDamageNumberModeKey, QStringLiteral(""));
  if (is_damage_number_mode(stored)) {
    return stored;
  }
  return load_ui_damage_numbers() ? QStringLiteral("all") : QStringLiteral("off");
}

inline void save_ui_damage_number_mode(const QString& mode) {
  if (!is_damage_number_mode(mode)) {
    return;
  }
  Detail::save_string(kUiDamageNumberModeKey, mode);
  Detail::save_bool(kUiDamageNumbersKey, mode != QLatin1String("off"));
}

inline auto load_ui_economy_numbers() -> bool {
  return Detail::load_bool(kUiEconomyNumbersKey, true);
}

inline void save_ui_economy_numbers(bool enabled) {
  Detail::save_bool(kUiEconomyNumbersKey, enabled);
}

inline auto load_ui_camera_legend_seen() -> bool {
  return Detail::load_bool(kUiCameraLegendSeenKey, false);
}

inline void save_ui_camera_legend_seen(bool seen) {
  Detail::save_bool(kUiCameraLegendSeenKey, seen);
}

inline auto load_ui_tutorial_completed() -> bool {
  return Detail::load_bool(kUiTutorialCompletedKey, false);
}

inline void save_ui_tutorial_completed(bool completed) {
  Detail::save_bool(kUiTutorialCompletedKey, completed);
}

inline auto load_ui_economy_coach() -> bool {
  return Detail::load_bool(kUiEconomyCoachKey, true);
}

inline void save_ui_economy_coach(bool enabled) {
  Detail::save_bool(kUiEconomyCoachKey, enabled);
}

inline auto load_ui_formation_hints() -> bool {
  return Detail::load_bool(kUiFormationHintsKey, true);
}

inline void save_ui_formation_hints(bool enabled) {
  Detail::save_bool(kUiFormationHintsKey, enabled);
}

inline auto load_ui_screen_effect_intensity() -> double {
  return Detail::load_bounded_double(
      kUiScreenEffectsKey, kDefaultScreenEffectIntensity, 0.0, 1.0);
}

inline void save_ui_screen_effect_intensity(double intensity) {
  Detail::save_bounded_double(kUiScreenEffectsKey, intensity, 0.0, 1.0);
}

inline auto is_supported_window_mode(const QString& mode) -> bool {
  return mode == QLatin1String("fullscreen") || mode == QLatin1String("borderless") ||
         mode == QLatin1String("windowed");
}

inline auto load_display_window_mode() -> QString {
  const QString stored = Detail::load_string(
      kDisplayWindowModeKey, QString::fromLatin1(kDefaultDisplayWindowMode));
  const QString normalized = stored.trimmed().toLower();
  if (!is_supported_window_mode(normalized)) {
    qWarning() << "Ignoring unknown saved window mode:" << stored;
    return QString::fromLatin1(kDefaultDisplayWindowMode);
  }
  return normalized;
}

inline void save_display_window_mode(const QString& mode) {
  const QString normalized = mode.trimmed().toLower();
  if (!is_supported_window_mode(normalized)) {
    qWarning() << "Refusing to save unknown window mode:" << mode;
    return;
  }
  Detail::save_string(kDisplayWindowModeKey, normalized);
}

inline auto load_display_vsync() -> bool {
  return Detail::load_bool(kDisplayVsyncKey, kDefaultDisplayVsync);
}

inline void save_display_vsync(bool enabled) {
  Detail::save_bool(kDisplayVsyncKey, enabled);
}

inline auto load_ui_show_fps() -> bool {
  return Detail::load_bool(kUiShowFpsKey, kDefaultUiShowFps);
}

inline void save_ui_show_fps(bool enabled) {
  Detail::save_bool(kUiShowFpsKey, enabled);
}

inline auto load_camera_pan_speed_scale() -> double {
  return Detail::load_bounded_double(kCameraPanSpeedScaleKey,
                                     kDefaultCameraSpeedScale,
                                     kMinCameraSpeedScale,
                                     kMaxCameraSpeedScale);
}

inline void save_camera_pan_speed_scale(double scale) {
  Detail::save_bounded_double(
      kCameraPanSpeedScaleKey, scale, kMinCameraSpeedScale, kMaxCameraSpeedScale);
}

inline auto load_camera_zoom_speed_scale() -> double {
  return Detail::load_bounded_double(kCameraZoomSpeedScaleKey,
                                     kDefaultCameraSpeedScale,
                                     kMinCameraSpeedScale,
                                     kMaxCameraSpeedScale);
}

inline void save_camera_zoom_speed_scale(double scale) {
  Detail::save_bounded_double(
      kCameraZoomSpeedScaleKey, scale, kMinCameraSpeedScale, kMaxCameraSpeedScale);
}

inline auto load_camera_rotation_speed_scale() -> double {
  return Detail::load_bounded_double(kCameraRotationSpeedScaleKey,
                                     kDefaultCameraSpeedScale,
                                     kMinCameraSpeedScale,
                                     kMaxCameraSpeedScale);
}

inline void save_camera_rotation_speed_scale(double scale) {
  Detail::save_bounded_double(
      kCameraRotationSpeedScaleKey, scale, kMinCameraSpeedScale, kMaxCameraSpeedScale);
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
