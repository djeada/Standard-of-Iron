#include <QVariantList>
#include <QVariantMap>

#include <gtest/gtest.h>

#include "app/models/selected_units_model.h"

namespace {

using App::Models::group_selection_by_type;
using App::Models::selection_groups_to_variant;

auto unit(const QString& type,
          const QString& name,
          double health,
          const QString& nation = QStringLiteral("roman_republic")) -> QVariant {
  QVariantMap entry;
  entry[QStringLiteral("unit_type")] = type;
  entry[QStringLiteral("name")] = name;
  entry[QStringLiteral("nation")] = nation;
  entry[QStringLiteral("health_ratio")] = health;
  return entry;
}

TEST(SelectionGroupingTest, EmptySelectionProducesNoRows) {
  EXPECT_TRUE(group_selection_by_type({}).empty());
}

TEST(SelectionGroupingTest, UnitsOfOneTypeCollapseIntoASingleRow) {
  const QVariantList units{unit("archer", "Archer", 1.0),
                           unit("archer", "Archer", 0.5),
                           unit("archer", "Archer", 0.25)};

  const auto groups = group_selection_by_type(units);

  ASSERT_EQ(groups.size(), 1U);
  EXPECT_EQ(groups[0].type_key, QStringLiteral("archer"));
  EXPECT_EQ(groups[0].count, 3);
  EXPECT_NEAR(groups[0].health, (1.0 + 0.5 + 0.25) / 3.0, 1e-9);
}

TEST(SelectionGroupingTest, RowsKeepFirstSeenOrder) {
  const QVariantList units{unit("spearman", "Spearman", 1.0),
                           unit("archer", "Archer", 1.0),
                           unit("spearman", "Spearman", 1.0),
                           unit("healer", "Healer", 1.0)};

  const auto groups = group_selection_by_type(units);

  ASSERT_EQ(groups.size(), 3U);
  EXPECT_EQ(groups[0].type_key, QStringLiteral("spearman"));
  EXPECT_EQ(groups[1].type_key, QStringLiteral("archer"));
  EXPECT_EQ(groups[2].type_key, QStringLiteral("healer"));
  EXPECT_EQ(groups[0].count, 2);
}

TEST(SelectionGroupingTest, OnlyDamagedUnitsCountAsWounded) {
  const QVariantList units{unit("swordsman", "Swordsman", 1.0),
                           unit("swordsman", "Swordsman", 0.99),
                           unit("swordsman", "Swordsman", 0.1)};

  const auto groups = group_selection_by_type(units);

  ASSERT_EQ(groups.size(), 1U);
  EXPECT_EQ(groups[0].wounded_count, 2);
}

TEST(SelectionGroupingTest, HealthRatiosAreClampedToTheUnitRange) {
  const QVariantList units{unit("archer", "Archer", 4.0),
                           unit("archer", "Archer", -2.0)};

  const auto groups = group_selection_by_type(units);

  ASSERT_EQ(groups.size(), 1U);
  EXPECT_NEAR(groups[0].health, 0.5, 1e-9);
}

TEST(SelectionGroupingTest, MissingTypeKeyFallsBackToTheDisplayName) {
  const QVariantList units{unit("", "Horse Archer", 1.0)};

  const auto groups = group_selection_by_type(units);

  ASSERT_EQ(groups.size(), 1U);
  EXPECT_EQ(groups[0].type_key, QStringLiteral("horse_archer"));
  EXPECT_EQ(groups[0].name, QStringLiteral("Horse Archer"));
}

TEST(SelectionGroupingTest, UnitsWithNeitherTypeNorNameAreDropped) {
  const QVariantList units{unit("", "", 1.0), unit("archer", "Archer", 1.0)};

  const auto groups = group_selection_by_type(units);

  ASSERT_EQ(groups.size(), 1U);
  EXPECT_EQ(groups[0].type_key, QStringLiteral("archer"));
}

TEST(SelectionGroupingTest, NationTravelsWithTheRowSoTheRosterCanPickAnIcon) {
  const QVariantList units{unit("archer", "Archer", 1.0, QStringLiteral("carthage"))};

  const auto groups = group_selection_by_type(units);

  ASSERT_EQ(groups.size(), 1U);
  EXPECT_EQ(groups[0].nation, QStringLiteral("carthage"));
}

TEST(SelectionGroupingTest, LargeSelectionsStayOneRowPerType) {
  QVariantList units;
  for (int i = 0; i < 250; ++i) {
    units.append(unit("spearman", "Spearman", 1.0));
  }
  for (int i = 0; i < 40; ++i) {
    units.append(unit("archer", "Archer", 0.5));
  }

  const auto groups = group_selection_by_type(units);

  ASSERT_EQ(groups.size(), 2U);
  EXPECT_EQ(groups[0].count, 250);
  EXPECT_EQ(groups[1].count, 40);
}

TEST(SelectionGroupingTest, VariantConversionExposesTheKeysTheHudBindsTo) {
  const auto groups = group_selection_by_type(
      {unit("archer", "Archer", 0.5, QStringLiteral("carthage"))});

  const QVariantList converted = selection_groups_to_variant(groups);

  ASSERT_EQ(converted.size(), 1);
  const QVariantMap row = converted.first().toMap();
  EXPECT_EQ(row.value(QStringLiteral("typeKey")).toString(), QStringLiteral("archer"));
  EXPECT_EQ(row.value(QStringLiteral("name")).toString(), QStringLiteral("Archer"));
  EXPECT_EQ(row.value(QStringLiteral("nation")).toString(), QStringLiteral("carthage"));
  EXPECT_EQ(row.value(QStringLiteral("count")).toInt(), 1);
  EXPECT_EQ(row.value(QStringLiteral("woundedCount")).toInt(), 1);
  EXPECT_NEAR(row.value(QStringLiteral("health")).toDouble(), 0.5, 1e-9);
}

} // namespace
