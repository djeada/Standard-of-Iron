#include <QSettings>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include "app/core/user_settings.h"
#include "ui/hints.h"

namespace {

namespace UserSettings = App::Core::UserSettings;

const QString k_formation = QStringLiteral("formation_readout");
const QString k_legend = QStringLiteral("camera_legend");
const QString k_coach = QStringLiteral("economy_coach");

class UiHintsTest : public ::testing::Test {
protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.isValid());
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temp_dir_.path());
    UserSettings::clear();
    UiHints::instance()->restore_all();
    UiHints::instance()->dismiss_all();
  }

  void TearDown() override {
    UiHints::instance()->restore_all();
    UiHints::instance()->dismiss_all();
    UserSettings::clear();
  }

  QTemporaryDir temp_dir_;
};

TEST_F(UiHintsTest, AHintStaysDownUntilSomethingArmsIt) {
  auto* hints = UiHints::instance();

  EXPECT_FALSE(hints->is_showing(k_formation))
      << "a hint must not be on screen before its trigger fires";

  hints->show(k_formation);

  EXPECT_TRUE(hints->is_showing(k_formation));
}

TEST_F(UiHintsTest, ClosingAHintLeavesItAvailableForTheNextTrigger) {
  auto* hints = UiHints::instance();
  hints->show(k_formation);

  hints->dismiss(k_formation);

  EXPECT_FALSE(hints->is_showing(k_formation));
  EXPECT_TRUE(hints->is_enabled(k_formation))
      << "closing one showing is not the same as turning the hint off";

  hints->show(k_formation);
  EXPECT_TRUE(hints->is_showing(k_formation));
}

TEST_F(UiHintsTest, SuppressingAHintOutlivesTheSession) {
  auto* hints = UiHints::instance();

  hints->suppress(k_formation);

  EXPECT_FALSE(hints->is_showing(k_formation));
  EXPECT_FALSE(hints->is_enabled(k_formation));
  EXPECT_FALSE(UserSettings::load_ui_formation_hints())
      << "never-show-again must reach the settings file";

  hints->show(k_formation);
  EXPECT_FALSE(hints->is_showing(k_formation))
      << "a suppressed hint must ignore its trigger";
}

TEST_F(UiHintsTest, ASelectionChangeOnlyClosesSelectionScopedHints) {
  auto* hints = UiHints::instance();
  hints->show(k_formation);
  hints->show(k_coach);
  ASSERT_TRUE(hints->is_showing(k_formation));
  ASSERT_TRUE(hints->is_showing(k_coach));

  hints->on_selection_changed();

  EXPECT_FALSE(hints->is_showing(k_formation))
      << "the formation readout describes the selection that was ordered";
  EXPECT_TRUE(hints->is_showing(k_coach))
      << "the economy prompts have nothing to do with the selection";
}

TEST_F(UiHintsTest, AShowOnceHintIsOfferedExactlyOnce) {
  auto* hints = UiHints::instance();

  hints->show_once(k_legend);
  EXPECT_TRUE(hints->is_showing(k_legend));
  EXPECT_FALSE(hints->is_enabled(k_legend));
  EXPECT_TRUE(UserSettings::load_ui_camera_legend_seen())
      << "the legend records that it has been seen";

  hints->dismiss(k_legend);
  hints->show_once(k_legend);
  EXPECT_FALSE(hints->is_showing(k_legend));
}

TEST_F(UiHintsTest, TheUserCanStillOpenAHintTheyTurnedOff) {
  auto* hints = UiHints::instance();
  hints->suppress(k_legend);

  hints->reveal(k_legend);

  EXPECT_TRUE(hints->is_showing(k_legend))
      << "asking for a hint by hand must work even when it never opens itself";
}

TEST_F(UiHintsTest, EveryChangeNotifiesTheBindingsOnce) {
  auto* hints = UiHints::instance();
  int notifications = 0;
  QObject context;
  QObject::connect(
      hints, &UiHints::changed, &context, [&notifications]() { ++notifications; });

  hints->show(k_formation);
  hints->show(k_formation);
  hints->dismiss(k_formation);
  hints->dismiss(k_formation);

  EXPECT_EQ(notifications, 2) << "a no-op must not churn every hint binding";
}

TEST_F(UiHintsTest, TheCatalogueCarriesEveryHintTheSettingsPanelLists) {
  const auto catalog = UiHints::instance()->catalog();

  QStringList ids;
  for (const auto& entry : catalog) {
    ids.append(entry.toMap().value(QStringLiteral("id")).toString());
  }

  EXPECT_TRUE(ids.contains(k_formation));
  EXPECT_TRUE(ids.contains(k_legend));
  EXPECT_TRUE(ids.contains(k_coach));
}

} // namespace
