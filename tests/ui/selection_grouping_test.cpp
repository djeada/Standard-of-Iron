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
          const QString& nation = QStringLiteral("roman_republic"),
          double stamina = 1.0,
          bool can_run = true) -> QVariant {
  QVariantMap entry;
  entry[QStringLiteral("unit_type")] = type;
  entry[QStringLiteral("name")] = name;
  entry[QStringLiteral("nation")] = nation;
  entry[QStringLiteral("health_ratio")] = health;
  entry[QStringLiteral("stamina_ratio")] = stamina;
  entry[QStringLiteral("can_run")] = can_run;
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

TEST(SelectionGroupingTest, MovementAndFatigueDoNotChangeAggregatedHealth) {
  const QVariantList rested{unit("archer", "Archer", 1.0, {}, 1.0),
                            unit("archer", "Archer", 0.5, {}, 0.9)};
  const QVariantList fatigued{unit("archer", "Archer", 1.0, {}, 0.2),
                              unit("archer", "Archer", 0.5, {}, 0.1)};

  const auto rested_group = group_selection_by_type(rested);
  const auto fatigued_group = group_selection_by_type(fatigued);

  ASSERT_EQ(rested_group.size(), 1U);
  ASSERT_EQ(fatigued_group.size(), 1U);
  EXPECT_DOUBLE_EQ(rested_group[0].health, 0.75);
  EXPECT_DOUBLE_EQ(fatigued_group[0].health, rested_group[0].health);
  EXPECT_NE(fatigued_group[0].stamina, rested_group[0].stamina);
}

TEST(SelectionGroupingTest, DamageHealingAndSelectionMembershipUpdateHealth) {
  const auto full = group_selection_by_type(
      {unit("spearman", "Spearman", 1.0), unit("spearman", "Spearman", 1.0)});
  const auto damaged = group_selection_by_type(
      {unit("spearman", "Spearman", 0.5), unit("spearman", "Spearman", 1.0)});
  const auto healed = group_selection_by_type(
      {unit("spearman", "Spearman", 0.8), unit("spearman", "Spearman", 1.0)});
  const auto casualty = group_selection_by_type({unit("spearman", "Spearman", 0.8)});

  EXPECT_DOUBLE_EQ(full[0].health, 1.0);
  EXPECT_DOUBLE_EQ(damaged[0].health, 0.75);
  EXPECT_DOUBLE_EQ(healed[0].health, 0.9);
  EXPECT_DOUBLE_EQ(casualty[0].health, 0.8);
  EXPECT_EQ(casualty[0].count, 1);
}

TEST(SelectionGroupingTest, MixedSelectionKeepsHealthAndStaminaIndependentByType) {
  const QVariantList units{unit("spearman", "Spearman", 0.9, {}, 0.1),
                           unit("spearman", "Spearman", 0.7, {}, 0.3),
                           unit("archer", "Archer", 0.4, {}, 0.95),
                           unit("catapult", "Catapult", 0.6, {}, 0.05, false)};

  const auto groups = group_selection_by_type(units);

  ASSERT_EQ(groups.size(), 3U);
  EXPECT_DOUBLE_EQ(groups[0].health, 0.8);
  EXPECT_DOUBLE_EQ(groups[0].stamina, 0.2);
  EXPECT_TRUE(groups[0].can_run);
  EXPECT_DOUBLE_EQ(groups[1].health, 0.4);
  EXPECT_DOUBLE_EQ(groups[1].stamina, 0.95);
  EXPECT_DOUBLE_EQ(groups[2].health, 0.6);
  EXPECT_DOUBLE_EQ(groups[2].stamina, 1.0);
  EXPECT_FALSE(groups[2].can_run);
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
  EXPECT_DOUBLE_EQ(row.value(QStringLiteral("stamina")).toDouble(), 1.0);
  EXPECT_TRUE(row.value(QStringLiteral("canRun")).toBool());
  EXPECT_EQ(row.value(QStringLiteral("activity")).toString(), QStringLiteral("idle"));
  EXPECT_EQ(row.value(QStringLiteral("activityState")).toString(),
            QStringLiteral("active"));
}

TEST(SelectionGroupingTest, AGroupReportsTheActivityMostOfItIsDoing) {
  QVariantList units;
  for (int i = 0; i < 3; ++i) {
    QVariantMap builder = unit("builder", "Builder", 1.0).toMap();
    builder[QStringLiteral("activity")] = QStringLiteral("chop_wood");
    builder[QStringLiteral("activity_state")] = QStringLiteral("active");
    units.append(builder);
  }
  QVariantMap stalled = unit("builder", "Builder", 1.0).toMap();
  stalled[QStringLiteral("activity")] = QStringLiteral("chop_wood");
  stalled[QStringLiteral("activity_state")] = QStringLiteral("interrupted");
  units.append(stalled);

  const auto groups = group_selection_by_type(units);

  ASSERT_EQ(groups.size(), 1U);
  EXPECT_EQ(groups[0].activity, QStringLiteral("chop_wood"));
  EXPECT_EQ(groups[0].activity_state, QStringLiteral("active"));
  EXPECT_EQ(groups[0].activity_count, 3);
  EXPECT_TRUE(groups[0].mixed_activity)
      << "a split crew must be flagged, not silently reported as uniform";
}

TEST(SelectionGroupingTest, AGroupWithOneMindIsNotFlaggedAsMixed) {
  QVariantList units;
  for (int i = 0; i < 2; ++i) {
    QVariantMap miner = unit("builder", "Builder", 1.0).toMap();
    miner[QStringLiteral("activity")] = QStringLiteral("mine_iron");
    miner[QStringLiteral("activity_state")] = QStringLiteral("queued");
    units.append(miner);
  }

  const auto groups = group_selection_by_type(units);

  ASSERT_EQ(groups.size(), 1U);
  EXPECT_EQ(groups[0].activity, QStringLiteral("mine_iron"));
  EXPECT_EQ(groups[0].activity_state, QStringLiteral("queued"));
  EXPECT_FALSE(groups[0].mixed_activity);
}

} // namespace
