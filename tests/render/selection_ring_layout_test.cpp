#include <array>
#include <gtest/gtest.h>
#include <vector>

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/systems/combat_system/formation_contact_processor.h"
#include "game/systems/formation_combat_geometry.h"
#include "render/selection_ring_layout.h"

namespace {

TEST(SelectionRingLayout, EmptyPublishedFormationUsesUnitRoot) {
  Render::GL::SelectionRingLayoutInput input;
  input.position = QVector3D(4.0F, 0.0F, -3.0F);
  input.ring_size = 1.2F;

  auto const placements = Render::GL::build_selection_ring_layout(input);

  ASSERT_EQ(placements.size(), 1U);
  EXPECT_FLOAT_EQ(placements.front().world_x, 4.0F);
  EXPECT_FLOAT_EQ(placements.front().world_z, -3.0F);
  EXPECT_FLOAT_EQ(placements.front().ring_size, 1.2F);
}

TEST(SelectionRingLayout, UsesOnlyLivingPublishedSlots) {
  std::vector<Engine::Core::FormationSoldierPresentation> soldiers(3);
  soldiers[0].local_x = -1.0F;
  soldiers[0].alive = true;
  soldiers[1].local_z = 2.0F;
  soldiers[1].alive = false;
  soldiers[2].local_x = 1.0F;
  soldiers[2].alive = true;

  auto const placements = Render::GL::build_selection_ring_layout({
      .soldiers = soldiers,
      .ring_size = 0.3F,
      .position = QVector3D(5.0F, 0.0F, 7.0F),
  });

  ASSERT_EQ(placements.size(), 2U);
  EXPECT_FLOAT_EQ(placements[0].world_x, 4.0F);
  EXPECT_FLOAT_EQ(placements[0].world_z, 7.0F);
  EXPECT_FLOAT_EQ(placements[1].world_x, 6.0F);
  EXPECT_FLOAT_EQ(placements[1].world_z, 7.0F);
}

TEST(SelectionRingLayout, AppliesUnitYawWithoutReinterpretingSoldierYaw) {
  Engine::Core::FormationSoldierPresentation soldier;
  soldier.local_x = 2.0F;
  soldier.local_z = 1.0F;
  soldier.local_yaw = 37.0F;
  soldier.alive = true;
  std::array soldiers{soldier};

  auto const placements = Render::GL::build_selection_ring_layout({
      .soldiers = soldiers,
      .position = QVector3D(10.0F, 0.0F, 20.0F),
      .yaw_degrees = 90.0F,
  });

  ASSERT_EQ(placements.size(), 1U);
  EXPECT_NEAR(placements[0].world_x, 11.0F, 1.0e-4F);
  EXPECT_NEAR(placements[0].world_z, 18.0F, 1.0e-4F);
}

TEST(SelectionRingLayout, MatchesGameplayPublishedFormationSlotPositions) {
  Engine::Core::World world;
  auto* entity = world.create_entity();
  auto* transform = entity->add_component<Engine::Core::TransformComponent>();
  auto* unit = entity->add_component<Engine::Core::UnitComponent>(120, 120);
  transform->position = {3.0F, 0.0F, -4.0F};
  transform->rotation.y = 31.0F;
  transform->scale = {0.55F, 0.55F, 0.55F};
  unit->spawn_type = Game::Units::SpawnType::Spearman;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  unit->render_individuals_per_unit_override = 6;

  Game::Systems::Combat::update_formation_contacts(&world);
  auto const* presentation =
      entity->get_component<Engine::Core::FormationPresentationComponent>();
  ASSERT_NE(presentation, nullptr);

  auto const placements = Render::GL::build_selection_ring_layout({
      .soldiers = presentation->soldiers,
      .ring_size = 0.3F,
      .position = QVector3D(
          transform->position.x, transform->position.y, transform->position.z),
      .yaw_degrees = transform->rotation.y,
  });
  auto const gameplay_layout = Game::Systems::FormationCombat::resolve_layout(*entity);

  ASSERT_EQ(placements.size(), gameplay_layout.live_slots.size());
  for (std::size_t index = 0; index < placements.size(); ++index) {
    EXPECT_NEAR(
        placements[index].world_x, gameplay_layout.live_slots[index].world_x, 1.0e-4F);
    EXPECT_NEAR(
        placements[index].world_z, gameplay_layout.live_slots[index].world_z, 1.0e-4F);
  }
}

TEST(SelectionRingLayout, MultiSoldierRingsUseCompactVisualSize) {
  float const size = Render::GL::Detail::selection_ring_visual_size(
      Game::Units::SpawnType::Archer, 20, 1.2F);

  EXPECT_FLOAT_EQ(size, 0.3F);
}

TEST(SelectionRingLayout, SingleSoldierRingKeepsConfiguredSize) {
  float const size = Render::GL::Detail::selection_ring_visual_size(
      Game::Units::SpawnType::Healer, 1, 1.2F);

  EXPECT_FLOAT_EQ(size, 1.2F);
}

} // namespace
