#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QTranslator>
#include <Qt>

#include <gtest/gtest.h>

#include "app/core/user_settings.h"
#include "ui/input_bindings.h"

namespace {

namespace UserSettings = App::Core::UserSettings;

class InputBindingsTest : public ::testing::Test {
protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.isValid());
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temp_dir_.path());
    UserSettings::clear();
    InputBindings::instance()->reset_to_defaults();
  }

  void TearDown() override {
    InputBindings::instance()->reset_to_defaults();
    UserSettings::clear();
  }

  QTemporaryDir temp_dir_;
};

TEST_F(InputBindingsTest, EveryCatalogedCommandShipsWithAReadableDefault) {
  ASSERT_FALSE(InputBindings::catalog().isEmpty());

  for (const auto& spec : InputBindings::catalog()) {
    EXPECT_FALSE(spec.id.isEmpty());
    EXPECT_FALSE(spec.name.isEmpty()) << spec.id.toStdString();
    EXPECT_FALSE(spec.category.isEmpty()) << spec.id.toStdString();

    const auto chord = InputBindings::parse(spec.default_shortcut);
    EXPECT_TRUE(chord.is_valid())
        << spec.id.toStdString() << " default " << spec.default_shortcut.toStdString();

    EXPECT_EQ(InputBindings::format(chord), spec.default_shortcut)
        << spec.id.toStdString();

    if (spec.default_alternate.isEmpty()) {
      continue;
    }
    const auto alternate = InputBindings::parse(spec.default_alternate);
    EXPECT_TRUE(alternate.is_valid()) << spec.id.toStdString() << " alternate "
                                      << spec.default_alternate.toStdString();
    EXPECT_EQ(InputBindings::format(alternate), spec.default_alternate)
        << spec.id.toStdString();
  }
}

TEST_F(InputBindingsTest, ActionIdsAreUnique) {
  QSet<QString> seen;
  for (const auto& spec : InputBindings::catalog()) {
    EXPECT_FALSE(seen.contains(spec.id)) << spec.id.toStdString();
    seen.insert(spec.id);
  }
}

TEST_F(InputBindingsTest, ShippedDefaultsAreFreeOfConflicts) {
  EXPECT_FALSE(InputBindings::instance()->has_conflicts());
}

TEST_F(InputBindingsTest, ChordsRoundTripThroughPortableText) {
  struct Case {
    int key;
    int modifiers;
    const char* expected;
  };
  const Case cases[] = {
      {Qt::Key_S, Qt::NoModifier, "S"},
      {Qt::Key_S, Qt::ControlModifier, "Ctrl+S"},
      {Qt::Key_Up, Qt::NoModifier, "Up"},
      {Qt::Key_F5, Qt::NoModifier, "F5"},
      {Qt::Key_Space, Qt::NoModifier, "Space"},
      {Qt::Key_Shift, Qt::NoModifier, "Shift"},
      {Qt::Key_Alt, Qt::NoModifier, "Alt"},
  };

  for (const auto& test_case : cases) {
    const QString encoded =
        InputBindings::encode_key(test_case.key, test_case.modifiers);
    EXPECT_EQ(encoded, QString::fromLatin1(test_case.expected));

    const auto chord = InputBindings::parse(encoded);
    EXPECT_EQ(chord.key, test_case.key) << test_case.expected;
    EXPECT_EQ(chord.modifiers, test_case.modifiers) << test_case.expected;
  }
}

TEST_F(InputBindingsTest, MouseChordsRoundTrip) {
  const QString encoded =
      InputBindings::encode_mouse(Qt::RightButton, Qt::ControlModifier);
  EXPECT_EQ(encoded, QStringLiteral("Ctrl+Mouse Right"));

  const auto chord = InputBindings::parse(encoded);
  EXPECT_EQ(chord.mouse_button, Qt::RightButton);
  EXPECT_EQ(chord.modifiers, Qt::ControlModifier);
  EXPECT_EQ(chord.key, 0);
}

