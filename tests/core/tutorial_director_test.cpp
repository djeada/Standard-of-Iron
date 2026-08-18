#include <QSet>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <gtest/gtest.h>

#include "game/mission/tutorial_director.h"

using Game::Mission::TutorialDirector;
using Game::Mission::TutorialObservation;
using Game::Mission::TutorialStepId;

namespace {

auto running() -> TutorialObservation {
  TutorialObservation o;
  o.mission_running = true;
  return o;
}

auto complete_with(TutorialDirector& director, const TutorialObservation& o) -> bool {
  director.advance(o, 0.2F);
  const bool completed = director.step_complete();
  director.advance(running(), 10.0F);
  return completed;
}

} // namespace

TEST(TutorialDirectorTest, StartsInactiveAndBeginsOnTheFirstStep) {
  TutorialDirector director;
  EXPECT_FALSE(director.active());
  EXPECT_FALSE(director.finished());
  EXPECT_EQ(TutorialDirector::step_count(), 15);

  director.begin();
  EXPECT_TRUE(director.active());
  EXPECT_EQ(director.step_index(), 0);
  EXPECT_EQ(director.step(), TutorialStepId::SelectTroops);
  EXPECT_FALSE(director.step_complete());
  EXPECT_FALSE(director.title().isEmpty());
  EXPECT_FALSE(director.body().isEmpty());
  EXPECT_FALSE(director.objective().isEmpty());
  EXPECT_EQ(director.objective_state(), QStringLiteral("active"));
}

TEST(TutorialDirectorTest, StepsFollowTheTeachingOrderAndFinishOnVictory) {
  TutorialDirector director;
  director.begin();

  TutorialObservation o = running();
  o.selected_troop_count = 1;
  ASSERT_TRUE(complete_with(director, o));
  EXPECT_EQ(director.step(), TutorialStepId::MoveTroops);

  o = running();
  o.move_order_accepted = true;
  ASSERT_TRUE(complete_with(director, o));
  EXPECT_EQ(director.step(), TutorialStepId::AttackScouts);

  o = running();
  o.enemy_troops_defeated = 5;
  director.advance(o, 0.2F);
  EXPECT_FALSE(director.step_complete());
  o.enemy_troops_defeated = 5 + Game::Mission::k_tutorial_scout_count;
  ASSERT_TRUE(complete_with(director, o));
  EXPECT_EQ(director.step(), TutorialStepId::GatherWood);

  o = running();
  o.harvested_wood = 10;
  director.advance(o, 0.2F);
  EXPECT_FALSE(director.step_complete());
  o.harvested_wood = 10 + Game::Mission::k_tutorial_wood_target;
  ASSERT_TRUE(complete_with(director, o));
  EXPECT_EQ(director.step(), TutorialStepId::GatherStoneAndIron);

  director.advance(running(), 0.2F);
  o = running();
  o.harvested_stone = Game::Mission::k_tutorial_stone_target;
  director.advance(o, 0.2F);
  EXPECT_FALSE(director.step_complete()) << "stone alone is not enough";
  o.harvested_iron = Game::Mission::k_tutorial_iron_target;
  ASSERT_TRUE(complete_with(director, o));
  EXPECT_EQ(director.step(), TutorialStepId::BuildHome);

  director.advance(running(), 0.2F);
  o = running();
  o.home_count = 1;
  ASSERT_TRUE(complete_with(director, o));
  EXPECT_EQ(director.step(), TutorialStepId::RecruitSoldier);

  o = running();
  o.soldier_count = 5;
  director.advance(o, 0.2F);
  EXPECT_FALSE(director.step_complete())
      << "the army the player started with is the baseline";
  o.soldier_count = 6;
  ASSERT_TRUE(complete_with(director, o));
  EXPECT_EQ(director.step(), TutorialStepId::AssembleArmy);

  o = running();
  o.soldier_count = Game::Mission::k_tutorial_army_size;
  ASSERT_TRUE(complete_with(director, o));
  EXPECT_EQ(director.step(), TutorialStepId::DefendCamp);

  director.advance(running(), 0.2F);
  o = running();
  o.waves_cleared = 1;
  ASSERT_TRUE(complete_with(director, o));
  EXPECT_EQ(director.step(), TutorialStepId::Stances);

  o = running();
  o.guard_order_accepted = true;
  ASSERT_TRUE(complete_with(director, o));
  EXPECT_EQ(director.step(), TutorialStepId::Commander);

  o = running();
  o.aura_active = true;
  ASSERT_TRUE(complete_with(director, o));
  EXPECT_EQ(director.step(), TutorialStepId::Camera);

  o = running();
  o.camera_used = true;
  ASSERT_TRUE(complete_with(director, o));
  EXPECT_EQ(director.step(), TutorialStepId::GameSpeed);

  o = running();
  o.speed_changed = true;
  ASSERT_TRUE(complete_with(director, o));
  EXPECT_EQ(director.step(), TutorialStepId::Objectives);

  director.note_objectives_opened();
  ASSERT_TRUE(complete_with(director, running()));
  EXPECT_EQ(director.step(), TutorialStepId::Assault);

  int finished_signals = 0;
  QObject::connect(&director, &TutorialDirector::tutorial_finished, &director, [&] {
    ++finished_signals;
  });
  o = running();
  o.victory = true;
  director.advance(o, 0.2F);
  EXPECT_TRUE(director.finished());
  EXPECT_FALSE(director.active());
  EXPECT_EQ(finished_signals, 1);
}

