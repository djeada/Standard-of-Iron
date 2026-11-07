#include <QJsonArray>
#include <QJsonDocument>

#include <gtest/gtest.h>

#include "game/strategic/strategic_encounter.h"
#include "game/strategic/strategic_events.h"
#include "game/strategic/strategic_graph.h"
#include "game/strategic/strategic_json.h"
#include "game/strategic/strategic_supply.h"
#include "strategic_test_world.h"

namespace {

namespace S = Game::Strategic;
using StrategicTest::micro_world;
using StrategicTest::micro_world_json;

TEST(StrategicWorldModel, LoadsEveryEntityKindFromJson) {
  QString error;
  const auto def = S::definition_from_json(micro_world_json(), &error);
  ASSERT_TRUE(def.has_value()) << error.toStdString();
  EXPECT_TRUE(def->load_issues.isEmpty());
  EXPECT_EQ(def->factions.size(), 2);
  EXPECT_EQ(def->regions.size(), 3);
  EXPECT_EQ(def->sites.size(), 6);
  EXPECT_EQ(def->routes.size(), 5);
  EXPECT_EQ(def->commanders.size(), 3);
  EXPECT_EQ(def->events.size(), 2);
  EXPECT_EQ(def->initial_state.armies.size(), 2);
  EXPECT_EQ(def->kind_of(QStringLiteral("saddle")), S::EntityKind::Site);
  EXPECT_EQ(def->kind_of(QStringLiteral("strait")), S::EntityKind::Route);
  EXPECT_EQ(def->kind_of(QStringLiteral("host")), S::EntityKind::Army);
  EXPECT_EQ(def->kind_of(QStringLiteral("relief")), S::EntityKind::Army);
  EXPECT_EQ(def->site(QStringLiteral("harbour"))->port, S::PortClass::Major);
  EXPECT_EQ(def->site(QStringLiteral("citadel"))->resources.get(S::Resource::Iron), 2);
  EXPECT_EQ(def->resolve_campaign_region(QStringLiteral("coastland"))->id,
            QStringLiteral("west"));
  EXPECT_EQ(def->site_for_mission(QStringLiteral("siege_of_citadel"))->id,
            QStringLiteral("citadel"));
}

TEST(StrategicWorldModel, RejectsUnsupportedSchemaVersions) {
  QByteArray json = micro_world_json();
  json.replace("\"schema_version\": 1", "\"schema_version\": 99");
  QString error;
  EXPECT_FALSE(S::definition_from_json(json, &error).has_value());
  EXPECT_TRUE(error.contains(QStringLiteral("schema_version")));
}

TEST(StrategicWorldModel, UnknownEnumerationsBecomeLoadIssuesNotSilentDefaults) {
  QByteArray json = micro_world_json();
  json.replace("\"kind\": \"major_road\"", "\"kind\": \"raod\"");
  const auto def = S::definition_from_json(json);
  ASSERT_TRUE(def.has_value());
  ASSERT_EQ(def->load_issues.size(), 1);
  EXPECT_EQ(def->load_issues.first().entity, QStringLiteral("coast_road"));
}

TEST(StrategicWorldModel, DefinitionRoundTripsThroughCanonicalJson) {
  const S::WorldDefinition def = micro_world();
  const QByteArray once = S::definition_to_bytes(def);
  const auto reread = S::definition_from_json(once);
  ASSERT_TRUE(reread.has_value());
  EXPECT_EQ(S::definition_to_bytes(*reread), once);
  EXPECT_EQ(reread->initial_state, def.initial_state);
  EXPECT_EQ(S::definition_fingerprint(*reread), S::definition_fingerprint(def));
}

TEST(StrategicWorldModel, ControlFallsBackFromSiteToRegion) {
  const S::WorldDefinition def = micro_world();
  const S::WorldState& state = def.initial_state;
  EXPECT_EQ(S::site_controller(def, state, QStringLiteral("harbour")),
            QStringLiteral("carthage"));
  EXPECT_EQ(S::site_controller(def, state, QStringLiteral("market")),
            QStringLiteral("neutral"));
  EXPECT_EQ(S::site_controller(def, state, QStringLiteral("citadel")),
            QStringLiteral("rome"));
  EXPECT_TRUE(S::are_hostile(def, QStringLiteral("rome"), QStringLiteral("carthage")));
  EXPECT_FALSE(S::are_hostile(def, QStringLiteral("rome"), QStringLiteral("neutral")));
}

TEST(StrategicGraphQueries, ShortestPathFollowsTheRoadsAndReportsCost) {
  const S::WorldDefinition def = micro_world();
  const S::StrategicGraph graph(def, def.initial_state);
  const S::Path path = graph.shortest_path(QStringLiteral("harbour"),
                                           QStringLiteral("citadel"),
                                           S::TraversalRules::anything());
  ASSERT_TRUE(path.found);
  EXPECT_EQ(path.sites,
            (QStringList{QStringLiteral("harbour"),
                         QStringLiteral("market"),
                         QStringLiteral("saddle"),
                         QStringLiteral("citadel")}));
  EXPECT_EQ(path.routes.size(), 3);
  EXPECT_EQ(path.min_throughput, 1);
  EXPECT_GT(path.days, 0.0);
  EXPECT_GT(path.km,
            S::great_circle_km(def.site(QStringLiteral("harbour"))->position,
                               def.site(QStringLiteral("citadel"))->position));
}

TEST(StrategicGraphQueries, BlockedRoutesAndHostileGroundChangeTheAnswer) {
  S::WorldDefinition def = micro_world();
  S::WorldState state = def.initial_state;
  state.routes[QStringLiteral("ascent")].status = S::RouteStatus::Blocked;
  const S::StrategicGraph blocked(def, state);
  EXPECT_FALSE(blocked
                   .shortest_path(QStringLiteral("harbour"),
                                  QStringLiteral("citadel"),
                                  S::TraversalRules::march(QStringLiteral("carthage")))
                   .found);

  const S::StrategicGraph open(def, def.initial_state);
  EXPECT_TRUE(open.shortest_path(QStringLiteral("harbour"),
                                 QStringLiteral("ford"),
                                 S::TraversalRules::march(QStringLiteral("carthage")))
                  .found);
  EXPECT_FALSE(
      open.shortest_path(QStringLiteral("harbour"),
                         QStringLiteral("ford"),
                         S::TraversalRules::safe_march(QStringLiteral("carthage")))
          .found)
      << "a safe march may not pass through the hostile citadel";
  EXPECT_TRUE(
      open.shortest_path(QStringLiteral("harbour"),
                         QStringLiteral("saddle"),
                         S::TraversalRules::safe_march(QStringLiteral("carthage")))
          .found)
      << "a hostile site is still a legal destination";
  EXPECT_FALSE(
      open.shortest_path(QStringLiteral("harbour"),
                         QStringLiteral("citadel"),
                         S::TraversalRules::safe_march(QStringLiteral("carthage")))
          .found)
      << "but the march may not cross the hostile saddle on the way";
}

TEST(StrategicGraphQueries, ArmiesCannotEmbarkFromHostileHarbours) {
  const S::WorldDefinition def = micro_world();
  const S::StrategicGraph graph(def, def.initial_state);
  EXPECT_TRUE(graph
                  .shortest_path(QStringLiteral("harbour"),
                                 QStringLiteral("anchorage"),
                                 S::TraversalRules::march(QStringLiteral("carthage")))
                  .found)
      << "sailing from a friendly port to a hostile one is an assault landing";
  EXPECT_FALSE(graph
                   .shortest_path(QStringLiteral("anchorage"),
                                  QStringLiteral("harbour"),
                                  S::TraversalRules::march(QStringLiteral("carthage")))
                   .found)
      << "a hostile port is no place to embark";
  S::WorldState closed = def.initial_state;
  closed.sites[QStringLiteral("anchorage")].port_open = false;
  EXPECT_FALSE(S::StrategicGraph(def, closed)
                   .shortest_path(QStringLiteral("harbour"),
                                  QStringLiteral("anchorage"),
                                  S::TraversalRules::march(QStringLiteral("carthage")))
                   .found);
}

TEST(StrategicGraphQueries, ReachabilityRespectsTheDayBudget) {
  const S::WorldDefinition def = micro_world();
  const S::StrategicGraph graph(def, def.initial_state);
  const auto all =
      graph.reachable(QStringLiteral("harbour"), S::TraversalRules::anything());
  EXPECT_EQ(all.size(), 6U);
  const double to_market = all.at(QStringLiteral("market"));
  const auto near = graph.reachable(
      QStringLiteral("harbour"), S::TraversalRules::anything(), to_market + 0.01);
  EXPECT_EQ(near.count(QStringLiteral("market")), 1U);
  EXPECT_EQ(near.count(QStringLiteral("citadel")), 0U);
}

TEST(StrategicGraphQueries, AlternateRouteCountIsEdgeDisjoint) {
  S::WorldDefinition def = micro_world();
  const S::StrategicGraph single(def, def.initial_state);
  EXPECT_EQ(single.route_count(QStringLiteral("harbour"),
                               QStringLiteral("citadel"),
                               S::TraversalRules::anything()),
            1);
  S::Route bypass;
  bypass.id = QStringLiteral("bypass");
  bypass.from = QStringLiteral("market");
  bypass.to = QStringLiteral("citadel");
  bypass.kind = S::RouteKind::Track;
  def.routes.append(bypass);
  const S::StrategicGraph doubled(def, def.initial_state);
  EXPECT_EQ(doubled.route_count(QStringLiteral("harbour"),
                                QStringLiteral("citadel"),
                                S::TraversalRules::anything()),
            1)
      << "the coast road is still the only way out of the harbour";
  EXPECT_EQ(doubled.route_count(QStringLiteral("market"),
                                QStringLiteral("citadel"),
                                S::TraversalRules::anything()),
            2);
}

TEST(StrategicGraphQueries, ChokepointsAndBridgesIgnoreTrivialLeaves) {
  const S::WorldDefinition def = micro_world();
  const S::StrategicGraph graph(def, def.initial_state);
  const QStringList sites = graph.chokepoint_sites(S::TraversalRules::anything());
  EXPECT_TRUE(sites.contains(QStringLiteral("market")));
  EXPECT_TRUE(sites.contains(QStringLiteral("saddle")));
  EXPECT_FALSE(sites.contains(QStringLiteral("citadel")))
      << "cutting the citadel strands only the ford, a single leaf";
  const QStringList bridges = graph.bridge_routes(S::TraversalRules::anything(), 2);
  EXPECT_TRUE(bridges.contains(QStringLiteral("ascent")));
  EXPECT_FALSE(bridges.contains(QStringLiteral("ford_road")));
}

TEST(StrategicGraphQueries, ComponentsSeparateUnlinkedSites) {
  S::WorldDefinition def = micro_world();
  def.routes.removeIf(
      [](const S::Route& route) { return route.id == QLatin1String("strait"); });
  const auto parts = S::StrategicGraph(def, def.initial_state)
                         .components(S::TraversalRules::anything());
  ASSERT_EQ(parts.size(), 2);
  EXPECT_EQ(parts[1], QStringList{QStringLiteral("anchorage")});
}

TEST(StrategicSupply, ArmiesAtAFriendlyDepotAreSupplied) {
  const S::WorldDefinition def = micro_world();
  const S::SupplyReport report =
      S::evaluate_supply(def, def.initial_state, QStringLiteral("host"));
  EXPECT_EQ(report.level, S::SupplyLevel::Supplied);
  EXPECT_EQ(report.source, QStringLiteral("harbour"));
}

TEST(StrategicSupply, DistanceStretchesAndThenCutsTheLine) {
  const S::WorldDefinition def = micro_world();
  const S::WorldState after_march = S::state_after(def, 1);
  const S::SupplyReport at_pass =
      S::evaluate_supply(def, after_march, QStringLiteral("host"));
  EXPECT_NE(at_pass.level, S::SupplyLevel::Supplied);
  EXPECT_TRUE(at_pass.path.found || at_pass.level == S::SupplyLevel::Foraging ||
              at_pass.level == S::SupplyLevel::CutOff);

  S::WorldState cut = after_march;
  cut.routes[QStringLiteral("ascent")].status = S::RouteStatus::Blocked;
  const S::SupplyReport stranded = S::evaluate_supply(def, cut, QStringLiteral("host"));
  EXPECT_TRUE(stranded.level == S::SupplyLevel::CutOff ||
              stranded.level == S::SupplyLevel::Foraging);
  EXPECT_TRUE(stranded.source.isEmpty());
}

TEST(StrategicSupply, ThroughputLimitsLargeArmies) {
  S::WorldDefinition def = micro_world();
  S::WorldState state = def.initial_state;
  state.armies[0].location = QStringLiteral("market");
  state.sites[QStringLiteral("market")].controller = QStringLiteral("rome");
  state.armies[0].infantry = 80000;
  def.routes[0].throughput = 1;
  const S::SupplyReport report = S::evaluate_supply(def, state, QStringLiteral("host"));
  EXPECT_EQ(report.source, QStringLiteral("harbour"));
  EXPECT_LT(report.capacity, report.demand);
  EXPECT_NE(report.level, S::SupplyLevel::Supplied);
}

TEST(StrategicSupply, IsolationAndThreatsAreReported) {
  S::WorldDefinition def = micro_world();
  def.site_mut(QStringLiteral("ford"))->kind = S::SiteKind::Town;
  S::WorldState state = def.initial_state;
  state.sites[QStringLiteral("ford")].controller = QStringLiteral("carthage");
  QStringList isolated;
  for (const auto& entry : S::isolated_sites(def, state)) {
    isolated.append(entry.site);
  }
  EXPECT_TRUE(isolated.contains(QStringLiteral("ford")))
      << "Carthage's ford lies beyond the Roman citadel";
  EXPECT_TRUE(isolated.contains(QStringLiteral("anchorage")))
      << "Rome's island is reached only through the Carthaginian harbour";
  EXPECT_FALSE(isolated.contains(QStringLiteral("market")))
      << "neutral sites belong to no one";
  const auto threats = S::threatened_routes(def, def.initial_state, 60.0);
  EXPECT_FALSE(threats.isEmpty());
}

TEST(StrategicEvents, ApplyingTheChronologyIsDeterministic) {
  const S::WorldDefinition def = micro_world();
  QVector<S::EffectError> errors;
  const S::WorldState a = S::state_after(def, 2, &errors);
  const S::WorldState b = S::state_after(def, 2);
  EXPECT_TRUE(errors.isEmpty());
  EXPECT_EQ(a, b);
  EXPECT_EQ(a.applied_events,
            (QStringList{QStringLiteral("march"), QStringLiteral("battle")}));
  EXPECT_EQ(S::replay(def, a.applied_events), a);
}

TEST(StrategicEvents, EffectsTransformTheState) {
  const S::WorldDefinition def = micro_world();
  const S::WorldState state = S::state_after(def, 2);
  EXPECT_EQ(state.army(QStringLiteral("host"))->location, QStringLiteral("citadel"));
  EXPECT_EQ(state.army(QStringLiteral("host"))->infantry, 24000);
  EXPECT_EQ(state.army(QStringLiteral("legion")), nullptr);
  ASSERT_NE(state.army(QStringLiteral("relief")), nullptr);
  EXPECT_EQ(state.army(QStringLiteral("relief"))->commander, QStringLiteral("legate"));
  EXPECT_EQ(S::commander_status(state, QStringLiteral("consul")),
            S::CommanderStatus::Dead);
  EXPECT_EQ(S::site_controller(def, state, QStringLiteral("citadel")),
            QStringLiteral("carthage"));
  EXPECT_TRUE(S::region_is_contested(def, state, QStringLiteral("east")));
  EXPECT_EQ(S::route_status(state, QStringLiteral("ford_road")),
            S::RouteStatus::Blocked);
  EXPECT_EQ(state.flags.at(QStringLiteral("citadel_fallen")), QStringLiteral("true"));
}

TEST(StrategicEvents, AnEventCannotBeAppliedTwice) {
  const S::WorldDefinition def = micro_world();
  S::WorldState state = def.initial_state;
  ASSERT_TRUE(S::apply_event(def, state, def.events[0]));
  QVector<S::EffectError> errors;
  EXPECT_FALSE(S::apply_event(def, state, def.events[0], &errors));
  ASSERT_EQ(errors.size(), 1);
}

TEST(StrategicEvents, FailingEffectsAreReportedWithTheirIndex) {
  const S::WorldDefinition def = micro_world();
  S::WorldEvent bad;
  bad.id = QStringLiteral("bad");
  S::Effect move;
  move.op = S::EffectOp::MoveArmy;
  move.target = QStringLiteral("ghost_army");
  move.to = QStringLiteral("market");
  bad.effects = {move};
  S::WorldState state = def.initial_state;
  QVector<S::EffectError> errors;
  EXPECT_FALSE(S::apply_event(def, state, bad, &errors));
  ASSERT_EQ(errors.size(), 1);
  EXPECT_EQ(errors.first().effect_index, 0);
}

TEST(StrategicEvents, DiffNamesEveryChange) {
  const S::WorldDefinition def = micro_world();
  const auto changes =
      S::diff_states(def, S::state_after(def, 1), S::state_after(def, 2));
  QStringList fields;
  for (const auto& change : changes) {
    fields.append(change.entity + QLatin1Char('.') + change.field);
  }
  EXPECT_TRUE(fields.contains(QStringLiteral("citadel.controller")));
  EXPECT_TRUE(fields.contains(QStringLiteral("host.location")));
  EXPECT_TRUE(fields.contains(QStringLiteral("legion.army")));
  EXPECT_TRUE(fields.contains(QStringLiteral("relief.army")));
  EXPECT_TRUE(fields.contains(QStringLiteral("consul.status")));
  EXPECT_TRUE(fields.contains(QStringLiteral("ford_road.status")));
  EXPECT_TRUE(fields.contains(QStringLiteral("east.contested")));
  EXPECT_TRUE(fields.contains(QStringLiteral("citadel_fallen.flag")));
}

TEST(StrategicSaves, StateRoundTripsThroughAVersionedSave) {
  const S::WorldDefinition def = micro_world();
  const S::WorldState state = S::state_after(def, 2);
  const QByteArray bytes = S::save_state(def, state);
  const QJsonObject root = QJsonDocument::fromJson(bytes).object();
  EXPECT_EQ(root.value(QStringLiteral("format")).toString(),
            QStringLiteral("soi.strategic_state"));
  EXPECT_EQ(root.value(QStringLiteral("version")).toInt(), S::k_state_format_version);
  QString error;
  const auto loaded = S::load_state(def, bytes, &error);
  ASSERT_TRUE(loaded.has_value()) << error.toStdString();
  EXPECT_EQ(*loaded, state);
  EXPECT_EQ(S::save_state(def, *loaded), bytes);
}

TEST(StrategicSaves, SavesFromAnotherWorldOrVersionAreRefused) {
  const S::WorldDefinition def = micro_world();
  QByteArray bytes = S::save_state(def, def.initial_state);
  QString error;
  QByteArray foreign = bytes;
  foreign.replace("\"world\": \"micro\"", "\"world\": \"other\"");
  EXPECT_FALSE(S::load_state(def, foreign, &error).has_value());
  QByteArray future = bytes;
  future.replace("\"version\": 1", "\"version\": 7");
  EXPECT_FALSE(S::load_state(def, future, &error).has_value());
  EXPECT_FALSE(S::load_state(def, QByteArrayLiteral("[]"), &error).has_value());
}

TEST(StrategicEncounters, APassBecomesAMountainPassRequest) {
  const S::WorldDefinition def = micro_world();
  const S::WorldState state = S::state_after(def, 1);
  const S::Encounter encounter = S::encounter_at(def, state, QStringLiteral("saddle"));
  const S::EncounterContext context =
      S::encounter_context(def, state, encounter, S::Season::Autumn);
  EXPECT_EQ(context.kind, QStringLiteral("pass"));
  EXPECT_EQ(context.preset, QStringLiteral("mountain_pass"));
  EXPECT_EQ(context.biome, QStringLiteral("winter"));
  const QJsonObject request = S::generation_request(def, state, context);
  EXPECT_EQ(request.value(QStringLiteral("version")).toInt(), 1);
  EXPECT_EQ(request.value(QStringLiteral("preset")).toString(),
            QStringLiteral("mountain_pass"));
  const QJsonObject parameters = request.value(QStringLiteral("parameters")).toObject();
  EXPECT_EQ(parameters.value(QStringLiteral("general.player_nation")).toString(),
            QStringLiteral("carthage"));
  EXPECT_DOUBLE_EQ(parameters.value(QStringLiteral("terrain.mountains")).toDouble(),
                   0.9);
  EXPECT_EQ(S::generation_request(def, state, context), request)
      << "requests are deterministic";
}

TEST(StrategicEncounters, AFortifiedCityUnderAttackIsASiege) {
  const S::WorldDefinition def = micro_world();
  S::WorldState state = def.initial_state;
  state.armies[0].location = QStringLiteral("citadel");
  const S::Encounter encounter = S::encounter_at(def, state, QStringLiteral("citadel"));
  EXPECT_EQ(encounter.attacker_army, QStringLiteral("host"));
  EXPECT_EQ(encounter.defender_army, QStringLiteral("legion"));
  const S::EncounterContext context =
      S::encounter_context(def, state, encounter, S::Season::Summer);
  EXPECT_EQ(context.kind, QStringLiteral("siege"));
  EXPECT_EQ(context.preset, QStringLiteral("siege_town"));
  const QJsonObject parameters = S::generation_request(def, state, context)
                                     .value(QStringLiteral("parameters"))
                                     .toObject();
  EXPECT_EQ(parameters.value(QStringLiteral("settlements.forts")).toInt(), 3);
  EXPECT_TRUE(parameters.value(QStringLiteral("settlements.walled_start")).toBool());
  EXPECT_EQ(parameters.value(QStringLiteral("spawns.army")).toInt(), 7);
}

TEST(StrategicEncounters, AnUnthreatenedSiteIsAttackedByTheNearestHostileArmy) {
  const S::WorldDefinition def = micro_world();
  const S::Encounter encounter =
      S::encounter_at(def, def.initial_state, QStringLiteral("market"));
  EXPECT_EQ(encounter.attacker_army, QStringLiteral("host"))
      << "unclaimed ground is taken by whichever army is nearest";
  EXPECT_GT(encounter.days_apart, 0.0);
  const S::Encounter held =
      S::encounter_at(def, def.initial_state, QStringLiteral("harbour"));
  EXPECT_EQ(held.defender_army, QStringLiteral("host"));
  EXPECT_EQ(held.attacker_army, QStringLiteral("legion"));
}

TEST(StrategicEncounters, HostileArmiesInRangeSuggestBattles) {
  const S::WorldDefinition def = micro_world();
  const auto encounters = S::suggest_encounters(def, def.initial_state, 60.0);
  ASSERT_FALSE(encounters.isEmpty());
  EXPECT_EQ(encounters.first().attacker_army, QStringLiteral("host"));
  EXPECT_EQ(encounters.first().site, QStringLiteral("saddle"))
      << "the battle is offered at the pass on the way, not at the walls";
}

} // namespace
