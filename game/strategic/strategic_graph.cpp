#include "strategic_graph.h"

#include <QSet>
#include <queue>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

namespace Game::Strategic {

namespace {

constexpr double k_infinity = std::numeric_limits<double>::infinity();

} // namespace

StrategicGraph::StrategicGraph(const WorldDefinition& def, const WorldState& state)
    : m_def(def)
    , m_state(state) {
  for (const auto& site : def.sites) {
    if (!m_node_index.contains(site.id)) {
      m_node_index.insert(site.id, static_cast<int>(m_nodes.size()));
      m_nodes.append(site.id);
    }
  }
  m_adjacency.resize(m_nodes.size());
  for (int r = 0; r < def.routes.size(); ++r) {
    const auto& route = def.routes[r];
    const int a = m_node_index.value(route.from, -1);
    const int b = m_node_index.value(route.to, -1);
    if (a < 0 || b < 0 || a == b) {
      continue;
    }
    m_adjacency[a].append({r, b});
    m_adjacency[b].append({r, a});
  }
}

auto StrategicGraph::route_usable(const Route& route,
                                  const TraversalRules& rules) const -> bool {
  if (rules.excluded_routes.contains(route.id)) {
    return false;
  }
  const RouteStatus status = route_status(m_state, route.id);
  if (status == RouteStatus::Blocked && !rules.allow_blocked) {
    return false;
  }
  if (status == RouteStatus::Contested && !rules.allow_contested) {
    return false;
  }
  if (route.kind == RouteKind::SeaLane) {
    if (!rules.allow_sea) {
      return false;
    }
    if (!rules.allow_blocked && (!port_usable(m_def, m_state, route.from) ||
                                 !port_usable(m_def, m_state, route.to))) {
      return false;
    }
  }
  return true;
}

auto StrategicGraph::site_enterable(const QString& site,
                                    const TraversalRules& rules,
                                    const QString& goal) const -> bool {
  if (rules.excluded_sites.contains(site)) {
    return false;
  }
  if (!rules.avoid_hostile || rules.faction.isEmpty() || site == goal) {
    return true;
  }
  return !are_hostile(m_def, rules.faction, site_controller(m_def, m_state, site));
}

auto StrategicGraph::can_embark(const Route& route,
                                const QString& from,
                                const TraversalRules& rules) const -> bool {
  if (route.kind != RouteKind::SeaLane || rules.faction.isEmpty()) {
    return true;
  }
  return !are_hostile(m_def, rules.faction, site_controller(m_def, m_state, from));
}

auto StrategicGraph::route_cost_days(const Route& route,
                                     const TraversalRules& rules) const -> double {
  double days = m_def.route_days(route);
  if (route_status(m_state, route.id) == RouteStatus::Contested) {
    days *= rules.contested_penalty;
  }
  return std::max(days, 0.05);
}

auto StrategicGraph::run_dijkstra(int source,
                                  const TraversalRules& rules,
                                  const QString& goal_hint) const
    -> std::pair<QVector<double>, QVector<Edge>> {
  QVector<double> dist(m_nodes.size(), k_infinity);
  QVector<Edge> parent(m_nodes.size());
  if (source < 0) {
    return {dist, parent};
  }
  using Item = std::pair<double, int>;
  std::priority_queue<Item, std::vector<Item>, std::greater<>> queue;
  dist[source] = 0.0;
  queue.emplace(0.0, source);
  while (!queue.empty()) {
    const auto [d, node] = queue.top();
    queue.pop();
    if (d > dist[node]) {
      continue;
    }
    if (node != source && !goal_hint.isEmpty() && m_nodes[node] == goal_hint) {
      continue;
    }
    for (const Edge& edge : m_adjacency[node]) {
      const Route& route = m_def.routes[edge.route];
      if (!route_usable(route, rules) || !can_embark(route, m_nodes[node], rules)) {
        continue;
      }
      if (!site_enterable(m_nodes[edge.to], rules, goal_hint)) {
        continue;
      }
      const double next = d + route_cost_days(route, rules);
      const bool better = next < dist[edge.to] - 1e-9;
      const bool tie_break =
          std::abs(next - dist[edge.to]) <= 1e-9 && parent[edge.to].route > edge.route;
      if (better || tie_break) {
        dist[edge.to] = next;
        parent[edge.to] = {edge.route, node};
        queue.emplace(next, edge.to);
      }
    }
  }
  return {dist, parent};
}

auto StrategicGraph::build_path(int source,
                                int target,
                                const QVector<double>& dist,
                                const QVector<Edge>& parent) const -> Path {
  Path path;
  if (source < 0 || target < 0 || dist[target] == k_infinity) {
    return path;
  }
  path.found = true;
  path.days = dist[target];
  path.min_throughput = k_throughput_max;
  int node = target;
  QStringList sites{m_nodes[target]};
  QStringList routes;
  while (node != source) {
    const Edge& edge = parent[node];
    const Route& route = m_def.routes[edge.route];
    routes.prepend(route.id);
    path.km += m_def.route_length_km(route);
    path.min_throughput = std::min(path.min_throughput, route.throughput);
    if (route.kind == RouteKind::SeaLane) {
      ++path.sea_legs;
    }
    node = edge.to;
    sites.prepend(m_nodes[node]);
  }
  path.sites = sites;
  path.routes = routes;
  return path;
}

auto StrategicGraph::shortest_path(const QString& from,
                                   const QString& to,
                                   const TraversalRules& rules) const -> Path {
  const int source = m_node_index.value(from, -1);
  const int target = m_node_index.value(to, -1);
  if (source < 0 || target < 0) {
    return {};
  }
  if (source == target) {
    Path path;
    path.found = true;
    path.sites = {from};
    path.min_throughput = k_throughput_max;
    return path;
  }
  const auto [dist, parent] = run_dijkstra(source, rules, to);
  return build_path(source, target, dist, parent);
}

auto StrategicGraph::nearest_of(const QString& from,
                                const QStringList& goals,
                                const TraversalRules& rules) const -> Path {
  const int source = m_node_index.value(from, -1);
  if (source < 0 || goals.isEmpty()) {
    return {};
  }
  if (goals.contains(from)) {
    Path path;
    path.found = true;
    path.sites = {from};
    path.min_throughput = k_throughput_max;
    return path;
  }
  const auto [dist, parent] = run_dijkstra(source, rules, {});
  int best = -1;
  for (const auto& goal : goals) {
    const int index = m_node_index.value(goal, -1);
    if (index >= 0 && dist[index] < k_infinity &&
        (best < 0 || dist[index] < dist[best] - 1e-9)) {
      best = index;
    }
  }
  return build_path(source, best, dist, parent);
}

auto StrategicGraph::reachable(const QString& from,
                               const TraversalRules& rules,
                               double max_days) const -> std::map<QString, double> {
  std::map<QString, double> out;
  const int source = m_node_index.value(from, -1);
  if (source < 0) {
    return out;
  }
  const auto [dist, parent] = run_dijkstra(source, rules, {});
  for (int i = 0; i < m_nodes.size(); ++i) {
    if (dist[i] < k_infinity && (max_days < 0.0 || dist[i] <= max_days)) {
      out[m_nodes[i]] = dist[i];
    }
  }
  return out;
}

auto StrategicGraph::route_count(const QString& from,
                                 const QString& to,
                                 const TraversalRules& rules) const -> int {
  const int source = m_node_index.value(from, -1);
  const int target = m_node_index.value(to, -1);
  if (source < 0 || target < 0 || source == target) {
    return 0;
  }
  const int n = static_cast<int>(m_nodes.size());
  QVector<QHash<int, int>> capacity(n);
  for (int node = 0; node < n; ++node) {
    for (const Edge& edge : m_adjacency[node]) {
      const Route& route = m_def.routes[edge.route];
      if (!route_usable(route, rules) || !can_embark(route, m_nodes[node], rules) ||
          !site_enterable(m_nodes[edge.to], rules, to) ||
          !site_enterable(m_nodes[node], rules, to)) {
        continue;
      }
      capacity[node][edge.to] += 1;
    }
  }
  int flow = 0;
  while (true) {
    QVector<int> prev(n, -1);
    prev[source] = source;
    std::queue<int> queue;
    queue.push(source);
    while (!queue.empty() && prev[target] < 0) {
      const int node = queue.front();
      queue.pop();
      for (auto it = capacity[node].cbegin(); it != capacity[node].cend(); ++it) {
        if (it.value() > 0 && prev[it.key()] < 0) {
          prev[it.key()] = node;
          queue.push(it.key());
        }
      }
    }
    if (prev[target] < 0) {
      break;
    }
    for (int node = target; node != source; node = prev[node]) {
      capacity[prev[node]][node] -= 1;
      capacity[node][prev[node]] += 1;
    }
    ++flow;
  }
  return flow;
}

auto StrategicGraph::components(const TraversalRules& rules) const
    -> QVector<QStringList> {
  QVector<QStringList> out;
  QVector<bool> seen(m_nodes.size(), false);
  for (int start = 0; start < m_nodes.size(); ++start) {
    if (seen[start]) {
      continue;
    }
    QStringList component;
    std::queue<int> queue;
    queue.push(start);
    seen[start] = true;
    while (!queue.empty()) {
      const int node = queue.front();
      queue.pop();
      component.append(m_nodes[node]);
      for (const Edge& edge : m_adjacency[node]) {
        if (seen[edge.to] || !route_usable(m_def.routes[edge.route], rules)) {
          continue;
        }
        seen[edge.to] = true;
        queue.push(edge.to);
      }
    }
    out.append(component);
  }
  std::stable_sort(
      out.begin(), out.end(), [](const QStringList& a, const QStringList& b) {
        return a.size() > b.size();
      });
  return out;
}

auto StrategicGraph::cut_off_by(const TraversalRules& rules) const -> int {
  const auto parts = components(rules);
  int stranded = 0;
  for (int i = 1; i < parts.size(); ++i) {
    stranded += static_cast<int>(parts[i].size());
  }
  return stranded;
}

auto StrategicGraph::chokepoint_sites(const TraversalRules& rules,
                                      int min_cut_off) const -> QStringList {
  const int n = static_cast<int>(m_nodes.size());
  QVector<int> disc(n, -1);
  QVector<int> low(n, 0);
  QVector<bool> articulation(n, false);
  int timer = 0;
  std::function<void(int, int)> visit = [&](int node, int parent_route) {
    disc[node] = low[node] = timer++;
    int children = 0;
    for (const Edge& edge : m_adjacency[node]) {
      if (edge.route == parent_route ||
          !route_usable(m_def.routes[edge.route], rules)) {
        continue;
      }
      if (disc[edge.to] >= 0) {
        low[node] = std::min(low[node], disc[edge.to]);
        continue;
      }
      ++children;
      visit(edge.to, edge.route);
      low[node] = std::min(low[node], low[edge.to]);
      if (parent_route >= 0 && low[edge.to] >= disc[node]) {
        articulation[node] = true;
      }
    }
    if (parent_route < 0 && children > 1) {
      articulation[node] = true;
    }
  };
  for (int i = 0; i < n; ++i) {
    if (disc[i] < 0) {
      visit(i, -1);
    }
  }
  const int baseline = cut_off_by(rules);
  QStringList out;
  for (int i = 0; i < n; ++i) {
    if (!articulation[i]) {
      continue;
    }
    TraversalRules without = rules;
    without.excluded_sites.append(m_nodes[i]);
    QStringList excluded_routes = without.excluded_routes;
    for (const Edge& edge : m_adjacency[i]) {
      excluded_routes.append(m_def.routes[edge.route].id);
    }
    without.excluded_routes = excluded_routes;
    if (cut_off_by(without) - baseline - 1 >= min_cut_off) {
      out.append(m_nodes[i]);
    }
  }
  return out;
}

auto StrategicGraph::bridge_routes(const TraversalRules& rules,
                                   int min_cut_off) const -> QStringList {
  const int n = static_cast<int>(m_nodes.size());
  QVector<int> disc(n, -1);
  QVector<int> low(n, 0);
  QStringList candidates;
  int timer = 0;
  std::function<void(int, int)> visit = [&](int node, int parent_route) {
    disc[node] = low[node] = timer++;
    for (const Edge& edge : m_adjacency[node]) {
      if (edge.route == parent_route ||
          !route_usable(m_def.routes[edge.route], rules)) {
        continue;
      }
      if (disc[edge.to] >= 0) {
        low[node] = std::min(low[node], disc[edge.to]);
        continue;
      }
      visit(edge.to, edge.route);
      low[node] = std::min(low[node], low[edge.to]);
      if (low[edge.to] > disc[node]) {
        candidates.append(m_def.routes[edge.route].id);
      }
    }
  };
  for (int i = 0; i < n; ++i) {
    if (disc[i] < 0) {
      visit(i, -1);
    }
  }
  const int baseline = cut_off_by(rules);
  QStringList out;
  for (const auto& id : candidates) {
    TraversalRules without = rules;
    without.excluded_routes.append(id);
    if (cut_off_by(without) - baseline >= min_cut_off) {
      out.append(id);
    }
  }
  std::sort(out.begin(), out.end());
  return out;
}

auto StrategicGraph::neighbours(const QString& site) const -> QStringList {
  QStringList out;
  const int node = m_node_index.value(site, -1);
  if (node < 0) {
    return out;
  }
  for (const Edge& edge : m_adjacency[node]) {
    if (!out.contains(m_nodes[edge.to])) {
      out.append(m_nodes[edge.to]);
    }
  }
  return out;
}

} // namespace Game::Strategic