TEST(TutorialDirectorTest, HoldsTheMissionClockUntilTheDefendStep) {
  TutorialDirector director;
  EXPECT_FALSE(director.holds_mission_clock())
      << "an inactive tutorial never blocks a mission";

  director.begin();
  EXPECT_TRUE(director.holds_mission_clock());

  while (director.step() != TutorialStepId::DefendCamp) {
    EXPECT_TRUE(director.holds_mission_clock()) << director.step_id().toStdString();
    director.skip_step();
  }
  EXPECT_FALSE(director.holds_mission_clock());

  director.stop();
  EXPECT_FALSE(director.holds_mission_clock());
}

TEST(TutorialDirectorTest, SkipReplayContinueAndRestartMoveTheStepPointer) {
  TutorialDirector director;
  director.begin();

  director.skip_step();
  EXPECT_EQ(director.step(), TutorialStepId::MoveTroops);
  const QVariantList after_skip = director.steps();
  EXPECT_EQ(after_skip.at(0).toMap().value("state").toString(),
            QStringLiteral("complete"));
  EXPECT_EQ(after_skip.at(1).toMap().value("state").toString(),
            QStringLiteral("active"));
  EXPECT_EQ(after_skip.at(2).toMap().value("state").toString(),
            QStringLiteral("pending"));

  TutorialObservation o = running();
  o.move_order_accepted = true;
  director.advance(o, 0.2F);
  ASSERT_TRUE(director.step_complete());
  EXPECT_EQ(director.objective_state(), QStringLiteral("complete"));

  director.continue_step();
  EXPECT_EQ(director.step(), TutorialStepId::AttackScouts);
  EXPECT_FALSE(director.step_complete());

  o = running();
  o.enemy_troops_defeated = 1;
  director.advance(o, 0.2F);
  director.replay_step();
  EXPECT_EQ(director.step(), TutorialStepId::AttackScouts);
  o.enemy_troops_defeated = 2;
  director.advance(o, 0.2F);
  EXPECT_FALSE(director.step_complete())
      << "after a replay the kills before the replay do not count";
  o.enemy_troops_defeated = 2 + Game::Mission::k_tutorial_scout_count;
  director.advance(o, 0.2F);
  EXPECT_TRUE(director.step_complete());

  director.restart();
  EXPECT_TRUE(director.active());
  EXPECT_EQ(director.step_index(), 0);
  EXPECT_FALSE(director.step_complete());
  for (const QVariant& entry : director.steps()) {
    EXPECT_NE(entry.toMap().value("state").toString(), QStringLiteral("complete"));
  }
}

