#include "render/creature/archetype_registry.h"
#include "render/creature/bpat/bpat_format.h"
#include "render/creature/bpat/bpat_registry.h"
#include "render/creature/pipeline/creature_asset.h"
#include "render/elephant/elephant_spec.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/horse/horse_spec.h"
#include "render/humanoid/humanoid_spec.h"
#include "render/humanoid/skeleton.h"
#include "tests/render/test_asset_paths.h"

#include <gtest/gtest.h>

namespace {

using namespace Render::Creature::Pipeline;

TEST(CreatureAssetRegistry, ResolvesHumanoidByKindFallback) {
  UnitVisualSpec spec{};
  spec.kind = CreatureKind::Humanoid;

  const auto *asset = CreatureAssetRegistry::instance().resolve(spec);
  ASSERT_NE(asset, nullptr);
  EXPECT_EQ(asset->kind, CreatureKind::Humanoid);
  EXPECT_EQ(asset->spec, &Render::Humanoid::humanoid_creature_spec());
  EXPECT_EQ(asset->role_count, Render::Humanoid::k_humanoid_role_count);
}

TEST(CreatureAssetRegistry, ExplicitAssetIdWinsWhenProvided) {
  UnitVisualSpec spec{};
  spec.kind = CreatureKind::Horse;
  spec.creature_asset_id = k_humanoid_asset;

  const auto *asset = CreatureAssetRegistry::instance().resolve(spec);
  ASSERT_NE(asset, nullptr);
  EXPECT_EQ(asset->kind, CreatureKind::Humanoid);
  EXPECT_EQ(asset->spec, &Render::Humanoid::humanoid_creature_spec());
}

TEST(CreatureAssetRegistry, SwordHumanoidAssetUsesDedicatedBpatSpecies) {
  UnitVisualSpec spec{};
  spec.kind = CreatureKind::Humanoid;
  spec.creature_asset_id = k_humanoid_sword_asset;

  const auto *asset = CreatureAssetRegistry::instance().resolve(spec);
  ASSERT_NE(asset, nullptr);
  EXPECT_EQ(asset->id, k_humanoid_sword_asset);
  EXPECT_EQ(asset->kind, CreatureKind::Humanoid);
  EXPECT_EQ(asset->bpat_species_id,
            Render::Creature::Bpat::k_species_humanoid_sword);
}

TEST(CreatureAssetRegistry, ResolvesHorseAndElephantByKindFallback) {
  UnitVisualSpec horse{};
  horse.kind = CreatureKind::Horse;
  const auto *horse_asset = CreatureAssetRegistry::instance().resolve(horse);
  ASSERT_NE(horse_asset, nullptr);
  EXPECT_EQ(horse_asset->kind, CreatureKind::Horse);
  EXPECT_EQ(horse_asset->spec, &Render::Horse::horse_creature_spec());
  EXPECT_EQ(horse_asset->role_count, 8u);
  EXPECT_EQ(horse_asset->snapshot_mesh_species_id,
            Render::Creature::Bpat::k_species_horse);
  EXPECT_NE(horse_asset->snapshot_mesh_lod_mask, 0U);

  UnitVisualSpec elephant{};
  elephant.kind = CreatureKind::Elephant;
  const auto *elephant_asset =
      CreatureAssetRegistry::instance().resolve(elephant);
  ASSERT_NE(elephant_asset, nullptr);
  EXPECT_EQ(elephant_asset->kind, CreatureKind::Elephant);
  EXPECT_EQ(elephant_asset->spec, &Render::Elephant::elephant_creature_spec());
  EXPECT_EQ(elephant_asset->role_count,
            Render::Elephant::k_elephant_role_count);
  EXPECT_EQ(elephant_asset->snapshot_mesh_species_id,
            Render::Creature::Bpat::k_species_elephant);
  EXPECT_NE(elephant_asset->snapshot_mesh_lod_mask, 0U);
}

TEST(CreatureAssetRegistry, MountedSpecsRequireExplicitSplitRows) {
  UnitVisualSpec spec{};
  spec.kind = CreatureKind::Mounted;

  EXPECT_EQ(CreatureAssetRegistry::instance().resolve(spec), nullptr);
}

TEST(CreatureAssetRegistry, AllAssetsHaveBindPaletteCallbacks) {
  const auto &reg = CreatureAssetRegistry::instance();

  UnitVisualSpec humanoid{};
  humanoid.kind = CreatureKind::Humanoid;
  const auto *h = reg.resolve(humanoid);
  ASSERT_NE(h, nullptr);
  EXPECT_NE(h->bind_palette, nullptr);
  EXPECT_EQ(h->max_bones, Render::Humanoid::k_bone_count);

  UnitVisualSpec horse{};
  horse.kind = CreatureKind::Horse;
  const auto *hr = reg.resolve(horse);
  ASSERT_NE(hr, nullptr);
  EXPECT_NE(hr->bind_palette, nullptr);
  EXPECT_EQ(hr->max_bones, Render::Horse::k_horse_bone_count);

  UnitVisualSpec elephant{};
  elephant.kind = CreatureKind::Elephant;
  const auto *el = reg.resolve(elephant);
  ASSERT_NE(el, nullptr);
  EXPECT_NE(el->bind_palette, nullptr);
  EXPECT_EQ(el->max_bones, Render::Elephant::k_elephant_bone_count);
}

TEST(CreatureAssetRegistry, BindPaletteCallbacksProduceValidOutput) {
  const auto &reg = CreatureAssetRegistry::instance();

  UnitVisualSpec hspec{};
  hspec.kind = CreatureKind::Humanoid;
  const auto *h = reg.resolve(hspec);
  ASSERT_NE(h, nullptr);
  auto hbind = h->bind_palette();
  EXPECT_FALSE(hbind.empty());

  UnitVisualSpec hrspec{};
  hrspec.kind = CreatureKind::Horse;
  const auto *hr = reg.resolve(hrspec);
  ASSERT_NE(hr, nullptr);
  auto hrbind = hr->bind_palette();
  EXPECT_FALSE(hrbind.empty());

  UnitVisualSpec elspec{};
  elspec.kind = CreatureKind::Elephant;
  const auto *el = reg.resolve(elspec);
  ASSERT_NE(el, nullptr);
  auto elbind = el->bind_palette();
  EXPECT_FALSE(elbind.empty());
}

TEST(CreatureAssetRegistry, MaxCreatureBonesCoversAllSpecies) {
  EXPECT_GE(k_max_creature_bones, Render::Humanoid::k_bone_count);
  EXPECT_GE(k_max_creature_bones, Render::Horse::k_horse_bone_count);
  EXPECT_GE(k_max_creature_bones, Render::Elephant::k_elephant_bone_count);
}

TEST(CreatureAssetRegistry, AllAssetsHaveFillRoleColorsCallback) {
  const auto &reg = CreatureAssetRegistry::instance();

  UnitVisualSpec humanoid{};
  humanoid.kind = CreatureKind::Humanoid;
  const auto *h = reg.resolve(humanoid);
  ASSERT_NE(h, nullptr);
  EXPECT_NE(h->fill_role_colors, nullptr);

  UnitVisualSpec horse{};
  horse.kind = CreatureKind::Horse;
  const auto *hr = reg.resolve(horse);
  ASSERT_NE(hr, nullptr);
  EXPECT_NE(hr->fill_role_colors, nullptr);

  UnitVisualSpec elephant{};
  elephant.kind = CreatureKind::Elephant;
  const auto *el = reg.resolve(elephant);
  ASSERT_NE(el, nullptr);
  EXPECT_NE(el->fill_role_colors, nullptr);
}

TEST(CreatureAssetRegistry, FillRoleColorsProducesValidOutput) {
  const auto &reg = CreatureAssetRegistry::instance();

  UnitVisualSpec hspec{};
  hspec.kind = CreatureKind::Humanoid;
  const auto *h = reg.resolve(hspec);
  ASSERT_NE(h, nullptr);
  Render::GL::HumanoidVariant hvar{};
  hvar.palette.cloth = QVector3D(0.8F, 0.1F, 0.1F);
  hvar.palette.skin = QVector3D(0.7F, 0.55F, 0.4F);
  std::array<QVector3D, 16> hroles{};
  auto hcount = h->fill_role_colors(&hvar, hroles.data(), hroles.size());
  EXPECT_GT(hcount, 0U);
  EXPECT_LE(hcount, h->role_count);

  UnitVisualSpec hrspec{};
  hrspec.kind = CreatureKind::Horse;
  const auto *hr = reg.resolve(hrspec);
  ASSERT_NE(hr, nullptr);
  Render::GL::HorseVariant hrvar{};
  hrvar.coat_color = QVector3D(0.6F, 0.4F, 0.2F);
  std::array<QVector3D, 16> hrroles{};
  auto hrcount = hr->fill_role_colors(&hrvar, hrroles.data(), hrroles.size());
  EXPECT_GT(hrcount, 0U);
  EXPECT_LE(hrcount, hr->role_count);

  UnitVisualSpec elspec{};
  elspec.kind = CreatureKind::Elephant;
  const auto *el = reg.resolve(elspec);
  ASSERT_NE(el, nullptr);
  Render::GL::ElephantVariant elvar{};
  elvar.skin_color = QVector3D(0.6F, 0.56F, 0.53F);
  std::array<QVector3D, 16> elroles{};
  auto elcount = el->fill_role_colors(&elvar, elroles.data(), elroles.size());
  EXPECT_GT(elcount, 0U);
  EXPECT_LE(elcount, el->role_count);
}

TEST(CreatureRenderAssetHandleRegistry, CreatesStableHandleAfterBpatLoad) {
  auto const root = TestAssets::find_creature_assets_dir("humanoid.bpat");
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found";
  }

