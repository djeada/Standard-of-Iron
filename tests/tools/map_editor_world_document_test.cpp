#include <QDir>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include "game/strategic/strategic_assets.h"
#include "game/strategic/strategic_json.h"
#include "tests/strategic/strategic_test_world.h"
#include "world_document.h"

namespace {

namespace S = Game::Strategic;

auto micro_document() -> std::unique_ptr<MapEditor::WorldDocument> {
  auto document = std::make_unique<MapEditor::WorldDocument>();
  document->adopt(StrategicTest::micro_world(), nullptr, {});
  return document;
}

TEST(WorldDocument, AdoptingAWorldValidatesItAndStartsAtTheOpening) {
  auto document = micro_document();
  EXPECT_FALSE(document->is_modified());
  EXPECT_EQ(document->timeline_index(), 0);
  EXPECT_EQ(document->report().errors(), 0);
  EXPECT_EQ(document->supply().size(), 2);
  EXPECT_FALSE(document->can_undo());
}

TEST(WorldDocument, AddingARouteIsUndoableAndRevalidates) {
  auto document = micro_document();
  const QString id = document->add_route(
      QStringLiteral("market"), QStringLiteral("anchorage"), S::RouteKind::Road);
  ASSERT_FALSE(id.isEmpty());
  EXPECT_TRUE(document->is_modified());
  EXPECT_EQ(document->selection(), id);
  EXPECT_TRUE(document->report().has(QStringLiteral("route_regions_not_adjacent")));
  document->undo();
  EXPECT_EQ(document->definition().route(id), nullptr);
  EXPECT_FALSE(document->report().has(QStringLiteral("route_regions_not_adjacent")));
  EXPECT_TRUE(document->selection().isEmpty())
      << "a selection that no longer exists is cleared";
  document->redo();
  EXPECT_NE(document->definition().route(id), nullptr);
}

TEST(WorldDocument, RemovingASiteTakesItsRoutesWithIt) {
  auto document = micro_document();
  ASSERT_TRUE(document->remove(QStringLiteral("saddle")));
  EXPECT_EQ(document->definition().route(QStringLiteral("ascent")), nullptr);
  EXPECT_EQ(document->definition().route(QStringLiteral("descent")), nullptr);
  EXPECT_TRUE(document->report().has(QStringLiteral("network_disconnected")));
}

TEST(WorldDocument, CompleteNetworkRepairsUnconnectedSitesAndStrandedGroups) {
  auto document = micro_document();
  document->add_site({2.6, 41.4}, S::SiteKind::Town);
  ASSERT_TRUE(document->remove(QStringLiteral("strait")));
  ASSERT_TRUE(document->remove(QStringLiteral("ascent")));
  EXPECT_TRUE(document->report().has(QStringLiteral("site_unconnected")));
  EXPECT_TRUE(document->report().has(QStringLiteral("network_disconnected")));
  const int added = document->complete_network();
  EXPECT_EQ(added, 3) << "the new town, the island and the stranded east";
  EXPECT_FALSE(document->report().has(QStringLiteral("site_unconnected")));
  EXPECT_FALSE(document->report().has(QStringLiteral("network_disconnected")));
  EXPECT_FALSE(document->report().has(QStringLiteral("route_regions_not_adjacent")))
      << "no road is laid between regions that do not border";
  for (const auto& route : document->definition().routes) {
    if (!route.tags.contains(QStringLiteral("auto"))) {
      continue;
    }
    if (route.touches(QStringLiteral("anchorage"))) {
      EXPECT_EQ(route.kind, S::RouteKind::SeaLane) << "the island is reached by sea";
      EXPECT_TRUE(route.touches(QStringLiteral("harbour")));
    } else {
      EXPECT_EQ(route.kind, S::RouteKind::Road);
    }
  }
  EXPECT_EQ(document->complete_network(), 0);
  document->undo();
  EXPECT_TRUE(document->report().has(QStringLiteral("network_disconnected")));
}

TEST(WorldDocument, TheTimelineReplaysEventsWithoutTouchingTheDefinition) {
  auto document = micro_document();
  int notifications = 0;
  QObject::connect(document.get(),
                   &MapEditor::WorldDocument::timeline_changed,
                   [&notifications](int) { ++notifications; });
  document->set_timeline_index(2);
  EXPECT_EQ(notifications, 1);
  EXPECT_EQ(document->current_event()->id, QStringLiteral("battle"));
  EXPECT_EQ(document->state().army(QStringLiteral("legion")), nullptr);
  EXPECT_NE(document->previous_state().army(QStringLiteral("legion")), nullptr);
  EXPECT_FALSE(document->is_modified());
  document->set_timeline_index(99);
  EXPECT_EQ(document->timeline_index(), 2);
}

TEST(WorldDocument, EditingTheOpeningControllerFeedsTheReplay) {
  auto document = micro_document();
  ASSERT_TRUE(document->set_initial_controller(QStringLiteral("market"),
                                               QStringLiteral("rome")));
  EXPECT_EQ(S::site_controller(
                document->definition(), document->state(), QStringLiteral("market")),
            QStringLiteral("rome"));
  ASSERT_TRUE(document->set_initial_army_location(QStringLiteral("host"),
                                                  QStringLiteral("market")));
  EXPECT_EQ(document->state().army(QStringLiteral("host"))->location,
            QStringLiteral("market"));
}

TEST(WorldDocument, SavesInTheCanonicalLayoutAndReloads) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  auto document = micro_document();
  ASSERT_TRUE(document->add_waypoint(QStringLiteral("descent"), {6.0, 43.6}));
  const QString path = dir.filePath(QStringLiteral("micro.json"));
  QString error;
  ASSERT_TRUE(document->save_as(path, &error)) << error.toStdString();
  EXPECT_FALSE(document->is_modified());
  const auto reread = S::load_definition_file(path, &error);
  ASSERT_TRUE(reread.has_value()) << error.toStdString();
  EXPECT_EQ(reread->route(QStringLiteral("descent"))->waypoints.size(), 1);
  EXPECT_EQ(S::definition_to_bytes(*reread),
            S::definition_to_bytes(document->definition()));
}

