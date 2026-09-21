#include "game/mission/difficulty_profile.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Game::Mission {
namespace {

struct PresetEntry {
  DifficultyPreset preset;
  const char* id;
  float resources;
  float units;
  float waves;
};

constexpr std::array<PresetEntry, k_difficulty_preset_count> k_presets{{
    {DifficultyPreset::Easy, "easy", 0.80F, 0.80F, 0.75F},
    {DifficultyPreset::Normal, "normal", 1.00F, 1.00F, 1.00F},
    {DifficultyPreset::Hard, "hard", 1.50F, 1.50F, 1.50F},
    {DifficultyPreset::VeryHard, "very_hard", 2.00F, 2.00F, 2.00F},
}};

auto entry_for(DifficultyPreset preset) -> const PresetEntry& {
  for (const auto& entry : k_presets) {
    if (entry.preset == preset) {
      return entry;
    }
  }
  return k_presets[static_cast<std::size_t>(DifficultyPreset::Normal)];
}

} // namespace

auto DifficultyProfile::id() const -> QString {
  return difficulty_id_for(preset);
}

auto default_difficulty_id() -> QString {
  return difficulty_id_for(DifficultyPreset::Normal);
}

auto difficulty_presets()
    -> const std::array<DifficultyPreset, k_difficulty_preset_count>& {
  static const std::array<DifficultyPreset, k_difficulty_preset_count> k_order{
      DifficultyPreset::Easy,
      DifficultyPreset::Normal,
      DifficultyPreset::Hard,
      DifficultyPreset::VeryHard};
  return k_order;
}

auto difficulty_id_for(DifficultyPreset preset) -> QString {
  return QString::fromLatin1(entry_for(preset).id);
}

auto difficulty_preset_from_id(const QString& id) -> DifficultyPreset {
  const QString key = id.trimmed().toLower();
  if (key.isEmpty()) {
    return DifficultyPreset::Normal;
  }
  for (const auto& entry : k_presets) {
    if (key == QLatin1String(entry.id)) {
      return entry.preset;
    }
  }

  if (key == QLatin1String("recruit")) {
    return DifficultyPreset::Easy;
  }
  if (key == QLatin1String("medium") || key == QLatin1String("standard")) {
    return DifficultyPreset::Normal;
  }
  if (key == QLatin1String("veteran")) {
    return DifficultyPreset::Hard;
  }
  if (key == QLatin1String("brutal") || key == QLatin1String("legendary") ||
      key == QLatin1String("veryhard") || key == QLatin1String("very hard") ||
      key == QLatin1String("very-hard")) {
    return DifficultyPreset::VeryHard;
  }
  return DifficultyPreset::Normal;
}

auto normalize_difficulty_id(const QString& id) -> QString {
  return difficulty_id_for(difficulty_preset_from_id(id));
}

auto difficulty_profile_for(DifficultyPreset preset) -> DifficultyProfile {
  const PresetEntry& entry = entry_for(preset);
  return DifficultyProfile{.preset = entry.preset,
                           .resource_multiplier = entry.resources,
                           .starting_unit_multiplier = entry.units,
                           .wave_multiplier = entry.waves};
}

auto resolve_difficulty(const QString& id) -> DifficultyProfile {
  return difficulty_profile_for(difficulty_preset_from_id(id));
}

namespace {

auto scaled(int authored, float multiplier) -> int {
  const float clamped = std::clamp(multiplier, 0.1F, 8.0F);
  const double product =
      std::round(static_cast<double>(authored) * static_cast<double>(clamped));
  const auto ceiling = static_cast<double>(std::numeric_limits<int>::max());
  const auto floor = static_cast<double>(std::numeric_limits<int>::lowest());
  return static_cast<int>(std::clamp(product, floor, ceiling));
}

} // namespace

auto scaled_force_count(int authored, float multiplier) -> int {
  if (authored <= 0) {
    return 0;
  }
  return std::max(1, scaled(authored, multiplier));
}

auto scaled_resource_amount(int authored, float multiplier) -> int {
  if (authored <= 0) {
    return authored;
  }
  return std::max(0, scaled(authored, multiplier));
}

void MatchDifficulty::set_baseline(const QString& id) {
  m_baseline = normalize_difficulty_id(id);
}

void MatchDifficulty::set_owner(int owner_id, const QString& id) {
  m_by_owner.insert(owner_id, normalize_difficulty_id(id));
}

auto MatchDifficulty::id_for(int owner_id) const -> QString {
  const auto found = m_by_owner.constFind(owner_id);
  return found != m_by_owner.constEnd() ? found.value() : m_baseline;
}

auto MatchDifficulty::profile_for(int owner_id) const -> DifficultyProfile {
  return resolve_difficulty(id_for(owner_id));
}

auto MatchDifficulty::is_baseline() const -> bool {
  if (resolve_difficulty(m_baseline).preset != DifficultyPreset::Normal) {
    return false;
  }
  return std::none_of(m_by_owner.begin(), m_by_owner.end(), [](const QString& id) {
    return resolve_difficulty(id).preset != DifficultyPreset::Normal;
  });
}

} // namespace Game::Mission
