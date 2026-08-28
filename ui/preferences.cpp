#include "preferences.h"

#include <QJSEngine>
#include <QQmlEngine>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>

#include "../game/accessibility/commander_input_settings.h"
#include "../game/accessibility/motion_settings.h"
#include "../game/accessibility/team_identity.h"
#include "../game/render_bridge/camera_speeds.h"
#include "app/core/user_settings.h"

namespace UserSettings = App::Core::UserSettings;

namespace {

auto is_finite_scale(qreal value) -> bool {
  static_assert(std::numeric_limits<double>::is_iec559,
                "UI scale validation assumes IEEE-754 doubles");
  const auto as_double = static_cast<double>(value);
  std::uint64_t bits = 0;
  std::memcpy(&bits, &as_double, sizeof(bits));
  constexpr std::uint64_t k_exponent_mask = 0x7FF0000000000000ULL;
  return (bits & k_exponent_mask) != k_exponent_mask;
}

} // namespace

UiPreferences* UiPreferences::m_instance = nullptr;

UiPreferences::UiPreferences(QObject* parent)
    : QObject(parent)
    , m_ui_scale(UserSettings::load_ui_scale())
    , m_reduced_motion(UserSettings::load_ui_reduced_motion())
    , m_high_contrast(UserSettings::load_ui_high_contrast())
    , m_color_vision_mode(UserSettings::load_ui_color_vision_mode())
    , m_always_show_focus(UserSettings::load_ui_always_show_focus())
    , m_team_patterns(UserSettings::load_ui_team_patterns())
    , m_edge_scroll_enabled(UserSettings::load_ui_edge_scroll_enabled())
    , m_edge_scroll_sensitivity(UserSettings::load_ui_edge_scroll_sensitivity())
    , m_camera_motion_scale(UserSettings::load_ui_camera_motion_scale())
    , m_commander_look_sensitivity_x(UserSettings::load_commander_look_sensitivity_x())
    , m_commander_look_sensitivity_y(UserSettings::load_commander_look_sensitivity_y())
    , m_commander_invert_look_y(UserSettings::load_commander_invert_look_y())
    , m_commander_camera_impulse(UserSettings::load_commander_camera_impulse())
    , m_commander_head_bob(UserSettings::load_commander_head_bob())
    , m_commander_field_of_view_scale(
          UserSettings::load_commander_field_of_view_scale())
    , m_commander_guard_is_toggle(UserSettings::load_commander_guard_is_toggle())
    , m_damage_numbers(UserSettings::load_ui_damage_numbers())
    , m_damage_number_mode(UserSettings::load_ui_damage_number_mode())
    , m_economy_numbers(UserSettings::load_ui_economy_numbers())
    , m_camera_legend_seen(UserSettings::load_ui_camera_legend_seen())
    , m_tutorial_completed(UserSettings::load_ui_tutorial_completed())
    , m_screen_effect_intensity(UserSettings::load_ui_screen_effect_intensity())
    , m_display_window_mode(UserSettings::load_display_window_mode())
    , m_display_vsync(UserSettings::load_display_vsync())
    , m_show_fps(UserSettings::load_ui_show_fps())
    , m_camera_pan_speed(UserSettings::load_camera_pan_speed_scale())
    , m_camera_zoom_speed(UserSettings::load_camera_zoom_speed_scale())
    , m_camera_rotation_speed(UserSettings::load_camera_rotation_speed_scale()) {

  Game::Accessibility::TeamIdentity::set_palette_variant_from_mode(
      m_color_vision_mode.toStdString());
  Game::Accessibility::TeamIdentity::set_patterns_enabled(effective_team_patterns());
  Game::Accessibility::MotionSettings::set_camera_motion_scale(
      static_cast<float>(m_camera_motion_scale));
  Game::Accessibility::MotionSettings::set_reduced_motion(m_reduced_motion);
  publish_commander_input_settings();
  publish_camera_speeds();
}

