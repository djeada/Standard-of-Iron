#include <QMatrix4x4>
#include <QVector3D>

#include <array>
#include <cmath>
#include <gtest/gtest.h>
#include <utility>
#include <vector>

#include "game/core/component.h"
#include "game/core/entity.h"
#include "render/entity/nations/carthage/ballista_renderer.h"
#include "render/entity/nations/carthage/barracks_renderer.h"
#include "render/entity/nations/carthage/catapult_renderer.h"
#include "render/entity/nations/carthage/defense_tower_renderer.h"
#include "render/entity/nations/carthage/home_renderer.h"
#include "render/entity/nations/carthage/wall_renderer.h"
#include "render/entity/nations/roman/ballista_renderer.h"
#include "render/entity/nations/roman/barracks_renderer.h"
#include "render/entity/nations/roman/catapult_renderer.h"
#include "render/entity/nations/roman/defense_tower_renderer.h"
#include "render/entity/nations/roman/home_renderer.h"
#include "render/entity/nations/roman/wall_renderer.h"
#include "render/entity/registry.h"
#include "render/equipment/equipment_submit.h"
#include "render/equipment/horse/armor/scale_barding_renderer.h"
#include "render/equipment/horse/decorations/saddle_bag_renderer.h"
#include "render/equipment/horse/saddles/carthage_saddle_renderer.h"
#include "render/equipment/horse/saddles/light_cavalry_saddle_renderer.h"
#include "render/equipment/horse/saddles/roman_saddle_renderer.h"
#include "render/gl/resources.h"
#include "render/horse/attachment_frames.h"
#include "render/horse/dimensions.h"
#include "render/render_archetype.h"
#include "render/submitter.h"

namespace {

constexpr float k_default_epsilon = 1e-4F;
constexpr std::size_t k_min_home_mesh_count = 47U;
constexpr float k_roman_tower_min_banner_height = 3.75F;
constexpr float k_carthage_tower_min_banner_height = 3.45F;
constexpr float k_saddle_max_height = 0.14F;
constexpr float k_saddle_front_arch_z_threshold = 0.14F;
constexpr float k_saddle_front_arch_y_threshold = 0.06F;
constexpr float k_saddle_rear_arch_z_threshold = -0.12F;
constexpr float k_saddle_rear_arch_y_threshold = 0.05F;
constexpr float k_saddle_length_to_height_ratio = 2.5F;
constexpr float k_saddle_width_to_height_ratio = 2.0F;
constexpr int k_default_unit_health = 100;
constexpr int k_default_unit_max_health = 100;

struct RecordedMesh {
  Render::GL::Mesh* mesh{nullptr};
  Render::GL::Texture* texture{nullptr};
  QMatrix4x4 model;
  QVector3D color{1.0F, 1.0F, 1.0F};
  float alpha{1.0F};
};

class RecordingSubmitter : public Render::GL::ISubmitter {
public:
  std::vector<RecordedMesh> meshes;
  std::size_t cylinder_count{0};

