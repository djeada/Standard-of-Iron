#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <vector>

#include "render/equipment/armor/arm_guards_renderer.h"
#include "render/equipment/armor/armor_heavy_carthage.h"
#include "render/equipment/armor/armor_light_carthage.h"
#include "render/equipment/armor/carthage_shoulder_cover.h"
#include "render/equipment/armor/cloak_renderer.h"
#include "render/equipment/armor/roman_armor.h"
#include "render/equipment/armor/roman_greaves.h"
#include "render/equipment/armor/roman_shoulder_cover.h"
#include "render/equipment/armor/tool_belt_renderer.h"
#include "render/equipment/armor/work_apron_renderer.h"
#include "render/equipment/equipment_registry.h"
#include "render/equipment/equipment_submit.h"
#include "render/humanoid/humanoid_renderer_base.h"

using namespace Render::GL;

namespace {

struct AABB {
  QVector3D mn{std::numeric_limits<float>::infinity(),
               std::numeric_limits<float>::infinity(),
               std::numeric_limits<float>::infinity()};
  QVector3D mx{-std::numeric_limits<float>::infinity(),
               -std::numeric_limits<float>::infinity(),
               -std::numeric_limits<float>::infinity()};

  void include(const QVector3D& p) {
    mn.setX(std::min(mn.x(), p.x()));
    mn.setY(std::min(mn.y(), p.y()));
    mn.setZ(std::min(mn.z(), p.z()));
    mx.setX(std::max(mx.x(), p.x()));
    mx.setY(std::max(mx.y(), p.y()));
    mx.setZ(std::max(mx.z(), p.z()));
  }
};

class CountingSubmitter : public ISubmitter {
public:
  void mesh(Mesh*,
            const QMatrix4x4&,
            const QVector3D&,
            Texture* = nullptr,
            float = 1.0F,
            int = 0) override {
    ++draw_count;
  }

  void cylinder(const QVector3D&,
                const QVector3D&,
                float,
                const QVector3D&,
                float = 1.0F) override {
    ++draw_count;
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
  void mode_indicator(const QMatrix4x4&, int, const QVector3D&, float = 1.0F) override {
  }

  int draw_count{0};
};

class MeshCaptureSubmitter : public ISubmitter {
public:
  void mesh(Mesh*,
            const QMatrix4x4& model,
            const QVector3D&,
            Texture* = nullptr,
            float = 1.0F,
            int = 0) override {
    models.push_back(model);
  }

  void cylinder(const QVector3D&,
                const QVector3D&,
                float,
                const QVector3D&,
                float = 1.0F) override {}

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
  void mode_indicator(const QMatrix4x4&, int, const QVector3D&, float = 1.0F) override {
  }

  std::vector<QMatrix4x4> models;
};

inline int draw_count_of(const EquipmentBatch& batch) {
  CountingSubmitter submitter;
  submit_equipment_batch(batch, submitter);
  return submitter.draw_count;
}

auto archetype_aabb(const EquipmentBatch& batch) -> AABB {
  AABB box;
  for (const auto& prim : batch.archetypes) {
    if (prim.archetype == nullptr) {
      continue;
    }
    const auto& slice = prim.archetype->lods[0];
    for (const auto& draw : slice.draws) {
      if (draw.mesh == nullptr) {
        continue;
      }
      const QMatrix4x4 world = prim.world * draw.local_model;
      for (const auto& v : draw.mesh->get_vertices()) {
        box.include(world.map(QVector3D(v.position[0], v.position[1], v.position[2])));
      }
    }
  }
  return box;
}

} // namespace

class ArmorRendererTest : public ::testing::Test {
protected:
  void SetUp() override {
    register_built_in_equipment();
    registry = &EquipmentRegistry::instance();
  }