void UiPreferences::publish_commander_input_settings() const {
  namespace CommanderInput = Game::Accessibility::CommanderInput;
  CommanderInput::set_look_sensitivity_x(
      static_cast<float>(m_commander_look_sensitivity_x));
  CommanderInput::set_look_sensitivity_y(
      static_cast<float>(m_commander_look_sensitivity_y));
  CommanderInput::set_invert_look_y(m_commander_invert_look_y);
  CommanderInput::set_camera_impulse_enabled(m_commander_camera_impulse);
  CommanderInput::set_head_bob_enabled(m_commander_head_bob);
  CommanderInput::set_field_of_view_scale(
      static_cast<float>(m_commander_field_of_view_scale));
  CommanderInput::set_guard_is_toggle(m_commander_guard_is_toggle);
}

void UiPreferences::publish_camera_speeds() const {
  namespace CameraSpeeds = Game::Systems::CameraSpeeds;
  CameraSpeeds::set_pan_scale(static_cast<float>(m_camera_pan_speed));
  CameraSpeeds::set_zoom_scale(static_cast<float>(m_camera_zoom_speed));
  CameraSpeeds::set_rotation_scale(static_cast<float>(m_camera_rotation_speed));
}

void UiPreferences::set_commander_look_sensitivity_x(qreal scale) {
  if (!is_finite_scale(scale)) {
    return;
  }
  const qreal clamped = std::clamp<qreal>(scale,
                                          UserSettings::kMinCommanderLookSensitivity,
                                          UserSettings::kMaxCommanderLookSensitivity);
  if (qFuzzyCompare(clamped, m_commander_look_sensitivity_x)) {
    return;
  }
  m_commander_look_sensitivity_x = clamped;
  UserSettings::save_commander_look_sensitivity_x(clamped);
  publish_commander_input_settings();
  emit commander_input_changed();
}

void UiPreferences::set_commander_look_sensitivity_y(qreal scale) {
  if (!is_finite_scale(scale)) {
    return;
  }
  const qreal clamped = std::clamp<qreal>(scale,
                                          UserSettings::kMinCommanderLookSensitivity,
                                          UserSettings::kMaxCommanderLookSensitivity);
  if (qFuzzyCompare(clamped, m_commander_look_sensitivity_y)) {
    return;
  }
  m_commander_look_sensitivity_y = clamped;
  UserSettings::save_commander_look_sensitivity_y(clamped);
  publish_commander_input_settings();
  emit commander_input_changed();
}

void UiPreferences::set_commander_invert_look_y(bool enabled) {
  if (enabled == m_commander_invert_look_y) {
    return;
  }
  m_commander_invert_look_y = enabled;
  UserSettings::save_commander_invert_look_y(enabled);
  publish_commander_input_settings();
  emit commander_input_changed();
}

void UiPreferences::set_commander_camera_impulse(bool enabled) {
  if (enabled == m_commander_camera_impulse) {
    return;
  }
  m_commander_camera_impulse = enabled;
  UserSettings::save_commander_camera_impulse(enabled);
  publish_commander_input_settings();
  emit commander_input_changed();
}

void UiPreferences::set_commander_head_bob(bool enabled) {
  if (enabled == m_commander_head_bob) {
    return;
  }
  m_commander_head_bob = enabled;
  UserSettings::save_commander_head_bob(enabled);
  publish_commander_input_settings();
  emit commander_input_changed();
}

void UiPreferences::set_commander_field_of_view_scale(qreal scale) {
  if (!is_finite_scale(scale)) {
    return;
  }
  const qreal clamped = std::clamp<qreal>(scale,
                                          UserSettings::kMinCommanderFieldOfViewScale,
                                          UserSettings::kMaxCommanderFieldOfViewScale);
  if (qFuzzyCompare(clamped, m_commander_field_of_view_scale)) {
    return;
  }
  m_commander_field_of_view_scale = clamped;
  UserSettings::save_commander_field_of_view_scale(clamped);
  publish_commander_input_settings();
  emit commander_input_changed();
}

