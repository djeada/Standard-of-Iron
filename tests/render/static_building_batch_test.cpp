#include <QMatrix4x4>
#include <QVector3D>

#include <array>
#include <cstdint>
#include <gtest/gtest.h>
#include <span>
#include <vector>

#include "render/entity/unseen_submitter.h"
#include "render/gl/primitives.h"
#include "render/material_classification.h"
#include "render/render_archetype.h"
#include "render/static_building_batch.h"

namespace {

using Render::GL::BuildingInstanceGpu;
using Render::GL::MergedBuildingMesh;
using Render::GL::MergedBuildingVertex;
using Render::GL::RenderArchetype;
using Render::GL::RenderArchetypeBuilder;
using Render::GL::RenderInstance;
using Render::GL::StaticBatchDraw;
using Render::GL::StaticBuildingBatch;
using Render::GL::Texture;

auto fake_texture() -> Texture* {
  static int storage = 0;
  return reinterpret_cast<Texture*>(&storage);
}

auto two_part_archetype() -> RenderArchetype {
  RenderArchetypeBuilder builder("static_batch_test");
  builder.add_box(
      QVector3D(0.0F, 0.5F, 0.0F), QVector3D(2.0F, 1.0F, 2.0F), {0.6F, 0.5F, 0.4F});
  builder.add_palette_box(QVector3D(0.0F, 1.5F, 0.0F), QVector3D(1.0F, 1.0F, 1.0F), 0);
  builder.add_box(QVector3D(0.0F, 3.0F, 0.0F),
                  QVector3D(0.5F, 0.5F, 0.5F),
                  {1.0F, 1.0F, 1.0F},
                  nullptr,
                  0.4F);
  return std::move(builder).build();
}

auto instance_at(const RenderArchetype& archetype,
                 std::uint32_t id,
                 float x,
                 const std::array<QVector3D, 1>& palette) -> RenderInstance {
  RenderInstance instance;
  instance.archetype = &archetype;
  instance.world.translate(x, 0.0F, 0.0F);
  instance.palette = palette;
  instance.default_texture = fake_texture();
  instance.static_id = id;
  return instance;
}

auto full_slice(const RenderArchetype& archetype)
    -> const Render::GL::RenderArchetypeSlice& {
  return archetype.lods[static_cast<std::size_t>(Render::GL::RenderArchetypeLod::Full)];
}

auto cube_vertex_count() -> std::size_t {
  return Render::GL::get_unit_cube()->get_vertices().size();
}

TEST(MergedBuildingMesh, VertexAndIndexCountsSumTheMergedParts) {
  const RenderArchetype archetype = two_part_archetype();
  const MergedBuildingMesh merged =
      Render::GL::build_merged_building_mesh(full_slice(archetype));

  const auto* cube = Render::GL::get_unit_cube();
  EXPECT_EQ(merged.vertices.size(), 2U * cube->get_vertices().size());
  EXPECT_EQ(merged.indices.size(), 2U * cube->get_indices().size());
  ASSERT_EQ(merged.ranges.size(), 1U);
  EXPECT_EQ(merged.ranges[0].texture, nullptr);
  EXPECT_EQ(merged.ranges[0].index_count, merged.indices.size());
  ASSERT_EQ(merged.dynamic_draws.size(), 1U);
  EXPECT_FLOAT_EQ(merged.dynamic_draws.front()->alpha, 0.4F);
  for (std::size_t i = cube->get_indices().size(); i < merged.indices.size(); ++i) {
    EXPECT_GE(merged.indices[i], cube_vertex_count());
  }
}

TEST(MergedBuildingMesh, PartsAreBakedIntoArchetypeSpace) {
  const RenderArchetype archetype = two_part_archetype();
  const MergedBuildingMesh merged =
      Render::GL::build_merged_building_mesh(full_slice(archetype));
  const auto& draw = full_slice(archetype).draws[1];
  const auto& source = Render::GL::get_unit_cube()->get_vertices();

  for (std::size_t i = 0; i < source.size(); ++i) {
    const MergedBuildingVertex& vertex = merged.vertices[source.size() + i];
    const QVector3D expected = draw.local_model.map(
        QVector3D(source[i].position[0], source[i].position[1], source[i].position[2]));
    EXPECT_FLOAT_EQ(vertex.position[0], expected.x());
    EXPECT_FLOAT_EQ(vertex.position[1], expected.y());
    EXPECT_FLOAT_EQ(vertex.position[2], expected.z());
    const QVector3D normal(vertex.normal[0], vertex.normal[1], vertex.normal[2]);
    const QVector3D source_normal(
        source[i].normal[0], source[i].normal[1], source[i].normal[2]);
    EXPECT_GT(QVector3D::dotProduct(normal.normalized(), source_normal), 0.999F);
  }
  EXPECT_NEAR(merged.bounds_center.y(), 1.0F, 1e-5F);
}

TEST(MergedBuildingMesh, VerticesCarryPaletteSlotAndResolvedMaterials) {
  RenderArchetypeBuilder builder("merged_materials");
  const QVector3D wood(0.45F, 0.30F, 0.18F);
  builder.add_box(QVector3D(), QVector3D(1.0F, 1.0F, 1.0F), wood);
  builder.add_box(QVector3D(), QVector3D(1.0F, 1.0F, 1.0F), wood, nullptr, 1.0F, 3);
  builder.add_palette_box(QVector3D(), QVector3D(1.0F, 1.0F, 1.0F), 1);
  builder.add_palette_box(QVector3D(), QVector3D(1.0F, 1.0F, 1.0F), 5);
  const RenderArchetype archetype = std::move(builder).build();
  const MergedBuildingMesh merged =
      Render::GL::build_merged_building_mesh(full_slice(archetype));
  const std::size_t stride = cube_vertex_count();
  ASSERT_EQ(merged.vertices.size(), 4U * stride);

  const auto& classified = merged.vertices[0].material;
  EXPECT_FLOAT_EQ(classified[0], -1.0F);
  EXPECT_FLOAT_EQ(classified[1],
                  static_cast<float>(Render::resolve_material_id(0, wood)));
  EXPECT_FLOAT_EQ(classified[2],
                  static_cast<float>(Render::resolve_material_id(
                      0, Render::GL::unseen_surface_color(wood))));
  EXPECT_FLOAT_EQ(classified[3], 0.0F);

  const auto& fixed_material = merged.vertices[stride].material;
  EXPECT_FLOAT_EQ(fixed_material[1], 3.0F);
  EXPECT_FLOAT_EQ(fixed_material[2], 3.0F);

  EXPECT_FLOAT_EQ(merged.vertices[2 * stride].material[0], 1.0F);
  EXPECT_FLOAT_EQ(merged.vertices[3 * stride].material[0], -1.0F);
}

TEST(MergedBuildingMesh, TexturedPartsGroupIntoRanges) {
  RenderArchetypeBuilder builder("merged_textures");
  builder.add_box(QVector3D(), QVector3D(1.0F, 1.0F, 1.0F), {0.5F, 0.5F, 0.5F});
  builder.add_box(
      QVector3D(), QVector3D(1.0F, 1.0F, 1.0F), {0.5F, 0.5F, 0.5F}, fake_texture());
  builder.add_box(QVector3D(), QVector3D(1.0F, 1.0F, 1.0F), {0.5F, 0.5F, 0.5F});
  const RenderArchetype archetype = std::move(builder).build();
  const MergedBuildingMesh merged =
      Render::GL::build_merged_building_mesh(full_slice(archetype));

  ASSERT_EQ(merged.ranges.size(), 2U);
  std::uint32_t total = 0;
  for (const auto& range : merged.ranges) {
    EXPECT_EQ(range.first_index, total);
    total += range.index_count;
  }
  EXPECT_EQ(total, merged.indices.size());
}

struct DecodedPart {
  QVector3D color;
  int material_id = 0;
};

auto decode_like_building_merged_vert(const MergedBuildingVertex& vertex,
                                      const BuildingInstanceGpu& record)
    -> DecodedPart {
  const int slot = static_cast<int>(vertex.material[0]);
  const bool unseen = record.state[1] > 0.5F;
  const float* palette = slot == 0 ? record.palette0 : record.palette1;
  const int base = static_cast<int>(vertex.material[3]);
  DecodedPart out;
  if (slot >= 0 && palette[3] >= 0.0F) {
    out.color = QVector3D(palette[0], palette[1], palette[2]);
    out.material_id = base;
    if (out.material_id % 10 == 0) {
      out.material_id += static_cast<int>(palette[3]);
    }
  } else {
    const QVector3D fixed(
        vertex.color_alpha[0], vertex.color_alpha[1], vertex.color_alpha[2]);
    out.color = unseen ? Render::GL::unseen_surface_color(fixed) : fixed;
    out.material_id =
        static_cast<int>(unseen ? vertex.material[2] : vertex.material[1]);
  }
  if (base < 10) {
    out.material_id += static_cast<int>(record.state[0]);
  }
  return out;
}

TEST(MergedBuildingMesh, DecodedVerticesMatchTheDynamicPathForEveryInstanceState) {
  RenderArchetypeBuilder builder("merged_parity");
  builder.add_box(QVector3D(), QVector3D(1.0F, 1.0F, 1.0F), {0.45F, 0.30F, 0.18F});
  builder.add_box(QVector3D(), QVector3D(1.0F, 1.0F, 1.0F), {0.9F, 0.9F, 0.85F});
  builder.add_box(
      QVector3D(), QVector3D(1.0F, 1.0F, 1.0F), {0.2F, 0.2F, 0.22F}, nullptr, 1.0F, 1);
  builder.add_box(
      QVector3D(), QVector3D(1.0F, 1.0F, 1.0F), {0.5F, 0.5F, 0.5F}, nullptr, 1.0F, 12);
  builder.add_palette_box(QVector3D(), QVector3D(1.0F, 1.0F, 1.0F), 0);
  builder.add_palette_box(
      QVector3D(), QVector3D(1.0F, 1.0F, 1.0F), 1, nullptr, 1.0F, 2);
  const RenderArchetype archetype = std::move(builder).build();
  const auto& draws = full_slice(archetype).draws;
  const MergedBuildingMesh merged =
      Render::GL::build_merged_building_mesh(full_slice(archetype));
  const std::size_t stride = cube_vertex_count();
  ASSERT_EQ(merged.vertices.size(), draws.size() * stride);

  const std::array<QVector3D, 2> two{QVector3D(0.8F, 0.1F, 0.1F),
                                     QVector3D(0.3F, 0.5F, 0.9F)};
  const std::array<QVector3D, 1> one{QVector3D(0.1F, 0.6F, 0.2F)};
  for (const std::span<const QVector3D> palette : {std::span<const QVector3D>(two),
                                                   std::span<const QVector3D>(one),
                                                   std::span<const QVector3D>()}) {
    for (const bool unseen : {false, true}) {
      for (const int damage : {0, 10, 20}) {
        RenderInstance instance;
        instance.archetype = &archetype;
        instance.palette = palette;
        instance.unseen = unseen;
        instance.damage_material_id = damage;
        const BuildingInstanceGpu record = Render::GL::pack_building_instance(instance);
        for (std::size_t part = 0; part < draws.size(); ++part) {
          const auto resolved = Render::GL::resolve_render_draw(instance, draws[part]);
          const DecodedPart decoded =
              decode_like_building_merged_vert(merged.vertices[part * stride], record);
          EXPECT_NEAR(decoded.color.x(), resolved.color.x(), 1e-6F) << part;
          EXPECT_NEAR(decoded.color.y(), resolved.color.y(), 1e-6F) << part;
          EXPECT_NEAR(decoded.color.z(), resolved.color.z(), 1e-6F) << part;
          EXPECT_EQ(decoded.material_id,
                    Render::resolve_material_id(resolved.material_id, resolved.color))
              << "part " << part << " unseen " << unseen << " damage " << damage;
        }
      }
    }
  }
}

TEST(StaticBuildingBatch, InstancePacksWorldPaletteUnseenAndDamage) {
  const RenderArchetype archetype = two_part_archetype();
  const std::array<QVector3D, 1> palette{QVector3D(0.8F, 0.1F, 0.1F)};
  RenderInstance instance = instance_at(archetype, 7U, 10.0F, palette);

  BuildingInstanceGpu seen = Render::GL::pack_building_instance(instance);
  EXPECT_FLOAT_EQ(seen.model_col0[3], 10.0F);
  EXPECT_FLOAT_EQ(seen.palette0[0], 0.8F);
  EXPECT_FLOAT_EQ(seen.palette0[3],
                  static_cast<float>(Render::classify_material_id(palette[0])));
  EXPECT_LT(seen.palette1[3], 0.0F);
  EXPECT_FLOAT_EQ(seen.state[0], 0.0F);
  EXPECT_FLOAT_EQ(seen.state[1], 0.0F);

  instance.unseen = true;
  instance.damage_material_id = 20;
  BuildingInstanceGpu unseen = Render::GL::pack_building_instance(instance);
  const QVector3D expected = Render::GL::unseen_surface_color(palette[0]);
  EXPECT_FLOAT_EQ(unseen.palette0[0], expected.x());
  EXPECT_FLOAT_EQ(unseen.palette0[3],
                  static_cast<float>(Render::classify_material_id(expected)));
  EXPECT_FLOAT_EQ(unseen.state[0], 20.0F);
  EXPECT_FLOAT_EQ(unseen.state[1], 1.0F);
}

TEST(StaticBuildingBatch, BuildingsJoinOnTheirFirstFrameAndGroupByArchetype) {
  const RenderArchetype first = two_part_archetype();
  const RenderArchetype second = two_part_archetype();
  const std::array<QVector3D, 1> palette{QVector3D(0.8F, 0.1F, 0.1F)};
  StaticBuildingBatch batch;

  batch.begin_frame();
  for (std::uint32_t id = 1; id <= 4; ++id) {
    const RenderArchetype& archetype = (id % 2U == 0U) ? first : second;
    const auto* dynamic = batch.place(
        instance_at(archetype, id, static_cast<float>(id) * 20.0F, palette));
    ASSERT_NE(dynamic, nullptr);
    EXPECT_EQ(dynamic->size(), 1U);
  }
  batch.finish_frame();

  ASSERT_NE(first.merged_full, nullptr);
  ASSERT_NE(second.merged_full, nullptr);
  ASSERT_EQ(batch.draws().size(), 2U);
  EXPECT_NE(batch.draws()[0].mesh, batch.draws()[1].mesh);
  EXPECT_EQ(batch.instances().size(), 4U);
  EXPECT_EQ(batch.bounds().size(), 4U);
  for (const StaticBatchDraw& draw : batch.draws()) {
    EXPECT_EQ(draw.count, 2U);
    EXPECT_EQ(draw.default_texture, fake_texture());
  }
  EXPECT_EQ(batch.draws()[1].first, 2U);

  batch.begin_frame();
  batch.finish_frame();
  EXPECT_TRUE(batch.draws().empty());
  EXPECT_TRUE(batch.instances().empty());
}

TEST(StaticBuildingBatch, PreviewsAndLargePalettesStayDynamic) {
  const RenderArchetype archetype = two_part_archetype();
  const std::array<QVector3D, 1> palette{QVector3D(0.8F, 0.1F, 0.1F)};
  StaticBuildingBatch batch;
  batch.begin_frame();
  EXPECT_EQ(batch.place(instance_at(archetype, 0U, 0.0F, palette)), nullptr);

  const std::array<QVector3D, 3> wide{QVector3D(), QVector3D(), QVector3D()};
  RenderInstance instance = instance_at(archetype, 7U, 0.0F, palette);
  instance.palette = wide;
  EXPECT_EQ(batch.place(instance), nullptr);
  batch.finish_frame();
  EXPECT_TRUE(batch.instances().empty());
}

TEST(StaticBuildingBatch, UnseenSubmitterMarksForwardedInstances) {
  struct Capture final : Render::GL::ISubmitter {
    RenderInstance last;
    void render_instance(const RenderInstance& instance) override { last = instance; }
    void mesh(Render::GL::Mesh*,
              const QMatrix4x4&,
              const QVector3D&,
              Texture*,
              float,
              int) override {}
    void cylinder(
        const QVector3D&, const QVector3D&, float, const QVector3D&, float) override {}
    void ground_marker(const Render::GL::GroundMarkerCmd&) override {}
    void grid(const QMatrix4x4&, const QVector3D&, float, float, float) override {}
    void selection_smoke(const QMatrix4x4&, const QVector3D&, float) override {}
    void healing_beam(const QVector3D&,
                      const QVector3D&,
                      const QVector3D&,
                      float,
                      float,
                      float,
                      float) override {}
    void healer_aura(const QVector3D&, const QVector3D&, float, float, float) override {
    }
    void combat_dust(const QVector3D&, const QVector3D&, float, float, float) override {
    }
    void
    stone_impact(const QVector3D&, const QVector3D&, float, float, float) override {}
    void mode_indicator(const QMatrix4x4&, int, const QVector3D&, float) override {}
  };

  Capture capture;
  Render::GL::UnseenSubmitter unseen(capture);
  RenderInstance instance;
  instance.static_id = 3U;
  unseen.render_instance(instance);

  EXPECT_TRUE(capture.last.unseen);
  EXPECT_EQ(capture.last.static_id, 3U);
}

} // namespace
