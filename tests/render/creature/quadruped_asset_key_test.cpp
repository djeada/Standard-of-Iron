

#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>

#include <gtest/gtest.h>
#include <utility>
#include <vector>

#include "animation/bpat/bpat_reader.h"
#include "render/creature/archetype_registry.h"
#include "render/creature/pipeline/creature_asset.h"
#include "render/creature/quadruped/quadruped_asset_key.h"
#include "render/creature/render_request.h"
#include "render/creature/schema/skeleton_schema_hash.h"
#include "render/elephant/elephant_spec.h"
#include "render/horse/horse_spec.h"

namespace {

using Render::Creature::CreatureRenderRequest;
using Render::Creature::Quadruped::asset_key_of;
using Render::Creature::Quadruped::QuadrupedAssetKey;
using Render::Creature::Quadruped::QuadrupedAssetKeyHash;

constexpr auto k_horse = Render::Creature::Pipeline::CreatureKind::Horse;

auto base_request() -> CreatureRenderRequest {
  CreatureRenderRequest request{};
  request.archetype = 3U;
  request.variant = 1U;
  request.creature_asset_id = Render::Creature::Pipeline::k_horse_asset;
  request.lod = Render::Creature::CreatureLOD::Full;
  request.state = Render::Creature::AnimationStateId::Walk;
  request.phase = 0.25F;
  request.seed = 4242U;
  request.base_color = QVector3D(0.4F, 0.3F, 0.2F);
  request.wear_params = QVector4D(0.1F, 0.2F, 0.3F, 0.4F);
  return request;
}

const std::vector<std::pair<const char*, void (*)(CreatureRenderRequest&)>>
    k_runtime_mutations{
        {"gait / animation state",
         [](CreatureRenderRequest& r) {
           r.state = Render::Creature::AnimationStateId::Run;
         }},
        {"animation phase",
         [](CreatureRenderRequest& r) {
           r.phase = 0.9F;
         }},
        {"world position",
         [](CreatureRenderRequest& r) {
           r.world.translate(5.0F, 0.0F, 2.0F);
         }},
        {"team tint",
         [](CreatureRenderRequest& r) {
           r.base_color = QVector3D(1.0F, 0.0F, 0.0F);
         }},
        {"wear",
         [](CreatureRenderRequest& r) {
           r.wear_params = QVector4D(0.9F, 0.9F, 0.9F, 0.9F);
         }},
        {"visual seed",
         [](CreatureRenderRequest& r) {
           r.seed = 7U;
         }},
        {"rider slot",
         [](CreatureRenderRequest& r) {
           r.instance_index = 3U;
         }},
        {"entity",
         [](CreatureRenderRequest& r) {
           r.entity_id = 99U;
         }},
        {"clip",
         [](CreatureRenderRequest& r) {
           r.clip_id = 12U;
         }},
        {"grounding",
         [](CreatureRenderRequest& r) {
           r.world_already_grounded = true;
         }},
        {"pass",
         [](CreatureRenderRequest& r) {
           r.pass = Render::Creature::Pipeline::RenderPassIntent::Shadow;
         }},
    };

} // namespace

TEST(QuadrupedAssetKey, RuntimeFieldsNeverChangeTheAssetKey) {
  const auto reference = asset_key_of(base_request(), k_horse);

  for (const auto& [name, mutate] : k_runtime_mutations) {
    auto request = base_request();
    mutate(request);
    const auto mutated = asset_key_of(request, k_horse);
    EXPECT_TRUE(reference == mutated)
        << "mutating '" << name << "' changed the quadruped asset key";
    EXPECT_EQ(QuadrupedAssetKeyHash{}(reference), QuadrupedAssetKeyHash{}(mutated))
        << "mutating '" << name << "' changed the quadruped asset key hash";
  }
}

