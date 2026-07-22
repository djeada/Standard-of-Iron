#include <algorithm>
#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>
#include <utility>
#include <vector>

#include "app/core/minimap_manager.h"
#include "app/core/visibility_coordinator.h"
#include "core/component.h"
#include "core/entity.h"
#include "core/world.h"
#include "map/map_definition.h"
#include "map/minimap/minimap_utils.h"
#include "map/render_visibility_rules.h"
#include "map/visibility_service.h"
#include "render/gl/camera.h"

using namespace Game::Map;

namespace {

auto make_test_map(int width = 12,
                   int height = 12,
                   float yaw_deg = 0.0F) -> MapDefinition {
  MapDefinition map;
  map.name = "Minimap Fog Test";
  map.grid.width = width;
  map.grid.height = height;
  map.grid.tile_size = 1.0F;
  map.camera.yaw_deg = yaw_deg;
  map.biome.grass_primary = QVector3D(0.3F, 0.6F, 0.28F);
  map.biome.grass_secondary = QVector3D(0.44F, 0.7F, 0.32F);
  map.biome.soil_color = QVector3D(0.28F, 0.24F, 0.18F);
  return map;
}

auto world_to_pixel(const QImage& image,
                    const MapDefinition& map,
                    float world_x,
                    float world_z) -> std::pair<int, int> {
  const auto& orient = Game::Map::Minimap::MinimapOrientation::instance();
  const float rotated_x = world_x * orient.cos_yaw() - world_z * orient.sin_yaw();
  const float rotated_z = world_x * orient.sin_yaw() + world_z * orient.cos_yaw();
  const float world_width = static_cast<float>(map.grid.width);
  const float world_height = static_cast<float>(map.grid.height);
  const float scale_x = static_cast<float>(image.width() - 1) / world_width;
  const float scale_y = static_cast<float>(image.height() - 1) / world_height;
  const float px = (rotated_x + world_width * 0.5F) * scale_x;
  const float py = (rotated_z + world_height * 0.5F) * scale_y;
  return {std::clamp(static_cast<int>(std::lround(px)), 0, image.width() - 1),
          std::clamp(static_cast<int>(std::lround(py)), 0, image.height() - 1)};
}

auto count_changed_pixels(const QImage& before, const QImage& after) -> int {
  int changed = 0;
  for (int y = 0; y < before.height(); ++y) {
    for (int x = 0; x < before.width(); ++x) {
      if (before.pixel(x, y) != after.pixel(x, y)) {
        ++changed;
      }
    }
  }
  return changed;
}

auto changed_pixels_in_radius(const QImage& before,
                              const QImage& after,
                              int center_x,
                              int center_y,
                              int radius) -> int {
  int changed = 0;
  const int min_x = std::max(0, center_x - radius);
  const int max_x = std::min(before.width() - 1, center_x + radius);
  const int min_y = std::max(0, center_y - radius);
  const int max_y = std::min(before.height() - 1, center_y + radius);
  for (int y = min_y; y <= max_y; ++y) {
    for (int x = min_x; x <= max_x; ++x) {
      if (before.pixel(x, y) != after.pixel(x, y)) {
        ++changed;
      }
    }
  }
  return changed;
}

auto add_unit(Engine::Core::World& world,
              float x,
              float z,
              int owner_id) -> Engine::Core::Entity* {
  auto* entity = world.create_entity();
  (void)entity->add_component<Engine::Core::TransformComponent>(x, 0.0F, z);
  auto* unit_comp =
      entity->add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  unit_comp->owner_id = owner_id;
  return entity;
}

auto make_visibility_snapshot(int width,
                              int height,
                              std::uint64_t version,
                              VisibilityState fill_state)
    -> VisibilityService::Snapshot {
  VisibilityService::Snapshot snapshot;
  snapshot.version = version;
  snapshot.initialized = true;
  snapshot.width = width;
  snapshot.height = height;
  snapshot.tile_size = 1.0F;
  snapshot.half_width = static_cast<float>(width) * 0.5F - 0.5F;
  snapshot.half_height = static_cast<float>(height) * 0.5F - 0.5F;
  snapshot.cells.assign(static_cast<std::size_t>(width * height),
                        static_cast<std::uint8_t>(fill_state));
  return snapshot;
}

class RecordingVisibilityFramePresenter final : public VisibilityFramePresenter {
public:
  void apply_visibility_frame(
      const Game::Map::VisibilityService::Snapshot& snapshot) override {
    applied_versions.push_back(snapshot.version);
  }

