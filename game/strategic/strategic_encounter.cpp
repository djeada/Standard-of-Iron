#include "strategic_encounter.h"

#include <QJsonArray>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <limits>

#include "strategic_graph.h"

namespace Game::Strategic {

namespace {

constexpr quint32 k_fnv_offset = 2166136261U;
constexpr quint32 k_fnv_prime = 16777619U;

auto fnv1a(const QString& text) -> quint32 {
  quint32 hash = k_fnv_offset;
  for (const char byte : text.toUtf8()) {
    hash ^= static_cast<quint8>(byte);
    hash *= k_fnv_prime;
  }
  return hash;
}

auto generator_nation(const QString& faction) -> QString {
  if (faction == QLatin1String("rome")) {
    return QStringLiteral("roman_republic");
  }
  return QStringLiteral("carthage");
}

auto clamp01(double value) -> double {
  return std::clamp(value, 0.0, 1.0);
}

auto rounded(double value) -> double {
  return std::round(value * 100.0) / 100.0;
}

auto pick_kind(const Site& site, int fortification, bool invading) -> QString {
  if (site.kind == SiteKind::Pass) {
    return QStringLiteral("pass");
  }
  if (site.kind == SiteKind::Crossing || site.terrain == Terrain::River) {
    return QStringLiteral("river crossing");
  }
  if (is_settlement(site.kind) && fortification >= 2 && invading) {
    return QStringLiteral("siege");
  }
  if (site.kind == SiteKind::Fort || site.kind == SiteKind::Camp) {
    return QStringLiteral("assault on a fortified camp");
  }
  if (site.terrain == Terrain::Forest) {
    return QStringLiteral("ambush");
  }
  if (site.has_port() || site.terrain == Terrain::Coast) {
    return QStringLiteral("coastal battle");
  }
  return QStringLiteral("field battle");
}

auto pick_preset(const Site& site, const QString& kind, int routes) -> QString {
  if (!site.battlefield.preset.isEmpty()) {
    return site.battlefield.preset;
  }
  if (kind == QLatin1String("pass")) {
    return QStringLiteral("mountain_pass");
  }
  if (kind == QLatin1String("river crossing")) {
    return QStringLiteral("river_crossing");
  }
  if (kind == QLatin1String("siege")) {
    return QStringLiteral("siege_town");
  }
  if (kind == QLatin1String("assault on a fortified camp")) {
    return QStringLiteral("fortified_frontier");
  }
  switch (site.terrain) {
  case Terrain::Forest:
    return QStringLiteral("forest_ambush");
  case Terrain::Marsh:
    return QStringLiteral("marsh_wetland");
  case Terrain::Hills:
  case Terrain::Mountains:
    return QStringLiteral("broken_hills");
  case Terrain::Coast:
    return QStringLiteral("coastal_battlefield");
  case Terrain::Plains:
  case Terrain::River:
  case Terrain::Desert:
    break;
  }
  if (site.has_port()) {
    return QStringLiteral("coastal_battlefield");
  }
  if (routes >= 4 && is_settlement(site.kind)) {
    return QStringLiteral("multi_settlement_valley");
  }
  return QStringLiteral("open_plains");
}

auto pick_biome(const Site& site, const Region* region, Season season) -> QString {
  const bool mountains =
      site.kind == SiteKind::Pass || site.terrain == Terrain::Mountains;
  if (mountains) {
    return season == Season::Winter || season == Season::Autumn
               ? QStringLiteral("winter")
               : QStringLiteral("alpine");
  }
  if (site.terrain == Terrain::Marsh) {
    return QStringLiteral("delta");
  }
  if (site.terrain == Terrain::Desert) {
    return QStringLiteral("canyon");
  }
  if (site.terrain == Terrain::Forest) {
    return QStringLiteral("pine_forest");
  }
  if (region != nullptr && region->tags.contains(QStringLiteral("iberian"))) {
    return QStringLiteral("iberian");
  }
  if (site.position.lat >= 43.5) {
    return season == Season::Winter ? QStringLiteral("winter")
                                    : QStringLiteral("temperate");
  }
  return QStringLiteral("mediterranean");
}

auto strongest(const QVector<const ArmyState*>& armies) -> const ArmyState* {
  const ArmyState* best = nullptr;
  for (const auto* army : armies) {
    if (best == nullptr || army->soldiers() > best->soldiers()) {
      best = army;
    }
  }
  return best;
}

auto battle_site_on_path(const WorldDefinition& def, const Path& path) -> QString {
  for (int i = 1; i + 1 < path.sites.size(); ++i) {
    const Site* site = def.site(path.sites[i]);
    if (site != nullptr &&
        (site->kind == SiteKind::Pass || site->kind == SiteKind::Crossing)) {
      return site->id;
    }
  }
  return path.sites.isEmpty() ? QString() : path.sites.constLast();
}

} // namespace

auto known_generator_presets() -> const QStringList& {
  static const QStringList k_presets = {
      QStringLiteral("broken_hills"),
      QStringLiteral("coastal_battlefield"),
      QStringLiteral("forest_ambush"),
      QStringLiteral("fortified_frontier"),
      QStringLiteral("marsh_wetland"),
      QStringLiteral("mountain_pass"),
      QStringLiteral("multi_settlement_valley"),
      QStringLiteral("open_plains"),
      QStringLiteral("river_crossing"),
      QStringLiteral("siege_town"),
  };
  return k_presets;
}

auto current_season(const WorldDefinition& def, const WorldState& state) -> Season {
  if (state.applied_events.isEmpty()) {
    return Season::Spring;
  }
  const WorldEvent* last = def.event(state.applied_events.constLast());
  return last != nullptr ? last->season : Season::Spring;
}

auto encounter_at(const WorldDefinition& def,
                  const WorldState& state,
                  const QString& site_id,
                  const QString& attacker_army) -> Encounter {
  Encounter encounter;
  encounter.site = site_id;
  const QString owner = site_controller(def, state, site_id);
  const ArmyState* attacker = state.army(attacker_army);
  QVector<const ArmyState*> present = state.armies_at(site_id);
  if (attacker == nullptr) {
    QVector<const ArmyState*> invaders;
    for (const auto* army : present) {
      if (are_hostile(def, army->faction, owner) ||
          owner == QLatin1String(k_neutral_faction)) {
        invaders.append(army);
      }
    }
    attacker = strongest(invaders);
  }
  if (attacker != nullptr) {
    encounter.attacker = attacker->faction;
    encounter.attacker_army = attacker->id;
  }
  QVector<const ArmyState*> defenders;
  for (const auto* army : present) {
    if (attacker == nullptr || are_hostile(def, army->faction, attacker->faction)) {
      defenders.append(army);
    }
  }
  if (const ArmyState* defender = strongest(defenders); defender != nullptr) {
    encounter.defender = defender->faction;
    encounter.defender_army = defender->id;
  } else {
    encounter.defender = owner;
  }
  encounter.reason = QStringLiteral("battle for %1").arg(def.display_name(site_id));
  if (encounter.attacker.isEmpty()) {
    const StrategicGraph graph(def, state);
    double best = std::numeric_limits<double>::infinity();
    for (const auto& army : state.armies) {
      const bool unclaimed = encounter.defender == QLatin1String(k_neutral_faction);
      if (army.soldiers() <= 0 ||
          (!unclaimed && !are_hostile(def, army.faction, encounter.defender))) {
        continue;
      }
      const Path path = graph.shortest_path(
          army.location, site_id, TraversalRules::march(army.faction));
      if (path.found && path.days < best) {
        best = path.days;
        encounter.attacker = army.faction;
        encounter.attacker_army = army.id;
        encounter.days_apart = path.days;
      }
    }
    if (!encounter.attacker_army.isEmpty()) {
      encounter.reason =
          QStringLiteral("%1 marches on %2 (%3 days)")
              .arg(def.display_name(encounter.attacker_army), def.display_name(site_id))
              .arg(best, 0, 'f', 1);
    }
  }
  if (encounter.attacker.isEmpty()) {
    for (const auto& faction : def.factions) {
      if (are_hostile(def, faction.id, encounter.defender)) {
        encounter.attacker = faction.id;
        break;
      }
    }
  }
  return encounter;
}

auto suggest_encounters(const WorldDefinition& def,
                        const WorldState& state,
                        double within_days) -> QVector<Encounter> {
  QVector<Encounter> out;
  const StrategicGraph graph(def, state);
  QSet<QString> pairs;
  for (int i = 0; i < state.armies.size(); ++i) {
    for (int j = 0; j < state.armies.size(); ++j) {
      const ArmyState& a = state.armies[i];
      const ArmyState& b = state.armies[j];
      if (i == j || !are_hostile(def, a.faction, b.faction) || a.soldiers() <= 0 ||
          b.soldiers() <= 0) {
        continue;
      }
      const bool a_invades =
          are_hostile(def, a.faction, site_controller(def, state, b.location)) ||
          site_controller(def, state, b.location) == QLatin1String(k_neutral_faction);
      const QString pair =
          a.id < b.id ? a.id + QLatin1Char('|') + b.id : b.id + QLatin1Char('|') + a.id;
      if (!a_invades || pairs.contains(pair)) {
        continue;
      }
      const Path path =
          graph.shortest_path(a.location, b.location, TraversalRules::march(a.faction));
      if (!path.found || path.days > within_days) {
        continue;
      }
      pairs.insert(pair);
      Encounter encounter;
      encounter.site = battle_site_on_path(def, path);
      encounter.attacker = a.faction;
      encounter.attacker_army = a.id;
      encounter.defender = b.faction;
      encounter.defender_army = b.id;
      encounter.days_apart = path.days;
      encounter.reason = QStringLiteral("%1 can reach %2 in %3 days")
                             .arg(def.display_name(a.id), def.display_name(b.id))
                             .arg(path.days, 0, 'f', 1);
      out.append(encounter);
    }
  }
  for (const auto& army : state.armies) {
    const Site* here = def.site(army.location);
    if (here == nullptr || !is_settlement(here->kind)) {
      continue;
    }
    const QString owner = site_controller(def, state, here->id);
    if (!are_hostile(def, army.faction, owner)) {
      continue;
    }
    Encounter encounter = encounter_at(def, state, here->id, army.id);
    encounter.reason = QStringLiteral("%1 stands before hostile %2")
                           .arg(def.display_name(army.id), here->name);
    out.append(encounter);
  }
  std::stable_sort(out.begin(), out.end(), [](const Encounter& a, const Encounter& b) {
    return a.days_apart < b.days_apart;
  });
  return out;
}

auto encounter_context(const WorldDefinition& def,
                       const WorldState& state,
                       const Encounter& encounter,
                       Season season) -> EncounterContext {
  EncounterContext context;
  const Site* site = def.site(encounter.site);
  if (site == nullptr) {
    return context;
  }
  const Region* region = def.region(site->region);
  context.site = site->id;
  context.site_name = site->name;
  context.region = site->region;
  context.region_name = region != nullptr ? region->name : site->region;
  context.terrain = site->terrain;
  context.season = season;
  context.fortification = site_fortification(def, state, site->id);
  const auto site_state = state.sites.find(site->id);
  context.garrison = site_state != state.sites.end() ? site_state->second.garrison : 0;
  context.controller = site_controller(def, state, site->id);
  context.attacker = encounter.attacker;
  context.defender = encounter.defender;
  if (const auto* army = state.army(encounter.attacker_army)) {
    context.attacker_soldiers = army->soldiers();
  }
  if (const auto* army = state.army(encounter.defender_army)) {
    context.defender_soldiers = army->soldiers();
  }
  for (const auto* route : def.routes_at(site->id)) {
    if (route->kind == RouteKind::SeaLane) {
      context.coastal = true;
      continue;
    }
    ++context.approach_routes;
    if (route->kind == RouteKind::RiverCrossing) {
      ++context.river_crossings;
    }
    context.approach_names.append(def.display_name(route->id));
    context.nearby_sites.append(route->other_end(site->id));
  }
  context.coastal =
      context.coastal || site->has_port() || site->terrain == Terrain::Coast;
  const bool invading = are_hostile(def, encounter.attacker, context.controller) ||
                        context.controller == QLatin1String(k_neutral_faction);
  context.kind = pick_kind(*site, context.fortification, invading);
  context.preset = pick_preset(*site, context.kind, context.approach_routes);
  context.biome = pick_biome(*site, region, season);
  context.battlefield_map = site->battlefield.map;
  context.mission = site->battlefield.mission;
  return context;
}

auto generation_request(const WorldDefinition& def,
                        const WorldState& state,
                        const EncounterContext& context) -> QJsonObject {
  QJsonObject parameters;
  parameters.insert(QStringLiteral("general.biome"), context.biome);
  const QString player = def.player_faction;
  const QString local = (context.attacker == player || context.defender == player)
                            ? player
                            : context.attacker;
  parameters.insert(QStringLiteral("general.player_nation"), generator_nation(local));
  parameters.insert(QStringLiteral("roads.routes"),
                    std::clamp(context.approach_routes, 1, 3));
  parameters.insert(QStringLiteral("settlements.forts"),
                    std::clamp(context.fortification, 0, 4));
  parameters.insert(QStringLiteral("settlements.walled_start"),
                    context.fortification >= 2);
  if (context.coastal) {
    parameters.insert(QStringLiteral("water.coast"), true);
  }
  if (context.kind == QLatin1String("river crossing")) {
    parameters.insert(QStringLiteral("water.rivers"), 1);
    parameters.insert(QStringLiteral("water.crossings"),
                      std::clamp(std::max(context.river_crossings, 1) + 1, 1, 4));
  }
  if (context.kind == QLatin1String("pass")) {
    parameters.insert(QStringLiteral("terrain.mountains"), 0.9);
    parameters.insert(QStringLiteral("tactical.choke_intensity"), 0.8);
  }
  if (context.terrain == Terrain::Forest) {
    parameters.insert(QStringLiteral("forests.density"), 0.85);
  } else if (context.terrain == Terrain::Plains) {
    parameters.insert(QStringLiteral("tactical.formation_space"), 0.8);
    parameters.insert(QStringLiteral("forests.density"), 0.3);
  }
  const int soldiers = std::max(context.attacker_soldiers, context.defender_soldiers);
  if (soldiers > 0) {
    parameters.insert(
        QStringLiteral("spawns.army"),
        std::clamp(static_cast<int>(std::lround(soldiers / 5000.0)), 3, 20));
  }
  const double garrison =
      context.garrison > 0 ? context.garrison / 10000.0 : context.fortification * 0.25;
  parameters.insert(QStringLiteral("spawns.garrison"), rounded(clamp01(garrison)));
  if (context.preset == QLatin1String("siege_town") ||
      context.preset == QLatin1String("multi_settlement_valley")) {
    parameters.insert(QStringLiteral("settlements.towns"),
                      context.fortification >= 3 ? 2 : 1);
  }

  const QString seed_key = QStringLiteral("%1|%2|%3|%4")
                               .arg(def.id, context.site, context.attacker)
                               .arg(state.applied_events.size());
  QJsonObject request;
  request.insert(QStringLiteral("version"), k_generator_request_version);
  request.insert(QStringLiteral("seed"),
                 static_cast<qint64>(fnv1a(seed_key) & 0x7fffffffU));
  request.insert(QStringLiteral("preset"), context.preset);
  request.insert(QStringLiteral("width"), 650);
  request.insert(QStringLiteral("height"), 650);
  request.insert(QStringLiteral("parameters"), parameters);
  request.insert(QStringLiteral("locks"), QJsonArray());
  return request;
}

auto context_to_json(const EncounterContext& context) -> QJsonObject {
  QJsonObject obj;
  obj.insert(QStringLiteral("site"), context.site);
  obj.insert(QStringLiteral("site_name"), context.site_name);
  obj.insert(QStringLiteral("region"), context.region);
  obj.insert(QStringLiteral("kind"), context.kind);
  obj.insert(QStringLiteral("preset"), context.preset);
  obj.insert(QStringLiteral("biome"), context.biome);
  obj.insert(QStringLiteral("terrain"), to_string(context.terrain));
  obj.insert(QStringLiteral("season"), to_string(context.season));
  obj.insert(QStringLiteral("fortification"), context.fortification);
  obj.insert(QStringLiteral("controller"), context.controller);
  obj.insert(QStringLiteral("attacker"), context.attacker);
  obj.insert(QStringLiteral("defender"), context.defender);
  obj.insert(QStringLiteral("attacker_soldiers"), context.attacker_soldiers);
  obj.insert(QStringLiteral("defender_soldiers"), context.defender_soldiers);
  obj.insert(QStringLiteral("approach_routes"),
             QJsonArray::fromStringList(context.approach_names));
  obj.insert(QStringLiteral("coastal"), context.coastal);
  if (!context.battlefield_map.isEmpty()) {
    obj.insert(QStringLiteral("battlefield_map"), context.battlefield_map);
  }
  if (!context.mission.isEmpty()) {
    obj.insert(QStringLiteral("mission"), context.mission);
  }
  return obj;
}

} // namespace Game::Strategic