TEST_F(InputBindingsTest, MouseLabelsAreTranslatedForDisplayButStoredInEnglish) {
  QTranslator translator;
  ASSERT_TRUE(translator.load(QStringLiteral(":/translations/app_de.qm")));
  ASSERT_TRUE(QCoreApplication::installTranslator(&translator));

  const QString stored = InputBindings::encode_mouse(Qt::LeftButton, Qt::NoModifier);
  EXPECT_EQ(stored, QStringLiteral("Mouse Left"));
  EXPECT_NE(InputBindings::describe(stored), stored);
  EXPECT_EQ(InputBindings::parse(stored).mouse_button, Qt::LeftButton);

  const QString modified =
      InputBindings::encode_mouse(Qt::RightButton, Qt::ControlModifier);
  EXPECT_EQ(modified, QStringLiteral("Ctrl+Mouse Right"));
  EXPECT_TRUE(InputBindings::describe(modified).startsWith(QStringLiteral("Ctrl+")));
  EXPECT_NE(InputBindings::describe(modified), modified);

  QCoreApplication::removeTranslator(&translator);
}

TEST_F(InputBindingsTest, ABareModifierBindsToItselfRatherThanQualifyingNothing) {

  EXPECT_EQ(InputBindings::encode_key(Qt::Key_Shift, Qt::ShiftModifier),
            QStringLiteral("Shift"));
  EXPECT_TRUE(InputBindings::is_modifier_key(Qt::Key_Shift));
  EXPECT_FALSE(InputBindings::is_modifier_key(Qt::Key_S));
}

TEST_F(InputBindingsTest, TakingAnotherCommandsChordIsReportedAsAConflict) {
  auto* bindings = InputBindings::instance();

  const QStringList conflicts =
      bindings->conflicts_for(QStringLiteral("rts.order_stop"), QStringLiteral("C"));

  EXPECT_TRUE(conflicts.contains(QStringLiteral("rts.order_attack")));
  EXPECT_FALSE(bindings->assign(QStringLiteral("rts.order_stop"), QStringLiteral("C")));

  EXPECT_EQ(bindings->shortcut_for(QStringLiteral("rts.order_stop")),
            QStringLiteral("Z"));
  EXPECT_EQ(bindings->shortcut_for(QStringLiteral("rts.order_attack")),
            QStringLiteral("C"));
}

TEST_F(InputBindingsTest, CommandsInSeparateContextsMayShareAChord) {
  auto* bindings = InputBindings::instance();

  EXPECT_EQ(bindings->shortcut_for(QStringLiteral("rts.pause")),
            QStringLiteral("Space"));
  EXPECT_EQ(bindings->shortcut_for(QStringLiteral("commander.dodge")),
            QStringLiteral("Space"));
  EXPECT_TRUE(
      bindings->conflicts_for(QStringLiteral("rts.pause"), QStringLiteral("Space"))
          .isEmpty());
}

TEST_F(InputBindingsTest, AGlobalCommandConflictsWithBothGameplayContexts) {
  auto* bindings = InputBindings::instance();

  const QStringList conflicts =
      bindings->conflicts_for(QStringLiteral("global.quicksave"), QStringLiteral("X"));

  EXPECT_TRUE(conflicts.contains(QStringLiteral("rts.select_all_troops")));
}

TEST_F(InputBindingsTest, OverridingAConflictUnbindsThePreviousHolder) {
  auto* bindings = InputBindings::instance();

  bindings->assign_overriding(QStringLiteral("rts.order_stop"), QStringLiteral("C"));

  EXPECT_EQ(bindings->shortcut_for(QStringLiteral("rts.order_stop")),
            QStringLiteral("C"));
  EXPECT_TRUE(bindings->shortcut_for(QStringLiteral("rts.order_attack")).isEmpty());
  EXPECT_FALSE(bindings->has_conflicts());
}

