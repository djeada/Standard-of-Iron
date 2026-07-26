#include "preferences.h"

#include <QJSEngine>
#include <QQmlEngine>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>

#include "../app/core/user_settings.h"

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
    , m_always_show_focus(UserSettings::load_ui_always_show_focus()) {
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

  m_color_vision_mode = normalized;
  UserSettings::save_ui_color_vision_mode(normalized);
  emit color_vision_mode_changed();
}

void UiPreferences::set_always_show_focus(bool enabled) {
  if (enabled == m_always_show_focus) {
    return;
  }

  m_always_show_focus = enabled;
  UserSettings::save_ui_always_show_focus(enabled);
  emit always_show_focus_changed();
}

void UiPreferences::reset_to_defaults() {
  set_ui_scale(UserSettings::kDefaultUiScale);
  set_reduced_motion(false);
  set_high_contrast(false);
  set_color_vision_mode(QStringLiteral("none"));
  set_always_show_focus(false);
}
