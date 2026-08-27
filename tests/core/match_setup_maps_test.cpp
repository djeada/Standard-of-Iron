#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <gtest/gtest.h>
#include <memory>
#include <mutex>
#include <set>

#include "app/core/client_context.h"
#include "app/core/loading_overlay_log.h"
#include "app/input/cursor_mode.h"
#include "app/viewmodels/match_setup_view_model.h"
#include "game/map/map_catalog.h"
#include "game/session/session_context.h"
#include "game/systems/default_content.h"
#include "game/systems/nation_registry.h"

namespace {

auto make_map(const QString& name) -> QVariantMap {
  QVariantMap entry;
  entry["name"] = name;
  entry["path"] = QStringLiteral(":/assets/maps/%1.json").arg(name);
  return entry;
}

} // namespace

TEST(MatchSetupMapListTest, ReopeningTheScreenDoesNotDuplicateTheBattlefields) {
  App::ViewModels::MapList list;

  list.begin_loading();
  list.append(make_map(QStringLiteral("map_rivers")));
  list.append(make_map(QStringLiteral("map_forest")));
  list.end_loading();
  ASSERT_EQ(list.maps().size(), 2);

  list.begin_loading();
  list.append(make_map(QStringLiteral("map_rivers")));
  list.append(make_map(QStringLiteral("map_forest")));
  list.end_loading();

  EXPECT_EQ(list.maps().size(), 2)
      << "reopening the skirmish screen appended a second copy of every map";
}

TEST(MatchSetupMapListTest, AFinishedScanKeepsWhatItCollected) {
  App::ViewModels::MapList list;
  list.begin_loading();
  list.append(make_map(QStringLiteral("map_rivers")));
  list.end_loading();

  ASSERT_EQ(list.maps().size(), 1);
  EXPECT_EQ(list.maps().first().toMap().value("name").toString(),
            QStringLiteral("map_rivers"));
  EXPECT_FALSE(list.empty());
}

TEST(LoadingOverlayLogTest, TheGapSinceThePreviousFrameIsNeverNegative) {
  const QString line = App::Core::format_loading_overlay_line(1, 5, 1994, 3341);

  EXPECT_FALSE(line.contains(QStringLiteral("+-")))
      << line.toStdString() << ": a second load in one session printed a negative gap";
  EXPECT_TRUE(line.contains(QStringLiteral("(+0ms since the previous one)")))
      << line.toStdString();
}

TEST(LoadingOverlayLogTest, AnOrdinaryGapIsReported) {
  const QString line = App::Core::format_loading_overlay_line(3, 5, 2967, 2774);

  EXPECT_TRUE(line.contains(QStringLiteral("frame 3 of 5"))) << line.toStdString();
  EXPECT_TRUE(line.contains(QStringLiteral("presented at 2967ms")))
      << line.toStdString();
  EXPECT_TRUE(line.contains(QStringLiteral("(+193ms since the previous one)")))
      << line.toStdString();
}

namespace {

class StubClientHost : public App::Core::ClientHost {
public:
  void ensure_initialized() override {}

  auto lock_frame() -> std::unique_lock<std::recursive_mutex> override {
    return std::unique_lock<std::recursive_mutex>(m_mutex);
  }

  void set_cursor_mode(CursorMode) override {}

private:
  std::recursive_mutex m_mutex;
};

class ObserverRosterTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto& session = Game::Session::SessionContext::active();
    session.nations().clear();
    Game::Systems::initialize_default_content(session.nations());
    m_context.session = &session;
    m_context.local_owner_id = 1;
    m_view_model =
        std::make_unique<App::ViewModels::MatchSetupViewModel>(m_context, m_host);
  }

  void TearDown() override {
    m_view_model.reset();
    Game::Session::SessionContext::active().nations().clear();
  }

  App::Core::ClientContext m_context{};
  StubClientHost m_host;
  std::unique_ptr<App::ViewModels::MatchSetupViewModel> m_view_model;
};

} // namespace

TEST_F(ObserverRosterTest, EverySlotIsSeatedUnderComputerControl) {
  const auto configs = m_view_model->build_observer_player_configs(
      QStringLiteral("assets/maps/map_spanish_grove.json"));

  ASSERT_EQ(configs.size(), 4) << "Sunbaked Terraces authors four camps";

  std::set<int> player_ids;
  std::set<int> teams;
  for (const QVariant& entry : configs) {
    const QVariantMap config = entry.toMap();
    EXPECT_FALSE(config.value("isHuman").toBool())
        << "an observed match seated a human";
    EXPECT_FALSE(config.value("nationId").toString().isEmpty());
    EXPECT_FALSE(config.value("commanderTroop").toString().isEmpty())
        << "an observed army marched without a commander";
    player_ids.insert(config.value("player_id").toInt());
    teams.insert(config.value("team_id").toInt());
  }

  EXPECT_EQ(player_ids, (std::set<int>{1, 2, 3, 4}));
  EXPECT_EQ(teams.size(), 2U) << "an observed match needs two sides to fight";
}

TEST_F(ObserverRosterTest, ATwoCampMapMakesAOneOnOne) {
  const auto configs = m_view_model->build_observer_player_configs(
      QStringLiteral("assets/maps/map_rivers.json"));

  ASSERT_EQ(configs.size(), 2);
  EXPECT_NE(configs.at(0).toMap().value("team_id").toInt(),
            configs.at(1).toMap().value("team_id").toInt());
}

TEST_F(ObserverRosterTest, AMapWithNothingToWatchIsRefused) {
  EXPECT_TRUE(m_view_model
                  ->build_observer_player_configs(
                      QStringLiteral("assets/maps/does_not_exist.json"))
                  .isEmpty());
  EXPECT_FALSE(m_view_model->start_observed_skirmish(
      QStringLiteral("assets/maps/does_not_exist.json")));
}

// The battlefield-list rows read `path`, `name` and `thumbnail` off each
// catalogue entry by name; QML looks roles up by name, so a renamed or missing
// key fails silently - the row falls back to a generic glyph and nothing warns.
// That is how the thumbnails came back empty twice, once because the fix went
// into a component nothing instantiates and once because the delegate reached
// for a role only reachable through `modelData`. Pin what the real catalogue
// hands the rows.
TEST(MatchSetupMapListTest, TheRealCatalogueCarriesTheKeysTheListRowsReadByName) {
  const auto maps = Game::Map::MapCatalog::available_maps();
  ASSERT_FALSE(maps.isEmpty()) << "the shipped battlefields did not load";

  for (const auto& row : maps) {
    const auto entry = row.toMap();
    ASSERT_TRUE(entry.contains(QStringLiteral("path")))
        << "a battlefield row has no map path, so it can draw no preview";
    EXPECT_FALSE(entry.value(QStringLiteral("path")).toString().isEmpty());
    EXPECT_TRUE(entry.contains(QStringLiteral("name")));
    EXPECT_TRUE(entry.contains(QStringLiteral("thumbnail")))
        << "the rows cannot tell an authored thumbnail from a missing one";
  }
}