  void mesh(Render::GL::Mesh* mesh,
            const QMatrix4x4& model,
            const QVector3D& color,
            Render::GL::Texture* texture,
            float alpha,
            int) override {
    meshes.push_back({mesh, texture, model, color, alpha});
  }
  void cylinder(
      const QVector3D&, const QVector3D&, float, const QVector3D&, float) override {
    ++cylinder_count;
  }
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

auto fake_texture(int id) -> Render::GL::Texture* {
  return reinterpret_cast<Render::GL::Texture*>(static_cast<intptr_t>(id));
}

auto near_vec3(const QVector3D& lhs,
               const QVector3D& rhs,
               float eps = k_default_epsilon) -> bool {
  return (lhs - rhs).length() <= eps;
}

auto has_mesh_center_near_axis(const std::vector<RecordedMesh>& meshes,
                               float seam_position,
                               bool use_x_axis,
                               float tolerance,
                               float min_center_y) -> bool {
  for (const RecordedMesh& mesh : meshes) {
    const QVector3D center = mesh.model.column(3).toVector3D();
    if (center.y() < min_center_y) {
      continue;
    }

    const float axis_value = use_x_axis ? center.x() : center.z();
    if (std::abs(axis_value - seam_position) <= tolerance) {
      return true;
    }
  }
  return false;
}

auto axis_scale_of(const QMatrix4x4& model, int column) -> float {
  return model.column(column).toVector3D().length();
}

auto count_meshes_with_color(const std::vector<RecordedMesh>& meshes,
                             const QVector3D& color,
                             float eps = k_default_epsilon) -> std::size_t {
  std::size_t count = 0;
  for (const RecordedMesh& mesh : meshes) {
    if (near_vec3(mesh.color, color, eps)) {
      ++count;
    }
  }
  return count;
}

auto bounds_for_recorded_meshes(const std::vector<RecordedMesh>& meshes)
    -> Render::GL::BoundingBox {
  auto bounds = Render::GL::empty_bounding_box();
  static const std::array<QVector3D, 8> k_unit_corners = {
      QVector3D(-0.5F, -0.5F, -0.5F),
      QVector3D(0.5F, -0.5F, -0.5F),
      QVector3D(-0.5F, 0.5F, -0.5F),
      QVector3D(0.5F, 0.5F, -0.5F),
      QVector3D(-0.5F, -0.5F, 0.5F),
      QVector3D(0.5F, -0.5F, 0.5F),
      QVector3D(-0.5F, 0.5F, 0.5F),
      QVector3D(0.5F, 0.5F, 0.5F),
  };

  for (const RecordedMesh& mesh : meshes) {
    for (const QVector3D& corner : k_unit_corners) {
      bounds.expand(mesh.model.map(corner));
    }
  }

  return bounds;
}

auto bounds_for_recorded_meshes_below_center_y(const std::vector<RecordedMesh>& meshes,
                                               float max_center_y)
    -> Render::GL::BoundingBox {
  auto bounds = Render::GL::empty_bounding_box();
  static const std::array<QVector3D, 8> k_unit_corners = {
      QVector3D(-0.5F, -0.5F, -0.5F),
      QVector3D(0.5F, -0.5F, -0.5F),
      QVector3D(-0.5F, 0.5F, -0.5F),
      QVector3D(0.5F, 0.5F, -0.5F),
      QVector3D(-0.5F, -0.5F, 0.5F),
      QVector3D(0.5F, -0.5F, 0.5F),
      QVector3D(-0.5F, 0.5F, 0.5F),
      QVector3D(0.5F, 0.5F, 0.5F),
  };

  for (const RecordedMesh& mesh : meshes) {
    if (mesh.model.column(3).y() > max_center_y) {
      continue;
    }
    for (const QVector3D& corner : k_unit_corners) {
      bounds.expand(mesh.model.map(corner));
    }
  }

  return bounds;
}

auto make_test_horse_frames() -> Render::GL::HorseBodyFrames {
  using namespace Render::GL;

  HorseBodyFrames frames;
  auto set_frame = [](HorseAttachmentFrame& frame, const QVector3D& origin) {
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
  builder.add_palette_mesh(
      fake_mesh(2),
      box_local_model(QVector3D(-1.0F, 0.0F, 0.0F), QVector3D(1.0F, 1.0F, 1.0F)),
      0);

  RenderArchetype archetype = std::move(builder).build();
  ASSERT_EQ(archetype.lods[0].draws.size(), 2U);
  EXPECT_TRUE(
      near_vec3(archetype.lods[0].local_bounds.min, QVector3D(-1.5F, -1.0F, -0.5F)));
  EXPECT_TRUE(
      near_vec3(archetype.lods[0].local_bounds.max, QVector3D(3.0F, 3.0F, 6.0F)));

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

  ASSERT_EQ(submitter.meshes.size(), 2U);
  EXPECT_TRUE(near_vec3(submitter.meshes[0].model.column(3).toVector3D(),
                        QVector3D(12.0F, 1.0F, 3.0F)));
  EXPECT_TRUE(near_vec3(submitter.meshes[1].model.column(3).toVector3D(),
                        QVector3D(9.0F, 0.0F, 0.0F)));
  EXPECT_TRUE(near_vec3(submitter.meshes[0].color, QVector3D(0.2F, 0.3F, 0.4F)));
  EXPECT_TRUE(near_vec3(submitter.meshes[1].color, palette[0]));
  EXPECT_EQ(submitter.meshes[0].texture, fake_texture(7));
  EXPECT_FLOAT_EQ(submitter.meshes[0].alpha, 0.5F);
}

TEST(RenderArchetype, StoredRenderInstanceOwnsPaletteData) {
  using namespace Render::GL;

  RenderArchetypeBuilder builder("stored_instance_test");
  builder.add_palette_box(QVector3D(0.0F, 0.0F, 0.0F), QVector3D(1.0F, 1.0F, 1.0F), 0);
  RenderArchetype const archetype = std::move(builder).build();

  StoredRenderInstance<2> stored;
  stored.archetype = &archetype;
  stored.default_texture = fake_texture(11);

  std::array<QVector3D, 1> palette{QVector3D(0.7F, 0.2F, 0.1F)};
  stored.set_palette(palette);
  palette[0] = QVector3D(0.1F, 0.8F, 0.3F);

  RecordingSubmitter submitter;
  submit_render_instance(submitter, stored.render_instance());

  ASSERT_EQ(submitter.meshes.size(), 1U);
  EXPECT_TRUE(near_vec3(submitter.meshes[0].color, QVector3D(0.7F, 0.2F, 0.1F)));
  EXPECT_EQ(submitter.meshes[0].texture, fake_texture(11));
}

TEST(RenderArchetypeEquipment, ReplaysArchetypeInstancesThroughEquipmentBatch) {
  using namespace Render::GL;

  RenderArchetypeBuilder builder("equipment_archetype");
  builder.add_palette_box(
      QVector3D(0.5F, 0.25F, -0.5F), QVector3D(0.5F, 0.25F, 0.75F), 0);
  RenderArchetype const archetype = std::move(builder).build();

  EquipmentBatch batch;
  std::array<QVector3D, 1> palette{QVector3D(0.3F, 0.5F, 0.8F)};
  QMatrix4x4 world;
  world.translate(2.0F, 3.0F, 4.0F);
  append_equipment_archetype(batch, archetype, world, palette, fake_texture(9), 0.75F);

  RecordingSubmitter submitter;
  submit_equipment_batch(batch, submitter);

  ASSERT_EQ(submitter.meshes.size(), 1U);
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
  ScaleBardingRenderer::submit(ctx, make_test_horse_frames(), variant, {}, batch);

  EXPECT_TRUE(batch.meshes.empty());
  EXPECT_TRUE(batch.cylinders.empty());
  ASSERT_EQ(batch.archetypes.size(), 3U);

  RecordingSubmitter submitter;
  submit_equipment_batch(batch, submitter);
  ASSERT_EQ(submitter.meshes.size(), 4U);
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
  ASSERT_EQ(batch.archetypes.size(), 1U);
  EXPECT_TRUE(batch.cylinders.empty());

  RecordingSubmitter submitter;
  submit_equipment_batch(batch, submitter);
  EXPECT_EQ(submitter.meshes.size(), 4U);
  EXPECT_EQ(submitter.cylinder_count, 0U);
}

TEST(RenderArchetypeEquipment, SaddleArchetypesStayLowAndWide) {
  using namespace Render::GL;

  std::array<const RenderArchetype*, 3> const saddles = {
      &roman_saddle_archetype(),
      &carthage_saddle_archetype(),
      &light_cavalry_saddle_archetype()};
  std::array<QVector3D, 1> palette{QVector3D(0.42F, 0.24F, 0.11F)};

  for (const RenderArchetype* saddle : saddles) {
    RecordingSubmitter submitter;
    QMatrix4x4 const world;
    RenderInstance const instance{
        .archetype = saddle, .world = world, .palette = palette};
    submit_render_instance(submitter, instance);

    ASSERT_GE(submitter.meshes.size(), 6U);
    BoundingBox const bounds = bounds_for_recorded_meshes(submitter.meshes);
    QVector3D const size = bounds.max - bounds.min;
    EXPECT_GT(size.z(), size.y() * k_saddle_length_to_height_ratio);
    EXPECT_GT(size.x(), size.y() * k_saddle_width_to_height_ratio);

    int front_arch_count = 0;
    int rear_arch_count = 0;
    for (const RecordedMesh& mesh : submitter.meshes) {
      QVector3D const center = mesh.model.column(3).toVector3D();
      if (center.z() > k_saddle_front_arch_z_threshold &&
          center.y() > k_saddle_front_arch_y_threshold) {
        ++front_arch_count;
      }
      if (center.z() < k_saddle_rear_arch_z_threshold &&
          center.y() > k_saddle_rear_arch_y_threshold) {
        ++rear_arch_count;
      }
    }

    EXPECT_LT(bounds.max.y(), k_saddle_max_height);
    EXPECT_GE(front_arch_count, 1);
    EXPECT_GE(rear_arch_count, 1);
  }
}

TEST(RenderArchetypeBuildings, RomanHomeRendersExpectedStaticMeshCount) {
  using namespace Render::GL;

  EntityRendererRegistry registry;
  Roman::register_home_renderer(registry);
  const auto renderer = registry.get("troops/roman/home");
  ASSERT_TRUE(static_cast<bool>(renderer));

  Engine::Core::Entity entity(1);
  auto* renderable = entity.add_component<Engine::Core::RenderableComponent>("", "");
  ASSERT_NE(renderable, nullptr);
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(
      k_default_unit_max_health, k_default_unit_health, 0.0F, 0.0F);
  ASSERT_NE(unit, nullptr);

  DrawContext ctx;
  ResourceManager resources;
  ctx.entity = &entity;
  ctx.resources = &resources;
  ctx.model = QMatrix4x4{};

  RecordingSubmitter submitter;
  renderer(ctx, submitter);

  EXPECT_GT(submitter.meshes.size(), k_min_home_mesh_count);
}

TEST(RenderArchetypeSiege, NationVariantRenderersAreRegistered) {
  using namespace Render::GL;

  EntityRendererRegistry registry;
  Roman::register_catapult_renderer(registry);
  Carthage::register_catapult_renderer(registry);
  Roman::register_ballista_renderer(registry);
  Carthage::register_ballista_renderer(registry);

  std::array<const char*, 4> const renderer_keys = {
      "troops/roman/catapult",
      "troops/carthage/catapult",
      "troops/roman/ballista",
      "troops/carthage/ballista",
  };
  for (const char* renderer_key : renderer_keys) {
    auto renderer = registry.get(renderer_key);
    ASSERT_TRUE(static_cast<bool>(renderer)) << renderer_key;
  }
}

TEST(RenderArchetypeBuildings, RendererHandleResolvesRomanHome) {
  using namespace Render::GL;

  EntityRendererRegistry registry;
  Roman::register_home_renderer(registry);
  const auto renderer_handle = registry.get_handle("troops/roman/home");
  ASSERT_NE(renderer_handle, k_invalid_renderer_handle);

  const auto* renderer = registry.get(renderer_handle);
  ASSERT_NE(renderer, nullptr);

  Engine::Core::Entity entity(2);
  auto* renderable = entity.add_component<Engine::Core::RenderableComponent>("", "");
  ASSERT_NE(renderable, nullptr);
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(
      k_default_unit_max_health, k_default_unit_health, 0.0F, 0.0F);
  ASSERT_NE(unit, nullptr);

  DrawContext ctx;
  ResourceManager resources;
  ctx.entity = &entity;
  ctx.resources = &resources;
  ctx.model = QMatrix4x4{};
  ctx.renderer_handle = renderer_handle;

  RecordingSubmitter submitter;
  (*renderer)(ctx, submitter);

  EXPECT_GT(submitter.meshes.size(), k_min_home_mesh_count);
}

TEST(RenderArchetypeBuildings, RomanHomeAppliesTeamPaletteSlot) {
  using namespace Render::GL;

  EntityRendererRegistry registry;
  Roman::register_home_renderer(registry);
  const auto renderer = registry.get("troops/roman/home");
  ASSERT_TRUE(static_cast<bool>(renderer));

  Engine::Core::Entity entity(3);
  auto* renderable = entity.add_component<Engine::Core::RenderableComponent>("", "");
  ASSERT_NE(renderable, nullptr);
  renderable->color = {0.8F, 0.1F, 0.2F};
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(
      k_default_unit_max_health, k_default_unit_health, 0.0F, 0.0F);
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
  QVector3D const expected_team(0.8F, 0.1F, 0.2F);
  for (const RecordedMesh& mesh : submitter.meshes) {
    if (near_vec3(mesh.color, expected_team)) {
      found_team_tint = true;
      break;
    }
  }
  EXPECT_TRUE(found_team_tint);
}

TEST(RenderArchetypeBuildings, RomanAndCarthageHomesRemainDistinctSilhouettes) {
  using namespace Render::GL;

  EntityRendererRegistry registry;
  Roman::register_home_renderer(registry);
  Carthage::register_home_renderer(registry);
  const auto roman_renderer = registry.get("troops/roman/home");
  const auto carthage_renderer = registry.get("troops/carthage/home");
  ASSERT_TRUE(static_cast<bool>(roman_renderer));
  ASSERT_TRUE(static_cast<bool>(carthage_renderer));

  auto render_bounds = [&](std::uint32_t entity_id, const RenderFunc& renderer) {
    Engine::Core::Entity entity(entity_id);
    auto* renderable = entity.add_component<Engine::Core::RenderableComponent>("", "");
    auto* unit = entity.add_component<Engine::Core::UnitComponent>(
        k_default_unit_max_health, k_default_unit_health, 0.0F, 0.0F);
    EXPECT_NE(renderable, nullptr);
    EXPECT_NE(unit, nullptr);

    DrawContext ctx;
    ResourceManager resources;
    ctx.entity = &entity;
    ctx.resources = &resources;
    ctx.model = QMatrix4x4{};

    RecordingSubmitter submitter;
    renderer(ctx, submitter);
    EXPECT_FALSE(submitter.meshes.empty());
    return std::make_pair(
        submitter.meshes.size(),
        bounds_for_recorded_meshes_below_center_y(submitter.meshes, 2.7F));
  };

  const auto [roman_count, roman_bounds] = render_bounds(21, roman_renderer);
  const auto [carthage_count, carthage_bounds] = render_bounds(22, carthage_renderer);

  EXPECT_GT(roman_bounds.max.y(), carthage_bounds.max.y());
  EXPECT_NE(roman_count, carthage_count);
}

TEST(RenderArchetypeBuildings, RomanTowerAppliesTeamPaletteSlot) {
  using namespace Render::GL;

  EntityRendererRegistry registry;
  Roman::register_defense_tower_renderer(registry);
  const auto renderer = registry.get("troops/roman/defense_tower");
  ASSERT_TRUE(static_cast<bool>(renderer));

  Engine::Core::Entity entity(2);
  auto* renderable = entity.add_component<Engine::Core::RenderableComponent>("", "");
  ASSERT_NE(renderable, nullptr);
  renderable->color = {1.2F, -0.1F, 0.35F};
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(
      k_default_unit_max_health, k_default_unit_health, 0.0F, 0.0F);
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
  for (const RecordedMesh& mesh : submitter.meshes) {
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
  auto* renderable = entity.add_component<Engine::Core::RenderableComponent>("", "");
  ASSERT_NE(renderable, nullptr);
  renderable->color = {0.8F, 0.2F, 0.6F};
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(
      k_default_unit_max_health, k_default_unit_health, 0.0F, 0.0F);
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
  for (const RecordedMesh& mesh : submitter.meshes) {
    if (near_vec3(mesh.color, expected_team)) {
      found_team_tint = true;
      break;
    }
  }
  EXPECT_TRUE(found_team_tint);
}

TEST(RenderArchetypeBuildings, TowerBannersRiseAboveRooflines) {
  using namespace Render::GL;

  auto render_bounds = [](auto register_renderer_fn,
                          const char* key,
                          std::uint32_t entity_id) -> BoundingBox {
    EntityRendererRegistry const registry;
    register_renderer_fn(registry);
    const auto renderer = registry.get(key);
    EXPECT_TRUE(static_cast<bool>(renderer));
    if (!renderer) {
      return {};
    }

    Engine::Core::Entity entity(entity_id);
    auto* renderable = entity.add_component<Engine::Core::RenderableComponent>("", "");
    auto* unit = entity.add_component<Engine::Core::UnitComponent>(
        k_default_unit_max_health, k_default_unit_health, 0.0F, 0.0F);
    EXPECT_NE(renderable, nullptr);
    EXPECT_NE(unit, nullptr);
    if (renderable == nullptr || unit == nullptr) {
      return {};
    }

    DrawContext ctx;
    ResourceManager resources;
    ctx.entity = &entity;
    ctx.resources = &resources;
    ctx.model = QMatrix4x4{};

    RecordingSubmitter submitter;
    renderer(ctx, submitter);
    EXPECT_FALSE(submitter.meshes.empty());
    return bounds_for_recorded_meshes(submitter.meshes);
  };

  const BoundingBox roman_bounds = render_bounds(
      Roman::register_defense_tower_renderer, "troops/roman/defense_tower", 200);
  const BoundingBox carthage_bounds = render_bounds(
      Carthage::register_defense_tower_renderer, "troops/carthage/defense_tower", 201);

  EXPECT_GT(roman_bounds.max.y(), k_roman_tower_min_banner_height);
  EXPECT_GT(carthage_bounds.max.y(), k_carthage_tower_min_banner_height);
}

TEST(RenderArchetypeBuildings, RomanTowerHealthBarOnlyShowsWhileUnderAttack) {
  using namespace Render::GL;

  EntityRendererRegistry registry;
  Roman::register_defense_tower_renderer(registry);
  const auto renderer = registry.get("troops/roman/defense_tower");
  ASSERT_TRUE(static_cast<bool>(renderer));

  auto render_mesh_count = [&](bool under_attack) {
    Engine::Core::Entity entity(30 + (under_attack ? 1U : 0U));
    auto* renderable = entity.add_component<Engine::Core::RenderableComponent>("", "");
    auto* unit = entity.add_component<Engine::Core::UnitComponent>(
        k_default_unit_max_health, k_default_unit_health, 0.0F, 0.0F);
    EXPECT_NE(renderable, nullptr);
    EXPECT_NE(unit, nullptr);
    if (renderable == nullptr || unit == nullptr) {
      return std::size_t{0};
    }
    unit->health = 50;
    if (under_attack) {
      auto* capture = entity.add_component<Engine::Core::CaptureComponent>();
      EXPECT_NE(capture, nullptr);
      if (capture == nullptr) {
        return std::size_t{0};
      }
      capture->is_being_captured = true;
      capture->capturing_player_id = -1;
      capture->capture_progress = 3.0F;
    }

    DrawContext ctx;
    ResourceManager resources;
    ctx.entity = &entity;
    ctx.resources = &resources;
    ctx.model = QMatrix4x4{};
    ctx.animation_time = 1.3F;

    RecordingSubmitter submitter;
    renderer(ctx, submitter);
    return submitter.meshes.size();
  };

  const std::size_t idle_count = render_mesh_count(false);
  const std::size_t under_attack_count = render_mesh_count(true);
  EXPECT_GT(under_attack_count, idle_count);
}

TEST(RenderArchetypeBuildings, RomanHomeHealthBarOnlyShowsWhileUnderAttack) {
  using namespace Render::GL;

  EntityRendererRegistry registry;
  Roman::register_home_renderer(registry);
  const auto renderer = registry.get("troops/roman/home");
  ASSERT_TRUE(static_cast<bool>(renderer));

  auto render_mesh_count = [&](bool under_attack) {
    Engine::Core::Entity entity(34 + (under_attack ? 1U : 0U));
    auto* renderable = entity.add_component<Engine::Core::RenderableComponent>("", "");
    auto* unit = entity.add_component<Engine::Core::UnitComponent>(
        k_default_unit_max_health, k_default_unit_health, 0.0F, 0.0F);
    EXPECT_NE(renderable, nullptr);
    EXPECT_NE(unit, nullptr);
    if (renderable == nullptr || unit == nullptr) {
      return std::size_t{0};
    }
    unit->health = 50;
    if (under_attack) {
      auto* capture = entity.add_component<Engine::Core::CaptureComponent>();
      EXPECT_NE(capture, nullptr);
      if (capture == nullptr) {
        return std::size_t{0};
      }
      capture->is_being_captured = true;
      capture->capturing_player_id = -1;
      capture->capture_progress = 4.0F;
    }

    DrawContext ctx;
    ResourceManager resources;
    ctx.entity = &entity;
    ctx.resources = &resources;
    ctx.model = QMatrix4x4{};
    ctx.animation_time = 0.9F;

    RecordingSubmitter submitter;
    renderer(ctx, submitter);
    return submitter.meshes.size();
  };

  const std::size_t idle_count = render_mesh_count(false);
  const std::size_t under_attack_count = render_mesh_count(true);
  EXPECT_GT(under_attack_count, idle_count);
}

TEST(RenderArchetypeBuildings, RomanHomeHealthBarShowsOnRecentCombatHit) {
  using namespace Render::GL;

  EntityRendererRegistry registry;
  Roman::register_home_renderer(registry);
  const auto renderer = registry.get("troops/roman/home");
  ASSERT_TRUE(static_cast<bool>(renderer));

  auto render_mesh_count = [&](bool recent_hit) {
    Engine::Core::Entity entity(36 + (recent_hit ? 1U : 0U));
    auto* renderable = entity.add_component<Engine::Core::RenderableComponent>("", "");
    auto* unit = entity.add_component<Engine::Core::UnitComponent>(
        k_default_unit_max_health, k_default_unit_health, 0.0F, 0.0F);
    EXPECT_NE(renderable, nullptr);
    EXPECT_NE(unit, nullptr);
    if (renderable == nullptr || unit == nullptr) {
      return std::size_t{0};
    }
    unit->health = 50;
    if (recent_hit) {
      auto* feedback = entity.add_component<Engine::Core::HitFeedbackComponent>();
      EXPECT_NE(feedback, nullptr);
      if (feedback == nullptr) {
        return std::size_t{0};
      }
      feedback->is_reacting = true;
      feedback->reaction_time = 0.0F;
    }

    DrawContext ctx;
    ResourceManager resources;
    ctx.entity = &entity;
    ctx.resources = &resources;
    ctx.model = QMatrix4x4{};
    ctx.animation_time = 0.5F;

    RecordingSubmitter submitter;
    renderer(ctx, submitter);
    return submitter.meshes.size();
  };

  const std::size_t idle_count = render_mesh_count(false);
  const std::size_t recent_hit_count = render_mesh_count(true);
  EXPECT_GT(recent_hit_count, idle_count);
}

TEST(RenderArchetypeBuildings, DestroyedRomanTowerRemovesBannerTint) {
  using namespace Render::GL;

  EntityRendererRegistry registry;
  Roman::register_defense_tower_renderer(registry);
  const auto renderer = registry.get("troops/roman/defense_tower");
  ASSERT_TRUE(static_cast<bool>(renderer));

  auto render_team_tint_count = [&](std::uint32_t entity_id, int health) {
    Engine::Core::Entity entity(entity_id);
    auto* renderable = entity.add_component<Engine::Core::RenderableComponent>("", "");
    auto* unit = entity.add_component<Engine::Core::UnitComponent>(
        k_default_unit_max_health, k_default_unit_health, 0.0F, 0.0F);
    EXPECT_NE(renderable, nullptr);
    EXPECT_NE(unit, nullptr);
    if (renderable == nullptr || unit == nullptr) {
      return std::size_t{0};
    }
    renderable->color = {0.82F, 0.25F, 0.55F};
    unit->health = health;

    DrawContext ctx;
    ResourceManager resources;
    ctx.entity = &entity;
    ctx.resources = &resources;
    ctx.model = QMatrix4x4{};

    RecordingSubmitter submitter;
    renderer(ctx, submitter);
    return count_meshes_with_color(submitter.meshes, QVector3D(0.82F, 0.25F, 0.55F));
  };

  const std::size_t normal_team_tint = render_team_tint_count(37, 100);
  const std::size_t destroyed_team_tint = render_team_tint_count(38, 10);
  EXPECT_GT(normal_team_tint, 0U);
  EXPECT_EQ(destroyed_team_tint, 0U);
}

TEST(RenderArchetypeBuildings, RomanTowerDamageStatesReduceSilhouette) {
  using namespace Render::GL;

  EntityRendererRegistry registry;
  Roman::register_defense_tower_renderer(registry);
  const auto renderer = registry.get("troops/roman/defense_tower");
  ASSERT_TRUE(static_cast<bool>(renderer));

  auto render_bounds = [&](std::uint32_t entity_id, int health) {
    Engine::Core::Entity entity(entity_id);
    auto* renderable = entity.add_component<Engine::Core::RenderableComponent>("", "");
    auto* unit = entity.add_component<Engine::Core::UnitComponent>(
        k_default_unit_max_health, k_default_unit_health, 0.0F, 0.0F);
    EXPECT_NE(renderable, nullptr);
    EXPECT_NE(unit, nullptr);
    if (renderable == nullptr || unit == nullptr) {
      return std::make_pair(std::size_t{0}, Render::GL::BoundingBox{});
    }
    unit->health = health;

    DrawContext ctx;
    ResourceManager resources;
    ctx.entity = &entity;
    ctx.resources = &resources;
    ctx.model = QMatrix4x4{};

    RecordingSubmitter submitter;
    renderer(ctx, submitter);
    EXPECT_FALSE(submitter.meshes.empty());
    return std::make_pair(
        submitter.meshes.size(),
        bounds_for_recorded_meshes_below_center_y(submitter.meshes, 2.0F));
  };

  const auto [normal_count, normal_bounds] = render_bounds(31, 100);
  const auto [damaged_count, damaged_bounds] = render_bounds(32, 50);
  const auto [destroyed_count, destroyed_bounds] = render_bounds(33, 10);

  EXPECT_GT(normal_count, damaged_count);
  EXPECT_GT(damaged_count, destroyed_count);
  EXPECT_GT(normal_bounds.max.y(), damaged_bounds.max.y());
  EXPECT_GT(damaged_bounds.max.y(), destroyed_bounds.max.y());
}

TEST(RenderArchetypeBuildings, CarthageTowerDamageStatesReduceSilhouette) {
  using namespace Render::GL;

  EntityRendererRegistry registry;
  Carthage::register_defense_tower_renderer(registry);
  const auto renderer = registry.get("troops/carthage/defense_tower");
  ASSERT_TRUE(static_cast<bool>(renderer));

  auto render_bounds = [&](std::uint32_t entity_id, int health) {
    Engine::Core::Entity entity(entity_id);
    auto* renderable = entity.add_component<Engine::Core::RenderableComponent>("", "");
    auto* unit = entity.add_component<Engine::Core::UnitComponent>(
        k_default_unit_max_health, k_default_unit_health, 0.0F, 0.0F);
    EXPECT_NE(renderable, nullptr);
    EXPECT_NE(unit, nullptr);
    if (renderable == nullptr || unit == nullptr) {
      return std::make_pair(std::size_t{0}, Render::GL::BoundingBox{});
    }
    unit->health = health;

    DrawContext ctx;
    ResourceManager resources;
    ctx.entity = &entity;
    ctx.resources = &resources;
    ctx.model = QMatrix4x4{};

    RecordingSubmitter submitter;
    renderer(ctx, submitter);
    EXPECT_FALSE(submitter.meshes.empty());
    return std::make_pair(submitter.meshes.size(),
                          bounds_for_recorded_meshes(submitter.meshes));
  };

  const auto [normal_count, normal_bounds] = render_bounds(41, 100);
  const auto [damaged_count, damaged_bounds] = render_bounds(42, 50);
  const auto [destroyed_count, destroyed_bounds] = render_bounds(43, 10);

  EXPECT_GT(normal_count, damaged_count);
  EXPECT_GT(damaged_count, destroyed_count);
  EXPECT_GT(normal_bounds.max.y(), damaged_bounds.max.y());
  EXPECT_GT(damaged_bounds.max.y(), destroyed_bounds.max.y());
}

TEST(RenderArchetypeBuildings, RomanStraightWallFormsTallContinuousPalisade) {
  using namespace Render::GL;

  EntityRendererRegistry registry;
  Roman::register_wall_renderer(registry);
  const auto renderer = registry.get("troops/roman/wall_segment_straight");
  ASSERT_TRUE(static_cast<bool>(renderer));

  Engine::Core::Entity entity(4);
  auto* renderable = entity.add_component<Engine::Core::RenderableComponent>("", "");
  ASSERT_NE(renderable, nullptr);
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(
      k_default_unit_max_health, k_default_unit_health, 0.0F, 0.0F);
  ASSERT_NE(unit, nullptr);

  DrawContext ctx;
  ResourceManager resources;
  ctx.entity = &entity;
  ctx.resources = &resources;

  RecordingSubmitter left_submitter;
  ctx.model = QMatrix4x4{};
  renderer(ctx, left_submitter);
  const auto left_bounds = bounds_for_recorded_meshes(left_submitter.meshes);

  RecordingSubmitter right_submitter;
  ctx.model = QMatrix4x4{};
  ctx.model.translate(2.0F, 0.0F, 0.0F);
  renderer(ctx, right_submitter);
  const auto right_bounds = bounds_for_recorded_meshes(right_submitter.meshes);

  EXPECT_GT(left_bounds.max.y(), 2.5F);
  EXPECT_GT(left_bounds.max.x() - left_bounds.min.x(), 2.0F);
  EXPECT_GE(left_bounds.max.x() + 0.001F, right_bounds.min.x());
}

TEST(RenderArchetypeBuildings, CarthageStraightWallFormsTallContinuousPalisade) {
  using namespace Render::GL;

  EntityRendererRegistry registry;
  Carthage::register_wall_renderer(registry);
  const auto renderer = registry.get("troops/carthage/wall_segment_straight");
  ASSERT_TRUE(static_cast<bool>(renderer));

  Engine::Core::Entity entity(44);
  auto* renderable = entity.add_component<Engine::Core::RenderableComponent>("", "");
  ASSERT_NE(renderable, nullptr);
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(
      k_default_unit_max_health, k_default_unit_health, 0.0F, 0.0F);
  ASSERT_NE(unit, nullptr);

  DrawContext ctx;
  ResourceManager resources;
  ctx.entity = &entity;
  ctx.resources = &resources;

  RecordingSubmitter left_submitter;
  ctx.model = QMatrix4x4{};
  renderer(ctx, left_submitter);
  const auto left_bounds = bounds_for_recorded_meshes(left_submitter.meshes);

  RecordingSubmitter right_submitter;
  ctx.model = QMatrix4x4{};
  ctx.model.translate(2.0F, 0.0F, 0.0F);
  renderer(ctx, right_submitter);
  const auto right_bounds = bounds_for_recorded_meshes(right_submitter.meshes);

  EXPECT_GT(left_bounds.max.y(), 2.4F);
  EXPECT_GT(left_bounds.max.x() - left_bounds.min.x(), 2.0F);
  EXPECT_GE(left_bounds.max.x() + 0.001F, right_bounds.min.x());
}

TEST(RenderArchetypeBuildings, RomanBarracksDamageStatesReduceSilhouette) {
  using namespace Render::GL;

  EntityRendererRegistry registry;
  Roman::register_barracks_renderer(registry);
  const auto renderer = registry.get("troops/roman/barracks");
  ASSERT_TRUE(static_cast<bool>(renderer));

  auto render_bounds = [&](std::uint32_t entity_id, int health) {
    Engine::Core::Entity entity(entity_id);
    auto* renderable = entity.add_component<Engine::Core::RenderableComponent>("", "");
    auto* transform = entity.add_component<Engine::Core::TransformComponent>();
    auto* unit = entity.add_component<Engine::Core::UnitComponent>(
        k_default_unit_max_health, k_default_unit_health, 0.0F, 0.0F);
    EXPECT_NE(renderable, nullptr);
    EXPECT_NE(transform, nullptr);
    EXPECT_NE(unit, nullptr);
    if (unit == nullptr) {
      return std::make_pair(std::size_t{0}, Render::GL::BoundingBox{});
    }
    unit->health = health;

    DrawContext ctx;
    ResourceManager resources;
    ctx.entity = &entity;
    ctx.resources = &resources;
    ctx.model = QMatrix4x4{};

    RecordingSubmitter submitter;
    renderer(ctx, submitter);
    EXPECT_FALSE(submitter.meshes.empty());
    return std::make_pair(submitter.meshes.size(),
                          bounds_for_recorded_meshes(submitter.meshes));
  };

  const auto [normal_count, normal_bounds] = render_bounds(51, 100);
  const auto [damaged_count, damaged_bounds] = render_bounds(52, 50);
  const auto [destroyed_count, destroyed_bounds] = render_bounds(53, 10);
  (void)normal_bounds;
  (void)damaged_bounds;
  (void)destroyed_bounds;

  EXPECT_GT(normal_count, damaged_count);
  EXPECT_GT(damaged_count, destroyed_count);
}

TEST(RenderArchetypeBuildings, CarthageBarracksDamageStatesReduceSilhouette) {
  using namespace Render::GL;

  EntityRendererRegistry registry;
  Carthage::register_barracks_renderer(registry);
  const auto renderer = registry.get("troops/carthage/barracks");
  ASSERT_TRUE(static_cast<bool>(renderer));

  auto render_bounds = [&](std::uint32_t entity_id, int health) {
    Engine::Core::Entity entity(entity_id);
    auto* renderable = entity.add_component<Engine::Core::RenderableComponent>("", "");
    auto* transform = entity.add_component<Engine::Core::TransformComponent>();
    auto* unit = entity.add_component<Engine::Core::UnitComponent>(
        k_default_unit_max_health, k_default_unit_health, 0.0F, 0.0F);
    EXPECT_NE(renderable, nullptr);
    EXPECT_NE(transform, nullptr);
    EXPECT_NE(unit, nullptr);
    if (unit == nullptr) {
      return std::make_pair(std::size_t{0}, Render::GL::BoundingBox{});
    }
    unit->health = health;

    DrawContext ctx;
    ResourceManager resources;
    ctx.entity = &entity;
    ctx.resources = &resources;
    ctx.model = QMatrix4x4{};

    RecordingSubmitter submitter;
    renderer(ctx, submitter);
    EXPECT_FALSE(submitter.meshes.empty());
    return std::make_pair(submitter.meshes.size(),
                          bounds_for_recorded_meshes(submitter.meshes));
  };

  const auto [normal_count, normal_bounds] = render_bounds(61, 100);
  const auto [damaged_count, damaged_bounds] = render_bounds(62, 50);
  const auto [destroyed_count, destroyed_bounds] = render_bounds(63, 10);
  (void)normal_bounds;
  (void)damaged_bounds;
  (void)destroyed_bounds;

  EXPECT_GT(normal_count, damaged_count);
  EXPECT_GT(damaged_count, destroyed_count);
}

TEST(RenderArchetypeBuildings, RomanWallEndKeepsFullSegmentFootprint) {
  using namespace Render::GL;

  EntityRendererRegistry registry;
  Roman::register_wall_renderer(registry);
  const auto renderer = registry.get("troops/roman/wall_segment_end");
  ASSERT_TRUE(static_cast<bool>(renderer));

  Engine::Core::Entity entity(5);
  auto* renderable = entity.add_component<Engine::Core::RenderableComponent>("", "");
  ASSERT_NE(renderable, nullptr);
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(
      k_default_unit_max_health, k_default_unit_health, 0.0F, 0.0F);
  ASSERT_NE(unit, nullptr);

  DrawContext ctx;
  ResourceManager resources;
  ctx.entity = &entity;
  ctx.resources = &resources;
  ctx.model = QMatrix4x4{};

  RecordingSubmitter submitter;
  renderer(ctx, submitter);
  const auto bounds = bounds_for_recorded_meshes(submitter.meshes);

  EXPECT_LT(bounds.min.x(), -0.9F);
  EXPECT_GT(bounds.max.x(), 0.9F);
}

TEST(RenderArchetypeBuildings, RomanFacingWallEndsPlaceTallBoardsAtEastWestSeam) {
  using namespace Render::GL;

  EntityRendererRegistry registry;
  Roman::register_wall_renderer(registry);
  const auto renderer = registry.get("troops/roman/wall_segment_end");
  ASSERT_TRUE(static_cast<bool>(renderer));

  Engine::Core::Entity entity(71);
  auto* renderable = entity.add_component<Engine::Core::RenderableComponent>("", "");
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(
      k_default_unit_max_health, k_default_unit_health, 0.0F, 0.0F);
  ASSERT_NE(renderable, nullptr);
  ASSERT_NE(unit, nullptr);

  DrawContext ctx;
  ResourceManager resources;
  ctx.entity = &entity;
  ctx.resources = &resources;

  std::vector<RecordedMesh> meshes;

  RecordingSubmitter left_submitter;
  ctx.model = QMatrix4x4{};
  renderer(ctx, left_submitter);
  meshes.insert(
      meshes.end(), left_submitter.meshes.begin(), left_submitter.meshes.end());

  RecordingSubmitter right_submitter;
  ctx.model = QMatrix4x4{};
  ctx.model.translate(2.0F, 0.0F, 0.0F);
  ctx.model.rotate(180.0F, 0.0F, 1.0F, 0.0F);
  renderer(ctx, right_submitter);
  meshes.insert(
      meshes.end(), right_submitter.meshes.begin(), right_submitter.meshes.end());

  EXPECT_TRUE(has_mesh_center_near_axis(meshes, 1.0F, true, 0.08F, 1.0F));
}

TEST(RenderArchetypeBuildings, CarthageFacingWallEndsPlaceTallBoardsAtEastWestSeam) {
  using namespace Render::GL;

  EntityRendererRegistry registry;
  Carthage::register_wall_renderer(registry);
  const auto renderer = registry.get("troops/carthage/wall_segment_end");
  ASSERT_TRUE(static_cast<bool>(renderer));

  Engine::Core::Entity entity(72);
  auto* renderable = entity.add_component<Engine::Core::RenderableComponent>("", "");
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(
      k_default_unit_max_health, k_default_unit_health, 0.0F, 0.0F);
  ASSERT_NE(renderable, nullptr);
  ASSERT_NE(unit, nullptr);

  DrawContext ctx;
  ResourceManager resources;
  ctx.entity = &entity;
  ctx.resources = &resources;

  std::vector<RecordedMesh> meshes;

  RecordingSubmitter left_submitter;
  ctx.model = QMatrix4x4{};
  renderer(ctx, left_submitter);
  meshes.insert(
      meshes.end(), left_submitter.meshes.begin(), left_submitter.meshes.end());

  RecordingSubmitter right_submitter;
  ctx.model = QMatrix4x4{};
  ctx.model.translate(2.0F, 0.0F, 0.0F);
  ctx.model.rotate(180.0F, 0.0F, 1.0F, 0.0F);
  renderer(ctx, right_submitter);
  meshes.insert(
      meshes.end(), right_submitter.meshes.begin(), right_submitter.meshes.end());

  EXPECT_TRUE(has_mesh_center_near_axis(meshes, 1.0F, true, 0.05F, 1.5F));
}

} // namespace
