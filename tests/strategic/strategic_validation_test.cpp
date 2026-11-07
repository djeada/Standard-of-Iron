#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <functional>
#include <gtest/gtest.h>
#include <string>
#include <utility>

#include "game/strategic/strategic_assets.h"
#include "game/strategic/strategic_encounter.h"
#include "game/strategic/strategic_events.h"
#include "game/strategic/strategic_graph.h"
#include "game/strategic/strategic_json.h"
#include "game/strategic/strategic_validation.h"
#include "strategic_test_world.h"

namespace {

namespace S = Game::Strategic;
using StrategicTest::micro_world;

auto micro_context() -> S::ValidationContext {
  S::ValidationContext context;
  context.resource_exists = [](const QString& path) {
    return path == QLatin1String("assets/maps/map_mountain.json");
  };
  context.mission_ids = QSet<QString>{QStringLiteral("siege_of_citadel")};
  context.campaign_missions = {
      {QStringLiteral("siege_of_citadel"), QStringLiteral("east"), 0}};
  return context;
}

auto codes(const S::ValidationReport& report) -> std::string {
  return report.codes().join(QStringLiteral(", ")).toStdString();
}

auto messages(const S::ValidationReport& report) -> std::string {
  std::string out;
  for (const auto& issue : report.issues) {
    out += "\n  [" + S::to_string(issue.severity).toStdString() + "] " +
           issue.code.toStdString() + " " + issue.entity.toStdString() + ": " +
           issue.message.toStdString();
  }
  return out;
}

struct Breakage {
  const char* name;
  std::function<void(S::WorldDefinition&, S::ValidationContext&)> mutate;
  const char* expected_code;
  S::Severity severity;
};

class StrategicValidationBreakage : public ::testing::TestWithParam<Breakage> {};

TEST(StrategicValidation, TheMicroWorldIsClean) {
  const S::ValidationReport report = S::validate_world(micro_world(), micro_context());
  EXPECT_EQ(report.errors(), 0) << messages(report);
  EXPECT_EQ(report.warnings(), 0) << messages(report);
}

TEST_P(StrategicValidationBreakage, IsCaught) {
  const Breakage& breakage = GetParam();
  S::WorldDefinition def = micro_world();
  S::ValidationContext context = micro_context();
  breakage.mutate(def, context);
  const S::ValidationReport report = S::validate_world(def, context);
  bool found = false;
  for (const auto& issue : report.issues) {
    if (issue.code == QLatin1String(breakage.expected_code)) {
      found = true;
      EXPECT_EQ(issue.severity, breakage.severity) << issue.message.toStdString();
    }
  }
  EXPECT_TRUE(found) << breakage.name << ": expected " << breakage.expected_code
                     << ", got " << codes(report);
}

auto site(S::WorldDefinition& def, const char* id) -> S::Site& {
  return *def.site_mut(QString::fromLatin1(id));
}

auto route(S::WorldDefinition& def, const char* id) -> S::Route& {
  return *def.route_mut(QString::fromLatin1(id));
}

const Breakage k_breakages[] = {
    {"duplicate id across kinds",
     [](S::WorldDefinition& def, S::ValidationContext&) {
       route(def, "strait").id = QStringLiteral("market");
     },
     "duplicate_id",
     S::Severity::Error},
    {"site in an unknown region",
     [](S::WorldDefinition& def, S::ValidationContext&) {
       site(def, "market").region = QStringLiteral("atlantis");
     },
     "site_region_unknown",
     S::Severity::Error},
    {"site in another region's district",
     [](S::WorldDefinition& def, S::ValidationContext&) {
       site(def, "citadel").district = QStringLiteral("coastland");
     },
     "site_district_unknown",
     S::Severity::Error},
    {"site outside its district outline",
     [](S::WorldDefinition& def, S::ValidationContext&) {
       site(def, "market").district = QStringLiteral("coastland");
     },
     "site_outside_district",
     S::Severity::Error},
    {"site outside the map",
     [](S::WorldDefinition& def, S::ValidationContext&) {
       site(def, "ford").position = {40.0, 44.0};
     },
     "site_out_of_bounds",
     S::Severity::Error},
    {"route to nowhere",
     [](S::WorldDefinition& def, S::ValidationContext&) {
       route(def, "ford_road").to = QStringLiteral("nowhere");
     },
     "route_endpoint_unknown",
     S::Severity::Error},
    {"route from a site to itself",
     [](S::WorldDefinition& def, S::ValidationContext&) {
       route(def, "ford_road").to = QStringLiteral("citadel");
     },
     "route_self_loop",
     S::Severity::Error},
    {"sea lane without a harbour",
     [](S::WorldDefinition& def, S::ValidationContext&) {
       site(def, "anchorage").port = S::PortClass::None;
     },
     "sea_lane_without_port",
     S::Severity::Error},
    {"authored length shorter than the crow flies",
     [](S::WorldDefinition& def, S::ValidationContext&) {
       route(def, "descent").length_km = 10.0;
     },
     "route_shorter_than_geodesic",
     S::Severity::Error},
    {"duplicate route",
     [](S::WorldDefinition& def, S::ValidationContext&) {
       S::Route copy = route(def, "coast_road");
       copy.id = QStringLiteral("coast_road_again");
       def.routes.append(copy);
     },
     "route_duplicate",
     S::Severity::Warning},
    {"a site with no road",
     [](S::WorldDefinition& def, S::ValidationContext&) {
       S::Site lonely = site(def, "market");
       lonely.id = QStringLiteral("lonely");
       lonely.position = {2.5, 41.5};
       def.sites.append(lonely);
     },
     "site_unconnected",
     S::Severity::Error},
    {"a stranded group of sites",
     [](S::WorldDefinition& def, S::ValidationContext&) {
       def.routes.removeIf(
           [](const S::Route& r) { return r.id == QLatin1String("ascent"); });
     },
     "network_disconnected",
     S::Severity::Error},
    {"bordering regions with no road between them",
     [](S::WorldDefinition& def, S::ValidationContext&) {
       route(def, "ascent").to = QStringLiteral("harbour");
       route(def, "ascent").from = QStringLiteral("anchorage");
       route(def, "ascent").kind = S::RouteKind::SeaLane;
       S::Route link = route(def, "descent");
       def.routes.append(link);
     },
     "adjacent_without_route",
     S::Severity::Warning},
    {"overland route between regions that do not border",
     [](S::WorldDefinition& def, S::ValidationContext&) {
       S::Route bridge;
       bridge.id = QStringLiteral("impossible_bridge");
       bridge.from = QStringLiteral("anchorage");
       bridge.to = QStringLiteral("market");
       def.routes.append(bridge);
     },
     "route_regions_not_adjacent",
     S::Severity::Warning},
    {"one-sided adjacency",
     [](S::WorldDefinition& def, S::ValidationContext&) {
       def.region_mut(QStringLiteral("isle"))->adjacent = {QStringLiteral("west")};
     },
     "region_adjacency_asymmetric",
     S::Severity::Warning},
    {"unknown faction in the opening state",
     [](S::WorldDefinition& def, S::ValidationContext&) {
       def.initial_state.sites[QStringLiteral("ford")].controller =
           QStringLiteral("atlantis");
     },
     "unknown_faction",
     S::Severity::Error},
    {"army standing nowhere",
     [](S::WorldDefinition& def, S::ValidationContext&) {
       def.initial_state.armies[1].location = QStringLiteral("nowhere");
     },
     "army_location_unknown",
     S::Severity::Error},
    {"one commander leading two armies",
     [](S::WorldDefinition& def, S::ValidationContext&) {
       def.initial_state.armies[1].commander = QStringLiteral("barca");
     },
     "commander_in_two_armies",
     S::Severity::Error},
    {"army marching on a site that does not exist",
     [](S::WorldDefinition& def, S::ValidationContext&) {
       def.initial_state.armies[0].objective = QStringLiteral("eldorado");
     },
     "army_objective_unknown",
     S::Severity::Error},
    {"battlefield map that is not on disk",
     [](S::WorldDefinition& def, S::ValidationContext&) {
       site(def, "saddle").battlefield.map =
           QStringLiteral("assets/maps/map_missing.json");
     },
     "battlefield_map_missing",
     S::Severity::Error},
    {"battlefield mission that does not exist",
     [](S::WorldDefinition& def, S::ValidationContext&) {
       site(def, "ford").battlefield.mission = QStringLiteral("no_such_mission");
     },
     "battlefield_mission_unknown",
     S::Severity::Error},
    {"battlefield map that disagrees with the mission",
     [](S::WorldDefinition& def, S::ValidationContext& context) {
       site(def, "citadel").battlefield.map =
           QStringLiteral("assets/maps/map_mountain.json");
       context.mission_maps.insert(QStringLiteral("siege_of_citadel"),
                                   QStringLiteral(":/assets/maps/map_citadel.json"));
     },
     "battlefield_map_mismatch",
     S::Severity::Error},
    {"unknown generator preset",
     [](S::WorldDefinition& def, S::ValidationContext&) {
       site(def, "saddle").battlefield.preset = QStringLiteral("volcano");
     },
     "battlefield_preset_unknown",
     S::Severity::Error},
    {"two sites claiming one mission",
     [](S::WorldDefinition& def, S::ValidationContext&) {
       site(def, "ford").battlefield.mission = QStringLiteral("siege_of_citadel");
     },
     "mission_site_duplicate",
     S::Severity::Error},
    {"event moving an army with no road to take",
     [](S::WorldDefinition& def, S::ValidationContext&) {
       S::Site far = site(def, "anchorage");
       far.id = QStringLiteral("far_isle");
       far.position = {9.5, 45.5};
       def.sites.append(far);
       def.events[0].effects[0].to = QStringLiteral("far_isle");
     },
     "event_army_teleports",
     S::Severity::Error},
    {"event naming an army that is gone",
     [](S::WorldDefinition& def, S::ValidationContext&) {
       S::Effect again;
       again.op = S::EffectOp::MoveArmy;
       again.target = QStringLiteral("legion");
       again.to = QStringLiteral("ford");
       def.events[1].effects.append(again);
     },
     "event_effect_failed",
     S::Severity::Error},
    {"events out of chronological order",
     [](S::WorldDefinition& def, S::ValidationContext&) { def.events[1].year = -219; },
     "event_out_of_order",
     S::Severity::Error},
    {"army led by a dead commander",
     [](S::WorldDefinition& def, S::ValidationContext&) {
       S::Effect kill;
       kill.op = S::EffectOp::SetCommander;
       kill.target = QStringLiteral("barca");
       kill.commander_status = S::CommanderStatus::Dead;
       def.events[0].effects.append(kill);
       S::Effect reassign;
       reassign.op = S::EffectOp::CreateArmy;
       reassign.target = QStringLiteral("ghost_host");
       reassign.faction = QStringLiteral("carthage");
       reassign.commander = QStringLiteral("barca");
       reassign.to = QStringLiteral("harbour");
       reassign.infantry = 100;
       def.events[0].effects.append(reassign);
     },
     "army_led_by_absent_commander",
     S::Severity::Error},
    {"campaign region that is neither region nor district",
     [](S::WorldDefinition&, S::ValidationContext& context) {
       context.campaign_missions[0].world_region_id = QStringLiteral("hyperborea");
     },
     "campaign_region_unknown",
     S::Severity::Error},
    {"campaign mission placed in the wrong region",
     [](S::WorldDefinition&, S::ValidationContext& context) {
       context.campaign_missions[0].world_region_id = QStringLiteral("west");
     },
     "campaign_region_mismatch",
     S::Severity::Error},
    {"campaign mission with no site",
     [](S::WorldDefinition&, S::ValidationContext& context) {
       context.campaign_missions.append(
           {QStringLiteral("unplaced"), QStringLiteral("east"), 1});
     },
     "campaign_mission_unplaced",
     S::Severity::Error},
    {"campaign mission site the player cannot reach",
     [](S::WorldDefinition& def, S::ValidationContext&) {
       def.routes.removeIf(
           [](const S::Route& r) { return r.id == QLatin1String("ascent"); });
     },
     "campaign_site_unreachable",
     S::Severity::Error},
    {"campaign mission whose result changes nothing",
     [](S::WorldDefinition& def, S::ValidationContext&) {
       def.events[1].mission.clear();
     },
     "mission_without_event",
     S::Severity::Warning},
    {"resource outside its scale",
     [](S::WorldDefinition& def, S::ValidationContext&) {
       site(def, "market").resources.set(S::Resource::Food, 9);
     },
     "value_out_of_range",
     S::Severity::Error},
    {"capital not held at the start",
     [](S::WorldDefinition& def, S::ValidationContext&) {
       def.initial_state.sites[QStringLiteral("citadel")].controller =
           QStringLiteral("carthage");
     },
     "capital_not_held",
     S::Severity::Warning},
    {"ally that is not declared",
     [](S::WorldDefinition& def, S::ValidationContext&) {
       def.factions[0].allies = {QStringLiteral("atlantis")};
     },
     "unknown_faction",
     S::Severity::Error},
    {"province mapping that disagrees with the site",
     [](S::WorldDefinition&, S::ValidationContext& context) {
       context.mission_provinces.insert(QStringLiteral("siege_of_citadel"),
                                        QStringLiteral("west"));
     },
     "province_mapping_mismatch",
     S::Severity::Error},
    {"mission fielding a commander on the wrong side",
     [](S::WorldDefinition& def, S::ValidationContext& context) {
       def.commanders[1].catalog_id = QStringLiteral("cat_consul");
       context.mission_facts.insert(QStringLiteral("siege_of_citadel"),
                                    {{{QStringLiteral("player"),
                                       QStringLiteral("carthage"),
                                       {QStringLiteral("cat_consul")}}},
                                     0,
                                     0});
     },
     "mission_commander_side",
     S::Severity::Error},
    {"mission fielding a commander who is already dead",
     [](S::WorldDefinition& def, S::ValidationContext& context) {
       def.commanders[2].catalog_id = QStringLiteral("cat_legate");
       def.initial_state.commanders[QStringLiteral("legate")].status =
           S::CommanderStatus::Dead;
       context.mission_facts.insert(QStringLiteral("siege_of_citadel"),
                                    {{{QStringLiteral("rome_side"),
                                       QStringLiteral("rome"),
                                       {QStringLiteral("cat_legate")}}},
                                     0,
                                     0});
     },
     "mission_commander_absent",
     S::Severity::Error},
    {"mission dated in another year than its event",
     [](S::WorldDefinition&, S::ValidationContext& context) {
       context.mission_facts.insert(QStringLiteral("siege_of_citadel"),
                                    {{}, -230, -230});
     },
     "mission_date_mismatch",
     S::Severity::Warning},
    {"enumeration typo in the file",
     [](S::WorldDefinition& def, S::ValidationContext&) {
       def.load_issues.append(
           {QStringLiteral("coast_road"), QStringLiteral("unknown kind 'raod'")});
     },
     "load_issue",
     S::Severity::Error},
};

INSTANTIATE_TEST_SUITE_P(EveryCheck,
                         StrategicValidationBreakage,
                         ::testing::ValuesIn(k_breakages),
                         [](const ::testing::TestParamInfo<Breakage>& info) {
                           std::string name = info.param.expected_code;
                           name += "_" + std::to_string(info.index);
                           return name;
                         });

TEST(StrategicValidation, EventIssuesAreReportedOncePerEntity) {
  S::WorldDefinition def = micro_world();
  def.initial_state.armies[1].infantry = 0;
  def.initial_state.armies[1].cavalry = 0;
  S::Effect noop;
  noop.op = S::EffectOp::SetFlag;
  noop.target = QStringLiteral("x");
  def.events[0].effects.append(noop);
  def.events[1].effects.removeIf(
      [](const S::Effect& e) { return e.op == S::EffectOp::DisbandArmy; });
  const S::ValidationReport report = S::validate_world(def, micro_context());
  int empty = 0;
  for (const auto& issue : report.issues) {
    empty += issue.code == QLatin1String("army_empty") &&
                     issue.entity == QLatin1String("legion")
                 ? 1
                 : 0;
  }
  EXPECT_EQ(empty, 1) << messages(report);
}

class ShippedStrategicWorld : public ::testing::Test {
protected:
  static void SetUpTestSuite() {
    const QString root = StrategicTest::repository_root();
    QString error;
    auto bundle = S::load_world_bundle(
        QDir(root).filePath(QString::fromLatin1(S::k_default_world_path)),
        root,
        &error);
    ASSERT_TRUE(bundle.has_value()) << error.toStdString();
    s_bundle = new S::WorldBundle(std::move(*bundle));
  }
  static void TearDownTestSuite() {
    delete s_bundle;
    s_bundle = nullptr;
  }
  static auto def() -> const S::WorldDefinition& { return s_bundle->definition; }
  static S::WorldBundle* s_bundle;
};

S::WorldBundle* ShippedStrategicWorld::s_bundle = nullptr;

TEST_F(ShippedStrategicWorld, PassesEveryCheckWithTheRealAssets) {
  ASSERT_NE(s_bundle->geometry, nullptr)
      << "provinces.json must load for the geometry checks";
  ASSERT_TRUE(s_bundle->context.mission_ids.has_value());
  ASSERT_FALSE(s_bundle->context.campaign_missions.isEmpty());
  ASSERT_GE(s_bundle->context.mission_facts.size(), 8)
      << "mission sides, cameo commanders and dates must reach the cross-checks";
  ASSERT_FALSE(s_bundle->context.mission_provinces.isEmpty());
  int cameos = 0;
  for (const auto& facts : s_bundle->context.mission_facts) {
    for (const auto& side : facts.sides) {
      cameos += static_cast<int>(side.commanders.size());
    }
  }
  EXPECT_GE(cameos, 11) << "every historical cameo is checked against the war";
  const S::ValidationReport report = S::validate_world(def(), s_bundle->context);
  EXPECT_EQ(report.errors(), 0) << messages(report);
  EXPECT_EQ(report.warnings(), 0) << messages(report);
}

TEST_F(ShippedStrategicWorld, IsStoredInCanonicalLayout) {
  QFile file(s_bundle->path);
  ASSERT_TRUE(file.open(QIODevice::ReadOnly));
  EXPECT_EQ(file.readAll(), S::definition_to_bytes(def()))
      << "run `strategic_world format` (or save from the editor) after hand edits";
}

TEST_F(ShippedStrategicWorld, EveryCampaignMissionHasASiteAndAResult) {
  for (const auto& mission : s_bundle->context.campaign_missions) {
    const S::Site* site = def().site_for_mission(mission.mission_id);
    ASSERT_NE(site, nullptr) << mission.mission_id.toStdString();
    EXPECT_FALSE(S::events_for_mission(def(), mission.mission_id).isEmpty())
        << mission.mission_id.toStdString();
    EXPECT_NE(def().resolve_campaign_region(mission.world_region_id), nullptr)
        << mission.world_region_id.toStdString();
  }
}

TEST_F(ShippedStrategicWorld, TheChronologyReplaysCleanlyToTheEndOfTheWar) {
  QVector<S::EffectError> errors;
  const S::WorldState end =
      S::state_after(def(), static_cast<int>(def().events.size()), &errors);
  for (const auto& error : errors) {
    ADD_FAILURE() << error.event.toStdString() << ": " << error.message.toStdString();
  }
  EXPECT_EQ(end.flags.count(QStringLiteral("war_over")), 1U);
  EXPECT_EQ(S::region_controller(def(), end, QStringLiteral("iberia_carthaginian")),
            QStringLiteral("rome"));
  EXPECT_EQ(end.army(QStringLiteral("hannibal_army")), nullptr);
}

TEST_F(ShippedStrategicWorld, HannibalFollowsTheRoadToEachMission) {
  const S::WorldDefinition& world = def();
  S::WorldState state = world.initial_state;
  for (const auto& event : world.events) {
    const S::ArmyState* before = state.army(QStringLiteral("hannibal_army"));
    S::WorldState next = state;
    ASSERT_TRUE(S::apply_event(world, next, event)) << event.id.toStdString();
    const S::ArmyState* after = next.army(QStringLiteral("hannibal_army"));
    if (before != nullptr && after != nullptr && before->location != after->location) {
      const S::Path path =
          S::StrategicGraph(world, state)
              .shortest_path(before->location,
                             after->location,
                             S::TraversalRules::march(QStringLiteral("carthage")));
      EXPECT_TRUE(path.found) << event.id.toStdString() << ": "
                              << before->location.toStdString() << " -> "
                              << after->location.toStdString();
    }
    state = next;
  }
}

TEST_F(ShippedStrategicWorld, EveryPointInTheChronologySavesAndLoads) {
  for (int i = 0; i <= def().events.size(); ++i) {
    const S::WorldState state = S::state_after(def(), i);
    QString error;
    const auto loaded = S::load_state(def(), S::save_state(def(), state), &error);
    ASSERT_TRUE(loaded.has_value()) << i << ": " << error.toStdString();
    EXPECT_EQ(S::replay(def(), loaded->applied_events), state) << i;
  }
}

TEST_F(ShippedStrategicWorld, EveryMissionSiteProducesAValidGeneratorRequest) {
  for (const auto& site : def().sites) {
    if (site.battlefield.empty()) {
      continue;
    }
    const S::Encounter encounter = S::encounter_at(def(), def().initial_state, site.id);
    const S::EncounterContext context =
        S::encounter_context(def(), def().initial_state, encounter, S::Season::Summer);
    EXPECT_TRUE(S::known_generator_presets().contains(context.preset))
        << site.id.toStdString();
    const QJsonObject request =
        S::generation_request(def(), def().initial_state, context);
    EXPECT_GE(request.value(QStringLiteral("seed")).toDouble(), 0.0);
    EXPECT_LE(request.value(QStringLiteral("seed")).toDouble(), 2147483647.0)
        << "the editor's seed box is a signed 32-bit spin box";
    const QJsonObject parameters =
        request.value(QStringLiteral("parameters")).toObject();
    EXPECT_GE(parameters.value(QStringLiteral("roads.routes")).toInt(), 1);
    EXPECT_LE(parameters.value(QStringLiteral("roads.routes")).toInt(), 3);
  }
}

TEST_F(ShippedStrategicWorld, TheAlpsAreTheOnlyOverlandWayIntoItaly) {
  const S::StrategicGraph graph(def(), def().initial_state);
  const S::Path march =
      graph.shortest_path(QStringLiteral("new_carthage"),
                          QStringLiteral("taurasia"),
                          S::TraversalRules::march(QStringLiteral("carthage")));
  ASSERT_TRUE(march.found);
  EXPECT_TRUE(march.sites.contains(QStringLiteral("alpine_pass")));
  EXPECT_EQ(march.sea_legs, 0);
  EXPECT_FALSE(
      graph
          .shortest_path(QStringLiteral("new_carthage"),
                         QStringLiteral("taurasia"),
                         S::TraversalRules::safe_march(QStringLiteral("carthage")))
          .sites.contains(QStringLiteral("massalia")))
      << "Roman-allied Massalia is not a safe road";
}

} // namespace