  auto &bpat = Render::Creature::Bpat::BpatRegistry::instance();
  ASSERT_TRUE(bpat.load_species(Render::Creature::Bpat::k_species_humanoid,
                                root + "/humanoid.bpat"));

  auto &handles = CreatureRenderAssetHandleRegistry::instance();
  handles.clear();

  auto const id = handles.get_or_create(
      k_humanoid_asset, Render::Creature::ArchetypeRegistry::k_humanoid_base);
  ASSERT_NE(id, Render::Creature::k_invalid_creature_render_asset_handle);
  EXPECT_EQ(handles.get_or_create(
                k_humanoid_asset,
                Render::Creature::ArchetypeRegistry::k_humanoid_base),
            id);

  const auto *handle = handles.get(id);
  ASSERT_NE(handle, nullptr);
  ASSERT_TRUE(handle->valid());
  ASSERT_NE(handle->asset, nullptr);
  ASSERT_NE(handle->archetype, nullptr);
  EXPECT_EQ(handle->asset->id, k_humanoid_asset);
  EXPECT_EQ(handle->archetype->id,
            Render::Creature::ArchetypeRegistry::k_humanoid_base);
  EXPECT_NE(handle->attachment_set_id, k_invalid_attachment_set_id);

  const auto &idle = handle->playback[static_cast<std::size_t>(
      Render::Creature::AnimationStateId::Idle)];
  EXPECT_NE(idle.blob, nullptr);
  EXPECT_NE(idle.clip_id,
            Render::Creature::ArchetypeDescriptor::k_unmapped_clip);
  EXPECT_GT(idle.frame_count, 0U);
}

