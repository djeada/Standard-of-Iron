#include <QMatrix4x4>
#include <QVector3D>

#include <array>
#include <cstdint>
#include <gtest/gtest.h>
#include <vector>

#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_ornaments.h"
#include "render/gl/primitives.h"
#include "render/render_archetype.h"
#include "render/submitter.h"

namespace {

struct RecordedMesh {
  QVector3D color{1.0F, 1.0F, 1.0F};
};

class RecordingSubmitter final : public Render::GL::ISubmitter {
public:
  std::vector<RecordedMesh> meshes;

  void mesh(Render::GL::Mesh*,
            const QMatrix4x4&,
            const QVector3D& color,
            Render::GL::Texture*,
            float,
            int) override {
    meshes.push_back({color});
  }

  void cylinder(
      const QVector3D&, const QVector3D&, float, const QVector3D&, float) override {}
  void selection_ring(const QMatrix4x4&, float, float, const QVector3D&) override {}
  void grid(const QMatrix4x4&, const QVector3D&, float, float, float) override {}
  void selection_smoke(const QMatrix4x4&, const QVector3D&, float) override {}
  void healing_beam(const QVector3D&,
                    const QVector3D&,
                    const QVector3D&,
                    float,
                    float,
                    float,
                    float) override {}
  void healer_aura(const QVector3D&, const QVector3D&, float, float, float) override {}
  void combat_dust(const QVector3D&, const QVector3D&, float, float, float) override {}
  void stone_impact(const QVector3D&, const QVector3D&, float, float, float) override {}
  void mode_indicator(const QMatrix4x4&, int, const QVector3D&, float) override {}
};

auto fake_mesh(int id) -> Render::GL::Mesh* {
  return reinterpret_cast<Render::GL::Mesh*>(static_cast<intptr_t>(id));
}

TEST(BuildingArchetypeDesc, FiltersPartsByBuildingStateMask) {
  using namespace Render::GL;

  BuildingArchetypeDesc desc("state_filter_test");
  desc.add_box(QVector3D(0.0F, 0.0F, 0.0F),
               QVector3D(1.0F, 1.0F, 1.0F),
               QVector3D(1.0F, 0.0F, 0.0F));
  desc.add_box(QVector3D(1.0F, 0.0F, 0.0F),
               QVector3D(1.0F, 1.0F, 1.0F),
               QVector3D(0.0F, 1.0F, 0.0F),
               k_building_state_mask_intact);

  const RenderArchetype normal = build_building_archetype(desc, BuildingState::Normal);
  const RenderArchetype destroyed =
      build_building_archetype(desc, BuildingState::Destroyed);

  EXPECT_EQ(normal.lods[0].draws.size(), 2U);
  EXPECT_EQ(destroyed.lods[0].draws.size(), 1U);
}

TEST(BuildingArchetypeDesc, PreservesPaletteDrivenParts) {
  using namespace Render::GL;

  BuildingArchetypeDesc desc("palette_test");
  desc.add_palette_box(QVector3D(0.0F, 0.0F, 0.0F), QVector3D(1.0F, 1.0F, 1.0F), 0);

  RenderArchetype const archetype =
      build_building_archetype(desc, BuildingState::Normal);
  std::array<QVector3D, 1> palette{QVector3D(0.2F, 0.4F, 0.8F)};

  RenderInstance instance;
  instance.archetype = &archetype;
  instance.palette = palette;

  RecordingSubmitter submitter;
  submit_render_instance(submitter, instance);

  ASSERT_EQ(submitter.meshes.size(), 1U);
  EXPECT_EQ(submitter.meshes[0].color, palette[0]);
}

TEST(BuildingArchetypeDesc, BuildsPointedConePartsForTimberSilhouettes) {
  using namespace Render::GL;

  BuildingArchetypeDesc desc("cone_test");
  desc.add_cone(QVector3D(0.0F, 1.0F, 0.0F),
                QVector3D(0.0F, 1.5F, 0.0F),
                0.2F,
                QVector3D(0.3F, 0.2F, 0.1F));

  RenderArchetype const archetype =
      build_building_archetype(desc, BuildingState::Normal);

  ASSERT_EQ(archetype.lods[0].draws.size(), 1U);
  EXPECT_EQ(archetype.lods[0].draws[0].mesh, get_unit_cone(8));
}

TEST(BuildingArchetypeDesc, ArchetypeSetSelectsStateVariant) {
  using namespace Render::GL;

  const BuildingArchetypeSet set =
      build_stateful_building_archetype_set([](BuildingState state) {
        BuildingArchetypeDesc desc("state_set_test");
        desc.add_box(QVector3D(0.0F, 0.0F, 0.0F),
                     QVector3D(1.0F, 1.0F, 1.0F),
                     state == BuildingState::Normal
                         ? QVector3D(1.0F, 0.0F, 0.0F)
                         : (state == BuildingState::Damaged
                                ? QVector3D(0.0F, 1.0F, 0.0F)
                                : QVector3D(0.0F, 0.0F, 1.0F)));
        return build_building_archetype(desc, state);
      });

  EXPECT_EQ(set.for_state(BuildingState::Normal).lods[0].draws[0].color,
            QVector3D(1.0F, 0.0F, 0.0F));
  EXPECT_EQ(set.for_state(BuildingState::Damaged).lods[0].draws[0].color,
            QVector3D(0.0F, 1.0F, 0.0F));
  EXPECT_EQ(set.for_state(BuildingState::Destroyed).lods[0].draws[0].color,
            QVector3D(0.0F, 0.0F, 1.0F));
}

TEST(BuildingArchetypeDesc, FiltersBuildingLODMask) {
  using namespace Render::GL;

  BuildingArchetypeDesc desc("lod_filter_test");
  desc.add_box(QVector3D(0.0F, 0.0F, 0.0F),
               QVector3D(1.0F, 1.0F, 1.0F),
               QVector3D(1.0F, 0.0F, 0.0F));
  desc.add_box(QVector3D(1.0F, 0.0F, 0.0F),
               QVector3D(1.0F, 1.0F, 1.0F),
               QVector3D(0.0F, 1.0F, 0.0F),
               BuildingStateMask::All,
               BuildingLODMask::Full);

  const RenderArchetype archetype =
      build_building_archetype(desc, BuildingState::Normal);

  EXPECT_EQ(archetype.lods[0].draws.size(), 2U);
  EXPECT_EQ(archetype.lods[1].draws.size(), 1U);
}

TEST(BuildingArchetypeDesc, SetFullLodMaxDistanceConfiguresSlice) {
  using namespace Render::GL;

  BuildingArchetypeDesc desc("distance_test");
  desc.set_full_lod_max_distance(45.0F);
  desc.add_box(QVector3D(0.0F, 0.0F, 0.0F),
               QVector3D(1.0F, 1.0F, 1.0F),
               QVector3D(1.0F, 1.0F, 1.0F));

  const RenderArchetype archetype =
      build_building_archetype(desc, BuildingState::Normal);

  EXPECT_FLOAT_EQ(archetype.lods[0].max_distance, 45.0F);
}

TEST(BuildingArchetypeDesc, RecordedLodsPreserveSeparateFullAndMinimalMeshes) {
  using namespace Render::GL;

  RecordedMeshCmd const full_cmd{.mesh = fake_mesh(11),
                                 .local_model = QMatrix4x4{},
                                 .color = QVector3D(1.0F, 0.0F, 0.0F)};
  RecordedMeshCmd const minimal_cmd{.mesh = fake_mesh(22),
                                    .local_model = QMatrix4x4{},
                                    .color = QVector3D(0.0F, 1.0F, 0.0F)};

  const RenderArchetype archetype = build_building_archetype_from_recorded_lods(
      "recorded_lods_test", {full_cmd}, {minimal_cmd}, 72.0F);

  ASSERT_EQ(archetype.lods[0].draws.size(), 1U);
  ASSERT_EQ(archetype.lods[1].draws.size(), 1U);
  EXPECT_EQ(archetype.lods[0].draws[0].mesh, fake_mesh(11));
  EXPECT_EQ(archetype.lods[1].draws[0].mesh, fake_mesh(22));
  EXPECT_FLOAT_EQ(archetype.lods[0].max_distance, 72.0F);
}

TEST(BuildingCulturalOrnaments, RomanAndPunicReliefsKeepDistinctProfiles) {
  using namespace Render::GL;

  BuildingArchetypeDesc roman("roman_aquila_test");
  add_roman_aquila_relief(roman,
                          QVector3D(0.0F, 1.0F, 0.0F),
                          BuildingFacadePlane::XY,
                          1.0F,
                          QVector3D(0.8F, 0.6F, 0.2F),
                          QVector3D(0.2F, 0.3F, 0.5F));
  BuildingArchetypeDesc punic("punic_tanit_test");
  add_punic_tanit_relief(punic,
                         QVector3D(0.0F, 1.0F, 0.0F),
                         BuildingFacadePlane::ZY,
                         1.0F,
                         QVector3D(0.7F, 0.5F, 0.2F),
                         QVector3D(0.3F, 0.2F, 0.2F));

  const RenderArchetype roman_archetype =
      build_building_archetype(roman, BuildingState::Normal);
  const RenderArchetype punic_archetype =
      build_building_archetype(punic, BuildingState::Normal);

  EXPECT_EQ(roman_archetype.lods[0].draws.size(), 9U);
  EXPECT_EQ(punic_archetype.lods[0].draws.size(), 6U);
  EXPECT_TRUE(roman_archetype.lods[1].draws.empty());
  EXPECT_TRUE(punic_archetype.lods[1].draws.empty());
  EXPECT_NE(roman_archetype.lods[0].draws[0].local_model,
            punic_archetype.lods[0].draws[0].local_model);
}

TEST(BuildingCulturalOrnaments, RoofSignaturesRemainVisibleAtMinimalLod) {
  using namespace Render::GL;

  BuildingArchetypeDesc roman("roman_roof_standard_test");
  add_roman_roof_standard(roman,
                          QVector3D(0.0F, 1.0F, 0.0F),
                          1.0F,
                          QVector3D(0.85F, 0.62F, 0.20F),
                          QVector3D(0.62F, 0.10F, 0.07F));
  BuildingArchetypeDesc punic("punic_horned_crown_test");
  add_punic_horned_crown(punic,
                         QVector3D(0.0F, 1.0F, 0.0F),
                         1.0F,
                         QVector3D(0.10F, 0.08F, 0.12F),
                         QVector3D(0.82F, 0.50F, 0.16F),
                         QVector3D(0.60F, 0.10F, 0.70F));

  const RenderArchetype roman_archetype =
      build_building_archetype(roman, BuildingState::Normal);
  const RenderArchetype punic_archetype =
      build_building_archetype(punic, BuildingState::Normal);

  EXPECT_GE(roman_archetype.lods[1].draws.size(), 8U);
  EXPECT_GE(punic_archetype.lods[1].draws.size(), 10U);
  EXPECT_NE(roman_archetype.lods[0].draws.size(), punic_archetype.lods[0].draws.size());
  EXPECT_NE(roman_archetype.lods[0].draws.front().local_model,
            punic_archetype.lods[0].draws.front().local_model);
}

} // namespace