TEST_F(InputBindingsTest, RebindingSurvivesAReload) {
  auto* bindings = InputBindings::instance();
  ASSERT_TRUE(
      bindings->assign(QStringLiteral("rts.order_stop"), QStringLiteral("Ctrl+T")));

  EXPECT_EQ(UserSettings::load_input_binding(QStringLiteral("rts.order_stop")),
            QStringLiteral("Ctrl+T"));

  bindings->reset_action(QStringLiteral("rts.order_stop"));
  EXPECT_EQ(bindings->shortcut_for(QStringLiteral("rts.order_stop")),
            QStringLiteral("Z"));
  EXPECT_TRUE(
      UserSettings::load_input_binding(QStringLiteral("rts.order_stop")).isEmpty());
}

TEST_F(InputBindingsTest, UnbindingIsRememberedSeparatelyFromNeverHavingChanged) {
  auto* bindings = InputBindings::instance();

  bindings->clear_binding(QStringLiteral("rts.order_hold"));

  EXPECT_TRUE(bindings->shortcut_for(QStringLiteral("rts.order_hold")).isEmpty());
  EXPECT_EQ(UserSettings::load_input_binding(QStringLiteral("rts.order_hold")),
            QStringLiteral("None"));
  EXPECT_TRUE(bindings
                  ->actions_for_key(Qt::Key_H,
                                    Qt::NoModifier,
                                    QString::fromLatin1(InputBindings::kContextRts))
                  .isEmpty());
}

TEST_F(InputBindingsTest, KeyResolutionHonoursTheActiveContext) {
  auto* bindings = InputBindings::instance();

  const auto rts = bindings->actions_for_key(
      Qt::Key_Space, Qt::NoModifier, QString::fromLatin1(InputBindings::kContextRts));
  EXPECT_EQ(rts, QStringList{QStringLiteral("rts.pause")});

  const auto commander =
      bindings->actions_for_key(Qt::Key_Space,
                                Qt::NoModifier,
                                QString::fromLatin1(InputBindings::kContextCommander));
  EXPECT_EQ(commander, QStringList{QStringLiteral("commander.dodge")});
}

TEST_F(InputBindingsTest, GlobalCommandsResolveFromEveryContext) {
  auto* bindings = InputBindings::instance();

  for (const char* context : {InputBindings::kContextRts,
                              InputBindings::kContextCommander,
                              InputBindings::kContextGlobal}) {
    EXPECT_TRUE(
        bindings
            ->actions_for_key(Qt::Key_F5, Qt::NoModifier, QString::fromLatin1(context))
            .contains(QStringLiteral("global.quicksave")))
        << context;
  }
}

TEST_F(InputBindingsTest, AnUnmodifiedBindingStillMatchesWhileShiftIsHeld) {
  auto* bindings = InputBindings::instance();

  const auto panned = bindings->actions_for_key(
      Qt::Key_Up, Qt::ShiftModifier, QString::fromLatin1(InputBindings::kContextRts));

  EXPECT_EQ(panned, QStringList{QStringLiteral("rts.camera_pan_up")});
}

TEST_F(InputBindingsTest, AnExactModifierMatchBeatsTheUnmodifiedFallback) {
  auto* bindings = InputBindings::instance();
  ASSERT_TRUE(
      bindings->assign(QStringLiteral("rts.order_hold"), QStringLiteral("Ctrl+S")));

  const auto resolved = bindings->actions_for_key(
      Qt::Key_S, Qt::ControlModifier, QString::fromLatin1(InputBindings::kContextRts));

  EXPECT_EQ(resolved, QStringList{QStringLiteral("rts.order_hold")});
}

TEST_F(InputBindingsTest, TheRallyFlagNoLongerHasToShareItsKeyWithTheCamera) {
  auto* bindings = InputBindings::instance();

  const auto resolved = bindings->actions_for_key(
      Qt::Key_R, Qt::NoModifier, QString::fromLatin1(InputBindings::kContextRts));

  ASSERT_EQ(resolved.size(), 1);
  EXPECT_EQ(resolved.at(0), QStringLiteral("rts.commander_rally"));
}