TEST(CreatureRenderAssetHandleRegistry,
     AttachmentSetIdDeduplicatesIdenticalSets) {
  auto const root = TestAssets::find_creature_assets_dir("humanoid.bpat");
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found";
  }

  auto &bpat = Render::Creature::Bpat::BpatRegistry::instance();
  ASSERT_TRUE(bpat.load_species(Render::Creature::Bpat::k_species_humanoid,
                                root + "/humanoid.bpat"));

  auto &handles = CreatureRenderAssetHandleRegistry::instance();
  handles.clear();

  const auto base_id = handles.get_or_create(
      k_humanoid_asset, Render::Creature::ArchetypeRegistry::k_humanoid_base);
  const auto rider_id = handles.get_or_create(
      k_humanoid_asset, Render::Creature::ArchetypeRegistry::k_rider_base);
  ASSERT_NE(base_id, Render::Creature::k_invalid_creature_render_asset_handle);
  ASSERT_NE(rider_id, Render::Creature::k_invalid_creature_render_asset_handle);

  const auto *base = handles.get(base_id);
  const auto *rider = handles.get(rider_id);
  ASSERT_NE(base, nullptr);
  ASSERT_NE(rider, nullptr);
  EXPECT_NE(base->attachment_set_id, k_invalid_attachment_set_id);
  EXPECT_NE(rider->attachment_set_id, k_invalid_attachment_set_id);
  EXPECT_EQ(base->attachment_set_id, rider->attachment_set_id);

  const auto base_id_again = handles.get_or_create(
      k_humanoid_asset, Render::Creature::ArchetypeRegistry::k_humanoid_base);
  const auto *base_again = handles.get(base_id_again);
  ASSERT_NE(base_again, nullptr);
  EXPECT_EQ(base_again->attachment_set_id, base->attachment_set_id);
}

} // namespace