  void clear_visibility_frame() override { ++clear_count; }

  std::vector<std::uint64_t> applied_versions;
  int clear_count = 0;
};

void sync_minimap_fog_from_visibility(MinimapManager& manager) {
  auto snapshot = VisibilityService::instance().snapshot_ptr();
  ASSERT_NE(snapshot, nullptr);
  manager.update_fog(*snapshot);
}

} // namespace

TEST(MinimapManagerTest, FogUpdatesOnlyWhenVisibilityCellsChange) {
  MinimapManager manager;
  manager.generate_for_map(make_test_map());
  EXPECT_TRUE(manager.consume_dirty_flag());
  EXPECT_FALSE(manager.consume_dirty_flag());

  const auto unseen_snapshot =
      make_visibility_snapshot(12, 12, 1, VisibilityState::Unseen);

  manager.update_fog(unseen_snapshot);
  EXPECT_TRUE(manager.consume_dirty_flag());
  EXPECT_FALSE(manager.consume_dirty_flag());

  manager.update_fog(unseen_snapshot);
  EXPECT_FALSE(manager.consume_dirty_flag());

  auto newer_snapshot = unseen_snapshot;
  newer_snapshot.version = 2;
  manager.update_fog(newer_snapshot);
  EXPECT_FALSE(manager.consume_dirty_flag());

  newer_snapshot.version = 3;
  newer_snapshot.cells[6 * 12 + 6] =
      static_cast<std::uint8_t>(VisibilityState::Visible);
  manager.update_fog(newer_snapshot);
  EXPECT_TRUE(manager.consume_dirty_flag());
}

TEST(MinimapManagerTest, ClearFogRestoresBaseImage) {
  MinimapManager manager;
  manager.generate_for_map(make_test_map());
  const QImage base = manager.get_image().copy();
  ASSERT_FALSE(base.isNull());

  manager.update_fog(make_visibility_snapshot(12, 12, 1, VisibilityState::Unseen));

  const QRgb base_pixel = base.pixel(6, 6);
  const QRgb fogged_pixel = manager.get_image().pixel(6, 6);
  EXPECT_NE(fogged_pixel, base_pixel);

  manager.clear_fog();
  EXPECT_TRUE(manager.consume_dirty_flag());
  EXPECT_EQ(manager.get_image().pixel(6, 6), base_pixel);
}

TEST(MinimapManagerTest, YawedFogStillDarkensRuntimeMinimap) {
  MinimapManager manager;
  manager.generate_for_map(make_test_map(64, 64, 225.0F));
  const QImage base = manager.get_image().copy();
  ASSERT_FALSE(base.isNull());

  manager.update_fog(make_visibility_snapshot(64, 64, 1, VisibilityState::Unseen));

  const QImage fogged = manager.get_image().copy();
  EXPECT_GT(count_changed_pixels(base, fogged), 0)
      << "Runtime-like yawed minimaps must still receive fog tint.";
}

