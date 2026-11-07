#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QStringList>

#include <cstdio>
#include <iostream>

#include "game/strategic/strategic_assets.h"
#include "game/strategic/strategic_encounter.h"
#include "game/strategic/strategic_events.h"
#include "game/strategic/strategic_graph.h"
#include "game/strategic/strategic_json.h"
#include "game/strategic/strategic_supply.h"
#include "game/strategic/strategic_validation.h"

namespace {

using namespace Game::Strategic;

void out(const QString& text) {
  std::cout << text.toStdString() << '\n';
}

void usage() {
  out(QStringLiteral(
      "usage: strategic_world [--world FILE] [--root DIR] [--at EVENT] COMMAND [ARGS]\n"
      "\n"
      "commands:\n"
      "  validate [--strict]            run every world check (errors fail; --strict "
      "fails on warnings)\n"
      "  summary                        counts, factions and who holds what\n"
      "  timeline                       the campaign chronology and what each event "
      "changes\n"
      "  path FROM TO [--faction F] [--safe] [--land]\n"
      "                                 cheapest march between two sites\n"
      "  reach SITE [--days N] [--faction F] [--safe]\n"
      "                                 every site within N days\n"
      "  routes FROM TO [--faction F]   number of independent routes between two "
      "sites\n"
      "  supply                         supply line of every army\n"
      "  isolated                       settlements cut off from their capital\n"
      "  threats [--days N]             routes a hostile army can reach\n"
      "  chokepoints                    sites and routes whose loss splits the "
      "network\n"
      "  encounters                     battles the current situation makes likely\n"
      "  encounter SITE [--attacker ARMY]\n"
      "                                 battlefield generation request for a site\n"
      "  save-state FILE                write the state at --at as a campaign save\n"
      "  format                         rewrite the world file in canonical layout\n"));
}

auto option(QStringList& args,
            const QString& name,
            const QString& fallback = {}) -> QString {
  const qsizetype index = args.indexOf(name);
  if (index < 0 || index + 1 >= args.size()) {
    return fallback;
  }
  const QString value = args.at(index + 1);
  args.remove(index, 2);
  return value;
}

auto flag(QStringList& args, const QString& name) -> bool {
  return args.removeAll(name) > 0;
}

auto rules_from(QStringList& args) -> TraversalRules {
  TraversalRules rules;
  rules.faction = option(args, QStringLiteral("--faction"));
  rules.avoid_hostile = flag(args, QStringLiteral("--safe"));
  rules.allow_sea = !flag(args, QStringLiteral("--land"));
  return rules;
}

auto name(const WorldDefinition& def, const QString& id) -> QString {
  return def.display_name(id);
}

auto print_path(const WorldDefinition& def, const Path& path) -> int {
  if (!path.found) {
    out(QStringLiteral("no route"));
    return 1;
  }
  out(QStringLiteral("%1 days, %2 km, %3 legs (%4 by sea), narrowest throughput %5")
          .arg(path.days, 0, 'f', 1)
          .arg(path.km, 0, 'f', 0)
          .arg(path.routes.size())
          .arg(path.sea_legs)
          .arg(path.min_throughput));
  for (int i = 0; i < path.routes.size(); ++i) {
    out(QStringLiteral("  %1 → %2  via %3")
            .arg(name(def, path.sites[i]),
                 name(def, path.sites[i + 1]),
                 name(def, path.routes[i])));
  }
  return 0;
}

auto cmd_validate(const WorldBundle& bundle, bool strict) -> int {
  const ValidationReport report = validate_world(bundle.definition, bundle.context);
  for (const auto& issue : report.issues) {
    out(QStringLiteral("[%1] %2 %3: %4")
            .arg(to_string(issue.severity).toUpper(),
                 issue.code,
                 issue.entity,
                 issue.message));
  }
  out(QStringLiteral(
          "%1: %2 error(s), %3 warning(s), %4 site(s), %5 route(s), %6 event(s)")
          .arg(bundle.path)
          .arg(report.errors())
          .arg(report.warnings())
          .arg(bundle.definition.sites.size())
          .arg(bundle.definition.routes.size())
          .arg(bundle.definition.events.size()));
  return (report.errors() > 0 || (strict && report.warnings() > 0)) ? 1 : 0;
}

auto cmd_summary(const WorldDefinition& def, const WorldState& state) -> int {
  out(QStringLiteral("%1 (%2)").arg(def.name, def.id));
  out(QStringLiteral("%1 regions, %2 sites, %3 routes, %4 commanders, %5 events")
          .arg(def.regions.size())
          .arg(def.sites.size())
          .arg(def.routes.size())
          .arg(def.commanders.size())
          .arg(def.events.size()));
  for (const auto& region : def.regions) {
    QStringList held;
    for (const auto* site : def.sites_in(region.id)) {
      held.append(
          QStringLiteral("%1:%2").arg(site->id, site_controller(def, state, site->id)));
    }
    out(QStringLiteral("  %1 [%2%3] %4")
            .arg(region.id,
                 region_controller(def, state, region.id),
                 region_is_contested(def, state, region.id)
                     ? QStringLiteral(", contested")
                     : QString(),
                 held.join(QLatin1Char(' '))));
  }
  for (const auto& army : state.armies) {
    out(QStringLiteral("  army %1 (%2) at %3: %4 foot, %5 horse, %6 elephants [%7]")
            .arg(army.id, army.faction, army.location)
            .arg(army.infantry)
            .arg(army.cavalry)
            .arg(army.elephants)
            .arg(to_string(army.band())));
  }
  return 0;
}

auto cmd_timeline(const WorldDefinition& def) -> int {
  WorldState state = def.initial_state;
  for (const auto& event : def.events) {
    const WorldState before = state;
    QVector<EffectError> errors;
    (void)apply_event(def, state, event, &errors);
    out(QStringLiteral("%1  %2%3")
            .arg(event.date_label(),
                 event.name,
                 event.mission.isEmpty()
                     ? QString()
                     : QStringLiteral("  [mission %1]").arg(event.mission)));
    for (const auto& change : diff_states(def, before, state)) {
      out(QStringLiteral("    %1 %2: %3 → %4")
              .arg(name(def, change.entity),
                   change.field,
                   change.before.isEmpty() ? QStringLiteral("–") : change.before,
                   change.after.isEmpty() ? QStringLiteral("–") : change.after));
    }
    for (const auto& error : errors) {
      out(QStringLiteral("    ! effect %1: %2")
              .arg(error.effect_index)
              .arg(error.message));
    }
  }
  return 0;
}

auto cmd_supply(const WorldDefinition& def, const WorldState& state) -> int {
  for (const auto& report : evaluate_all_supply(def, state)) {
    out(QStringLiteral("%1: %2 (%3)")
            .arg(name(def, report.army), to_string(report.level), report.reason));
    if (report.path.found && report.path.sites.size() > 1) {
      QStringList names;
      for (const auto& site : report.path.sites) {
        names.append(name(def, site));
      }
      out(QStringLiteral("    line: %1").arg(names.join(QStringLiteral(" → "))));
    }
  }
  return 0;
}

auto cmd_encounter(const WorldDefinition& def,
                   const WorldState& state,
                   const QString& site,
                   const QString& attacker) -> int {
  if (def.site(site) == nullptr) {
    out(QStringLiteral("unknown site '%1'").arg(site));
    return 2;
  }
  const Encounter encounter = encounter_at(def, state, site, attacker);
  const EncounterContext context =
      encounter_context(def, state, encounter, current_season(def, state));
  QJsonObject root;
  root.insert(QStringLiteral("context"), context_to_json(context));
  root.insert(QStringLiteral("request"), generation_request(def, state, context));
  std::cout << pretty_json(root).toStdString() << '\n';
  return 0;
}

} // namespace

