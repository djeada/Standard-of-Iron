

#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>

#include <gtest/gtest.h>
#include <utility>
#include <vector>

#include "render/creature/pipeline/creature_asset.h"
#include "render/creature/render_request.h"
#include "render/humanoid/asset/humanoid_asset_key.h"
#include "render/rigged_mesh_cache.h"

namespace {

using Render::Creature::CreatureRenderRequest;
using Render::Humanoid::asset_key_of;
using Render::Humanoid::HumanoidAssetKey;
using Render::Humanoid::HumanoidAssetKeyHash;

auto base_request() -> CreatureRenderRequest {
  CreatureRenderRequest request{};
  request.archetype = 7U;
  request.variant = 2U;
  request.creature_asset_id = Render::Creature::Pipeline::k_humanoid_sword_asset;
  request.lod = Render::Creature::CreatureLOD::Full;
  request.state = Render::Creature::AnimationStateId::Idle;
  request.phase = 0.25F;
  request.seed = 1234U;
  request.clip_id = 11U;
  request.clip_variant = 1U;
  request.base_color = QVector3D(0.2F, 0.4F, 0.6F);
  request.wear_params = QVector4D(0.1F, 0.2F, 0.3F, 0.4F);
  request.role_color_count = 5U;
  return request;
}

const std::vector<std::pair<const char*, void (*)(CreatureRenderRequest&)>>
    k_runtime_mutations{
        {"team colour",
         [](CreatureRenderRequest& r) {
           r.base_color = QVector3D(1.0F, 0.0F, 0.0F);
         }},
        {"wear intensity",
         [](CreatureRenderRequest& r) {
           r.wear_params = QVector4D(0.9F, 0.9F, 0.9F, 0.9F);
         }},
        {"animation state",
         [](CreatureRenderRequest& r) {
           r.state = Render::Creature::AnimationStateId::Walk;
         }},
        {"animation time",
         [](CreatureRenderRequest& r) {
           r.phase = 0.87F;
         }},
        {"clip",
         [](CreatureRenderRequest& r) {
           r.clip_id = 42U;
         }},
        {"attack phase",
         [](CreatureRenderRequest& r) {
           r.clip_variant = 3U;
         }},
        {"world position",
         [](CreatureRenderRequest& r) {
           r.world.translate(3.0F, 0.0F, -2.0F);
         }},
        {"formation slot",
         [](CreatureRenderRequest& r) {
           r.instance_index = 5U;
         }},
        {"seed",
         [](CreatureRenderRequest& r) {
           r.seed = 999U;
         }},
        {"entity",
         [](CreatureRenderRequest& r) {
           r.entity_id = 77U;
         }},
        {"pass",
         [](CreatureRenderRequest& r) {
           r.pass = Render::Creature::Pipeline::RenderPassIntent::Shadow;
         }},
        {"grounding",
         [](CreatureRenderRequest& r) {
           r.world_already_grounded = true;
         }},
        {"role colour count",
         [](CreatureRenderRequest& r) {
           r.role_color_count = 9U;
         }},
        {"hit reaction overlay",
         [](CreatureRenderRequest& r) {
           r.upper_body_overlay.mode =
               Render::Creature::PlaybackLayerMode::UpperBodyOverlay;
           r.upper_body_overlay.archetype = 7U;
           r.upper_body_overlay.weight = 0.5F;
           r.upper_body_overlay.phase = 0.3F;
         }},
        {"full body blend",
         [](CreatureRenderRequest& r) {
           r.full_body_blend.mode = Render::Creature::PlaybackLayerMode::FullBodyBlend;
           r.full_body_blend.archetype = 7U;
           r.full_body_blend.weight = 0.75F;
         }},
    };

} // namespace