void UiPreferences::set_commander_guard_is_toggle(bool enabled) {
  if (enabled == m_commander_guard_is_toggle) {
    return;
  }
  m_commander_guard_is_toggle = enabled;
  UserSettings::save_commander_guard_is_toggle(enabled);
  publish_commander_input_settings();
  emit commander_input_changed();
}

void UiPreferences::set_display_window_mode(const QString& mode) {
  const QString normalized = mode.trimmed().toLower();
  if (!UserSettings::is_supported_window_mode(normalized) ||
      normalized == m_display_window_mode) {
    return;
  }

  m_display_window_mode = normalized;
  UserSettings::save_display_window_mode(normalized);
  emit display_window_mode_changed();
}

void UiPreferences::set_display_vsync(bool enabled) {
  if (enabled == m_display_vsync) {
    return;
  }

  m_display_vsync = enabled;
  UserSettings::save_display_vsync(enabled);
  emit display_vsync_changed();
}

void UiPreferences::set_show_fps(bool enabled) {
  if (enabled == m_show_fps) {
    return;
  }

  m_show_fps = enabled;
  UserSettings::save_ui_show_fps(enabled);
  emit show_fps_changed();
}

void UiPreferences::set_camera_pan_speed(qreal scale) {
  if (!is_finite_scale(scale)) {
    return;
  }
  const qreal clamped = std::clamp<qreal>(
      scale, UserSettings::kMinCameraSpeedScale, UserSettings::kMaxCameraSpeedScale);
  if (qFuzzyCompare(clamped, m_camera_pan_speed)) {
    return;
  }

  m_camera_pan_speed = clamped;
  UserSettings::save_camera_pan_speed_scale(clamped);
  publish_camera_speeds();
  emit camera_speeds_changed();
}

void UiPreferences::set_camera_zoom_speed(qreal scale) {
  if (!is_finite_scale(scale)) {
    return;
  }
  const qreal clamped = std::clamp<qreal>(
      scale, UserSettings::kMinCameraSpeedScale, UserSettings::kMaxCameraSpeedScale);
  if (qFuzzyCompare(clamped, m_camera_zoom_speed)) {
    return;
  }

  m_camera_zoom_speed = clamped;
  UserSettings::save_camera_zoom_speed_scale(clamped);
  publish_camera_speeds();
  emit camera_speeds_changed();
}

void UiPreferences::set_camera_rotation_speed(qreal scale) {
  if (!is_finite_scale(scale)) {
    return;
  }
  const qreal clamped = std::clamp<qreal>(
      scale, UserSettings::kMinCameraSpeedScale, UserSettings::kMaxCameraSpeedScale);
  if (qFuzzyCompare(clamped, m_camera_rotation_speed)) {
    return;
  }

  m_camera_rotation_speed = clamped;
  UserSettings::save_camera_rotation_speed_scale(clamped);
  publish_camera_speeds();
  emit camera_speeds_changed();
}

auto UiPreferences::instance() -> UiPreferences* {
  if (m_instance == nullptr) {
    m_instance = new UiPreferences();
  }
  return m_instance;
}

auto UiPreferences::create(QQmlEngine* engine,
                           QJSEngine* scriptEngine) -> UiPreferences* {
  Q_UNUSED(engine)
  Q_UNUSED(scriptEngine)
  auto* prefs = instance();
  QQmlEngine::setObjectOwnership(prefs, QQmlEngine::CppOwnership);
  return prefs;
}

auto UiPreferences::color_vision_modes() -> QStringList {
  return {QStringLiteral("none"),
          QStringLiteral("protanopia"),
          QStringLiteral("deuteranopia"),
          QStringLiteral("tritanopia")};
}

auto UiPreferences::min_ui_scale() -> qreal {
  return UserSettings::kMinUiScale;
}

auto UiPreferences::max_ui_scale() -> qreal {
  return UserSettings::kMaxUiScale;
}

auto UiPreferences::min_edge_scroll_sensitivity() -> qreal {
  return UserSettings::kMinEdgeScrollSensitivity;
}

