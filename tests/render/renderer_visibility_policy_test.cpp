#include <gtest/gtest.h>

#include "game/map/visibility_service.h"
#include "render/scene_renderer.h"
#include "scene/camera.h"

TEST(RendererVisibilityPolicyTest, ModeConfigUsesTotalVisibilityRules) {
  Render::GL::Renderer renderer;

  EXPECT_TRUE(renderer.non_local_unit_visibility_filter_enabled());
  EXPECT_TRUE(renderer.static_world_visibility_filter_enabled());

  renderer.set_world_render_mode(Render::GL::Renderer::WorldRenderMode::Rts);
  EXPECT_TRUE(renderer.non_local_unit_visibility_filter_enabled());
  EXPECT_TRUE(renderer.static_world_visibility_filter_enabled());

  renderer.set_world_render_mode(Render::GL::Renderer::WorldRenderMode::Rpg);
  EXPECT_TRUE(renderer.non_local_unit_visibility_filter_enabled());
  EXPECT_FALSE(renderer.static_world_visibility_filter_enabled());
}

TEST(RendererVisibilityPolicyTest, OrderMarkersBelongOnlyToTheLocalHumanViewer) {
  Render::GL::Renderer renderer;
  renderer.set_local_owner_id(3);

  EXPECT_TRUE(renderer.order_markers_visible_for_owner(3));
  EXPECT_FALSE(renderer.order_markers_visible_for_owner(4));

  renderer.set_local_owner_id(4);
  EXPECT_FALSE(renderer.order_markers_visible_for_owner(3));
  EXPECT_TRUE(renderer.order_markers_visible_for_owner(4));
}

TEST(RendererVisibilityPolicyTest, SpectatorsNeedAnExplicitDebugReveal) {
  Render::GL::Renderer renderer;
  renderer.set_local_owner_id(3);
  renderer.set_order_marker_spectator_mode(true);

  EXPECT_FALSE(renderer.order_markers_visible_for_owner(3));
  EXPECT_FALSE(renderer.order_markers_visible_for_owner(4));

  renderer.set_debug_reveal_non_local_order_markers(true);
  EXPECT_TRUE(renderer.order_markers_visible_for_owner(3));
  EXPECT_TRUE(renderer.order_markers_visible_for_owner(4));
}

TEST(CameraFrustumCacheTest, CameraChangesInvalidateCachedPlanes) {
  Game::Map::VisibilityService::instance().reset();

  Render::GL::Camera camera;
  camera.set_perspective(60.0F, 1.0F, 0.1F, 100.0F);
  camera.look_at(
      QVector3D(0.0F, 0.0F, 5.0F), QVector3D(0.0F, 0.0F, 0.0F), QVector3D(0, 1, 0));

  EXPECT_TRUE(camera.is_in_frustum(QVector3D(0.0F, 0.0F, 0.0F), 0.1F));
  EXPECT_FALSE(camera.is_in_frustum(QVector3D(0.0F, 0.0F, 10.0F), 0.1F));

  camera.look_at(
      QVector3D(0.0F, 0.0F, 5.0F), QVector3D(0.0F, 0.0F, 10.0F), QVector3D(0, 1, 0));

  EXPECT_FALSE(camera.is_in_frustum(QVector3D(0.0F, 0.0F, 0.0F), 0.1F));
  EXPECT_TRUE(camera.is_in_frustum(QVector3D(0.0F, 0.0F, 10.0F), 0.1F));
}

TEST(RendererVisibilityPolicyTest, CentralPolicyDistinguishesVisibleAndRevealedFog) {
  Game::Map::VisibilityService::Snapshot snapshot;
  snapshot.initialized = true;
  snapshot.width = 3;
  snapshot.height = 3;
  snapshot.tile_size = 1.0F;
  snapshot.half_width = 1.0F;
  snapshot.half_height = 1.0F;
  snapshot.cells.assign(9U,
                        static_cast<std::uint8_t>(Game::Map::VisibilityState::Unseen));
  snapshot.cells[4] = static_cast<std::uint8_t>(Game::Map::VisibilityState::Explored);

  Render::GL::SubmissionVisibilityPolicy policy;
  policy.reset(nullptr, &snapshot);

  EXPECT_FALSE(policy.accepts_sphere(
      QVector3D(0.0F, 0.0F, 0.0F), 1.0F, Render::GL::SubmissionFogMode::VisibleOnly));
  EXPECT_TRUE(policy.accepts_sphere(
      QVector3D(0.0F, 0.0F, 0.0F), 1.0F, Render::GL::SubmissionFogMode::Revealed));
  EXPECT_TRUE(policy.accepts_sphere(
      QVector3D(0.0F, 0.0F, 0.0F), 1.0F, Render::GL::SubmissionFogMode::Ignore));
}

