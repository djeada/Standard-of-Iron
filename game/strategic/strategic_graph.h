#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

#include <map>

#include "strategic_world.h"

namespace Game::Strategic {

struct TraversalRules {
  QString faction;
  bool avoid_hostile = false;
  bool allow_sea = true;
  bool allow_blocked = false;
  bool allow_contested = true;
  double contested_penalty = 1.5;
  QStringList excluded_routes;
  QStringList excluded_sites;

  [[nodiscard]] static auto anything() -> TraversalRules {
    TraversalRules rules;
    rules.allow_blocked = true;
    return rules;
  }
  [[nodiscard]] static auto land_only() -> TraversalRules {
    TraversalRules rules;
    rules.allow_sea = false;
    rules.allow_blocked = true;
    return rules;
  }
  [[nodiscard]] static auto march(const QString& faction) -> TraversalRules {
    TraversalRules rules;
    rules.faction = faction;
    return rules;
  }
  [[nodiscard]] static auto safe_march(const QString& faction) -> TraversalRules {
    TraversalRules rules;
    rules.faction = faction;
    rules.avoid_hostile = true;
    return rules;
  }
};

struct Path {
  bool found = false;
  QStringList sites;
  QStringList routes;
  double days = 0.0;
  double km = 0.0;
  int min_throughput = 0;
  int sea_legs = 0;
};

class StrategicGraph {
public:
  StrategicGraph(const WorldDefinition& def, const WorldState& state);

  [[nodiscard]] auto definition() const -> const WorldDefinition& { return m_def; }
  [[nodiscard]] auto state() const -> const WorldState& { return m_state; }

  [[nodiscard]] auto route_usable(const Route& route,
                                  const TraversalRules& rules) const -> bool;
  [[nodiscard]] auto site_enterable(const QString& site,
                                    const TraversalRules& rules,
                                    const QString& goal) const -> bool;
  [[nodiscard]] auto can_embark(const Route& route,
                                const QString& from,
                                const TraversalRules& rules) const -> bool;
  [[nodiscard]] auto route_cost_days(const Route& route,
                                     const TraversalRules& rules) const -> double;

  [[nodiscard]] auto shortest_path(const QString& from,
                                   const QString& to,
                                   const TraversalRules& rules) const -> Path;
  [[nodiscard]] auto nearest_of(const QString& from,
                                const QStringList& goals,
                                const TraversalRules& rules) const -> Path;
  [[nodiscard]] auto
  reachable(const QString& from,
            const TraversalRules& rules,
            double max_days = -1.0) const -> std::map<QString, double>;
  [[nodiscard]] auto route_count(const QString& from,
                                 const QString& to,
                                 const TraversalRules& rules) const -> int;
  [[nodiscard]] auto
  components(const TraversalRules& rules) const -> QVector<QStringList>;
  [[nodiscard]] auto chokepoint_sites(const TraversalRules& rules,
                                      int min_cut_off = 2) const -> QStringList;
  [[nodiscard]] auto bridge_routes(const TraversalRules& rules,
                                   int min_cut_off = 1) const -> QStringList;
  [[nodiscard]] auto cut_off_by(const TraversalRules& rules) const -> int;
  [[nodiscard]] auto neighbours(const QString& site) const -> QStringList;

private:
  struct Edge {
    int route = -1;
    int to = -1;
  };

  [[nodiscard]] auto
  run_dijkstra(int source, const TraversalRules& rules, const QString& goal_hint) const
      -> std::pair<QVector<double>, QVector<Edge>>;
  [[nodiscard]] auto build_path(int source,
                                int target,
                                const QVector<double>& dist,
                                const QVector<Edge>& parent) const -> Path;

  const WorldDefinition& m_def;
  const WorldState& m_state;
  QHash<QString, int> m_node_index;
  QStringList m_nodes;
  QVector<QVector<Edge>> m_adjacency;
};

} // namespace Game::Strategic