TEST(NonLocalVisibilityRulesTest, OnlyCurrentlyVisibleCellsRenderNonLocalUnits) {
  Game::Map::VisibilityService::Snapshot snapshot;
  snapshot.initialized = true;
  snapshot.width = 4;
  snapshot.height = 4;
  snapshot.tile_size = 1.0F;
  snapshot.half_width = 1.5F;
  snapshot.half_height = 1.5F;
  snapshot.cells.assign(16, static_cast<std::uint8_t>(VisibilityState::Unseen));

  snapshot.cells[5] = static_cast<std::uint8_t>(VisibilityState::Visible);
  snapshot.cells[6] = static_cast<std::uint8_t>(VisibilityState::Explored);

  EXPECT_TRUE(Game::Map::should_render_non_local_unit(snapshot, -0.5F, -0.5F));
  EXPECT_FALSE(Game::Map::should_render_non_local_unit(snapshot, 0.5F, -0.5F));
  EXPECT_FALSE(Game::Map::should_render_non_local_unit(snapshot, 1.5F, 1.5F));
}

TEST(MinimapManagerTest, UpdateUnitsMarksDirtyOnFirstCallAfterFogChange) {
  auto world = std::make_unique<Engine::Core::World>();
  auto* entity = world->create_entity();
  (void)entity->add_component<Engine::Core::TransformComponent>(1.0F, 0.0F, 1.0F);
  auto* unit_comp =
      entity->add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 5.0F);
  unit_comp->owner_id = 1;

  MinimapManager manager;
  manager.generate_for_map(make_test_map());
  (void)manager.consume_dirty_flag();

  auto visible_snapshot = make_visibility_snapshot(12, 12, 1, VisibilityState::Visible);
  manager.update_fog(visible_snapshot);
  (void)manager.consume_dirty_flag();

  manager.update_units(world.get(), nullptr, 1);
  EXPECT_TRUE(manager.consume_dirty_flag())
      << "update_units() must mark dirty on first call (fog composite version "
         "was unset).";

  manager.update_units(world.get(), nullptr, 1);
  EXPECT_FALSE(manager.consume_dirty_flag())
      << "update_units() must not mark dirty when neither fog nor units "
         "changed.";

  visible_snapshot.version = 2;
  manager.update_fog(visible_snapshot);
  (void)manager.consume_dirty_flag();

  manager.update_units(world.get(), nullptr, 1);
  EXPECT_FALSE(manager.consume_dirty_flag())
      << "A snapshot version bump with identical cells must not force a unit-layer "
         "recomposition.";

  visible_snapshot.version = 3;
  visible_snapshot.cells[0] = static_cast<std::uint8_t>(VisibilityState::Explored);
  manager.update_fog(visible_snapshot);
  (void)manager.consume_dirty_flag();
  manager.update_units(world.get(), nullptr, 1);
  EXPECT_TRUE(manager.consume_dirty_flag())
      << "A real fog pixel change must invalidate the unit-layer composite.";
}

TEST(MinimapManagerTest, UpdateUnitsMarksDirtyWhenRenderedMarkerStateChanges) {
  auto world = std::make_unique<Engine::Core::World>();
  auto* entity = world->create_entity();
  (void)entity->add_component<Engine::Core::TransformComponent>(1.0F, 0.0F, 1.0F);
  auto* unit_comp =
      entity->add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 5.0F);
  unit_comp->owner_id = 1;

  MinimapManager manager;
  manager.generate_for_map(make_test_map());
  (void)manager.consume_dirty_flag();

  manager.update_units(world.get(), nullptr, 1);
  EXPECT_TRUE(manager.consume_dirty_flag());

  manager.update_units(world.get(), nullptr, 1);
  EXPECT_FALSE(manager.consume_dirty_flag());

  unit_comp->owner_id = 2;
  manager.update_units(world.get(), nullptr, 1);
  EXPECT_TRUE(manager.consume_dirty_flag())
      << "owner color changes must invalidate the cached unit overlay.";

  manager.update_units(world.get(), nullptr, 2);
  EXPECT_TRUE(manager.consume_dirty_flag())
      << "local owner changes can change minimap visibility filtering.";
}