auto UiPreferences::max_edge_scroll_sensitivity() -> qreal {
  return UserSettings::kMaxEdgeScrollSensitivity;
}

auto UiPreferences::effective_team_patterns() const -> bool {
  return m_team_patterns || m_color_vision_mode != QLatin1String("none");
}

void UiPreferences::set_ui_scale(qreal scale) {
  if (!is_finite_scale(scale)) {
    return;
  }

  const qreal clamped =
      std::clamp<qreal>(scale, UserSettings::kMinUiScale, UserSettings::kMaxUiScale);
  if (qFuzzyCompare(clamped, m_ui_scale)) {
    return;
  }

  m_ui_scale = clamped;
  UserSettings::save_ui_scale(clamped);
  emit ui_scale_changed();
}

void UiPreferences::set_reduced_motion(bool enabled) {
  if (enabled == m_reduced_motion) {
    return;
  }

  m_reduced_motion = enabled;
  UserSettings::save_ui_reduced_motion(enabled);
  Game::Accessibility::MotionSettings::set_reduced_motion(enabled);
  emit reduced_motion_changed();
}

void UiPreferences::set_high_contrast(bool enabled) {
  if (enabled == m_high_contrast) {
    return;
  }

  m_high_contrast = enabled;
  UserSettings::save_ui_high_contrast(enabled);
  emit high_contrast_changed();
}

void UiPreferences::set_color_vision_mode(const QString& mode) {
  const QString normalized = mode.trimmed().toLower();
  if (!UserSettings::is_supported_color_vision_mode(normalized)) {
    return;
  }
  if (normalized == m_color_vision_mode) {
    return;
  }

  const bool patterns_were_effective = effective_team_patterns();
  m_color_vision_mode = normalized;
  UserSettings::save_ui_color_vision_mode(normalized);
  Game::Accessibility::TeamIdentity::set_palette_variant_from_mode(
      normalized.toStdString());
  emit color_vision_mode_changed();
  if (patterns_were_effective != effective_team_patterns()) {
    Game::Accessibility::TeamIdentity::set_patterns_enabled(effective_team_patterns());
    emit team_patterns_changed();
  }
}

void UiPreferences::set_always_show_focus(bool enabled) {
  if (enabled == m_always_show_focus) {
    return;
  }

  m_always_show_focus = enabled;
  UserSettings::save_ui_always_show_focus(enabled);
  emit always_show_focus_changed();
}

void UiPreferences::set_team_patterns(bool enabled) {
  if (enabled == m_team_patterns) {
    return;
  }

  const bool was_effective = effective_team_patterns();
  m_team_patterns = enabled;
  UserSettings::save_ui_team_patterns(enabled);
  if (was_effective != effective_team_patterns()) {
    Game::Accessibility::TeamIdentity::set_patterns_enabled(effective_team_patterns());
    emit team_patterns_changed();
  }
}

void UiPreferences::set_edge_scroll_enabled(bool enabled) {
  if (enabled == m_edge_scroll_enabled) {
    return;
  }

  m_edge_scroll_enabled = enabled;
  UserSettings::save_ui_edge_scroll_enabled(enabled);
  emit edge_scroll_enabled_changed();
}

void UiPreferences::set_edge_scroll_sensitivity(qreal sensitivity) {
  if (!is_finite_scale(sensitivity)) {
    return;
  }

  const qreal clamped = std::clamp<qreal>(sensitivity,
                                          UserSettings::kMinEdgeScrollSensitivity,
                                          UserSettings::kMaxEdgeScrollSensitivity);
  if (qFuzzyCompare(clamped, m_edge_scroll_sensitivity)) {
    return;
  }

  m_edge_scroll_sensitivity = clamped;
  UserSettings::save_ui_edge_scroll_sensitivity(clamped);
  emit edge_scroll_sensitivity_changed();
}

