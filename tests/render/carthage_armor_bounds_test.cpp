#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <gtest/gtest.h>
#include <limits>
#include <sstream>

#include "render/equipment/armor/armor_heavy_carthage.h"
#include "render/equipment/armor/armor_light_carthage.h"
#include "render/equipment/equipment_submit.h"
#include "render/gl/humanoid/humanoid_types.h"

using namespace Render::GL;

namespace {

auto reference_frames() -> BodyFrames {
  BodyFrames frames{};
  frames.torso.origin = QVector3D(0.0F, 1.10F, 0.0F);
  frames.torso.right = QVector3D(1.0F, 0.0F, 0.0F);
  frames.torso.up = QVector3D(0.0F, 1.0F, 0.0F);
  frames.torso.forward = QVector3D(0.0F, 0.0F, 1.0F);
  frames.torso.radius = 0.34F;
  frames.torso.depth = 0.25F;
  frames.waist.origin = QVector3D(0.0F, 0.72F, 0.0F);
  frames.waist.right = frames.torso.right;
  frames.waist.up = frames.torso.up;
  frames.waist.forward = frames.torso.forward;
  frames.waist.radius = 0.28F;
  frames.waist.depth = 0.22F;
  frames.head.origin = QVector3D(0.0F, 1.78F, 0.0F);
  frames.head.up = frames.torso.up;
  frames.head.radius = 0.16F;
  frames.foot_l.origin = QVector3D(-0.12F, 0.0F, 0.0F);
  frames.foot_r.origin = QVector3D(0.12F, 0.0F, 0.0F);
  return frames;
}

struct BatchBounds {
  float min_y = std::numeric_limits<float>::max();
  float max_y = std::numeric_limits<float>::lowest();
  std::size_t vertex_count = 0;
};

auto bounds_of(const EquipmentBatch& batch) -> BatchBounds {
  BatchBounds bounds;

  const auto include = [&bounds](const QMatrix4x4& world, const Mesh* mesh) {
    if (mesh == nullptr) {
      return;
    }
    for (const auto& vertex : mesh->get_vertices()) {
      const QVector3D point = world.map(
          QVector3D(vertex.position[0], vertex.position[1], vertex.position[2]));
      bounds.min_y = std::min(bounds.min_y, point.y());
      bounds.max_y = std::max(bounds.max_y, point.y());
      ++bounds.vertex_count;
    }
  };

  for (const auto& submission : batch.meshes) {
    include(submission.model, submission.mesh);
  }
  for (const auto& prim : batch.archetypes) {
    if (prim.archetype == nullptr) {
      continue;
    }
    for (const auto& draw : prim.archetype->lods[0].draws) {
      include(prim.world * draw.local_model, draw.mesh);
    }
  }

  return bounds;
}

auto describe(const char* what, const BatchBounds& bounds) -> std::string {
  std::ostringstream out;
  out << what << ": y=[" << bounds.min_y << ", " << bounds.max_y << "] over "
      << bounds.vertex_count << " vertices";
  return out.str();
}

template <typename Renderer>
auto armor_bounds() -> BatchBounds {
  const DrawContext ctx{};
  const HumanoidPalette palette{};
  const HumanoidAnimationContext anim{};
  EquipmentBatch batch;

  Renderer armor;
  armor.render(ctx, reference_frames(), palette, anim, batch);
  return bounds_of(batch);
}

} // namespace

TEST(CarthageArmorBoundsTest, LightArmorIsABandAtTheWaist) {
  const BodyFrames frames = reference_frames();
  const BatchBounds bounds = armor_bounds<ArmorLightCarthageRenderer>();
  ASSERT_GT(bounds.vertex_count, 0U) << "the light cuirass emitted no geometry";
  SCOPED_TRACE(describe("light", bounds));

  EXPECT_GT(bounds.min_y, frames.waist.origin.y() - frames.waist.radius);
  EXPECT_LT(bounds.max_y, frames.head.origin.y());
}

TEST(CarthageArmorBoundsTest, HeavyArmorHangsBelowTheLightOneButStaysOffTheGround) {
  const BodyFrames frames = reference_frames();
  const BatchBounds heavy = armor_bounds<ArmorHeavyCarthageRenderer>();
  const BatchBounds light = armor_bounds<ArmorLightCarthageRenderer>();
  ASSERT_GT(heavy.vertex_count, 0U) << "the heavy cuirass emitted no geometry";
  SCOPED_TRACE(describe("heavy", heavy) + "  " + describe("light", light));

  EXPECT_LT(heavy.min_y, light.min_y);
  EXPECT_GT(heavy.min_y, frames.foot_l.origin.y());
  EXPECT_LT(heavy.max_y, frames.head.origin.y());
}
