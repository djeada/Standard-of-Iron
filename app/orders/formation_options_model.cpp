#include "app/orders/formation_options_model.h"

#include <algorithm>
#include <cmath>

namespace App::Controllers {

namespace {

struct PresetScale {
  float low;
  float mid;
  float high;
};

constexpr PresetScale k_frontage_scales{0.7F, 1.0F, 1.45F};
constexpr PresetScale k_depth_scales{0.7F, 1.0F, 1.45F};
constexpr PresetScale k_spacing_scales{0.75F, 1.0F, 1.35F};

auto scale_for_preset(const QString& preset, const PresetScale& scales) -> float {
  const QString lowered = preset.trimmed().toLower();
  if (lowered == QStringLiteral("narrow") || lowered == QStringLiteral("shallow") ||
      lowered == QStringLiteral("tight")) {
    return scales.low;
  }
  if (lowered == QStringLiteral("wide") || lowered == QStringLiteral("deep") ||
      lowered == QStringLiteral("loose")) {
    return scales.high;
  }
  return scales.mid;
}

auto preset_index_for_scale(float scale, const PresetScale& scales) -> int {
  float const to_low = std::abs(scale - scales.low);
  float const to_mid = std::abs(scale - scales.mid);
  float const to_high = std::abs(scale - scales.high);
  if (to_low <= to_mid && to_low <= to_high) {
    return 0;
  }
  return to_mid <= to_high ? 1 : 2;
}

} // namespace

auto FormationOptionsModel::set_intent(const QString& intent_id) -> bool {
  auto parsed = Game::Formation::try_parse_intent(intent_id);
  if (!parsed || m_intent == *parsed) {
    return false;
  }
  m_intent = *parsed;
  return true;
}

auto FormationOptionsModel::set_flank_preference(const QString& preference) -> bool {
  auto parsed = Game::Formation::try_parse_flank_preference(preference);
  if (parsed) {
    m_options.flank_preference = *parsed;
  }
  return parsed.has_value();
}

auto FormationOptionsModel::set_ranged_placement(const QString& placement) -> bool {
  auto parsed = Game::Formation::try_parse_ranged_placement(placement);
  if (parsed) {
    m_options.ranged_placement = *parsed;
  }
  return parsed.has_value();
}

auto FormationOptionsModel::set_movement_policy(const QString& policy) -> bool {
  auto parsed = Game::Formation::try_parse_movement_policy(policy);
  if (parsed) {
    m_options.movement_policy = *parsed;
  }
  return parsed.has_value();
}

auto FormationOptionsModel::set_mixed_policy(const QString& policy) -> bool {
  auto parsed = Game::Formation::try_parse_mixed_policy(policy);
  if (parsed) {
    m_options.mixed_policy = *parsed;
  }
  return parsed.has_value();
}

void FormationOptionsModel::adjust_depth(float wheel_delta) {
  float const step = wheel_delta > 0.0F ? 1.15F : (1.0F / 1.15F);
  m_options.depth_scale = std::clamp(m_options.depth_scale * step, 0.4F, 3.0F);
}

void FormationOptionsModel::set_preserve_order(bool preserve) {
  m_options.preserve_member_order = preserve;
}

void FormationOptionsModel::set_frontage_preset(const QString& preset) {
  m_options.frontage_scale = scale_for_preset(preset, k_frontage_scales);
}

void FormationOptionsModel::set_depth_preset(const QString& preset) {
  m_options.depth_scale = scale_for_preset(preset, k_depth_scales);
}

void FormationOptionsModel::set_spacing_preset(const QString& preset) {
  m_options.spacing_scale = scale_for_preset(preset, k_spacing_scales);
}

void FormationOptionsModel::set_reserve_rows(int rows) {
  m_options.reserve_rows = std::clamp(rows, -1, 2);
}

void FormationOptionsModel::set_doctrine_override(const QString& doctrine) {
  const QString trimmed = doctrine.trimmed().toLower();
  if (trimmed.isEmpty() || trimmed == QStringLiteral("automatic")) {
    m_doctrine_override.clear();
    m_options.doctrine_locked = false;
    return;
  }
  m_doctrine_override = trimmed.toStdString();
  m_options.doctrine_locked = true;
}

void FormationOptionsModel::reset() {
  m_options = Game::Formation::ArmyFormationOptions{};
  m_doctrine_override.clear();
  m_intent = Game::Formation::ArmyFormationIntent::FactionDefault;
}

auto FormationOptionsModel::movement_from_doctrine() const -> bool {
  return m_options.movement_policy == Game::Formation::MovementPolicy::DoctrineDefault;
}

auto FormationOptionsModel::movement_index(
    Game::Formation::MovementPolicy effective) const -> int {
  return movement_from_doctrine() ? 0 : static_cast<int>(effective) + 1;
}

auto FormationOptionsModel::preset_indices() const -> FormationPresetIndices {
  return {
      .frontage = preset_index_for_scale(m_options.frontage_scale, k_frontage_scales),
      .depth = preset_index_for_scale(m_options.depth_scale, k_depth_scales),
      .spacing = preset_index_for_scale(m_options.spacing_scale, k_spacing_scales),
      .flank = static_cast<int>(m_options.flank_preference),
      .ranged =
          m_options.ranged_placement == Game::Formation::RangedPlacement::Automatic
              ? 0
              : static_cast<int>(m_options.ranged_placement) + 1,
      .reserve = std::clamp(m_options.reserve_rows, -1, 2) + 1,
      .mixed = static_cast<int>(m_options.mixed_policy),
      .preserve = m_options.preserve_member_order ? 1 : 0,
  };
}

} // namespace App::Controllers