  EquipmentRegistry* registry = nullptr;
};

TEST_F(ArmorRendererTest, RomanHeavyArmorRegistered) {
  EXPECT_TRUE(registry->has(EquipmentCategory::Armor, "roman_heavy_armor"));
  EXPECT_NE(registry->resolve_handle(EquipmentCategory::Armor, "roman_heavy_armor"),
            k_invalid_equipment_handle);
}

TEST_F(ArmorRendererTest, RomanLightArmorRegistered) {
  EXPECT_TRUE(registry->has(EquipmentCategory::Armor, "roman_light_armor"));
  EXPECT_NE(registry->resolve_handle(EquipmentCategory::Armor, "roman_light_armor"),
            k_invalid_equipment_handle);
}

TEST_F(ArmorRendererTest, ToolBeltRegistered) {
  EXPECT_TRUE(registry->has(EquipmentCategory::Armor, "tool_belt_roman"));
  EXPECT_NE(registry->resolve_handle(EquipmentCategory::Armor, "tool_belt_roman"),
            k_invalid_equipment_handle);
}

TEST_F(ArmorRendererTest, ArmGuardsRegistered) {
  EXPECT_TRUE(registry->has(EquipmentCategory::Armor, "arm_guards"));
  EXPECT_NE(registry->resolve_handle(EquipmentCategory::Armor, "arm_guards"),
            k_invalid_equipment_handle);
}

TEST_F(ArmorRendererTest, WorkApronRegistered) {
  EXPECT_TRUE(registry->has(EquipmentCategory::Armor, "work_apron_roman"));
  EXPECT_TRUE(registry->has(EquipmentCategory::Armor, "work_apron_carthage"));
}

TEST_F(ArmorRendererTest, RomanShoulderCoverRegistered) {
  EXPECT_TRUE(registry->has(EquipmentCategory::Armor, "roman_shoulder_cover"));
  EXPECT_NE(registry->resolve_handle(EquipmentCategory::Armor, "roman_shoulder_cover"),
            k_invalid_equipment_handle);
}

TEST_F(ArmorRendererTest, CarthageShoulderCoverRegistered) {
  EXPECT_TRUE(registry->has(EquipmentCategory::Armor, "carthage_shoulder_cover"));
  EXPECT_NE(
      registry->resolve_handle(EquipmentCategory::Armor, "carthage_shoulder_cover"),
      k_invalid_equipment_handle);
}

TEST_F(ArmorRendererTest, RomanGreavesRegistered) {
  EXPECT_TRUE(registry->has(EquipmentCategory::Armor, "roman_greaves"));
  EXPECT_NE(registry->resolve_handle(EquipmentCategory::Armor, "roman_greaves"),
            k_invalid_equipment_handle);
}

TEST_F(ArmorRendererTest, ArmorLightCarthageRegistered) {
  EXPECT_TRUE(registry->has(EquipmentCategory::Armor, "armor_light_carthage"));
  EXPECT_NE(registry->resolve_handle(EquipmentCategory::Armor, "armor_light_carthage"),
            k_invalid_equipment_handle);
}

TEST_F(ArmorRendererTest, ArmorHeavyCarthageRegistered) {
  EXPECT_TRUE(registry->has(EquipmentCategory::Armor, "armor_heavy_carthage"));
  EXPECT_NE(registry->resolve_handle(EquipmentCategory::Armor, "armor_heavy_carthage"),
            k_invalid_equipment_handle);
}

TEST_F(ArmorRendererTest, CarthageArcherArmorSharesHelmet) {
  EXPECT_TRUE(registry->has(EquipmentCategory::Armor, "armor_light_carthage"));
  EXPECT_TRUE(registry->has(EquipmentCategory::Armor, "armor_heavy_carthage"));
  EXPECT_TRUE(registry->has(EquipmentCategory::Helmet, "carthage_light"));
}

TEST_F(ArmorRendererTest, CloakRendererCreation) {
  CloakConfig config;
  config.length_scale = 1.1F;
  config.width_scale = 1.05F;

  auto cloak = std::make_shared<CloakRenderer>(config);
  ASSERT_NE(cloak, nullptr);
}

TEST_F(ArmorRendererTest, CloakBackMeshHasDepthAndTaper) {
  CloakMeshes const meshes = shared_cloak_meshes();
  ASSERT_NE(meshes.back, nullptr);

  float min_y = std::numeric_limits<float>::infinity();
  float max_y = -std::numeric_limits<float>::infinity();
  float top_half_width = 0.0F;
  float bottom_half_width = 0.0F;
  for (const auto& v : meshes.back->get_vertices()) {
    QVector3D const pos(v.position[0], v.position[1], v.position[2]);
    min_y = std::min(min_y, pos.y());
    max_y = std::max(max_y, pos.y());
    if (pos.z() <= -0.45F) {
      top_half_width = std::max(top_half_width, std::abs(pos.x()));
    }
    if (pos.z() >= 0.45F) {
      bottom_half_width = std::max(bottom_half_width, std::abs(pos.x()));
    }
  }

  EXPECT_GT(max_y - min_y, 0.08F);
  EXPECT_LT(bottom_half_width, top_half_width - 0.05F);
}

TEST_F(ArmorRendererTest, CloakMeshesStaySymmetricLeftToRight) {
  auto assert_mirror_symmetry = [](Mesh* mesh, int row_stride, int row_count) {
    ASSERT_NE(mesh, nullptr);
    const auto& vertices = mesh->get_vertices();
    ASSERT_GE(vertices.size(), static_cast<std::size_t>(row_stride * row_count * 2));
    for (int row = 0; row < row_count; ++row) {
      for (int col = 0; col < row_stride; ++col) {
        int const mirror_col = row_stride - 1 - col;
        const auto& lhs = vertices[static_cast<std::size_t>(row * row_stride + col)];
        const auto& rhs =
            vertices[static_cast<std::size_t>(row * row_stride + mirror_col)];
        EXPECT_NEAR(lhs.position[0], -rhs.position[0], 1e-4F);
        EXPECT_NEAR(lhs.position[1], rhs.position[1], 1e-4F);
        EXPECT_NEAR(lhs.position[2], rhs.position[2], 1e-4F);
      }
    }
  };

  CloakMeshes const meshes = shared_cloak_meshes();
  assert_mirror_symmetry(meshes.back, 13, 19);
  assert_mirror_symmetry(meshes.shoulder, 11, 7);
}

TEST_F(ArmorRendererTest, MountedCloaksRegisterLowerShoulderAnchors) {
  CloakConfig cloak{};
  cloak.primary_color = QVector3D(0.70F, 0.15F, 0.18F);
  cloak.trim_color = QVector3D(0.78F, 0.72F, 0.58F);
  cloak.back_material_id = 12;
  cloak.shoulder_material_id = 13;

  CloakConfig mounted = cloak;
  mounted.length_scale = 0.94F;
  mounted.shoulder_anchor_up = 0.06F;
  mounted.drape_anchor_up = 0.00F;
  mounted.drape_anchor_back = 0.62F;
  mounted.clasp_anchor_up = 0.06F;

  EXPECT_LT(mounted.shoulder_anchor_up, cloak.shoulder_anchor_up);
  EXPECT_LT(mounted.drape_anchor_up, cloak.drape_anchor_up);
  EXPECT_LT(mounted.length_scale, cloak.length_scale);
}

TEST_F(ArmorRendererTest, ArmorCategoryIsDistinct) {
  EXPECT_TRUE(registry->has(EquipmentCategory::Helmet, "carthage_heavy"));
  EXPECT_TRUE(registry->has(EquipmentCategory::Armor, "roman_heavy_armor"));
  EXPECT_TRUE(registry->has(EquipmentCategory::Weapon, "bow"));

  EXPECT_FALSE(registry->has(EquipmentCategory::Armor, "carthage_heavy"));
  EXPECT_FALSE(registry->has(EquipmentCategory::Helmet, "roman_heavy_armor"));
}

TEST_F(ArmorRendererTest, TorsoArmorMeshFrontFacesBodyForward) {
  BodyFrames frames{};
  frames.torso.origin = QVector3D(0.0F, 1.1F, 0.0F);
  frames.torso.right = QVector3D(1.0F, 0.0F, 0.0F);
  frames.torso.up = QVector3D(0.0F, 1.0F, 0.0F);
  frames.torso.forward = QVector3D(0.0F, 0.0F, 1.0F);
  frames.torso.radius = 0.34F;
  frames.torso.depth = 0.25F;

  frames.waist.origin = QVector3D(0.0F, 0.72F, 0.0F);
  frames.waist.up = frames.torso.up;
  frames.waist.radius = 0.28F;

  frames.head.origin = QVector3D(0.0F, 1.78F, 0.0F);
  frames.head.up = frames.torso.up;
  frames.head.radius = 0.16F;

  DrawContext const ctx{};
  HumanoidPalette const palette{};
  HumanoidAnimationContext const anim{};
  EquipmentBatch batch;

  RomanLightArmorRenderer armor;
  armor.render(ctx, frames, palette, anim, batch);

  ASSERT_FALSE(batch.archetypes.empty());
  MeshCaptureSubmitter submitter;
  submit_equipment_batch(batch, submitter);
  ASSERT_FALSE(submitter.models.empty());
  QVector3D const mesh_front =
      submitter.models.front().mapVector(QVector3D(1.0F, 0.0F, 0.0F)).normalized();
  EXPECT_GT(QVector3D::dotProduct(mesh_front, frames.torso.forward), 0.95F);
}

TEST_F(ArmorRendererTest, RomanHeavyArmorRendersThroughArchetypePath) {
  BodyFrames frames{};
  frames.torso.origin = QVector3D(0.0F, 1.10F, 0.0F);
  frames.torso.right = QVector3D(1.0F, 0.0F, 0.0F);
  frames.torso.up = QVector3D(0.0F, 1.0F, 0.0F);
  frames.torso.forward = QVector3D(0.0F, 0.0F, 1.0F);
  frames.torso.radius = 0.34F;
  frames.torso.depth = 0.25F;
  frames.waist.origin = QVector3D(0.0F, 0.72F, 0.0F);
  frames.waist.up = frames.torso.up;
  frames.waist.radius = 0.28F;
  frames.head.origin = QVector3D(0.0F, 1.78F, 0.0F);
  frames.head.up = frames.torso.up;
  frames.head.radius = 0.16F;
  frames.shoulder_l.origin = QVector3D(-0.22F, 1.32F, 0.0F);
  frames.shoulder_r.origin = QVector3D(0.22F, 1.32F, 0.0F);

  DrawContext const ctx{};
  HumanoidPalette const palette{};
  HumanoidAnimationContext const anim{};
  EquipmentBatch batch;

  RomanHeavyArmorRenderer armor;
  armor.render(ctx, frames, palette, anim, batch);

  EXPECT_TRUE(batch.meshes.empty());
  EXPECT_EQ(batch.archetypes.size(), 1U);
  EXPECT_EQ(draw_count_of(batch), 7);
}

TEST_F(ArmorRendererTest, RomanLightArmorRendersThroughArchetypePath) {
  BodyFrames frames{};
  frames.torso.origin = QVector3D(0.0F, 1.10F, 0.0F);
  frames.torso.right = QVector3D(1.0F, 0.0F, 0.0F);
  frames.torso.up = QVector3D(0.0F, 1.0F, 0.0F);
  frames.torso.forward = QVector3D(0.0F, 0.0F, 1.0F);
  frames.torso.radius = 0.34F;
  frames.torso.depth = 0.25F;
  frames.waist.origin = QVector3D(0.0F, 0.72F, 0.0F);
  frames.waist.up = frames.torso.up;
  frames.waist.radius = 0.28F;
  frames.head.origin = QVector3D(0.0F, 1.78F, 0.0F);
  frames.head.up = frames.torso.up;
  frames.head.radius = 0.16F;

  DrawContext const ctx{};
  HumanoidPalette const palette{};
  HumanoidAnimationContext const anim{};
  EquipmentBatch batch;

  RomanLightArmorRenderer armor;
  armor.render(ctx, frames, palette, anim, batch);

  EXPECT_TRUE(batch.meshes.empty());
  EXPECT_EQ(batch.archetypes.size(), 1U);
  EXPECT_EQ(draw_count_of(batch), 5);
}

TEST_F(ArmorRendererTest, ArmorLightCarthageRendersThroughArchetypePath) {
  BodyFrames frames{};
  frames.torso.origin = QVector3D(0.0F, 1.10F, 0.0F);
  frames.torso.right = QVector3D(1.0F, 0.0F, 0.0F);
  frames.torso.up = QVector3D(0.0F, 1.0F, 0.0F);
  frames.torso.forward = QVector3D(0.0F, 0.0F, 1.0F);
  frames.torso.radius = 0.34F;
  frames.torso.depth = 0.25F;
  frames.waist.origin = QVector3D(0.0F, 0.72F, 0.0F);
  frames.waist.up = frames.torso.up;
  frames.waist.radius = 0.28F;
  frames.head.origin = QVector3D(0.0F, 1.78F, 0.0F);
  frames.head.up = frames.torso.up;
  frames.head.radius = 0.16F;

  DrawContext const ctx{};
  HumanoidPalette const palette{};
  HumanoidAnimationContext const anim{};
  EquipmentBatch batch;

  ArmorLightCarthageRenderer armor;
  armor.render(ctx, frames, palette, anim, batch);

  EXPECT_TRUE(batch.meshes.empty());
  EXPECT_EQ(batch.archetypes.size(), 1U);
  EXPECT_EQ(draw_count_of(batch), 5);
}

TEST_F(ArmorRendererTest, ArmorHeavyCarthageRendersThroughArchetypePath) {
  BodyFrames frames{};
  frames.torso.origin = QVector3D(0.0F, 1.10F, 0.0F);
  frames.torso.right = QVector3D(1.0F, 0.0F, 0.0F);
  frames.torso.up = QVector3D(0.0F, 1.0F, 0.0F);
  frames.torso.forward = QVector3D(0.0F, 0.0F, 1.0F);
  frames.torso.radius = 0.34F;
  frames.torso.depth = 0.25F;
  frames.waist.origin = QVector3D(0.0F, 0.72F, 0.0F);
  frames.waist.up = frames.torso.up;
  frames.waist.radius = 0.28F;
  frames.head.origin = QVector3D(0.0F, 1.78F, 0.0F);
  frames.head.up = frames.torso.up;
  frames.head.radius = 0.16F;

  DrawContext const ctx{};
  HumanoidPalette const palette{};
  HumanoidAnimationContext const anim{};
  EquipmentBatch batch;

  ArmorHeavyCarthageRenderer armor;
  armor.render(ctx, frames, palette, anim, batch);

  EXPECT_TRUE(batch.meshes.empty());
  EXPECT_EQ(batch.archetypes.size(), 1U);
  EXPECT_EQ(draw_count_of(batch), 3);
}

TEST_F(ArmorRendererTest, CloakRendersThroughArchetypePath) {
  BodyFrames frames{};
  frames.torso.origin = QVector3D(0.0F, 1.10F, 0.0F);
  frames.torso.right = QVector3D(1.0F, 0.0F, 0.0F);
  frames.torso.up = QVector3D(0.0F, 1.0F, 0.0F);
  frames.torso.forward = QVector3D(0.0F, 0.0F, 1.0F);
  frames.torso.radius = 0.34F;
  frames.shoulder_l.origin = QVector3D(-0.22F, 1.32F, 0.0F);
  frames.shoulder_r.origin = QVector3D(0.22F, 1.32F, 0.0F);

  DrawContext const ctx{};
  HumanoidPalette const palette{};
  HumanoidAnimationContext const anim{};
  EquipmentBatch batch;

  CloakRenderer cloak;
  cloak.render(ctx, frames, palette, anim, batch);

  EXPECT_TRUE(batch.meshes.empty());
  EXPECT_EQ(batch.archetypes.size(), 1U);
  EXPECT_EQ(draw_count_of(batch), 3);
}

TEST_F(ArmorRendererTest, CloakTopEdgeStaysNearShoulders) {
  BodyFrames frames{};
  frames.torso.origin = QVector3D(0.0F, 1.10F, 0.0F);
  frames.torso.right = QVector3D(1.0F, 0.0F, 0.0F);
  frames.torso.up = QVector3D(0.0F, 1.0F, 0.0F);
  frames.torso.forward = QVector3D(0.0F, 0.0F, 1.0F);
  frames.torso.radius = 0.34F;
  frames.head.origin = QVector3D(0.0F, 1.78F, 0.0F);
  frames.shoulder_l.origin = QVector3D(-0.22F, 1.32F, 0.0F);
  frames.shoulder_r.origin = QVector3D(0.22F, 1.32F, 0.0F);

  DrawContext const ctx{};
  HumanoidPalette const palette{};
  HumanoidAnimationContext const anim{};
  EquipmentBatch batch;

  CloakRenderer cloak;
  cloak.render(ctx, frames, palette, anim, batch);

  AABB const box = archetype_aabb(batch);
  float const shoulder_mid_y =
      (frames.shoulder_l.origin.y() + frames.shoulder_r.origin.y()) * 0.5F;
  EXPECT_LT(box.mx.y(), shoulder_mid_y + frames.torso.radius * 0.40F);
  EXPECT_LT(box.mn.y(), shoulder_mid_y - frames.torso.radius * 2.5F);
}

TEST_F(ArmorRendererTest, ToolBeltRendersThroughArchetypePath) {
  BodyFrames frames{};
  frames.waist.origin = QVector3D(0.0F, 0.72F, 0.0F);
  frames.waist.right = QVector3D(1.0F, 0.0F, 0.0F);
  frames.waist.up = QVector3D(0.0F, 1.0F, 0.0F);
  frames.waist.forward = QVector3D(0.0F, 0.0F, 1.0F);
  frames.waist.radius = 0.28F;
  frames.waist.depth = 0.22F;

  DrawContext const ctx{};
  HumanoidPalette const palette{};
  HumanoidAnimationContext const anim{};
  EquipmentBatch batch;

  ToolBeltRenderer tool_belt;
  tool_belt.render(ctx, frames, palette, anim, batch);

  EXPECT_GT(batch.archetypes.size(), 0U);
  EXPECT_GE(draw_count_of(batch), 16);
}

TEST_F(ArmorRendererTest, RomanShoulderCoverRendersThroughArchetypePath) {
  BodyFrames frames{};
  frames.torso.right = QVector3D(1.0F, 0.0F, 0.0F);
  frames.torso.up = QVector3D(0.0F, 1.0F, 0.0F);
  frames.shoulder_l.origin = QVector3D(-0.22F, 1.32F, 0.0F);
  frames.shoulder_r.origin = QVector3D(0.22F, 1.32F, 0.0F);

  DrawContext const ctx{};
  HumanoidPalette const palette{};
  HumanoidAnimationContext const anim{};
  EquipmentBatch batch;

  RomanShoulderCoverRenderer shoulder_cover;
  shoulder_cover.render(ctx, frames, palette, anim, batch);

  EXPECT_TRUE(batch.meshes.empty());
  EXPECT_EQ(batch.archetypes.size(), 2U);
  EXPECT_EQ(draw_count_of(batch), 6);
}

TEST_F(ArmorRendererTest, CarthageShoulderCoverRendersThroughArchetypePath) {
  BodyFrames frames{};
  frames.torso.right = QVector3D(1.0F, 0.0F, 0.0F);
  frames.torso.up = QVector3D(0.0F, 1.0F, 0.0F);
  frames.shoulder_l.origin = QVector3D(-0.22F, 1.32F, 0.0F);
  frames.shoulder_r.origin = QVector3D(0.22F, 1.32F, 0.0F);

  DrawContext const ctx{};
  HumanoidPalette const palette{};
  HumanoidAnimationContext const anim{};
  EquipmentBatch batch;

  CarthageShoulderCoverRenderer shoulder_cover;
  shoulder_cover.render(ctx, frames, palette, anim, batch);

  EXPECT_TRUE(batch.meshes.empty());
  EXPECT_EQ(batch.archetypes.size(), 2U);
  EXPECT_EQ(draw_count_of(batch), 6);
}

TEST_F(ArmorRendererTest, RomanGreavesRenderThroughArchetypePath) {
  BodyFrames frames{};
  frames.shin_l.origin = QVector3D(-0.10F, 0.18F, 0.02F);
  frames.shin_l.right = QVector3D(1.0F, 0.0F, 0.0F);
  frames.shin_l.up = QVector3D(0.0F, 1.0F, 0.0F);
  frames.shin_l.forward = QVector3D(0.0F, 0.0F, 1.0F);
  frames.shin_l.radius = 0.045F;
  frames.shin_r.origin = QVector3D(0.10F, 0.18F, 0.02F);
  frames.shin_r.right = QVector3D(1.0F, 0.0F, 0.0F);
  frames.shin_r.up = QVector3D(0.0F, 1.0F, 0.0F);
  frames.shin_r.forward = QVector3D(0.0F, 0.0F, 1.0F);
  frames.shin_r.radius = 0.045F;

  DrawContext const ctx{};
  HumanoidPalette const palette{};
  HumanoidAnimationContext const anim{};
  EquipmentBatch batch;

  RomanGreavesRenderer greaves;
  greaves.render(ctx, frames, palette, anim, batch);

  EXPECT_TRUE(batch.meshes.empty());
  EXPECT_EQ(batch.archetypes.size(), 2U);
  EXPECT_EQ(draw_count_of(batch), 6);
}

TEST_F(ArmorRendererTest, ArmGuardsRenderThroughArchetypePath) {
  BodyFrames frames{};
  frames.shoulder_l.origin = QVector3D(-0.22F, 1.30F, 0.00F);
  frames.hand_l.origin = QVector3D(-0.42F, 0.98F, 0.12F);
  frames.shoulder_r.origin = QVector3D(0.22F, 1.30F, 0.00F);
  frames.hand_r.origin = QVector3D(0.42F, 0.98F, 0.12F);

  DrawContext const ctx{};
  HumanoidPalette const palette{};
  HumanoidAnimationContext const anim{};
  EquipmentBatch batch;

  ArmGuardsRenderer arm_guards;
  arm_guards.render(ctx, frames, palette, anim, batch);

  EXPECT_TRUE(batch.meshes.empty());
  EXPECT_EQ(batch.archetypes.size(), 2U);
  EXPECT_EQ(draw_count_of(batch), 16);
}

TEST_F(ArmorRendererTest, WorkApronRendersThroughArchetypePath) {
  BodyFrames frames{};
  frames.torso.origin = QVector3D(0.0F, 1.10F, 0.0F);
  frames.torso.right = QVector3D(1.0F, 0.0F, 0.0F);
  frames.torso.up = QVector3D(0.0F, 1.0F, 0.0F);
  frames.torso.forward = QVector3D(0.0F, 0.0F, 1.0F);
  frames.torso.radius = 0.34F;
  frames.waist.origin = QVector3D(0.0F, 0.72F, 0.0F);
  frames.waist.right = QVector3D(1.0F, 0.0F, 0.0F);
  frames.waist.up = QVector3D(0.0F, 1.0F, 0.0F);
  frames.waist.forward = QVector3D(0.0F, 0.0F, 1.0F);
  frames.waist.radius = 0.28F;
  frames.waist.depth = 0.22F;

  DrawContext const ctx{};
  HumanoidPalette const palette{};
  HumanoidAnimationContext const anim{};
  EquipmentBatch batch;

  WorkApronRenderer apron;
  apron.render(ctx, frames, palette, anim, batch);

  EXPECT_TRUE(batch.meshes.empty());
  EXPECT_EQ(batch.archetypes.size(), 3U);
  EXPECT_EQ(draw_count_of(batch), 8);
}