TEST_F(InputBindingsTest, ContextualCommandsLayerOverTheGeneralOneTheyShareAKeyWith) {
  auto* bindings = InputBindings::instance();
  bindings->assign_overriding(QStringLiteral("rts.camera_tilt_up"),
                              QStringLiteral("R"));

  const auto resolved = bindings->actions_for_key(
      Qt::Key_R, Qt::NoModifier, QString::fromLatin1(InputBindings::kContextRts));

  ASSERT_EQ(resolved.size(), 2);
  EXPECT_EQ(resolved.at(0), QStringLiteral("rts.commander_rally"));
  EXPECT_EQ(resolved.at(1), QStringLiteral("rts.camera_tilt_up"));
}

TEST_F(InputBindingsTest, MouseResolutionHonoursTheActiveContext) {
  auto* bindings = InputBindings::instance();

  EXPECT_TRUE(bindings
                  ->actions_for_mouse(Qt::RightButton,
                                      Qt::NoModifier,
                                      QString::fromLatin1(InputBindings::kContextRts))
                  .contains(QStringLiteral("rts.command")));
  EXPECT_TRUE(
      bindings
          ->actions_for_mouse(Qt::LeftButton,
                              Qt::NoModifier,
                              QString::fromLatin1(InputBindings::kContextCommander))
          .contains(QStringLiteral("commander.primary_action")));
}

TEST_F(InputBindingsTest, RebindingCommanderMovementKeepsTheEngineFacingKeyCode) {
  auto* bindings = InputBindings::instance();
  ASSERT_TRUE(
      bindings->assign(QStringLiteral("commander.move_forward"), QStringLiteral("Up")));

  const auto resolved =
      bindings->actions_for_key(Qt::Key_Up,
                                Qt::NoModifier,
                                QString::fromLatin1(InputBindings::kContextCommander));
  ASSERT_EQ(resolved, QStringList{QStringLiteral("commander.move_forward")});

  EXPECT_EQ(bindings->canonical_key_for(QStringLiteral("commander.move_forward")),
            static_cast<int>(Qt::Key_W));
}

TEST_F(InputBindingsTest, AssigningTheDefaultBackDropsTheStoredOverride) {
  auto* bindings = InputBindings::instance();
  ASSERT_TRUE(
      bindings->assign(QStringLiteral("rts.order_hold"), QStringLiteral("Ctrl+H")));
  EXPECT_FALSE(bindings->is_default());

  ASSERT_TRUE(bindings->assign(QStringLiteral("rts.order_hold"), QStringLiteral("H")));

  EXPECT_TRUE(bindings->is_default());
  EXPECT_TRUE(
      UserSettings::load_input_binding(QStringLiteral("rts.order_hold")).isEmpty());
}

TEST_F(InputBindingsTest, GarbageInTheSettingsFileIsIgnored) {
  UserSettings::save_input_binding(QStringLiteral("rts.order_stop"),
                                   QStringLiteral("not-a-key"));

  EXPECT_FALSE(InputBindings::parse(QStringLiteral("not-a-key")).is_valid());
  EXPECT_FALSE(InputBindings::parse(QStringLiteral("Mouse Nowhere")).is_valid());
  EXPECT_FALSE(InputBindings::parse(QString()).is_valid());
}

TEST_F(InputBindingsTest, UnknownActionIdsAreRejectedRatherThanStored) {
  auto* bindings = InputBindings::instance();

  EXPECT_FALSE(bindings->assign(QStringLiteral("nope.not.real"), QStringLiteral("Z")));
  EXPECT_TRUE(bindings->shortcut_for(QStringLiteral("nope.not.real")).isEmpty());
  EXPECT_TRUE(bindings->is_default());
}

TEST_F(InputBindingsTest, TheQmlActionListMirrorsTheCatalogAndFlagsConflicts) {
  auto* bindings = InputBindings::instance();
  bindings->assign_overriding(QStringLiteral("rts.order_stop"), QStringLiteral("A"));

  const QVariantList actions = bindings->actions();
  ASSERT_EQ(actions.size(), InputBindings::catalog().size());

  bool saw_stop = false;
  for (const QVariant& entry : actions) {
    const QVariantMap action = entry.toMap();
    if (action[QStringLiteral("id")].toString() != QLatin1String("rts.order_stop")) {
      continue;
    }
    saw_stop = true;
    EXPECT_EQ(action[QStringLiteral("shortcut")].toString(), QStringLiteral("A"));
    EXPECT_FALSE(action[QStringLiteral("isDefault")].toBool());
    EXPECT_FALSE(action[QStringLiteral("displayShortcut")].toString().isEmpty());
  }
  EXPECT_TRUE(saw_stop);
}