TEST(TutorialDirectorTest, CompletedStepAdvancesOnItsOwnAfterTheHold) {
  TutorialDirector director;
  director.begin();
  TutorialObservation o = running();
  o.selected_troop_count = 2;
  director.advance(o, 0.2F);
  ASSERT_TRUE(director.step_complete());

  director.advance(running(), 1.0F);
  EXPECT_EQ(director.step(), TutorialStepId::SelectTroops) << "still showing the tick";
  director.advance(running(), 5.0F);
  EXPECT_EQ(director.step(), TutorialStepId::MoveTroops);
}

TEST(TutorialDirectorTest, HintsSayWhyTheOrderCannotBeGiven) {
  TutorialDirector director;
  director.begin();
  director.skip_step();
  ASSERT_EQ(director.step(), TutorialStepId::MoveTroops);

  director.advance(running(), 0.2F);
  EXPECT_FALSE(director.hint().isEmpty())
      << "no selection: the move order goes to nobody";

  TutorialObservation o = running();
  o.selected_builder_count = 1;
  director.advance(o, 0.2F);
  EXPECT_TRUE(director.hint().contains(QStringLiteral("Builders")));

  o = running();
  o.selected_troop_count = 1;
  director.advance(o, 0.2F);
  EXPECT_TRUE(director.hint().isEmpty()) << "nothing is blocking a selected soldier";

  o = running();
  o.selected_troop_count = 1;
  o.last_rejection_reason = QStringLiteral("No ground under the cursor");
  director.advance(o, 0.2F);
  EXPECT_EQ(director.hint(), QStringLiteral("No ground under the cursor"))
      << "the engine's own rejection reason is surfaced verbatim";

  while (director.step() != TutorialStepId::BuildHome) {
    director.skip_step();
  }
  o = running();
  o.selected_builder_count = 1;
  o.wood = 5;
  o.stone = 0;
  director.advance(o, 0.2F);
  EXPECT_TRUE(director.hint().contains(QStringLiteral("wood")))
      << "a short purse is explained in the hint";
  o.wood = 500;
  o.stone = 500;
  o.construction_preview_active = true;
  o.construction_preview_valid = false;
  director.advance(o, 0.2F);
  EXPECT_TRUE(director.hint().contains(QStringLiteral("blocked")));

  while (director.step() != TutorialStepId::RecruitSoldier) {
    director.skip_step();
  }
  o = running();
  director.advance(o, 0.2F);
  EXPECT_TRUE(director.hint().contains(QStringLiteral("barracks")));
  o.selected_barracks_count = 1;
  o.barracks_manpower = 10;
  director.advance(o, 0.2F);
  EXPECT_TRUE(director.hint().contains(QStringLiteral("population")));

  while (director.step() != TutorialStepId::Commander) {
    director.skip_step();
  }
  o = running();
  director.advance(o, 0.2F);
  EXPECT_TRUE(director.hint().contains(QStringLiteral("commander")));
  o.commander_selected = true;
  o.aura_ready = false;
  director.advance(o, 0.2F);
  EXPECT_TRUE(director.hint().contains(QStringLiteral("recharging")));
}

TEST(TutorialDirectorTest, DefeatStopsTheTutorialAndEndClearsIt) {
  TutorialDirector director;
  director.begin();
  director.skip_step();

  TutorialObservation o = running();
  o.defeat = true;
  director.advance(o, 0.2F);
  EXPECT_FALSE(director.active());
  EXPECT_FALSE(director.finished());

  director.begin();
  director.skip_step();
  director.end();
  EXPECT_FALSE(director.active());
  EXPECT_EQ(director.step_index(), 0);
  EXPECT_TRUE(director.hint().isEmpty());
}

TEST(TutorialDirectorTest, EveryStepHasTextAndAUniqueId) {
  QSet<QString> ids;
  const QVariantList steps = TutorialDirector().steps();
  ASSERT_EQ(steps.size(), TutorialDirector::step_count());
  for (const QVariant& entry : steps) {
    const QVariantMap step = entry.toMap();
    const QString id = step.value("id").toString();
    EXPECT_FALSE(id.isEmpty());
    EXPECT_FALSE(ids.contains(id)) << id.toStdString();
    ids.insert(id);
    EXPECT_FALSE(step.value("title").toString().isEmpty()) << id.toStdString();
    EXPECT_FALSE(step.value("objective").toString().isEmpty()) << id.toStdString();
    EXPECT_EQ(step.value("state").toString(), QStringLiteral("pending"));
  }
}

