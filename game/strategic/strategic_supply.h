#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include "strategic_graph.h"
#include "strategic_world.h"

namespace Game::Strategic {

inline constexpr double k_supplied_days = 4.0;
inline constexpr double k_stretched_days = 10.0;
inline constexpr int k_men_per_throughput = 9000;
inline constexpr int k_forage_food_threshold = 3;

struct SupplyReport {
  QString army;
  SupplyLevel level = SupplyLevel::CutOff;
  QString source;
  Path path;
  QStringList alternative_sources;
  int demand = 0;
  int capacity = 0;
  QString reason;
};

struct IsolatedSite {
  QString site;
  QString faction;
  QString capital;
};

struct RouteThreat {
  QString route;
  QString owner;
  QString army;
  double days = 0.0;
};

[[nodiscard]] auto supply_sources(const WorldDefinition& def,
                                  const WorldState& state,
                                  const QString& faction) -> QStringList;

[[nodiscard]] auto evaluate_supply(const WorldDefinition& def,
                                   const WorldState& state,
                                   const QString& army) -> SupplyReport;

[[nodiscard]] auto
evaluate_all_supply(const WorldDefinition& def,
                    const WorldState& state) -> QVector<SupplyReport>;

[[nodiscard]] auto isolated_sites(const WorldDefinition& def,
                                  const WorldState& state) -> QVector<IsolatedSite>;

[[nodiscard]] auto threatened_routes(const WorldDefinition& def,
                                     const WorldState& state,
                                     double within_days = 5.0) -> QVector<RouteThreat>;

[[nodiscard]] auto armies_supplied_by(const WorldDefinition& def,
                                      const WorldState& state,
                                      const QString& site) -> QStringList;

} // namespace Game::Strategic