TEST(MinimapManagerTest, UnchangedCameraDoesNotRecomposeFullMinimapImage) {
  MinimapManager manager;
  manager.generate_for_map(make_test_map(64, 64));
  (void)manager.consume_dirty_flag();

  Render::GL::Camera camera;
  camera.set_rts_view(QVector3D(3.0F, 0.0F, -4.0F), 18.0F, 45.0F, 30.0F);

  manager.update_camera_viewport(&camera, 1920.0F, 1080.0F);
  EXPECT_TRUE(manager.consume_dirty_flag());
  const qint64 first_composite_key = manager.get_image().cacheKey();

  manager.update_camera_viewport(&camera, 1920.0F, 1080.0F);
  EXPECT_FALSE(manager.consume_dirty_flag());
  EXPECT_EQ(manager.get_image().cacheKey(), first_composite_key);
}

TEST(MinimapManagerTest, NonLocalMarkersDisappearWhenCellsFallBackToExplored) {
  constexpr int kMapSize = 64;
  const MapDefinition map = make_test_map(kMapSize, kMapSize, 225.0F);
  auto& visibility = VisibilityService::instance();
  visibility.initialize(kMapSize, kMapSize, 1.0F);
  visibility.reset();

  auto world = std::make_unique<Engine::Core::World>();
  auto* scout = add_unit(*world, 0.0F, 0.0F, 1);
  (void)add_unit(*world, 1.0F, 1.0F, 2);

  MinimapManager manager;
  manager.generate_for_map(map);
  (void)manager.consume_dirty_flag();

  visibility.compute_immediate(*world, 1);
  sync_minimap_fog_from_visibility(manager);
  const QImage fog_with_enemy_visible = manager.get_image().copy();

  manager.update_units(world.get(), nullptr, 1);
  const QImage image_with_enemy_visible = manager.get_image().copy();
  const auto [enemy_px, enemy_py] =
      world_to_pixel(image_with_enemy_visible, map, 1.0F, 1.0F);
  EXPECT_GT(
      changed_pixels_in_radius(
          fog_with_enemy_visible, image_with_enemy_visible, enemy_px, enemy_py, 3),
      0)
      << "Enemy markers should render while their cell is currently visible.";

  auto* scout_transform = scout->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(scout_transform, nullptr);
  scout_transform->position.x = 24.0F;
  scout_transform->position.z = 24.0F;

  visibility.compute_immediate(*world, 1);
  sync_minimap_fog_from_visibility(manager);
  const QImage fog_with_enemy_explored = manager.get_image().copy();

  manager.update_units(world.get(), nullptr, 1);
  const QImage image_with_enemy_explored = manager.get_image().copy();
  EXPECT_EQ(
      changed_pixels_in_radius(
          fog_with_enemy_explored, image_with_enemy_explored, enemy_px, enemy_py, 3),
      0)
      << "The minimap must cull enemy markers once the cell is no longer visible.";
}

TEST(VisibilityServiceSnapshotTest, SnapshotIfNewerReturnsPublishedFrames) {
  auto& visibility = VisibilityService::instance();
  visibility.initialize(4, 4, 1.0F);

  const auto initial_snapshot = visibility.snapshot_if_newer(0);
  ASSERT_NE(initial_snapshot, nullptr);
  EXPECT_GT(initial_snapshot->version, 0U);
  EXPECT_EQ(initial_snapshot->cells.size(), 16U);
  EXPECT_EQ(visibility.snapshot_if_newer(initial_snapshot->version), nullptr);

  visibility.reveal_all();

  const auto revealed_snapshot =
      visibility.snapshot_if_newer(initial_snapshot->version);
  ASSERT_NE(revealed_snapshot, nullptr);
  EXPECT_GT(revealed_snapshot->version, initial_snapshot->version);
  EXPECT_TRUE(std::all_of(revealed_snapshot->cells.begin(),
                          revealed_snapshot->cells.end(),
                          [](std::uint8_t cell) {
                            return cell ==
                                   static_cast<std::uint8_t>(VisibilityState::Visible);
                          }));
}