TEST(TutorialDirectorTest, StartAsksTheEngineToLaunchTheMission) {
  TutorialDirector director;
  int requests = 0;
  QObject::connect(
      &director, &TutorialDirector::start_requested, &director, [&] { ++requests; });
  director.start();
  EXPECT_EQ(requests, 1);
  EXPECT_FALSE(director.active())
      << "the director only becomes active once the mission has loaded";
}

TEST(TutorialDirectorTest, EveryStepPointsAtSomethingUntilItIsDone) {
  TutorialDirector director;
  director.begin();

  QSet<QString> unpointed;
  for (int i = 0; i < TutorialDirector::step_count(); ++i) {
    director.advance(running(), 0.2F);
    const bool points_somewhere = !director.focus_actions().isEmpty() ||
                                  !director.focus_region().isEmpty() ||
                                  !director.focus_target().isEmpty();
    if (!points_somewhere) {
      unpointed.insert(director.step_id());
    }
    director.skip_step();
  }

  EXPECT_TRUE(unpointed.isEmpty())
      << "a step with nothing to point at leaves a new player hunting: "
      << unpointed.values().join(QStringLiteral(", ")).toStdString();
}

TEST(TutorialDirectorTest, FocusFollowsWhatTheStepStillNeeds) {
  TutorialDirector director;
  director.begin();

  director.advance(running(), 0.2F);
  EXPECT_EQ(director.focus_target(), QStringLiteral("own_troops"))
      << "with nothing selected, the ring belongs on the troops themselves";

  while (director.step() != TutorialStepId::GatherWood) {
    director.skip_step();
  }

  TutorialObservation o = running();
  director.advance(o, 0.2F);
  EXPECT_EQ(director.focus_target(), QStringLiteral("builders"))
      << "no builder selected: point at the builders, not at the trees";
  EXPECT_TRUE(director.focus_actions().isEmpty());

  o.selected_builder_count = 1;
  director.advance(o, 0.2F);
  EXPECT_EQ(director.focus_target(), QStringLiteral("timber"));
  EXPECT_TRUE(director.focus_actions().contains(QStringLiteral("collect")));

  while (director.step() != TutorialStepId::RecruitSoldier) {
    director.skip_step();
  }
  o = running();
  o.selected_barracks_count = 1;
  director.advance(o, 0.2F);
  EXPECT_EQ(director.focus_region(), QStringLiteral("production"));

  while (director.step() != TutorialStepId::Commander) {
    director.skip_step();
  }
  o = running();
  director.advance(o, 0.2F);
  EXPECT_EQ(director.focus_target(), QStringLiteral("commander"));
  o.commander_selected = true;
  director.advance(o, 0.2F);
  EXPECT_EQ(director.focus_actions(), QStringList{QStringLiteral("aura")});
  EXPECT_TRUE(director.focus_target().isEmpty());
}

TEST(TutorialDirectorTest, FocusPointsAreDroppedWhenTheTargetChanges) {
  TutorialDirector director;
  director.begin();
  director.advance(running(), 0.2F);
  ASSERT_EQ(director.focus_target(), QStringLiteral("own_troops"));

  QVariantMap point;
  point["world_x"] = 12.0;
  point["world_z"] = 34.0;
  director.set_focus_points(QVariantList{point});
  EXPECT_TRUE(director.has_focus_point());

  TutorialObservation o = running();
  o.selected_troop_count = 1;
  director.advance(o, 0.2F);
  director.advance(running(), 10.0F);
  ASSERT_EQ(director.step(), TutorialStepId::MoveTroops);
  EXPECT_FALSE(director.has_focus_point())
      << "stale world rings must not survive the step they belonged to";
}

TEST(TutorialDirectorTest, CompletedStepStopsPointing) {
  TutorialDirector director;
  director.begin();

  TutorialObservation o = running();
  o.selected_troop_count = 1;
  director.advance(o, 0.2F);
  ASSERT_TRUE(director.step_complete());
  EXPECT_TRUE(director.focus_actions().isEmpty());
  EXPECT_TRUE(director.focus_region().isEmpty());
  EXPECT_TRUE(director.focus_target().isEmpty());
}
