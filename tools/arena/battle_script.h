#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>
#include <vector>

#include "arena_scenario.h"
#include "game/formation/army_formation_types.h"
#include "game/units/troop_type.h"

namespace Game::Units {
struct CommanderDefinition;
}

namespace Arena::BattleScript {

inline constexpr const char* k_schema = "soi.battle_script/1";
inline constexpr float k_default_scale = 0.1F;
inline constexpr const char* k_phase_event_prefix = "phase:";

struct Diagnostic {
  QString path;
  QString message;
};

struct LoadOptions {

  std::optional<float> scale_override;

  std::function<int(Game::Units::TroopType)> individuals_per_unit;

  QString base_directory;
};

struct GroupSummary {
  QString id;
  QString army;
  QString troop;
  QString nation;
  int historical{0};
  int soldiers{0};
  int units{0};
  bool commander{false};
  bool ambush{false};
};

struct PhaseSummary {
  QString id;
  QString event;
  QString label;
};

struct CompileResult {
  std::optional<ArenaScenarioDefinition> scenario;
  std::vector<Diagnostic> errors;
  std::vector<Diagnostic> warnings;
  std::vector<GroupSummary> groups;
  std::vector<PhaseSummary> phases;
  float scale{k_default_scale};
  float authored_scale{k_default_scale};
  float geometry_factor{1.0F};

  [[nodiscard]] auto ok() const noexcept -> bool {
    return scenario.has_value() && errors.empty();
  }
};

[[nodiscard]] auto compile(const QJsonObject& root,
                           const LoadOptions& options = {}) -> CompileResult;

[[nodiscard]] auto load_file(const QString& path,
                             const LoadOptions& options = {}) -> CompileResult;

[[nodiscard]] auto format_diagnostics(const CompileResult& result) -> QString;

[[nodiscard]] auto summary_text(const CompileResult& result) -> QString;

[[nodiscard]] auto register_file(const QString& path,
                                 const LoadOptions& options,
                                 QString* error) -> std::optional<QString>;

[[nodiscard]] auto phase_event_name(const QString& event) -> QString;

[[nodiscard]] auto known_formation_names() -> QStringList;
[[nodiscard]] auto resolve_formation(const QString& name)
    -> std::optional<Game::Formation::ArmyFormationIntent>;

[[nodiscard]] auto known_commander_ids() -> QStringList;
[[nodiscard]] auto
resolve_commander(const QString& catalog_id) -> const Game::Units::CommanderDefinition*;

struct FordSegment {
  int river{0};
  QVector3D at;
  float width{6.0F};
};

[[nodiscard]] auto apply_fords(const std::vector<FordSegment>& fords,
                               ArenaScenarioDefinition& scenario) -> QString;

} // namespace Arena::BattleScript