TEST_F(InputBindingsTest, TheFormationOrderOwnsTheKeyItIsDocumentedWith) {
  auto* bindings = InputBindings::instance();

  const auto resolved = bindings->actions_for_key(
      Qt::Key_F, Qt::NoModifier, QString::fromLatin1(InputBindings::kContextRts));

  ASSERT_EQ(resolved.size(), 1);
  EXPECT_EQ(resolved.at(0), QStringLiteral("rts.order_formation"));
  EXPECT_EQ(bindings->display_shortcut_for(QStringLiteral("rts.order_formation")),
            QStringLiteral("F"));
}

TEST_F(InputBindingsTest, BattleSpeedIsReachableFromTheKeyboardInBothDirections) {
  auto* bindings = InputBindings::instance();
  const QString rts = QString::fromLatin1(InputBindings::kContextRts);

  EXPECT_EQ(bindings->display_shortcut_for(QStringLiteral("rts.speed_up")),
            QStringLiteral("+"));
  EXPECT_EQ(bindings->display_shortcut_for(QStringLiteral("rts.speed_down")),
            QStringLiteral("-"));

  const auto faster = bindings->actions_for_key(Qt::Key_Plus, Qt::NoModifier, rts);
  ASSERT_EQ(faster.size(), 1);
  EXPECT_EQ(faster.at(0), QStringLiteral("rts.speed_up"));

  const auto slower = bindings->actions_for_key(Qt::Key_Minus, Qt::NoModifier, rts);
  ASSERT_EQ(slower.size(), 1);
  EXPECT_EQ(slower.at(0), QStringLiteral("rts.speed_down"));

  const auto shifted = bindings->actions_for_key(Qt::Key_Plus, Qt::ShiftModifier, rts);
  ASSERT_EQ(shifted.size(), 1);
  EXPECT_EQ(shifted.at(0), QStringLiteral("rts.speed_up"));
}

TEST_F(InputBindingsTest, PanningAnswersToBothTheArrowsAndWasd) {
  auto* bindings = InputBindings::instance();
  const QString rts = QString::fromLatin1(InputBindings::kContextRts);

  struct Case {
    const char* action;
    Qt::Key arrow;
    Qt::Key letter;
  };
  const Case cases[] = {{"rts.camera_pan_up", Qt::Key_Up, Qt::Key_W},
                        {"rts.camera_pan_down", Qt::Key_Down, Qt::Key_S},
                        {"rts.camera_pan_left", Qt::Key_Left, Qt::Key_A},
                        {"rts.camera_pan_right", Qt::Key_Right, Qt::Key_D}};

  for (const auto& test_case : cases) {
    const QString action = QString::fromLatin1(test_case.action);
    EXPECT_EQ(bindings->actions_for_key(test_case.arrow, Qt::NoModifier, rts),
              QStringList{action})
        << test_case.action;
    EXPECT_EQ(bindings->actions_for_key(test_case.letter, Qt::NoModifier, rts),
              QStringList{action})
        << test_case.action;
  }
}

TEST_F(InputBindingsTest, TheOrderHotkeysWasdDisplacedMovedRatherThanVanished) {
  auto* bindings = InputBindings::instance();
  const QString rts = QString::fromLatin1(InputBindings::kContextRts);

  EXPECT_EQ(bindings->actions_for_key(Qt::Key_C, Qt::NoModifier, rts),
            QStringList{QStringLiteral("rts.order_attack")});
  EXPECT_EQ(bindings->actions_for_key(Qt::Key_Z, Qt::NoModifier, rts),
            QStringList{QStringLiteral("rts.order_stop")});
}