TEST(QuadrupedAssetKey, RuntimeFieldsDoReachTheInstanceState) {
  const auto reference = Render::Creature::Quadruped::instance_state_of(base_request());

  auto advanced = base_request();
  advanced.phase = 0.9F;
  EXPECT_NE(Render::Creature::Quadruped::instance_state_of(advanced).phase,
            reference.phase);

  auto moved = base_request();
  moved.world.translate(5.0F, 0.0F, 2.0F);
  EXPECT_NE(Render::Creature::Quadruped::instance_state_of(moved).world,
            reference.world);

  auto regaited = base_request();
  regaited.state = Render::Creature::AnimationStateId::Run;
  EXPECT_NE(Render::Creature::Quadruped::instance_state_of(regaited).animation,
            reference.animation);
}

TEST(QuadrupedAssetKey, GeometryFieldsDoChangeTheAssetKey) {
  const auto reference = asset_key_of(base_request(), k_horse);

  auto other_lod = base_request();
  other_lod.lod = Render::Creature::CreatureLOD::Minimal;
  EXPECT_FALSE(reference == asset_key_of(other_lod, k_horse));

  auto other_archetype = base_request();
  other_archetype.archetype = 9U;
  EXPECT_FALSE(reference == asset_key_of(other_archetype, k_horse));

  auto other_asset = base_request();
  other_asset.creature_asset_id = Render::Creature::Pipeline::k_elephant_asset;
  EXPECT_FALSE(reference == asset_key_of(other_asset, k_horse));

  auto other_body = base_request();
  other_body.variant = 5U;
  EXPECT_FALSE(reference == asset_key_of(other_body, k_horse));

  EXPECT_FALSE(
      reference ==
      asset_key_of(base_request(), Render::Creature::Pipeline::CreatureKind::Elephant));
}

TEST(CreatureSkeletonSchema, EachSpeciesHashIsStableAndDistinct) {
  const auto horse =
      Render::Creature::skeleton_schema_hash(Render::Horse::horse_topology());
  const auto elephant =
      Render::Creature::skeleton_schema_hash(Render::Elephant::elephant_topology());

  EXPECT_EQ(horse,
            Render::Creature::skeleton_schema_hash(Render::Horse::horse_topology()))
      << "the same topology hashed twice must give the same answer";
  EXPECT_NE(horse, elephant) << "two different skeletons share a schema hash";
  EXPECT_NE(horse, 0U);
}

TEST(CreatureSkeletonSchema, BakedAnimationMatchesTheRuntimeSkeleton) {
  struct Species {
    const char* name;
    const Render::Creature::SkeletonTopology& topology;
    Render::Creature::Pipeline::CreatureAssetId asset;
    Render::Creature::ArchetypeId archetype;
  };

  const std::vector<Species> species{
      {"horse",
       Render::Horse::horse_topology(),
       Render::Creature::Pipeline::k_horse_asset,
       Render::Creature::ArchetypeRegistry::k_horse_base},
      {"elephant",
       Render::Elephant::elephant_topology(),
       Render::Creature::Pipeline::k_elephant_asset,
       Render::Creature::ArchetypeRegistry::k_elephant_base}};

  auto& registry =
      Render::Creature::Pipeline::CreatureRenderAssetHandleRegistry::instance();
  for (const auto& entry : species) {
    SCOPED_TRACE(entry.name);
    const auto id = registry.get_or_create(entry.asset, entry.archetype);
    const auto* handle = registry.get(id);
    ASSERT_NE(handle, nullptr);
    ASSERT_TRUE(handle->valid());

    const Render::Creature::Bpat::BpatBlob* blob = nullptr;
    for (const auto& playback : handle->playback) {
      if (playback.blob != nullptr && playback.frame_count != 0U) {
        blob = playback.blob;
        break;
      }
    }
    ASSERT_NE(blob, nullptr) << "no baked clips to check against";

    EXPECT_TRUE(Render::Creature::bone_parents_match(
        entry.topology, blob->bone_parents(), blob->bone_count()))
        << "the baked animation was built against a different skeleton";
  }
}
