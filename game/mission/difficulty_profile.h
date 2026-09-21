#pragma once

#include <QHash>
#include <QList>
#include <QString>

#include <array>
#include <cstdint>

namespace Game::Mission {

enum class DifficultyPreset : std::uint8_t {
  Easy = 0,
  Normal = 1,
  Hard = 2,
  VeryHard = 3,
};

inline constexpr int k_difficulty_preset_count = 4;

struct DifficultyProfile {
  DifficultyPreset preset = DifficultyPreset::Normal;

  float resource_multiplier = 1.0F;

  float starting_unit_multiplier = 1.0F;

  float wave_multiplier = 1.0F;

  [[nodiscard]] auto id() const -> QString;

  [[nodiscard]] auto is_baseline() const -> bool {
    return preset == DifficultyPreset::Normal;
  }
};

[[nodiscard]] auto default_difficulty_id() -> QString;

[[nodiscard]] auto
difficulty_presets() -> const std::array<DifficultyPreset, k_difficulty_preset_count>&;

[[nodiscard]] auto difficulty_id_for(DifficultyPreset preset) -> QString;

[[nodiscard]] auto normalize_difficulty_id(const QString& id) -> QString;

[[nodiscard]] auto difficulty_preset_from_id(const QString& id) -> DifficultyPreset;

[[nodiscard]] auto difficulty_profile_for(DifficultyPreset preset) -> DifficultyProfile;

[[nodiscard]] auto resolve_difficulty(const QString& id) -> DifficultyProfile;

[[nodiscard]] auto scaled_force_count(int authored, float multiplier) -> int;

[[nodiscard]] auto scaled_resource_amount(int authored, float multiplier) -> int;

class MatchDifficulty {
public:
  MatchDifficulty() = default;

  explicit MatchDifficulty(const QString& baseline_id) { set_baseline(baseline_id); }

  void set_baseline(const QString& id);

  void set_owner(int owner_id, const QString& id);

  [[nodiscard]] auto baseline_id() const -> QString { return m_baseline; }

  [[nodiscard]] auto has_owner(int owner_id) const -> bool {
    return m_by_owner.contains(owner_id);
  }

  [[nodiscard]] auto id_for(int owner_id) const -> QString;

  [[nodiscard]] auto profile_for(int owner_id) const -> DifficultyProfile;

  [[nodiscard]] auto is_baseline() const -> bool;

  [[nodiscard]] auto owner_ids() const -> QList<int> { return m_by_owner.keys(); }

private:
  QString m_baseline = QStringLiteral("normal");
  QHash<int, QString> m_by_owner;
};

} // namespace Game::Mission