TEST_F(InputBindingsTest, AnAlternateIsRemappedAndResetIndependentlyOfThePrimary) {
  auto* bindings = InputBindings::instance();
  const QString pan_up = QStringLiteral("rts.camera_pan_up");

  ASSERT_TRUE(bindings->assign(pan_up, QStringLiteral("I"), InputBindings::Alternate));

  EXPECT_EQ(bindings->shortcut_for(pan_up), QStringLiteral("Up"));
  EXPECT_EQ(bindings->shortcut_for(pan_up, InputBindings::Alternate),
            QStringLiteral("I"));
  EXPECT_EQ(UserSettings::load_input_binding(pan_up + QStringLiteral("|alt")),
            QStringLiteral("I"));
  EXPECT_TRUE(UserSettings::load_input_binding(pan_up).isEmpty());

  const QString rts = QString::fromLatin1(InputBindings::kContextRts);
  EXPECT_EQ(bindings->actions_for_key(Qt::Key_I, Qt::NoModifier, rts),
            QStringList{pan_up});
  EXPECT_TRUE(bindings->actions_for_key(Qt::Key_W, Qt::NoModifier, rts).isEmpty());

  bindings->reset_action(pan_up);
  EXPECT_EQ(bindings->shortcut_for(pan_up, InputBindings::Alternate),
            QStringLiteral("W"));
  EXPECT_TRUE(bindings->is_default());
}

TEST_F(InputBindingsTest, AnAlternateCollidingWithAnotherCommandIsAConflict) {
  auto* bindings = InputBindings::instance();

  const QStringList conflicts = bindings->conflicts_for(
      QStringLiteral("rts.order_move"), QStringLiteral("W"), InputBindings::Primary);

  EXPECT_TRUE(conflicts.contains(QStringLiteral("rts.camera_pan_up")));
  EXPECT_FALSE(bindings->assign(QStringLiteral("rts.order_move"), QStringLiteral("W")));

  bindings->assign_overriding(QStringLiteral("rts.order_move"), QStringLiteral("W"));
  EXPECT_TRUE(
      bindings
          ->shortcut_for(QStringLiteral("rts.camera_pan_up"), InputBindings::Alternate)
          .isEmpty());
  EXPECT_EQ(bindings->shortcut_for(QStringLiteral("rts.camera_pan_up")),
            QStringLiteral("Up"));
  EXPECT_FALSE(bindings->has_conflicts());
}

TEST_F(InputBindingsTest, ACommandCannotHoldTheSameChordInBothSlots) {
  auto* bindings = InputBindings::instance();
  const QString pan_up = QStringLiteral("rts.camera_pan_up");

  EXPECT_EQ(
      bindings->conflicts_for(pan_up, QStringLiteral("Up"), InputBindings::Alternate),
      QStringList{pan_up});
  EXPECT_FALSE(
      bindings->assign(pan_up, QStringLiteral("Up"), InputBindings::Alternate));
  EXPECT_EQ(bindings->shortcut_for(pan_up, InputBindings::Alternate),
            QStringLiteral("W"));
}

TEST_F(InputBindingsTest, TheQmlActionListCarriesBothSlots) {
  auto* bindings = InputBindings::instance();

  bool saw_pan = false;
  for (const QVariant& entry : bindings->actions()) {
    const QVariantMap action = entry.toMap();
    if (action[QStringLiteral("id")].toString() !=
        QLatin1String("rts.camera_pan_left")) {
      continue;
    }
    saw_pan = true;
    EXPECT_EQ(action[QStringLiteral("shortcut")].toString(), QStringLiteral("Left"));
    EXPECT_EQ(action[QStringLiteral("alternate")].toString(), QStringLiteral("A"));
    EXPECT_EQ(action[QStringLiteral("defaultAlternate")].toString(),
              QStringLiteral("A"));
    EXPECT_FALSE(action[QStringLiteral("alternateUnbound")].toBool());
    EXPECT_TRUE(action[QStringLiteral("isDefault")].toBool());
    EXPECT_FALSE(action[QStringLiteral("displayAlternate")].toString().isEmpty());
  }
  EXPECT_TRUE(saw_pan);
}