TEST(WorldDocument, MovingASiteAcrossTheMapReassignsItsRegion) {
  const QString root = StrategicTest::repository_root();
  MapEditor::WorldDocument document;
  QString error;
  ASSERT_TRUE(document.load(
      QDir(root).filePath(QString::fromLatin1(S::k_default_world_path)), &error))
      << error.toStdString();
  ASSERT_NE(document.geometry(), nullptr);
  EXPECT_EQ(document.report().errors(), 0);
  ASSERT_TRUE(document.move_site(QStringLiteral("cannae"), {12.2, 43.3}));
  const auto* cannae = document.definition().site(QStringLiteral("cannae"));
  EXPECT_EQ(cannae->region, QStringLiteral("etruria"));
  EXPECT_TRUE(cannae->district.isEmpty());
  EXPECT_TRUE(document.report().has(QStringLiteral("campaign_region_mismatch")))
      << "the Cannae mission is set in Apulia";
  document.undo();
  EXPECT_EQ(document.definition().site(QStringLiteral("cannae"))->region,
            QStringLiteral("southern_italy"));
  EXPECT_EQ(document.report().errors(), 0);
}

} // namespace

namespace {

TEST(WorldDocument, BendsCanBeMovedAndRemovedWithUndo) {
  auto document = micro_document();
  ASSERT_TRUE(document->add_waypoint(QStringLiteral("descent"), {6.0, 43.6}));
  ASSERT_TRUE(document->move_waypoint(QStringLiteral("descent"), 0, {6.1, 43.2}));
  EXPECT_EQ(document->definition().route(QStringLiteral("descent"))->waypoints.first(),
            (Game::Strategic::LonLat{6.1, 43.2}));
  EXPECT_FALSE(document->move_waypoint(QStringLiteral("descent"), 5, {6.1, 43.2}));
  ASSERT_TRUE(document->remove_waypoint(QStringLiteral("descent"), 0));
  EXPECT_TRUE(
      document->definition().route(QStringLiteral("descent"))->waypoints.isEmpty());
  document->undo();
  EXPECT_EQ(document->definition().route(QStringLiteral("descent"))->waypoints.size(),
            1);
}

TEST(WorldDocument, OpeningRouteStatusFeedsTheGraph) {
  auto document = micro_document();
  ASSERT_TRUE(document->set_initial_route_status(
      QStringLiteral("ascent"), Game::Strategic::RouteStatus::Blocked));
  EXPECT_EQ(Game::Strategic::route_status(document->state(), QStringLiteral("ascent")),
            Game::Strategic::RouteStatus::Blocked);
  EXPECT_FALSE(document->set_initial_route_status(
      QStringLiteral("ascent"), Game::Strategic::RouteStatus::Blocked));
  ASSERT_TRUE(document->set_initial_route_status(QStringLiteral("ascent"),
                                                 Game::Strategic::RouteStatus::Open));
  EXPECT_TRUE(document->definition().initial_state.routes.empty())
      << "an open route is the default and is not written out";
}

} // namespace
