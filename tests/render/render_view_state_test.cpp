#include <gtest/gtest.h>

#include "render/render_view_state.h"

// These rules used to live on Renderer, which meant exercising them needed a
// backend and a GL context. They are presentation policy, not drawing, so they
// are now a plain value type and testable on their own.

namespace {

using Render::GL::RenderViewState;
using Render::GL::WorldRenderMode;

TEST(RenderViewStateTest, DefaultsToTheRtsViewOwnedByPlayerOne) {
  RenderViewState const view;

  EXPECT_EQ(view.world_render_mode(), WorldRenderMode::Rts);
  EXPECT_EQ(view.local_owner_id(), 1);
  EXPECT_EQ(view.hovered_entity_id(), 0U);
  EXPECT_EQ(view.rpg_camera_focus(), 0U);
  EXPECT_FALSE(view.cinematic_mode());
  EXPECT_FALSE(view.order_marker_spectator_mode());
  EXPECT_FALSE(view.force_full_creature_lod());
}

TEST(RenderViewStateTest, OrderMarkersBelongToTheLocalPlayerOnly) {
  RenderViewState view;
  view.set_local_owner_id(2);

  EXPECT_TRUE(view.order_markers_visible_for_owner(2));
  EXPECT_FALSE(view.order_markers_visible_for_owner(1));
  EXPECT_FALSE(view.order_markers_visible_for_owner(3));
}

TEST(RenderViewStateTest, SpectatorsSeeNobodysOrderMarkers) {
  RenderViewState view;
  view.set_local_owner_id(2);
  view.set_order_marker_spectator_mode(true);

  EXPECT_FALSE(view.order_markers_visible_for_owner(1));
  EXPECT_FALSE(view.order_markers_visible_for_owner(2));
  EXPECT_FALSE(view.order_markers_visible_for_owner(3));
}

TEST(RenderViewStateTest, CinematicFramesCarryNoOrderMarkers) {
  RenderViewState view;
  view.set_local_owner_id(2);
  ASSERT_TRUE(view.order_markers_visible_for_owner(2));

  view.set_cinematic_mode(true);
  EXPECT_FALSE(view.order_markers_visible_for_owner(2));

  view.set_cinematic_mode(false);
  EXPECT_TRUE(view.order_markers_visible_for_owner(2));
}

TEST(RenderViewStateTest, RpgFocusAndModeAreRememberedIndependently) {
  RenderViewState view;
  view.set_world_render_mode(WorldRenderMode::Rpg);
  view.set_rpg_camera_focus(77U);
  view.set_hovered_entity_id(12U);
  view.set_force_full_creature_lod(true);

  EXPECT_EQ(view.world_render_mode(), WorldRenderMode::Rpg);
  EXPECT_EQ(view.rpg_camera_focus(), 77U);
  EXPECT_EQ(view.hovered_entity_id(), 12U);
  EXPECT_TRUE(view.force_full_creature_lod());
}

} // namespace
