#include "render/creature/pipeline/creature_asset.h"
#include "render/creature/pipeline/creature_visual_definition.h"
#include "render/elephant/elephant_spec.h"
#include "render/horse/horse_spec.h"

#include <gtest/gtest.h>

namespace {

using namespace Render::Creature::Pipeline;

TEST(CreatureVisualDefinition, ResolvesHorseAndElephantThroughSharedShape) {
  UnitVisualSpec horse{};
  horse.kind = CreatureKind::Horse;
  const auto *horse_definition = resolve_creature_visual_definition(horse);
  ASSERT_NE(horse_definition, nullptr);
  EXPECT_EQ(horse_definition->species, CreatureKind::Horse);
  ASSERT_NE(horse_definition->mesh_recipe, nullptr);
  ASSERT_NE(horse_definition->rig_definition, nullptr);
  ASSERT_NE(horse_definition->motion_controller, nullptr);
  ASSERT_NE(horse_definition->grounding_model, nullptr);
  ASSERT_NE(horse_definition->attachment_frame_extractor, nullptr);
  ASSERT_NE(horse_definition->lod_strategy, nullptr);
  EXPECT_EQ(horse_definition->rig_definition->bone_count,
            Render::Horse::k_horse_bone_count);
  EXPECT_EQ(horse_definition->mesh_recipe->resolve_spec,
            &Render::Horse::horse_creature_spec);
  ASSERT_NE(horse_definition->mesh_recipe->resolve_part_graph, nullptr);
  EXPECT_EQ(horse_definition->mesh_recipe
                ->resolve_part_graph(Render::Creature::CreatureLOD::Full)
                .primitives.size(),
            Render::Horse::horse_creature_spec().lod_full.primitives.size());

  UnitVisualSpec elephant{};
  elephant.kind = CreatureKind::Elephant;
  const auto *elephant_definition =
      resolve_creature_visual_definition(elephant);
  ASSERT_NE(elephant_definition, nullptr);
  EXPECT_EQ(elephant_definition->species, CreatureKind::Elephant);
  ASSERT_NE(elephant_definition->mesh_recipe, nullptr);
  ASSERT_NE(elephant_definition->rig_definition, nullptr);
  ASSERT_NE(elephant_definition->motion_controller, nullptr);
  ASSERT_NE(elephant_definition->grounding_model, nullptr);
  ASSERT_NE(elephant_definition->attachment_frame_extractor, nullptr);
  ASSERT_NE(elephant_definition->lod_strategy, nullptr);
  EXPECT_EQ(elephant_definition->rig_definition->bone_count,
            Render::Elephant::k_elephant_bone_count);
  EXPECT_EQ(elephant_definition->mesh_recipe->resolve_spec,
            &Render::Elephant::elephant_creature_spec);
  ASSERT_NE(elephant_definition->mesh_recipe->resolve_part_graph, nullptr);
  EXPECT_EQ(
      elephant_definition->mesh_recipe
          ->resolve_part_graph(Render::Creature::CreatureLOD::Full)
          .primitives.size(),
      Render::Elephant::elephant_creature_spec().lod_full.primitives.size());
}

TEST(CreatureVisualDefinition, ExplicitUnitVisualDefinitionWins) {
  UnitVisualSpec spec{};
  spec.kind = CreatureKind::Elephant;
  spec.creature_definition = &horse_creature_visual_definition();

  const auto *definition = resolve_creature_visual_definition(spec);
  ASSERT_NE(definition, nullptr);
  EXPECT_EQ(definition->species, CreatureKind::Horse);
}

TEST(CreatureVisualDefinition, HumanoidAndMountedStayHighLevelOnly) {
  UnitVisualSpec humanoid{};
  humanoid.kind = CreatureKind::Humanoid;
  EXPECT_EQ(resolve_creature_visual_definition(humanoid), nullptr);

  UnitVisualSpec mounted{};
  mounted.kind = CreatureKind::Mounted;
  EXPECT_EQ(resolve_creature_visual_definition(mounted), nullptr);
}

TEST(CreatureVisualDefinition, CreatureAssetsExposeLargeCreatureDefinitions) {
  const auto &registry = CreatureAssetRegistry::instance();

  UnitVisualSpec horse{};
  horse.kind = CreatureKind::Horse;
  const auto *horse_asset = registry.resolve(horse);
  ASSERT_NE(horse_asset, nullptr);
  EXPECT_EQ(horse_asset->visual_definition,
            &horse_creature_visual_definition());

  UnitVisualSpec elephant{};
  elephant.kind = CreatureKind::Elephant;
  const auto *elephant_asset = registry.resolve(elephant);
  ASSERT_NE(elephant_asset, nullptr);
  EXPECT_EQ(elephant_asset->visual_definition,
            &elephant_creature_visual_definition());
}

TEST(CreatureVisualDefinition, SharedMotionContractsDefaultSafely) {
  CreatureMotionInputs inputs{};
  EXPECT_FLOAT_EQ(inputs.facing_velocity_dot, 1.0F);

  CreatureMotionOutput output{};
  EXPECT_EQ(output.locomotion_mode, CreatureLocomotionMode::Idle);
  EXPECT_EQ(output.contacts.size(), kLargeCreatureLegCount);

  CreatureMotionState state{};
  EXPECT_FALSE(state.initialized);
  EXPECT_EQ(state.planted_targets.size(), kLargeCreatureLegCount);
}

} // namespace
