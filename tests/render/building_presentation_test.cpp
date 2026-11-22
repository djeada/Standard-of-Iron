#include <QMatrix4x4>
#include <QVector3D>

#include <gtest/gtest.h>

#include "game/core/component.h"
#include "game/core/component_gameplay.h"
#include "game/core/world.h"
#include "render/entity/building_collapse.h"
#include "render/entity/building_decay.h"
#include "render/entity/building_state.h"

// The building presentation systems turn snapshot state into parameters with
// no GL and no submitter: these tests drive them from a plain World.

namespace {

using Engine::Core::DeathSequenceProfile;
using Engine::Core::DeathSequenceState;

struct CollapsingBuilding {
  Engine::Core::World world;
  Engine::Core::Entity* entity = nullptr;
  Engine::Core::DeathAnimationComponent* death = nullptr;

  CollapsingBuilding() {
    entity = world.create_entity();
    entity->add_component<Engine::Core::BuildingComponent>();
    auto* transform = entity->add_component<Engine::Core::TransformComponent>();
    transform->position = {3.0F, 0.5F, -2.0F};
    death = entity->add_component<Engine::Core::DeathAnimationComponent>();
    death->profile = DeathSequenceProfile::Structure;
    death->state = DeathSequenceState::Dying;
    death->state_duration = 4.0F;
  }

  [[nodiscard]] auto at(float state_time) -> Render::GL::BuildingCollapse {
    death->state = DeathSequenceState::Dying;
    death->state_time = state_time;
    return Render::GL::resolve_building_collapse(world, entity->get_id());
  }
};

TEST(BuildingCollapsePresentation, OnlyAStructureDeathCollapses) {
  CollapsingBuilding building;
  building.death->profile = DeathSequenceProfile::Infantry;
  EXPECT_FALSE(
      Render::GL::resolve_building_collapse(building.world, building.entity->get_id())
          .active);

  Engine::Core::World world;
  auto* plain = world.create_entity();
  plain->add_component<Engine::Core::TransformComponent>();
  EXPECT_FALSE(Render::GL::resolve_building_collapse(world, plain->get_id()).active)
      << "a non-building never collapses";
}

TEST(BuildingCollapsePresentation, TheFallOnlyGrowsAndEndsFullyDown) {
  CollapsingBuilding building;
  float previous = -1.0F;
  for (float t = 0.0F; t <= 4.0F; t += 0.25F) {
    const auto collapse = building.at(t);
    ASSERT_TRUE(collapse.active);
    EXPECT_GE(collapse.fall, previous) << "at t=" << t;
    EXPECT_GE(collapse.fall, 0.0F);
    EXPECT_LE(collapse.fall, 1.0F);
    previous = collapse.fall;
  }
  EXPECT_FLOAT_EQ(building.at(4.0F).fall, 1.0F);
  EXPECT_FLOAT_EQ(building.at(0.0F).fall, 0.0F) << "nothing falls during the shudder";

  building.death->state = DeathSequenceState::DeadHold;
  building.death->state_time = 0.5F;
  const auto settled =
      Render::GL::resolve_building_collapse(building.world, building.entity->get_id());
  EXPECT_FLOAT_EQ(settled.fall, 1.0F);
  EXPECT_GT(settled.settled_for, 0.0F);
}

TEST(BuildingCollapsePresentation, TheSameBuildingCollapsesTheSameWayEveryTime) {
  CollapsingBuilding first;
  const auto a = first.at(1.5F);
  const auto b = first.at(1.5F);
  EXPECT_EQ(a.seed, b.seed);
  EXPECT_FLOAT_EQ(a.fall, b.fall);
  EXPECT_FLOAT_EQ(a.shudder, b.shudder);
  EXPECT_EQ(a.base, QVector3D(3.0F, 0.5F, -2.0F));
}

TEST(BuildingCollapsePresentation, AStandingBuildingKeepsItsModel) {
  Render::GL::BuildingCollapse standing;
  QMatrix4x4 model;
  model.translate(1.0F, 2.0F, 3.0F);
  EXPECT_EQ(Render::GL::building_collapse_model(model, standing), model);
}

TEST(BuildingDecayPresentation, AnIntactBuildingKeepsItsColourAndDecayIsStablePerSeed) {
  using Render::GL::BuildingState;
  const QVector3D authored(0.7F, 0.6F, 0.5F);

  EXPECT_EQ(Render::GL::decayed_color(authored, BuildingState::Normal, 7), authored);
  const QVector3D damaged =
      Render::GL::decayed_color(authored, BuildingState::Damaged, 7);
  const QVector3D destroyed =
      Render::GL::decayed_color(authored, BuildingState::Destroyed, 7);
  EXPECT_NE(damaged, authored);
  EXPECT_NE(destroyed, damaged);
  EXPECT_EQ(Render::GL::decayed_color(authored, BuildingState::Damaged, 7), damaged)
      << "decay must be a pure function of colour, state and seed";
  EXPECT_LT(Render::GL::building_decay_profile(BuildingState::Destroyed).darkening,
            Render::GL::building_decay_profile(BuildingState::Damaged).darkening);
}

} // namespace