TEST(RendererVisibilityPolicyTest, BoundsRemainVisibleWhenCenterCrossesFogCell) {
  Game::Map::VisibilityService::Snapshot snapshot;
  snapshot.initialized = true;
  snapshot.width = 5;
  snapshot.height = 5;
  snapshot.tile_size = 1.0F;
  snapshot.half_width = 2.0F;
  snapshot.half_height = 2.0F;
  snapshot.cells.assign(25U,
                        static_cast<std::uint8_t>(Game::Map::VisibilityState::Unseen));
  snapshot.cells[2U * 5U + 2U] =
      static_cast<std::uint8_t>(Game::Map::VisibilityState::Visible);

  Render::GL::SubmissionVisibilityPolicy policy;
  policy.reset(nullptr, &snapshot);

  EXPECT_TRUE(policy.accepts_sphere(
      QVector3D(0.65F, 0.0F, 0.0F), 0.7F, Render::GL::SubmissionFogMode::VisibleOnly));
  EXPECT_FALSE(policy.accepts_sphere(
      QVector3D(2.0F, 0.0F, 0.0F), 0.4F, Render::GL::SubmissionFogMode::VisibleOnly));
}

TEST(RendererVisibilityPolicyTest, AnchorExtentHidesFormationsStandingInFog) {
  Game::Map::VisibilityService::Snapshot snapshot;
  snapshot.initialized = true;
  snapshot.width = 16;
  snapshot.height = 16;
  snapshot.tile_size = 1.0F;
  snapshot.half_width = 7.5F;
  snapshot.half_height = 7.5F;
  snapshot.cells.assign(16U * 16U,
                        static_cast<std::uint8_t>(Game::Map::VisibilityState::Unseen));

  snapshot.cells[8U * 16U + 8U] =
      static_cast<std::uint8_t>(Game::Map::VisibilityState::Visible);

  Render::GL::SubmissionVisibilityPolicy policy;
  policy.reset(nullptr, &snapshot);

  const QVector3D in_fog(5.0F, 0.0F, 0.0F);
  EXPECT_TRUE(
      policy.accepts_sphere(in_fog, 5.0F, Render::GL::SubmissionFogMode::VisibleOnly));
  EXPECT_FALSE(policy.accepts_sphere(in_fog,
                                     5.0F,
                                     Render::GL::SubmissionFogMode::VisibleOnly,
                                     Render::GL::FogExtent::Anchor));

  const QVector3D watched(0.0F, 0.0F, 0.0F);
  EXPECT_TRUE(policy.accepts_sphere(watched,
                                    5.0F,
                                    Render::GL::SubmissionFogMode::VisibleOnly,
                                    Render::GL::FogExtent::Anchor));
}

TEST(RendererVisibilityPolicyTest, AnchorExtentLeavesRememberedSceneryAlone) {
  Game::Map::VisibilityService::Snapshot snapshot;
  snapshot.initialized = true;
  snapshot.width = 8;
  snapshot.height = 8;
  snapshot.tile_size = 1.0F;
  snapshot.half_width = 3.5F;
  snapshot.half_height = 3.5F;
  snapshot.cells.assign(
      64U, static_cast<std::uint8_t>(Game::Map::VisibilityState::Explored));

  Render::GL::SubmissionVisibilityPolicy policy;
  policy.reset(nullptr, &snapshot);

  EXPECT_TRUE(policy.accepts_sphere(QVector3D(0.0F, 0.0F, 0.0F),
                                    3.0F,
                                    Render::GL::SubmissionFogMode::Revealed,
                                    Render::GL::FogExtent::Anchor));
}

TEST(RendererVisibilityPolicyTest, FrustumBroadPhaseHasStableEdgeGuardBand) {
  Render::GL::Camera camera;
  camera.set_perspective(60.0F, 1.0F, 0.1F, 100.0F);
  camera.look_at(
      QVector3D(0.0F, 0.0F, 5.0F), QVector3D(0.0F, 0.0F, 0.0F), QVector3D(0, 1, 0));

  const QVector3D near_edge(3.5F, 0.0F, 0.0F);
  EXPECT_FALSE(camera.is_in_frustum(near_edge, 0.1F));

  Render::GL::SubmissionVisibilityPolicy policy;
  policy.reset(&camera, nullptr);
  EXPECT_TRUE(policy.accepts_sphere(near_edge, 0.1F));
}