auto main(int argc, char* argv[]) -> int {
  QCoreApplication app(argc, argv);
  QStringList args = QCoreApplication::arguments();
  args.removeFirst();
  if (args.isEmpty() || flag(args, QStringLiteral("--help")) ||
      flag(args, QStringLiteral("-h"))) {
    usage();
    return args.isEmpty() ? 2 : 0;
  }
  const QString root_arg = option(args, QStringLiteral("--root"));
  const QString root =
      root_arg.isEmpty() ? find_repository_root(QDir::currentPath()) : root_arg;
  const QString world_path =
      option(args,
             QStringLiteral("--world"),
             QDir(root).filePath(QString::fromLatin1(k_default_world_path)));
  const QString at = option(args, QStringLiteral("--at"));

  QString error;
  auto bundle = load_world_bundle(world_path, root, &error);
  if (!bundle.has_value()) {
    out(QStringLiteral("cannot load %1: %2").arg(world_path, error));
    return 2;
  }
  const WorldDefinition& def = bundle->definition;
  WorldState state = def.initial_state;
  if (!at.isEmpty()) {
    if (def.event(at) == nullptr) {
      out(QStringLiteral("unknown event '%1'").arg(at));
      return 2;
    }
    state = state_after_event(def, at);
  }
  if (args.isEmpty()) {
    usage();
    return 2;
  }
  const QString command = args.takeFirst();
  if (command == QLatin1String("validate")) {
    return cmd_validate(*bundle, flag(args, QStringLiteral("--strict")));
  }
  if (command == QLatin1String("summary")) {
    return cmd_summary(def, state);
  }
  if (command == QLatin1String("timeline")) {
    return cmd_timeline(def);
  }
  if (command == QLatin1String("path") || command == QLatin1String("routes")) {
    TraversalRules rules = rules_from(args);
    if (args.size() < 2) {
      usage();
      return 2;
    }
    const StrategicGraph graph(def, state);
    if (command == QLatin1String("routes")) {
      out(QString::number(graph.route_count(args[0], args[1], rules)));
      return 0;
    }
    return print_path(def, graph.shortest_path(args[0], args[1], rules));
  }
  if (command == QLatin1String("reach")) {
    const double days =
        option(args, QStringLiteral("--days"), QStringLiteral("-1")).toDouble();
    TraversalRules rules = rules_from(args);
    if (args.isEmpty()) {
      usage();
      return 2;
    }
    const StrategicGraph graph(def, state);
    for (const auto& [site, d] : graph.reachable(args[0], rules, days)) {
      out(QStringLiteral("%1  %2 days").arg(name(def, site), -24).arg(d, 0, 'f', 1));
    }
    return 0;
  }
  if (command == QLatin1String("supply")) {
    return cmd_supply(def, state);
  }
  if (command == QLatin1String("isolated")) {
    for (const auto& entry : isolated_sites(def, state)) {
      out(QStringLiteral("%1 (%2) cannot reach %3")
              .arg(name(def, entry.site), entry.faction, name(def, entry.capital)));
    }
    return 0;
  }
  if (command == QLatin1String("threats")) {
    const double days =
        option(args, QStringLiteral("--days"), QStringLiteral("5")).toDouble();
    for (const auto& threat : threatened_routes(def, state, days)) {
      out(QStringLiteral("%1 (%2) — %3 is %4 days away")
              .arg(name(def, threat.route), threat.owner, name(def, threat.army))
              .arg(threat.days, 0, 'f', 1));
    }
    return 0;
  }
  if (command == QLatin1String("chokepoints")) {
    const StrategicGraph graph(def, state);
    out(QStringLiteral("sites: %1")
            .arg(graph.chokepoint_sites(TraversalRules::anything())
                     .join(QStringLiteral(", "))));
    out(QStringLiteral("routes: %1")
            .arg(graph.bridge_routes(TraversalRules::anything())
                     .join(QStringLiteral(", "))));
    return 0;
  }
  if (command == QLatin1String("encounters")) {
    for (const auto& encounter : suggest_encounters(def, state)) {
      out(QStringLiteral("%1: %2 (%3) vs %4 (%5) — %6")
              .arg(name(def, encounter.site),
                   name(def, encounter.attacker_army),
                   encounter.attacker,
                   name(def, encounter.defender_army),
                   encounter.defender,
                   encounter.reason));
    }
    return 0;
  }
  if (command == QLatin1String("encounter")) {
    const QString attacker = option(args, QStringLiteral("--attacker"));
    if (args.isEmpty()) {
      usage();
      return 2;
    }
    return cmd_encounter(def, state, args[0], attacker);
  }
  if (command == QLatin1String("save-state")) {
    if (args.isEmpty()) {
      usage();
      return 2;
    }
    QFile file(args[0]);
    if (!file.open(QIODevice::WriteOnly)) {
      out(QStringLiteral("cannot write %1").arg(args[0]));
      return 1;
    }
    file.write(save_state(def, state));
    return 0;
  }
  if (command == QLatin1String("format")) {
    QFile file(world_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      out(QStringLiteral("cannot write %1").arg(world_path));
      return 1;
    }
    file.write(definition_to_bytes(def));
    out(QStringLiteral("formatted %1").arg(world_path));
    return 0;
  }
  usage();
  return 2;
}
