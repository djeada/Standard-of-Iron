#include "game/core/component.h"
#include "game/core/entity.h"
#include "render/entity/nations/carthage/defense_tower_renderer.h"
#include "render/entity/nations/roman/defense_tower_renderer.h"
#include "render/entity/nations/roman/home_renderer.h"
#include "render/entity/registry.h"
#include "render/equipment/equipment_submit.h"
#include "render/equipment/horse/armor/scale_barding_renderer.h"
#include "render/equipment/horse/decorations/saddle_bag_renderer.h"
#include "render/gl/resources.h"
#include "render/horse/attachment_frames.h"
#include "render/horse/dimensions.h"
#include "render/render_archetype.h"
#include "render/submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <array>
#include <gtest/gtest.h>
#include <vector>

namespace {

struct RecordedMesh {
  Render::GL::Mesh *mesh{nullptr};
  Render::GL::Texture *texture{nullptr};
  QMatrix4x4 model;
  QVector3D color{1.0F, 1.0F, 1.0F};
  float alpha{1.0F};
};

class RecordingSubmitter : public Render::GL::ISubmitter {
public:
  std::vector<RecordedMesh> meshes;
  std::size_t cylinder_count{0};

  void mesh(Render::GL::Mesh *mesh, const QMatrix4x4 &model,
            const QVector3D &color, Render::GL::Texture *texture, float alpha,
            int) override {
    meshes.push_back({mesh, texture, model, color, alpha});
  }
  void cylinder(const QVector3D &, const QVector3D &, float, const QVector3D &,
                float) override {
    ++cylinder_count;
  }
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

auto fake_mesh(int id) -> Render::GL::Mesh * {
  return reinterpret_cast<Render::GL::Mesh *>(static_cast<intptr_t>(id));
}

auto fake_texture(int id) -> Render::GL::Texture * {
  return reinterpret_cast<Render::GL::Texture *>(static_cast<intptr_t>(id));
}

auto near_vec3(const QVector3D &lhs, const QVector3D &rhs,
               float eps = 1e-4F) -> bool {
  return (lhs - rhs).length() <= eps;
}

auto make_test_horse_frames() -> Render::GL::HorseBodyFrames {
  using namespace Render::GL;

  HorseBodyFrames frames;
  auto set_frame = [](HorseAttachmentFrame &frame, const QVector3D &origin) {
    frame.origin = origin;
    frame.right = QVector3D(1.0F, 0.0F, 0.0F);
    frame.up = QVector3D(0.0F, 1.0F, 0.0F);
    frame.forward = QVector3D(0.0F, 0.0F, 1.0F);
  };

  set_frame(frames.head, QVector3D(0.0F, 1.0F, 1.6F));
  set_frame(frames.neck_base, QVector3D(0.0F, 1.1F, 1.1F));
  set_frame(frames.withers, QVector3D(0.0F, 1.2F, 0.5F));
  set_frame(frames.back_center, QVector3D(0.0F, 1.1F, 0.0F));
  set_frame(frames.croup, QVector3D(0.0F, 1.1F, -0.7F));
  set_frame(frames.chest, QVector3D(0.0F, 0.9F, 1.0F));
  set_frame(frames.barrel, QVector3D(0.0F, 0.9F, 0.1F));
  set_frame(frames.rump, QVector3D(0.0F, 0.95F, -0.8F));
  set_frame(frames.tail_base, QVector3D(0.0F, 1.0F, -1.1F));
  set_frame(frames.muzzle, QVector3D(0.0F, 1.0F, 1.9F));
  return frames;
}

TEST(RenderArchetype, AppliesPaletteAndTracksBounds) {
  using namespace Render::GL;

  RenderArchetypeBuilder builder("test_archetype");
  builder.add_mesh(
      fake_mesh(1),
      box_local_model(QVector3D(2.0F, 1.0F, 3.0F), QVector3D(2.0F, 4.0F, 6.0F)),
      QVector3D(0.2F, 0.3F, 0.4F));
  builder.add_palette_mesh(fake_mesh(2),
                           box_local_model(QVector3D(-1.0F, 0.0F, 0.0F),
                                           QVector3D(1.0F, 1.0F, 1.0F)),
                           0);

  RenderArchetype archetype = std::move(builder).build();
  ASSERT_EQ(archetype.lods[0].draws.size(), 2u);
  EXPECT_TRUE(near_vec3(archetype.lods[0].local_bounds.min,
                        QVector3D(-1.5F, -1.0F, -0.5F)));
  EXPECT_TRUE(near_vec3(archetype.lods[0].local_bounds.max,
                        QVector3D(3.0F, 3.0F, 6.0F)));

  std::array<QVector3D, 1> palette{QVector3D(0.9F, 0.1F, 0.2F)};
  RecordingSubmitter submitter;
  QMatrix4x4 world;
  world.translate(10.0F, 0.0F, 0.0F);

  RenderInstance instance;
  instance.archetype = &archetype;
  instance.world = world;
  instance.palette = palette;
  instance.default_texture = fake_texture(7);
  instance.alpha_multiplier = 0.5F;
  submit_render_instance(submitter, instance);

  ASSERT_EQ(submitter.meshes.size(), 2u);
  EXPECT_TRUE(near_vec3(submitter.meshes[0].model.column(3).toVector3D(),
                        QVector3D(12.0F, 1.0F, 3.0F)));
  EXPECT_TRUE(near_vec3(submitter.meshes[1].model.column(3).toVector3D(),
                        QVector3D(9.0F, 0.0F, 0.0F)));
  EXPECT_TRUE(
      near_vec3(submitter.meshes[0].color, QVector3D(0.2F, 0.3F, 0.4F)));
  EXPECT_TRUE(near_vec3(submitter.meshes[1].color, palette[0]));
  EXPECT_EQ(submitter.meshes[0].texture, fake_texture(7));
  EXPECT_FLOAT_EQ(submitter.meshes[0].alpha, 0.5F);
}

TEST(RenderArchetype, StoredRenderInstanceOwnsPaletteData) {
  using namespace Render::GL;

  RenderArchetypeBuilder builder("stored_instance_test");
  builder.add_palette_box(QVector3D(0.0F, 0.0F, 0.0F),
                          QVector3D(1.0F, 1.0F, 1.0F), 0);
  RenderArchetype archetype = std::move(builder).build();

  StoredRenderInstance<2> stored;
  stored.archetype = &archetype;
  stored.default_texture = fake_texture(11);

  std::array<QVector3D, 1> palette{QVector3D(0.7F, 0.2F, 0.1F)};
  stored.set_palette(palette);
  palette[0] = QVector3D(0.1F, 0.8F, 0.3F);

  RecordingSubmitter submitter;
  submit_render_instance(submitter, stored.render_instance());

  ASSERT_EQ(submitter.meshes.size(), 1u);
  EXPECT_TRUE(
      near_vec3(submitter.meshes[0].color, QVector3D(0.7F, 0.2F, 0.1F)));
  EXPECT_EQ(submitter.meshes[0].texture, fake_texture(11));
}

TEST(RenderArchetypeEquipment, ReplaysArchetypeInstancesThroughEquipmentBatch) {
  using namespace Render::GL;

  RenderArchetypeBuilder builder("equipment_archetype");
  builder.add_palette_box(QVector3D(0.5F, 0.25F, -0.5F),
                          QVector3D(0.5F, 0.25F, 0.75F), 0);
  RenderArchetype archetype = std::move(builder).build();

  EquipmentBatch batch;
  std::array<QVector3D, 1> palette{QVector3D(0.3F, 0.5F, 0.8F)};
  QMatrix4x4 world;
  world.translate(2.0F, 3.0F, 4.0F);
  append_equipment_archetype(batch, archetype, world, palette, fake_texture(9),
                             0.75F);

  RecordingSubmitter submitter;
  submit_equipment_batch(batch, submitter);

  ASSERT_EQ(submitter.meshes.size(), 1u);
  EXPECT_TRUE(near_vec3(submitter.meshes[0].model.column(3).toVector3D(),
                        QVector3D(2.5F, 3.25F, 3.5F)));
  EXPECT_TRUE(near_vec3(submitter.meshes[0].color, palette[0]));
  EXPECT_EQ(submitter.meshes[0].texture, fake_texture(9));
  EXPECT_FLOAT_EQ(submitter.meshes[0].alpha, 0.75F);
}

TEST(RenderArchetypeEquipment, ScaleBardingUsesMultipleAnchoredArchetypes) {
  using namespace Render::GL;

  DrawContext ctx;
  ctx.model = QMatrix4x4{};

  HorseVariant variant{};
  variant.tack_color = QVector3D(0.45F, 0.40F, 0.34F);

  EquipmentBatch batch;
  ScaleBardingRenderer::submit(ctx, make_test_horse_frames(), variant, {},
                               batch);

  EXPECT_TRUE(batch.meshes.empty());
  EXPECT_TRUE(batch.cylinders.empty());
  ASSERT_EQ(batch.archetypes.size(), 3u);

  RecordingSubmitter submitter;
  submit_equipment_batch(batch, submitter);
  ASSERT_EQ(submitter.meshes.size(), 4u);
}

TEST(RenderArchetypeEquipment, SaddleBagsBakeAllRigidPieces) {
  using namespace Render::GL;

  DrawContext ctx;
  ctx.model = QMatrix4x4{};

  HorseVariant variant{};
  variant.saddle_color = QVector3D(0.42F, 0.24F, 0.11F);
  variant.tack_color = QVector3D(0.12F, 0.10F, 0.08F);

  EquipmentBatch batch;
  SaddleBagRenderer::submit(ctx, make_test_horse_frames(), variant, {}, batch);

  EXPECT_TRUE(batch.meshes.empty());
  ASSERT_EQ(batch.archetypes.size(), 1u);
  EXPECT_TRUE(batch.cylinders.empty());

  RecordingSubmitter submitter;
  submit_equipment_batch(batch, submitter);
  EXPECT_EQ(submitter.meshes.size(), 4u);
  EXPECT_EQ(submitter.cylinder_count, 0u);
}

TEST(RenderArchetypeBuildings, RomanHomeRendersExpectedStaticMeshCount) {
  using namespace Render::GL;

  EntityRendererRegistry registry;
  Roman::register_home_renderer(registry);
  const auto renderer = registry.get("troops/roman/home");
  ASSERT_TRUE(static_cast<bool>(renderer));

  Engine::Core::Entity entity(1);
  auto *renderable =
      entity.add_component<Engine::Core::RenderableComponent>("", "");
  ASSERT_NE(renderable, nullptr);
  auto *unit =
      entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  ASSERT_NE(unit, nullptr);

  DrawContext ctx;
  ResourceManager resources;
  ctx.entity = &entity;
  ctx.resources = &resources;
  ctx.model = QMatrix4x4{};

  RecordingSubmitter submitter;
  renderer(ctx, submitter);

  EXPECT_EQ(submitter.meshes.size(), 22u);
}

TEST(RenderArchetypeBuildings, RomanTowerAppliesTeamPaletteSlot) {
  using namespace Render::GL;

  EntityRendererRegistry registry;
  Roman::register_defense_tower_renderer(registry);
  const auto renderer = registry.get("troops/roman/defense_tower");
  ASSERT_TRUE(static_cast<bool>(renderer));

  Engine::Core::Entity entity(2);
  auto *renderable =
      entity.add_component<Engine::Core::RenderableComponent>("", "");
  ASSERT_NE(renderable, nullptr);
  renderable->color = {1.2F, -0.1F, 0.35F};
  auto *unit =
      entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  ASSERT_NE(unit, nullptr);

  DrawContext ctx;
  ResourceManager resources;
  ctx.entity = &entity;
  ctx.resources = &resources;
  ctx.model = QMatrix4x4{};

  RecordingSubmitter submitter;
  renderer(ctx, submitter);

  ASSERT_FALSE(submitter.meshes.empty());
  bool found_team_tint = false;
  QVector3D const expected_team(1.0F, 0.0F, 0.35F);
  for (const RecordedMesh &mesh : submitter.meshes) {
    if (near_vec3(mesh.color, expected_team)) {
      found_team_tint = true;
      break;
    }
  }
  EXPECT_TRUE(found_team_tint);
}

TEST(RenderArchetypeBuildings, CarthageTowerAppliesTeamPaletteSlot) {
  using namespace Render::GL;

  EntityRendererRegistry registry;
  Carthage::register_defense_tower_renderer(registry);
  const auto renderer = registry.get("troops/carthage/defense_tower");
  ASSERT_TRUE(static_cast<bool>(renderer));

  Engine::Core::Entity entity(3);
  auto *renderable =
      entity.add_component<Engine::Core::RenderableComponent>("", "");
  ASSERT_NE(renderable, nullptr);
  renderable->color = {0.8F, 0.2F, 0.6F};
  auto *unit =
      entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  ASSERT_NE(unit, nullptr);

  DrawContext ctx;
  ResourceManager resources;
  ctx.entity = &entity;
  ctx.resources = &resources;
  ctx.model = QMatrix4x4{};

  RecordingSubmitter submitter;
  renderer(ctx, submitter);

  ASSERT_FALSE(submitter.meshes.empty());
  bool found_team_tint = false;
  QVector3D const expected_team(0.8F, 0.2F, 0.6F);
  for (const RecordedMesh &mesh : submitter.meshes) {
    if (near_vec3(mesh.color, expected_team)) {
      found_team_tint = true;
      break;
    }
  }
  EXPECT_TRUE(found_team_tint);
}

} // namespace