TEST(VisibilityServiceSnapshotTest, CommanderRallyFlagAddsVisionSource) {
  auto& visibility = VisibilityService::instance();
  visibility.initialize(64, 64, 1.0F);
  visibility.reset();

  auto world = std::make_unique<Engine::Core::World>();
  auto* commander = world->create_entity();
  auto* transform =
      commander->add_component<Engine::Core::TransformComponent>(-20.0F, 0.0F, -20.0F);
  auto* unit =
      commander->add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  auto* commander_data = commander->add_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(transform, nullptr);
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(commander_data, nullptr);
  unit->owner_id = 1;
  commander_data->flag_rally_flag_x = 20.0F;
  commander_data->flag_rally_flag_z = 20.0F;

  visibility.compute_immediate(*world, 1);
  EXPECT_FALSE(visibility.is_visible_world(20.0F, 20.0F));

  commander_data->flag_rally_flag_active = true;
  visibility.compute_immediate(*world, 1);
  EXPECT_TRUE(visibility.is_visible_world(20.0F, 20.0F));
}

TEST(VisibilityCoordinatorTest, ForceRepublishSyncsFogAfterMinimapGeneration) {
  auto world = std::make_unique<Engine::Core::World>();
  auto* scout = add_unit(*world, 0.0F, 0.0F, 1);
  auto* scout_unit = scout->get_component<Engine::Core::UnitComponent>();
  ASSERT_NE(scout_unit, nullptr);
  scout_unit->vision_range = 2.0F;

  VisibilityCoordinator coordinator;
  MinimapManager manager;
  coordinator.set_presenters(nullptr, &manager);
  coordinator.initialize_for_world(*world, 1, 64, 64, 1.0F, false);

  manager.generate_for_map(make_test_map(64, 64));
  const QImage base = manager.get_image().copy();
  (void)manager.consume_dirty_flag();

  coordinator.publish_current_frame(true);
  const QImage fogged = manager.get_image().copy();

  EXPECT_GT(count_changed_pixels(base, fogged), 0);
}

TEST(VisibilityCoordinatorTest, InitializePublishesSameSnapshotVersionToAllPresenters) {
  auto world = std::make_unique<Engine::Core::World>();
  auto* scout = add_unit(*world, 0.0F, 0.0F, 1);
  auto* scout_unit = scout->get_component<Engine::Core::UnitComponent>();
  ASSERT_NE(scout_unit, nullptr);
  scout_unit->vision_range = 2.0F;

  RecordingVisibilityFramePresenter fog_presenter;
  RecordingVisibilityFramePresenter minimap_presenter;
  VisibilityCoordinator coordinator;
  coordinator.set_frame_presenters(&fog_presenter, &minimap_presenter);
  coordinator.initialize_for_world(*world, 1, 32, 32, 1.0F, false);

  ASSERT_EQ(fog_presenter.applied_versions.size(), 1U);
  ASSERT_EQ(minimap_presenter.applied_versions.size(), 1U);
  EXPECT_EQ(fog_presenter.applied_versions.back(),
            minimap_presenter.applied_versions.back());
  EXPECT_EQ(fog_presenter.applied_versions.back(),
            coordinator.last_published_version());
}

TEST(VisibilityCoordinatorTest, UpdateSkipsPresenterRepublishWhenVersionIsUnchanged) {
  auto world = std::make_unique<Engine::Core::World>();
  auto* scout = add_unit(*world, 0.0F, 0.0F, 1);
  auto* scout_unit = scout->get_component<Engine::Core::UnitComponent>();
  ASSERT_NE(scout_unit, nullptr);
  scout_unit->vision_range = 2.0F;

  RecordingVisibilityFramePresenter fog_presenter;
  RecordingVisibilityFramePresenter minimap_presenter;
  VisibilityCoordinator coordinator;
  coordinator.set_frame_presenters(&fog_presenter, &minimap_presenter);
  coordinator.initialize_for_world(*world, 1, 32, 32, 1.0F, false);

  coordinator.update(*world, 1, 0.01F, 10.0F, false);

  EXPECT_EQ(fog_presenter.applied_versions.size(), 1U);
  EXPECT_EQ(minimap_presenter.applied_versions.size(), 1U);
}

