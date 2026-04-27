// Tests for the per-unit `ArchetypeId` plumbing introduced by
// `p5-merged-archetype-mesh` Step 1.
//
// Verifies:
//   * `ArchetypeRegistry::register_unit_archetype` clones a baseline
//     descriptor's clip / snapshot / role state and overlays a span of
//     `StaticAttachmentSpec`s, returning a fresh, dense id distinct
//     from the species baseline.
//   * The new descriptor exposes its attachments via `attachments_view()`
//     so the bake/cache pipeline picks them up.
//   * Excess attachments past `kMaxBakeAttachments` are dropped (clamped).
//   * `Mounted` species is rejected.
//   * `UnitVisualSpec::archetype_id` defaults to `kInvalidArchetype`.

#include "render/creature/archetype_registry.h"
#include "render/creature/pipeline/unit_visual_spec.h"
#include "render/creature/render_request.h"
#include "render/render_archetype.h"
#include "render/static_attachment_spec.h"

#include <QVector3D>
#include <array>
#include <gtest/gtest.h>

namespace {

using namespace Render::Creature;
using Render::Creature::Pipeline::CreatureKind;
using Render::Creature::Pipeline::UnitVisualSpec;
using Render::GL::RenderArchetype;
using Render::GL::RenderArchetypeBuilder;
using Render::GL::RenderArchetypeLod;

auto make_test_archetype() -> const RenderArchetype & {
  static const RenderArchetype arch = [] {
    RenderArchetypeBuilder b("per_unit_archetype_test_helmet");
    b.use_lod(RenderArchetypeLod::Full);
    b.add_palette_box(QVector3D{0.0F, 0.0F, 0.0F}, QVector3D{1.0F, 1.0F, 1.0F},
                      /*palette_slot=*/1);
    return std::move(b).build();
  }();
  return arch;
}

TEST(PerUnitArchetype, UnitVisualSpecDefaultsToInvalidArchetype) {
  UnitVisualSpec spec{};
  EXPECT_EQ(spec.archetype_id, kInvalidArchetype);
}

TEST(PerUnitArchetype, RegisterUnitArchetypeReturnsDistinctIdFromBaseline) {
  auto &reg = ArchetypeRegistry::instance();

  StaticAttachmentSpec att{};
  att.archetype = &make_test_archetype();
  att.socket_bone_index = 0;
  att.palette_role_remap[1] = 5;
  std::array<StaticAttachmentSpec, 1> attachments{att};

  auto const id = reg.register_unit_archetype(
      "per_unit_archetype_test/humanoid_unit", CreatureKind::Humanoid,
      std::span<const StaticAttachmentSpec>{attachments});

  ASSERT_NE(id, kInvalidArchetype);
  EXPECT_NE(id, ArchetypeRegistry::kHumanoidBase);
  EXPECT_NE(id, ArchetypeRegistry::kHorseBase);
  EXPECT_NE(id, ArchetypeRegistry::kElephantBase);

  auto const *desc = reg.get(id);
  ASSERT_NE(desc, nullptr);
  EXPECT_EQ(desc->species, CreatureKind::Humanoid);
  EXPECT_EQ(desc->bake_attachment_count, 1U);
  ASSERT_EQ(desc->attachments_view().size(), 1U);
  EXPECT_EQ(desc->attachments_view()[0].archetype, &make_test_archetype());
  EXPECT_EQ(desc->attachments_view()[0].palette_role_remap[1], 5U);

  // Clip table inherits from the humanoid baseline.
  auto const *base = reg.get(ArchetypeRegistry::kHumanoidBase);
  ASSERT_NE(base, nullptr);
  for (std::size_t i = 0; i < animation_state_count(); ++i) {
    EXPECT_EQ(desc->bpat_clip[i], base->bpat_clip[i]) << "state idx " << i;
    EXPECT_EQ(desc->snapshot[i], base->snapshot[i]) << "snapshot idx " << i;
  }
}

TEST(PerUnitArchetype, BaselineStillExposesNoAttachments) {
  auto const &reg = ArchetypeRegistry::instance();
  auto const *humanoid = reg.get(ArchetypeRegistry::kHumanoidBase);
  ASSERT_NE(humanoid, nullptr);
  EXPECT_EQ(humanoid->bake_attachment_count, 0U);
  EXPECT_TRUE(humanoid->attachments_view().empty());
}

TEST(PerUnitArchetype, RegisterUnitArchetypeForHorseInheritsHorseClipTable) {
  auto &reg = ArchetypeRegistry::instance();
  auto const id = reg.register_unit_archetype(
      "per_unit_archetype_test/horse_unit", CreatureKind::Horse, {});
  ASSERT_NE(id, kInvalidArchetype);

  auto const *desc = reg.get(id);
  ASSERT_NE(desc, nullptr);
  EXPECT_EQ(desc->species, CreatureKind::Horse);
  EXPECT_EQ(desc->bake_attachment_count, 0U);

  auto const *base = reg.get(ArchetypeRegistry::kHorseBase);
  ASSERT_NE(base, nullptr);
  for (std::size_t i = 0; i < animation_state_count(); ++i) {
    EXPECT_EQ(desc->bpat_clip[i], base->bpat_clip[i]);
    EXPECT_EQ(desc->snapshot[i], base->snapshot[i]);
  }
}

TEST(PerUnitArchetype, ExcessAttachmentsAreClampedToCap) {
  auto &reg = ArchetypeRegistry::instance();

  std::array<StaticAttachmentSpec, ArchetypeDescriptor::kMaxBakeAttachments + 4>
      lots{};
  for (auto &a : lots) {
    a.archetype = &make_test_archetype();
    a.socket_bone_index = 0;
  }

  auto const id = reg.register_unit_archetype(
      "per_unit_archetype_test/clamp", CreatureKind::Humanoid,
      std::span<const StaticAttachmentSpec>{lots});
  ASSERT_NE(id, kInvalidArchetype);

  auto const *desc = reg.get(id);
  ASSERT_NE(desc, nullptr);
  EXPECT_EQ(desc->bake_attachment_count,
            ArchetypeDescriptor::kMaxBakeAttachments);
}

TEST(PerUnitArchetype, MountedSpeciesIsRejected) {
  auto &reg = ArchetypeRegistry::instance();
  auto const id = reg.register_unit_archetype(
      "per_unit_archetype_test/mounted_should_fail", CreatureKind::Mounted, {});
  EXPECT_EQ(id, kInvalidArchetype);
}

} // namespace
