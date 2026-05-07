#include "app/core/minimap_manager.h"
#include "core/component.h"
#include "core/entity.h"
#include "core/world.h"
#include "map/map_definition.h"
#include "map/visibility_service.h"

#include <gtest/gtest.h>

using namespace Game::Map;

namespace {

auto make_test_map() -> MapDefinition {
  MapDefinition map;
  map.name = "Minimap Fog Test";
  map.grid.width = 12;
  map.grid.height = 12;
  map.grid.tile_size = 1.0F;
  map.camera.yaw_deg = 0.0F;
  map.biome.grass_primary = QVector3D(0.3F, 0.6F, 0.28F);
  map.biome.grass_secondary = QVector3D(0.44F, 0.7F, 0.32F);
  map.biome.soil_color = QVector3D(0.28F, 0.24F, 0.18F);
  return map;
}

} // namespace

TEST(MinimapManagerTest, FogUpdatesOnlyWhenVisibilityVersionChanges) {
  MinimapManager manager;
  manager.generate_for_map(make_test_map());
  EXPECT_TRUE(manager.consume_dirty_flag());
  EXPECT_FALSE(manager.consume_dirty_flag());

  const std::vector<std::uint8_t> cells(
      12 * 12, static_cast<std::uint8_t>(VisibilityState::Unseen));

  manager.update_fog(12, 12, cells, 1);
  EXPECT_TRUE(manager.consume_dirty_flag());
  EXPECT_FALSE(manager.consume_dirty_flag());

  manager.update_fog(12, 12, cells, 1);
  EXPECT_FALSE(manager.consume_dirty_flag());

  manager.update_fog(12, 12, cells, 2);
  EXPECT_TRUE(manager.consume_dirty_flag());
}

TEST(MinimapManagerTest, ClearFogRestoresBaseImage) {
  MinimapManager manager;
  manager.generate_for_map(make_test_map());
  const QImage base = manager.get_image().copy();
  ASSERT_FALSE(base.isNull());

  const std::vector<std::uint8_t> cells(
      12 * 12, static_cast<std::uint8_t>(VisibilityState::Unseen));
  manager.update_fog(12, 12, cells, 1);

  const QRgb base_pixel = base.pixel(6, 6);
  const QRgb fogged_pixel = manager.get_image().pixel(6, 6);
  EXPECT_NE(fogged_pixel, base_pixel);

  manager.clear_fog();
  EXPECT_TRUE(manager.consume_dirty_flag());
  EXPECT_EQ(manager.get_image().pixel(6, 6), base_pixel);
}

TEST(MinimapManagerTest, UpdateUnitsMarksDirtyOnFirstCallAfterFogChange) {
  auto world = std::make_unique<Engine::Core::World>();
  auto *entity = world->create_entity();
  (void)entity->add_component<Engine::Core::TransformComponent>(1.0F, 0.0F,
                                                                1.0F);
  auto *unit_comp =
      entity->add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 5.0F);
  unit_comp->owner_id = 1;

  MinimapManager manager;
  manager.generate_for_map(make_test_map());
  manager.consume_dirty_flag();

  const std::vector<std::uint8_t> cells(
      12 * 12, static_cast<std::uint8_t>(VisibilityState::Visible));
  manager.update_fog(12, 12, cells, 1);
  manager.consume_dirty_flag();

  manager.update_units(world.get(), nullptr, 1);
  EXPECT_TRUE(manager.consume_dirty_flag())
      << "update_units() must mark dirty on first call (fog composite version "
         "was unset).";

  manager.update_units(world.get(), nullptr, 1);
  EXPECT_FALSE(manager.consume_dirty_flag())
      << "update_units() must not mark dirty when neither fog nor units "
         "changed.";

  manager.update_fog(12, 12, cells, 2);
  manager.consume_dirty_flag();

  manager.update_units(world.get(), nullptr, 1);
  EXPECT_TRUE(manager.consume_dirty_flag())
      << "update_units() must mark dirty when the fog version changes, even if "
         "the unit hash is unchanged.";
}
