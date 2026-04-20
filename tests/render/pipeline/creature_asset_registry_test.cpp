#include "render/creature/pipeline/creature_asset.h"
#include "render/elephant/elephant_spec.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/horse/horse_spec.h"
#include "render/humanoid/humanoid_spec.h"
#include "render/humanoid/skeleton.h"

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
  EXPECT_EQ(asset->role_count, Render::Humanoid::kHumanoidRoleCount);
}

TEST(CreatureAssetRegistry, ExplicitAssetIdWinsWhenProvided) {
  UnitVisualSpec spec{};
  spec.kind = CreatureKind::Horse;
  spec.creature_asset_id = 0;

  const auto *asset = CreatureAssetRegistry::instance().resolve(spec);
  ASSERT_NE(asset, nullptr);
  EXPECT_EQ(asset->kind, CreatureKind::Humanoid);
  EXPECT_EQ(asset->spec, &Render::Humanoid::humanoid_creature_spec());
}

TEST(CreatureAssetRegistry, ResolvesHorseAndElephantByKindFallback) {
  UnitVisualSpec horse{};
  horse.kind = CreatureKind::Horse;
  const auto *horse_asset = CreatureAssetRegistry::instance().resolve(horse);
  ASSERT_NE(horse_asset, nullptr);
  EXPECT_EQ(horse_asset->kind, CreatureKind::Horse);
  EXPECT_EQ(horse_asset->spec, &Render::Horse::horse_creature_spec());
  EXPECT_EQ(horse_asset->role_count, 8u);

  UnitVisualSpec elephant{};
  elephant.kind = CreatureKind::Elephant;
  const auto *elephant_asset =
      CreatureAssetRegistry::instance().resolve(elephant);
  ASSERT_NE(elephant_asset, nullptr);
  EXPECT_EQ(elephant_asset->kind, CreatureKind::Elephant);
  EXPECT_EQ(elephant_asset->spec, &Render::Elephant::elephant_creature_spec());
  EXPECT_EQ(elephant_asset->role_count, Render::Elephant::kElephantRoleCount);
}

TEST(CreatureAssetRegistry, MountedSpecsRequireExplicitSplitRows) {
  UnitVisualSpec spec{};
  spec.kind = CreatureKind::Mounted;

  EXPECT_EQ(CreatureAssetRegistry::instance().resolve(spec), nullptr);
}

TEST(CreatureAssetRegistry, AllAssetsHaveBonePaletteCallbacks) {
  const auto &reg = CreatureAssetRegistry::instance();

  UnitVisualSpec humanoid{};
  humanoid.kind = CreatureKind::Humanoid;
  const auto *h = reg.resolve(humanoid);
  ASSERT_NE(h, nullptr);
  EXPECT_NE(h->compute_bones, nullptr);
  EXPECT_NE(h->bind_palette, nullptr);
  EXPECT_EQ(h->max_bones, Render::Humanoid::kBoneCount);

  UnitVisualSpec horse{};
  horse.kind = CreatureKind::Horse;
  const auto *hr = reg.resolve(horse);
  ASSERT_NE(hr, nullptr);
  EXPECT_NE(hr->compute_bones, nullptr);
  EXPECT_NE(hr->bind_palette, nullptr);
  EXPECT_EQ(hr->max_bones, Render::Horse::kHorseBoneCount);

  UnitVisualSpec elephant{};
  elephant.kind = CreatureKind::Elephant;
  const auto *el = reg.resolve(elephant);
  ASSERT_NE(el, nullptr);
  EXPECT_NE(el->compute_bones, nullptr);
  EXPECT_NE(el->bind_palette, nullptr);
  EXPECT_EQ(el->max_bones, Render::Elephant::kElephantBoneCount);
}

TEST(CreatureAssetRegistry, BonePaletteCallbacksProduceValidOutput) {
  const auto &reg = CreatureAssetRegistry::instance();

  // Humanoid
  UnitVisualSpec hspec{};
  hspec.kind = CreatureKind::Humanoid;
  const auto *h = reg.resolve(hspec);
  ASSERT_NE(h, nullptr);
  Render::GL::HumanoidPose hpose{};
  std::array<QMatrix4x4, kMaxCreatureBones> hbones{};
  auto hcount = h->compute_bones(&hpose, hbones);
  EXPECT_GT(hcount, 0U);
  EXPECT_LE(hcount, Render::Humanoid::kBoneCount);
  auto hbind = h->bind_palette();
  EXPECT_FALSE(hbind.empty());

  // Horse
  UnitVisualSpec hrspec{};
  hrspec.kind = CreatureKind::Horse;
  const auto *hr = reg.resolve(hrspec);
  ASSERT_NE(hr, nullptr);
  Render::Horse::HorseSpecPose hrpose{};
  std::array<QMatrix4x4, kMaxCreatureBones> hrbones{};
  auto hrcount = hr->compute_bones(&hrpose, hrbones);
  EXPECT_GT(hrcount, 0U);
  EXPECT_LE(hrcount, Render::Horse::kHorseBoneCount);
  auto hrbind = hr->bind_palette();
  EXPECT_FALSE(hrbind.empty());

  // Elephant
  UnitVisualSpec elspec{};
  elspec.kind = CreatureKind::Elephant;
  const auto *el = reg.resolve(elspec);
  ASSERT_NE(el, nullptr);
  Render::Elephant::ElephantSpecPose elpose{};
  std::array<QMatrix4x4, kMaxCreatureBones> elbones{};
  auto elcount = el->compute_bones(&elpose, elbones);
  EXPECT_GT(elcount, 0U);
  EXPECT_LE(elcount, Render::Elephant::kElephantBoneCount);
  auto elbind = el->bind_palette();
  EXPECT_FALSE(elbind.empty());
}

TEST(CreatureAssetRegistry, MaxCreatureBonesCoversAllSpecies) {
  EXPECT_GE(kMaxCreatureBones, Render::Humanoid::kBoneCount);
  EXPECT_GE(kMaxCreatureBones, Render::Horse::kHorseBoneCount);
  EXPECT_GE(kMaxCreatureBones, Render::Elephant::kElephantBoneCount);
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

  // Humanoid
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

  // Horse
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

  // Elephant
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

} // namespace