void UiPreferences::set_camera_motion_scale(qreal scale) {
  if (!is_finite_scale(scale)) {
    return;
  }

  const qreal clamped = std::clamp<qreal>(scale, 0.0, 1.0);
  if (qFuzzyCompare(clamped, m_camera_motion_scale)) {
    return;
  }

  m_camera_motion_scale = clamped;
  UserSettings::save_ui_camera_motion_scale(clamped);
  Game::Accessibility::MotionSettings::set_camera_motion_scale(
      static_cast<float>(clamped));
  emit camera_motion_scale_changed();
}

void UiPreferences::set_camera_legend_seen(bool seen) {
  if (seen == m_camera_legend_seen) {
    return;
  }

  m_camera_legend_seen = seen;
  UserSettings::save_ui_camera_legend_seen(seen);
  emit camera_legend_seen_changed();
}

void UiPreferences::set_tutorial_completed(bool completed) {
  if (completed == m_tutorial_completed) {
    return;
  }

  m_tutorial_completed = completed;
  UserSettings::save_ui_tutorial_completed(completed);
  emit tutorial_completed_changed();
}

void UiPreferences::set_damage_numbers(bool enabled) {
  set_damage_number_mode(enabled ? QStringLiteral("all") : QStringLiteral("off"));
}

void UiPreferences::set_damage_number_mode(const QString& mode) {
  if (!UserSettings::is_damage_number_mode(mode) || mode == m_damage_number_mode) {
    return;
  }

  m_damage_number_mode = mode;
  const bool enabled = mode != QStringLiteral("off");
  UserSettings::save_ui_damage_number_mode(mode);
  emit damage_number_mode_changed();

  if (enabled != m_damage_numbers) {
    m_damage_numbers = enabled;
    emit damage_numbers_changed();
  }
}

void UiPreferences::set_economy_numbers(bool enabled) {
  if (enabled == m_economy_numbers) {
    return;
  }
  m_economy_numbers = enabled;
  UserSettings::save_ui_economy_numbers(enabled);
  emit economy_numbers_changed();
}

void UiPreferences::set_screen_effect_intensity(qreal intensity) {
  if (!is_finite_scale(intensity)) {
    return;
  }

  const qreal clamped = std::clamp<qreal>(intensity, 0.0, 1.0);
  if (qFuzzyCompare(clamped, m_screen_effect_intensity)) {
    return;
  }

  m_screen_effect_intensity = clamped;
  UserSettings::save_ui_screen_effect_intensity(clamped);
  emit screen_effect_intensity_changed();
}

void UiPreferences::reset_to_defaults() {
  set_ui_scale(UserSettings::kDefaultUiScale);
  set_reduced_motion(false);
  set_high_contrast(false);
  set_color_vision_mode(QStringLiteral("none"));
  set_always_show_focus(false);
  set_team_patterns(false);
  set_edge_scroll_enabled(true);
  set_edge_scroll_sensitivity(UserSettings::kDefaultEdgeScrollSensitivity);
  set_camera_motion_scale(UserSettings::kDefaultCameraMotionScale);
  set_damage_number_mode(QStringLiteral("all"));
  set_economy_numbers(true);
  set_camera_legend_seen(false);
  set_screen_effect_intensity(UserSettings::kDefaultScreenEffectIntensity);
  set_commander_look_sensitivity_x(UserSettings::kDefaultCommanderLookSensitivity);
  set_commander_look_sensitivity_y(UserSettings::kDefaultCommanderLookSensitivity);
  set_commander_invert_look_y(false);
  set_commander_camera_impulse(true);
  set_commander_head_bob(true);
  set_commander_field_of_view_scale(UserSettings::kDefaultCommanderFieldOfViewScale);
  set_commander_guard_is_toggle(false);
  set_display_window_mode(QString::fromLatin1(UserSettings::kDefaultDisplayWindowMode));
  set_display_vsync(UserSettings::kDefaultDisplayVsync);
  set_show_fps(UserSettings::kDefaultUiShowFps);
  set_camera_pan_speed(UserSettings::kDefaultCameraSpeedScale);
  set_camera_zoom_speed(UserSettings::kDefaultCameraSpeedScale);
  set_camera_rotation_speed(UserSettings::kDefaultCameraSpeedScale);
}
