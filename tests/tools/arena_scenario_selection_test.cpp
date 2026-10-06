#include <QStringList>

#include <gtest/gtest.h>

#include "tools/arena/arena_scenario.h"

namespace {

auto catalogue_ids() -> QStringList {
  QStringList ids;
  for (auto const& scenario : Arena::Scenarios::definitions()) {
    ids.push_back(scenario.id);
  }
  return ids;
}

TEST(ArenaScenarioSelectionTest, ExactIdsKeepTheOrderTheyWereGiven) {
  const QStringList ids = catalogue_ids();
  ASSERT_GE(ids.size(), 2);

  QString error;
  const QStringList selected = Arena::Scenarios::select_definition_ids(
      ids[1] + QStringLiteral(" , ") + ids[0] + QStringLiteral(",") + ids[1], &error);

  EXPECT_TRUE(error.isEmpty()) << error.toStdString();
  EXPECT_EQ(selected, (QStringList{ids[1], ids[0]}));
}

TEST(ArenaScenarioSelectionTest, WildcardsExpandInCatalogueOrder) {
  const QStringList ids = catalogue_ids();
  const QString prefix = ids.front().section(QLatin1Char('_'), 0, 0);
  QStringList expected;
  for (const QString& id : ids) {
    if (id.startsWith(prefix)) {
      expected.push_back(id);
    }
  }

  QString error;
  const QStringList selected =
      Arena::Scenarios::select_definition_ids(prefix + QStringLiteral("*"), &error);

  EXPECT_TRUE(error.isEmpty()) << error.toStdString();
  EXPECT_EQ(selected, expected);
}

TEST(ArenaScenarioSelectionTest, WildcardIsAnchoredToTheWholeId) {
  const QString id = catalogue_ids().front();
  QString error;
  const QStringList selected = Arena::Scenarios::select_definition_ids(
      id.left(id.size() - 1) + QStringLiteral("?"), &error);

  EXPECT_TRUE(selected.contains(id));
  for (const QString& match : selected) {
    EXPECT_EQ(match.size(), id.size()) << match.toStdString();
  }
}

TEST(ArenaScenarioSelectionTest, UnknownIdsAndEmptyPatternsAreErrors) {
  QString error;
  EXPECT_TRUE(Arena::Scenarios::select_definition_ids(
                  QStringLiteral("no_such_scenario_anywhere"), &error)
                  .isEmpty());
  EXPECT_TRUE(error.contains(QStringLiteral("no_such_scenario_anywhere")));

  error.clear();
  EXPECT_TRUE(Arena::Scenarios::select_definition_ids(
                  QStringLiteral("zz_never_matches_*"), &error)
                  .isEmpty());
  EXPECT_TRUE(error.contains(QStringLiteral("matched nothing")));

  error.clear();
  EXPECT_TRUE(
      Arena::Scenarios::select_definition_ids(QStringLiteral(" , "), &error).isEmpty());
  EXPECT_FALSE(error.isEmpty());
}

} // namespace