TEST_F(InputBindingsTest, AnAlternateOnlyLeavesTheDefaultStateWhileItIsRebound) {
  auto* bindings = InputBindings::instance();
  const QString pan_down = QStringLiteral("rts.camera_pan_down");

  ASSERT_TRUE(
      bindings->assign(pan_down, QStringLiteral("K"), InputBindings::Alternate));
  EXPECT_FALSE(bindings->is_default());

  ASSERT_TRUE(
      bindings->assign(pan_down, QStringLiteral("S"), InputBindings::Alternate));
  EXPECT_TRUE(bindings->is_default());
  EXPECT_TRUE(
      UserSettings::load_input_binding(pan_down + QStringLiteral("|alt")).isEmpty());
}

TEST_F(InputBindingsTest, TheCameraCategoryNamesWhatEachCommandActuallyDoes) {
  auto* bindings = InputBindings::instance();
  const QString rts = QString::fromLatin1(InputBindings::kContextRts);

  const struct {
    const char* action;
    Qt::Key key;
    int modifiers;
  } cases[] = {{"rts.camera_rotate_left", Qt::Key_Q, Qt::NoModifier},
               {"rts.camera_rotate_right", Qt::Key_E, Qt::NoModifier},
               {"rts.camera_tilt_up", Qt::Key_Up, Qt::ControlModifier},
               {"rts.camera_tilt_down", Qt::Key_Down, Qt::ControlModifier},
               {"rts.camera_zoom_in", Qt::Key_PageUp, Qt::NoModifier},
               {"rts.camera_zoom_out", Qt::Key_PageDown, Qt::NoModifier},
               {"rts.camera_reset", Qt::Key_Home, Qt::NoModifier}};

  for (const auto& test_case : cases) {
    const QString action = QString::fromLatin1(test_case.action);
    EXPECT_EQ(bindings->actions_for_key(test_case.key, test_case.modifiers, rts),
              QStringList{action})
        << test_case.action;
  }

  for (const auto& spec : InputBindings::catalog()) {
    if (!spec.id.startsWith(QLatin1String("rts.camera_"))) {
      continue;
    }
    EXPECT_FALSE(spec.description.isEmpty()) << spec.id.toStdString();
    EXPECT_FALSE(spec.id.contains(QLatin1String("orbit"))) << spec.id.toStdString();
    EXPECT_FALSE(spec.name.contains(QLatin1String("Orbit"))) << spec.id.toStdString();
  }
}

TEST_F(InputBindingsTest, AKeyBoundToTheOldOrbitNameFollowsItToTilt) {
  UserSettings::save_input_binding(QStringLiteral("rts.camera_orbit_left"),
                                   QStringLiteral("B"));
  UserSettings::save_input_binding(QStringLiteral("rts.camera_orbit_right"),
                                   QStringLiteral("N"));

  InputBindings::instance()->reload_from_settings();

  auto* bindings = InputBindings::instance();
  EXPECT_EQ(bindings->shortcut_for(QStringLiteral("rts.camera_tilt_down")),
            QStringLiteral("B"))
      << "orbit left lowered the camera, which is what tilt down does";
  EXPECT_EQ(bindings->shortcut_for(QStringLiteral("rts.camera_tilt_up")),
            QStringLiteral("N"));
  EXPECT_TRUE(UserSettings::load_input_binding(QStringLiteral("rts.camera_orbit_left"))
                  .isEmpty());
}

TEST_F(InputBindingsTest, EveryCommandBarChipResolvesToARealAction) {

  auto* bindings = InputBindings::instance();
  for (const auto* action_id : {"rts.order_attack",
                                "rts.order_guard",
                                "rts.order_patrol",
                                "rts.order_stop",
                                "rts.order_hold",
                                "rts.commander_rally"}) {
    const QString id = QString::fromLatin1(action_id);
    EXPECT_FALSE(bindings->default_shortcut_for(id).isEmpty()) << action_id;
    EXPECT_FALSE(bindings->display_shortcut_for(id).isEmpty()) << action_id;
  }
}

} // namespace
