#include "render/entity/building_archetype_desc.h"
#include "render/render_archetype.h"
#include "render/submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <gtest/gtest.h>

#include <array>
#include <vector>

namespace {

struct RecordedMesh {
  QVector3D color{1.0F, 1.0F, 1.0F};
};

class RecordingSubmitter final : public Render::GL::ISubmitter {
public:
  std::vector<RecordedMesh> meshes;

  void mesh(Render::GL::Mesh *, const QMatrix4x4 &, const QVector3D &color,
            Render::GL::Texture *, float, int) override {
    meshes.push_back({color});
  }

  void cylinder(const QVector3D &, const QVector3D &, float, const QVector3D &,
                float) override {}
  void selection_ring(const QMatrix4x4 &, float, float,
                      const QVector3D &) override {}
  void grid(const QMatrix4x4 &, const QVector3D &, float, float,
            float) override {}
  void selection_smoke(const QMatrix4x4 &, const QVector3D &, float) override {}
  void healing_beam(const QVector3D &, const QVector3D &, const QVector3D &,
                    float, float, float, float) override {}
  void healer_aura(const QVector3D &, const QVector3D &, float, float,
                   float) override {}
  void combat_dust(const QVector3D &, const QVector3D &, float, float,
                   float) override {}
  void stone_impact(const QVector3D &, const QVector3D &, float, float,
                    float) override {}
  void mode_indicator(const QMatrix4x4 &, int, const QVector3D &,
                      float) override {}
};

TEST(BuildingArchetypeDesc, FiltersPartsByBuildingStateMask) {
  using namespace Render::GL;

  BuildingArchetypeDesc desc("state_filter_test");
  desc.add_box(QVector3D(0.0F, 0.0F, 0.0F), QVector3D(1.0F, 1.0F, 1.0F),
               QVector3D(1.0F, 0.0F, 0.0F));
  desc.add_box(QVector3D(1.0F, 0.0F, 0.0F), QVector3D(1.0F, 1.0F, 1.0F),
               QVector3D(0.0F, 1.0F, 0.0F), kBuildingStateMaskIntact);

  const RenderArchetype normal =
      build_building_archetype(desc, BuildingState::Normal);
  const RenderArchetype destroyed =
      build_building_archetype(desc, BuildingState::Destroyed);

  EXPECT_EQ(normal.lods[0].draws.size(), 2u);
  EXPECT_EQ(destroyed.lods[0].draws.size(), 1u);
}

TEST(BuildingArchetypeDesc, PreservesPaletteDrivenParts) {
  using namespace Render::GL;

  BuildingArchetypeDesc desc("palette_test");
  desc.add_palette_box(QVector3D(0.0F, 0.0F, 0.0F), QVector3D(1.0F, 1.0F, 1.0F),
                       0);

  RenderArchetype archetype =
      build_building_archetype(desc, BuildingState::Normal);
  std::array<QVector3D, 1> palette{QVector3D(0.2F, 0.4F, 0.8F)};

  RenderInstance instance;
  instance.archetype = &archetype;
  instance.palette = palette;

  RecordingSubmitter submitter;
  submit_render_instance(submitter, instance);

  ASSERT_EQ(submitter.meshes.size(), 1u);
  EXPECT_EQ(submitter.meshes[0].color, palette[0]);
}

TEST(BuildingArchetypeDesc, ArchetypeSetSelectsStateVariant) {
  using namespace Render::GL;

  const BuildingArchetypeSet set =
      build_stateful_building_archetype_set([](BuildingState state) {
        BuildingArchetypeDesc desc("state_set_test");
        desc.add_box(QVector3D(0.0F, 0.0F, 0.0F), QVector3D(1.0F, 1.0F, 1.0F),
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
  desc.add_box(QVector3D(0.0F, 0.0F, 0.0F), QVector3D(1.0F, 1.0F, 1.0F),
               QVector3D(1.0F, 0.0F, 0.0F));
  desc.add_box(QVector3D(1.0F, 0.0F, 0.0F), QVector3D(1.0F, 1.0F, 1.0F),
               QVector3D(0.0F, 1.0F, 0.0F), BuildingStateMask::All,
               BuildingLODMask::Full);

  const RenderArchetype archetype =
      build_building_archetype(desc, BuildingState::Normal);

  EXPECT_EQ(archetype.lods[0].draws.size(), 2u);
  EXPECT_EQ(archetype.lods[1].draws.size(), 1u);
}

TEST(BuildingArchetypeDesc, SetFullLodMaxDistanceConfiguresSlice) {
  using namespace Render::GL;

  BuildingArchetypeDesc desc("distance_test");
  desc.set_full_lod_max_distance(45.0F);
  desc.add_box(QVector3D(0.0F, 0.0F, 0.0F), QVector3D(1.0F, 1.0F, 1.0F),
               QVector3D(1.0F, 1.0F, 1.0F));

  const RenderArchetype archetype =
      build_building_archetype(desc, BuildingState::Normal);

  EXPECT_FLOAT_EQ(archetype.lods[0].max_distance, 45.0F);
}

} // namespace