TEST(HumanoidAssetKey, RuntimeFieldsNeverChangeTheAssetKey) {
  const auto reference = asset_key_of(base_request(), nullptr);

  for (const auto& [name, mutate] : k_runtime_mutations) {
    auto request = base_request();
    mutate(request);
    const auto mutated = asset_key_of(request, nullptr);
    EXPECT_TRUE(reference == mutated)
        << "mutating '" << name << "' changed the humanoid asset key";
    EXPECT_EQ(HumanoidAssetKeyHash{}(reference), HumanoidAssetKeyHash{}(mutated))
        << "mutating '" << name << "' changed the humanoid asset key hash";
  }
}

TEST(HumanoidAssetKey, RuntimeFieldsDoReachTheInstanceParameters) {
  const auto reference = Render::Humanoid::instance_params_of(base_request());

  auto moved = base_request();
  moved.world.translate(3.0F, 0.0F, -2.0F);
  EXPECT_NE(Render::Humanoid::instance_params_of(moved).world, reference.world);

  auto tinted = base_request();
  tinted.base_color = QVector3D(1.0F, 0.0F, 0.0F);
  EXPECT_NE(Render::Humanoid::instance_params_of(tinted).team_tint,
            reference.team_tint);

  auto advanced = base_request();
  advanced.phase = 0.87F;
  EXPECT_NE(Render::Humanoid::instance_params_of(advanced).frame, reference.frame);

  auto reclipped = base_request();
  reclipped.clip_id = 42U;
  EXPECT_NE(Render::Humanoid::instance_params_of(reclipped).clip, reference.clip);
}

TEST(HumanoidAssetKey, GeometryFieldsDoChangeTheAssetKey) {
  const auto reference = asset_key_of(base_request(), nullptr);

  auto other_archetype = base_request();
  other_archetype.archetype = 8U;
  EXPECT_FALSE(reference == asset_key_of(other_archetype, nullptr));

  auto other_lod = base_request();
  other_lod.lod = Render::Creature::CreatureLOD::Minimal;
  EXPECT_FALSE(reference == asset_key_of(other_lod, nullptr));

  auto other_variant = base_request();
  other_variant.variant = 3U;
  EXPECT_FALSE(reference == asset_key_of(other_variant, nullptr));

  auto other_asset = base_request();
  other_asset.creature_asset_id = Render::Creature::Pipeline::k_humanoid_spear_asset;
  EXPECT_FALSE(reference == asset_key_of(other_asset, nullptr));
}

TEST(HumanoidAssetKey, RiggedCacheKeyIsAProjectionOfTheAssetKey) {
  Render::Creature::Pipeline::CreatureAsset asset{};
  asset.id = Render::Creature::Pipeline::k_humanoid_sword_asset;
  asset.bpat_species_id = 3U;
  const auto* spec_marker = reinterpret_cast<const Render::Creature::CreatureSpec*>(
      static_cast<std::uintptr_t>(0x1000U));
  asset.spec = spec_marker;

  HumanoidAssetKey key{};
  key.archetype = 7U;
  key.lod = Render::Creature::CreatureLOD::Minimal;
  key.attachment_set = 12U;
  key.attachments_hash = 0xdeadbeefULL;
  key.skin_species = 3U;
  key.creature_asset = asset.id;

  const auto cache_key = Render::Humanoid::rigged_cache_key(asset, key);
  EXPECT_EQ(cache_key.spec, spec_marker);
  EXPECT_EQ(cache_key.lod, key.lod);
  EXPECT_EQ(cache_key.skin_species_id, key.skin_species);
  EXPECT_EQ(cache_key.attachment_set_id, key.attachment_set);
  EXPECT_EQ(cache_key.attachments_hash, key.attachments_hash);

  auto same = key;
  EXPECT_TRUE(Render::Humanoid::rigged_cache_key(asset, same) == cache_key);

  auto different_lod = key;
  different_lod.lod = Render::Creature::CreatureLOD::Full;
  EXPECT_FALSE(Render::Humanoid::rigged_cache_key(asset, different_lod) == cache_key);
}
