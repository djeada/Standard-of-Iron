#include <gtest/gtest.h>

#include "app/economy/placement_session.h"
#include "app/economy/wall_placement_session.h"
#include "app/orders/formation_options_model.h"
#include "app/orders/formation_placement.h"

namespace {

using App::Economy::placement_allows;
using App::Economy::placement_phase;
using App::Economy::PlacementEvent;
using App::Economy::PlacementKind;
using App::Economy::PlacementPhase;

TEST(PlacementStateMachine, PhaseFollowsPlacingPreviewAndDrag) {
  EXPECT_EQ(placement_phase(false, false, false), PlacementPhase::Idle);
  EXPECT_EQ(placement_phase(false, true, true), PlacementPhase::Idle);
  EXPECT_EQ(placement_phase(true, false, false), PlacementPhase::Aiming);
  EXPECT_EQ(placement_phase(true, false, true), PlacementPhase::Previewing);
  EXPECT_EQ(placement_phase(true, true, false), PlacementPhase::DraggingWall);
  EXPECT_EQ(placement_phase(true, true, true), PlacementPhase::Previewing);
}

TEST(PlacementStateMachine, IdleAcceptsOnlyStartConfirmAndReset) {
  for (const auto kind : {PlacementKind::Structure, PlacementKind::Wall}) {
    EXPECT_TRUE(placement_allows(PlacementPhase::Idle, kind, PlacementEvent::Start));
    EXPECT_TRUE(placement_allows(PlacementPhase::Idle, kind, PlacementEvent::Confirm));
    EXPECT_TRUE(
        placement_allows(PlacementPhase::Idle, kind, PlacementEvent::MatchReset));
    EXPECT_FALSE(
        placement_allows(PlacementPhase::Idle, kind, PlacementEvent::PointerMotion));
    EXPECT_FALSE(
        placement_allows(PlacementPhase::Idle, kind, PlacementEvent::PointerPress));
    EXPECT_FALSE(
        placement_allows(PlacementPhase::Idle, kind, PlacementEvent::PointerRelease));
    EXPECT_FALSE(placement_allows(PlacementPhase::Idle, kind, PlacementEvent::Rotate));
    EXPECT_FALSE(placement_allows(PlacementPhase::Idle, kind, PlacementEvent::Cancel));
  }
}

TEST(PlacementStateMachine, OnlyWallsTakeAPressAndOnlyAPreviewRotates) {
  EXPECT_FALSE(placement_allows(
      PlacementPhase::Aiming, PlacementKind::Structure, PlacementEvent::PointerPress));
  EXPECT_TRUE(placement_allows(
      PlacementPhase::Aiming, PlacementKind::Wall, PlacementEvent::PointerPress));
  EXPECT_TRUE(placement_allows(
      PlacementPhase::Aiming, PlacementKind::Gate, PlacementEvent::PointerPress));

  EXPECT_FALSE(placement_allows(
      PlacementPhase::Aiming, PlacementKind::Structure, PlacementEvent::Rotate));
  EXPECT_TRUE(placement_allows(
      PlacementPhase::Previewing, PlacementKind::Structure, PlacementEvent::Rotate));
  EXPECT_TRUE(placement_allows(
      PlacementPhase::Previewing, PlacementKind::Wall, PlacementEvent::PointerRelease));
  EXPECT_TRUE(placement_allows(
      PlacementPhase::DraggingWall, PlacementKind::Wall, PlacementEvent::Cancel));
}

TEST(PlacementStateMachine, ItemKindsAreClassifiedByTheirType) {
  using App::Economy::placement_kind_for;
  EXPECT_EQ(placement_kind_for(QStringLiteral("wall_segment")), PlacementKind::Wall);
  EXPECT_EQ(placement_kind_for(QStringLiteral("wall_gate")), PlacementKind::Gate);
  EXPECT_EQ(placement_kind_for(QStringLiteral("barracks")), PlacementKind::Structure);
  EXPECT_EQ(placement_kind_for(QStringLiteral("collect")), PlacementKind::Harvest);
}

TEST(PlacementSession, DirectPlacementRemembersItsOwnerUntilItEnds) {
  App::Economy::PlacementSession session;
  session.begin_direct(
      QStringLiteral("barracks"), 3, Game::Systems::NationID::RomanRepublic);
  session.set_rotation_y(45.0F);
  session.set_targets(7, 9);

  EXPECT_TRUE(session.active());
  EXPECT_TRUE(session.direct());
  EXPECT_EQ(session.owner_id(nullptr), 3);
  EXPECT_FLOAT_EQ(session.effective_rotation_y(), 45.0F);

  session.end();
  EXPECT_FALSE(session.active());
  EXPECT_FALSE(session.direct());
  EXPECT_EQ(session.owner_id(nullptr), 0);
  EXPECT_TRUE(session.construction_type().isEmpty());
  EXPECT_TRUE(session.pending_building_type().isEmpty());
  EXPECT_EQ(session.harvest_target_id(), 0U);
  EXPECT_EQ(session.food_target_id(), 0U);
  EXPECT_FLOAT_EQ(session.rotation_y(), 0.0F);
}

TEST(PlacementSession, OnlyRotatableStructuresUseThePreviewRotation) {
  App::Economy::PlacementSession session;
  session.begin_direct(
      QStringLiteral("wall_segment"), 1, Game::Systems::NationID::RomanRepublic);
  session.set_rotation_y(30.0F);
  EXPECT_FLOAT_EQ(session.effective_rotation_y(), 0.0F);
  EXPECT_TRUE(session.is_wall());
  EXPECT_FALSE(session.is_gate());
}

TEST(PlacementSession, StartingOverAnEarlierPlacementDropsItsState) {
  App::Economy::PlacementSession session;
  session.begin_direct(
      QStringLiteral("barracks"), 1, Game::Systems::NationID::RomanRepublic);
  session.set_targets(4, 5);
  session.begin_direct(
      QStringLiteral("home"), 2, Game::Systems::NationID::RomanRepublic);
  EXPECT_TRUE(session.construction_type() == QStringLiteral("home"));
  EXPECT_EQ(session.owner_id(nullptr), 2);
  EXPECT_EQ(session.harvest_target_id(), 0U);
}

TEST(WallPlacementSession, HoverAnchorsOnlyWhileNotDragging) {
  App::Economy::WallPlacementSession wall;
  wall.hover_at(QVector3D(1.0F, 0.0F, 2.0F));
  EXPECT_TRUE(wall.anchor_set());
  EXPECT_FALSE(wall.drag_active());

  wall.begin_drag(QVector3D(3.0F, 0.0F, 4.0F));
  EXPECT_TRUE(wall.drag_active());

  wall.hover_without_target();
  EXPECT_TRUE(wall.anchor_set()) << "a drag keeps its anchor when the pointer is lost";

  wall.reset();
  EXPECT_FALSE(wall.drag_active());
  EXPECT_FALSE(wall.anchor_set());
  EXPECT_FLOAT_EQ(wall.rotation_y(), 0.0F);
}

TEST(WallPlacementSession, RotationStepsInQuarterTurnsAndWraps) {
  App::Economy::WallPlacementSession wall;
  wall.rotate(1.0F);
  EXPECT_FLOAT_EQ(wall.rotation_y(), 90.0F);
  wall.rotate(3.0F);
  EXPECT_FLOAT_EQ(wall.rotation_y(), 0.0F);
  wall.rotate(-1.0F);
  EXPECT_FLOAT_EQ(wall.rotation_y(), 270.0F);
}

using App::Controllers::FormationPlacement;

TEST(FormationPlacement, AimingSetsAnExplicitFacingTowardThePoint) {
  FormationPlacement placement;
  placement.begin({1, 2}, QVector3D(0.0F, 0.0F, 0.0F), true);
  EXPECT_TRUE(placement.placing());
  EXPECT_TRUE(placement.right_drag());

  EXPECT_FALSE(placement.aim_at(QVector3D(0.0F, 0.0F, 0.05F)))
      << "a point on top of the anchor gives no direction";
  EXPECT_TRUE(placement.aim_at(QVector3D(5.0F, 0.0F, 0.0F)));
  EXPECT_NEAR(placement.facing_degrees(), 90.0F, 0.001F);
  EXPECT_TRUE(placement.facing_explicit());
  EXPECT_NEAR(placement.aim_distance(), 5.0F, 0.001F);
}

TEST(FormationPlacement, AutoFacingYieldsToAnExplicitChoice) {
  FormationPlacement placement;
  placement.begin({1, 2}, QVector3D(), false);
  placement.follow_auto_facing(30.0F);
  EXPECT_NEAR(placement.facing_degrees(), 30.0F, 0.001F);
  placement.set_facing(-90.0F, true);
  placement.follow_auto_facing(30.0F);
  EXPECT_NEAR(placement.facing_degrees(), -90.0F, 0.001F);
  placement.clear_facing_choice();
  placement.follow_auto_facing(30.0F);
  EXPECT_NEAR(placement.facing_degrees(), 30.0F, 0.001F);
}

TEST(FormationPlacement, DraggingAcrossAnArmyMakesFrontageAndCentersTheBlock) {
  FormationPlacement placement;
  placement.begin({1, 2, 3}, QVector3D(), false);
  placement.begin_drag(QVector3D(0.0F, 0.0F, 0.0F));

  const auto step = placement.drag_to(QVector3D(8.0F, 0.0F, 0.0F));
  EXPECT_TRUE(step.frontage_changed);
  EXPECT_FALSE(step.follow_auto_facing);
  EXPECT_NEAR(placement.frontage(), 8.0F, 0.001F);
  EXPECT_NEAR(placement.position().x(), 4.0F, 0.001F);
  EXPECT_TRUE(placement.facing_explicit());

  const auto short_drag = placement.drag_to(QVector3D(0.2F, 0.0F, 0.0F));
  EXPECT_TRUE(short_drag.follow_auto_facing);
  EXPECT_FALSE(short_drag.frontage_changed);
}

TEST(FormationPlacement, DraggingASingleUnitAimsItInsteadOfWideningIt) {
  FormationPlacement placement;
  placement.begin({1}, QVector3D(), false);
  placement.begin_drag(QVector3D(2.0F, 0.0F, 2.0F));

  const auto step = placement.drag_to(QVector3D(2.0F, 0.0F, 6.0F));
  EXPECT_FALSE(step.frontage_changed);
  EXPECT_NEAR(placement.position().z(), 2.0F, 0.001F);
  EXPECT_NEAR(placement.aim_distance(), 4.0F, 0.001F);
  EXPECT_FLOAT_EQ(placement.frontage(), 0.0F);
}

TEST(FormationPlacement, DeploymentKeepsTheGestureForTheCaller) {
  FormationPlacement placement;
  placement.begin({1, 2}, QVector3D(1.0F, 0.0F, 1.0F), true);
  placement.finish_deployment();
  EXPECT_FALSE(placement.placing());
  EXPECT_TRUE(placement.units().empty());
  EXPECT_TRUE(placement.right_drag());
  placement.end_right_drag();
  EXPECT_FALSE(placement.right_drag());
}

using App::Controllers::FormationOptionsModel;

TEST(FormationOptionsModel, PresetsMapToScalesAndBackToIndices) {
  FormationOptionsModel model;
  model.set_frontage_preset(QStringLiteral("Wide"));
  model.set_depth_preset(QStringLiteral("shallow"));
  model.set_spacing_preset(QStringLiteral("nonsense"));
  const auto indices = model.preset_indices();
  EXPECT_EQ(indices.frontage, 2);
  EXPECT_EQ(indices.depth, 0);
  EXPECT_EQ(indices.spacing, 1);
}

TEST(FormationOptionsModel, ParsingSettersReportWhetherTheyApplied) {
  FormationOptionsModel model;
  EXPECT_FALSE(model.set_flank_preference(QStringLiteral("not-a-flank")));
  EXPECT_FALSE(model.set_intent(QStringLiteral("not-an-intent")));
  EXPECT_FALSE(model.set_intent(QStringLiteral("faction_default")))
      << "re-selecting the current intent changes nothing";
}

TEST(FormationOptionsModel, DoctrineOverrideLocksAndAutomaticUnlocks) {
  FormationOptionsModel model;
  model.set_doctrine_override(QStringLiteral("  Phalanx "));
  EXPECT_EQ(model.doctrine_override(), "phalanx");
  EXPECT_TRUE(model.options().doctrine_locked);
  model.set_doctrine_override(QStringLiteral("Automatic"));
  EXPECT_TRUE(model.doctrine_override().empty());
  EXPECT_FALSE(model.options().doctrine_locked);
}

TEST(FormationOptionsModel, DepthWheelIsClampedAndResetRestoresDefaults) {
  FormationOptionsModel model;
  for (int i = 0; i < 40; ++i) {
    model.adjust_depth(1.0F);
  }
  EXPECT_FLOAT_EQ(model.options().depth_scale, 3.0F);
  for (int i = 0; i < 80; ++i) {
    model.adjust_depth(-1.0F);
  }
  EXPECT_FLOAT_EQ(model.options().depth_scale, 0.4F);
  model.set_reserve_rows(9);
  EXPECT_EQ(model.options().reserve_rows, 2);
  model.reset();
  EXPECT_FLOAT_EQ(model.options().depth_scale, 1.0F);
  EXPECT_EQ(model.options().reserve_rows, -1);
}

} // namespace