TEST(VisibilityCoordinatorTest, SpectatorToggleClearsAndRepublishesAllPresenters) {
  auto world = std::make_unique<Engine::Core::World>();
  auto* scout = add_unit(*world, 0.0F, 0.0F, 1);
  auto* scout_unit = scout->get_component<Engine::Core::UnitComponent>();
  ASSERT_NE(scout_unit, nullptr);
  scout_unit->vision_range = 2.0F;

  RecordingVisibilityFramePresenter fog_presenter;
  RecordingVisibilityFramePresenter minimap_presenter;
  VisibilityCoordinator coordinator;
  coordinator.set_frame_presenters(&fog_presenter, &minimap_presenter);
  coordinator.initialize_for_world(*world, 1, 32, 32, 1.0F, false);

  coordinator.update(*world, 1, 0.01F, 10.0F, true);

  EXPECT_EQ(fog_presenter.clear_count, 1);
  EXPECT_EQ(minimap_presenter.clear_count, 1);
  EXPECT_EQ(coordinator.last_published_version(), 0U);

  coordinator.update(*world, 1, 0.01F, 10.0F, false);

  ASSERT_EQ(fog_presenter.applied_versions.size(), 2U);
  ASSERT_EQ(minimap_presenter.applied_versions.size(), 2U);
  EXPECT_EQ(fog_presenter.applied_versions.back(),
            minimap_presenter.applied_versions.back());
}

TEST(MinimapManagerTest, NonLocalMarkersRemainHiddenInUnseenCells) {
  constexpr int kMapSize = 64;
  const MapDefinition map = make_test_map(kMapSize, kMapSize, 225.0F);
  auto& visibility = VisibilityService::instance();
  visibility.initialize(kMapSize, kMapSize, 1.0F);
  visibility.reset();

  auto world = std::make_unique<Engine::Core::World>();
  (void)add_unit(*world, -20.0F, -20.0F, 1);
  (void)add_unit(*world, 20.0F, 20.0F, 2);

  MinimapManager manager;
  manager.generate_for_map(map);
  (void)manager.consume_dirty_flag();

  visibility.compute_immediate(*world, 1);
  sync_minimap_fog_from_visibility(manager);
  const QImage fog_only = manager.get_image().copy();

  manager.update_units(world.get(), nullptr, 1);
  const QImage image_with_units = manager.get_image().copy();
  const auto [enemy_px, enemy_py] = world_to_pixel(image_with_units, map, 20.0F, 20.0F);
  EXPECT_EQ(changed_pixels_in_radius(fog_only, image_with_units, enemy_px, enemy_py, 3),
            0)
      << "Enemy markers must not reveal unseen positions through fog.";
}

TEST(MinimapManagerTest, LocalMarkersIgnoreFogVisibilityFiltering) {
  auto& visibility = VisibilityService::instance();
  visibility.initialize(12, 12, 1.0F);
  visibility.reset();

  auto world = std::make_unique<Engine::Core::World>();
  (void)add_unit(*world, 1.0F, 1.0F, 1);
  const MapDefinition map = make_test_map();

  MinimapManager manager;
  manager.generate_for_map(map);
  (void)manager.consume_dirty_flag();

  sync_minimap_fog_from_visibility(manager);
  const QImage fog_only = manager.get_image().copy();

  manager.update_units(world.get(), nullptr, 1);
  const QImage with_local_marker = manager.get_image().copy();
  const auto [local_px, local_py] = world_to_pixel(with_local_marker, map, 1.0F, 1.0F);
  EXPECT_GT(
      changed_pixels_in_radius(fog_only, with_local_marker, local_px, local_py, 3), 0)
      << "Local markers must remain visible even if the fog snapshot is fully unseen.";
}
