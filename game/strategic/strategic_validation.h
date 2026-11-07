#pragma once

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>
#include <optional>

#include "strategic_geometry.h"
#include "strategic_world.h"

namespace Game::Strategic {

enum class Severity {
  Error,
  Warning,
  Info
};

[[nodiscard]] auto to_string(Severity severity) -> QString;

struct Issue {
  Severity severity = Severity::Error;
  QString code;
  QString entity;
  QString message;
  QString related;
};

struct CampaignMissionRef {
  QString mission_id;
  QString world_region_id;
  int order_index = 0;
};

struct MissionSideRef {
  QString side;
  QString faction;
  QStringList commanders;
};

struct MissionFacts {
  QVector<MissionSideRef> sides;
  int first_year = 0;
  int last_year = 0;
};

struct ValidationContext {
  std::function<bool(const QString&)> resource_exists;
  std::optional<QSet<QString>> mission_ids;
  std::optional<QSet<QString>> commander_catalog_ids;
  QVector<CampaignMissionRef> campaign_missions;
  QHash<QString, QString> mission_maps;
  QHash<QString, QString> mission_provinces;
  QHash<QString, MissionFacts> mission_facts;
  const RegionGeometry* geometry = nullptr;
  double region_tolerance_uv = 0.012;
};

struct ValidationReport {
  QVector<Issue> issues;

  [[nodiscard]] auto errors() const -> int;
  [[nodiscard]] auto warnings() const -> int;
  [[nodiscard]] auto ok() const -> bool { return errors() == 0; }
  [[nodiscard]] auto has(const QString& code) const -> bool;
  [[nodiscard]] auto for_entity(const QString& entity) const -> QVector<Issue>;
  [[nodiscard]] auto codes() const -> QStringList;
  void add(Severity severity,
           const QString& code,
           const QString& entity,
           const QString& message,
           const QString& related = {});
};

[[nodiscard]] auto
validate_world(const WorldDefinition& def,
               const ValidationContext& context = {}) -> ValidationReport;

[[nodiscard]] auto validate_state(const WorldDefinition& def,
                                  const WorldState& state,
                                  const QString& label = {}) -> ValidationReport;

[[nodiscard]] auto validation_code_help(const QString& code) -> QString;

} // namespace Game::Strategic
