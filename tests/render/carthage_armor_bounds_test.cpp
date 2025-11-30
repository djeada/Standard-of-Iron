#include "render/equipment/armor/armor_heavy_carthage.h"
#include "render/equipment/armor/armor_light_carthage.h"
#include "render/humanoid/rig.h"
#include "render/humanoid/style_palette.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <gtest/gtest.h>
#include <limits>
#include <sstream>
#include <vector>

using namespace Render::GL;

namespace {

struct MeshBounds {
  QVector3D min;
  QVector3D max;
  int materialId = 0;
};

class BoundsSubmitter : public ISubmitter {
public:
  std::vector<MeshBounds> meshes;

  void mesh(Mesh *mesh, const QMatrix4x4 &model, const QVector3D & /*color*/,
            Texture * /*tex*/ = nullptr, float /*alpha*/ = 1.0F,
            int materialId = 0) override {
    if (mesh == nullptr) {
      return;
    }

    MeshBounds b;
    b.min = QVector3D(std::numeric_limits<float>::max(),
                      std::numeric_limits<float>::max(),
                      std::numeric_limits<float>::max());
    b.max = QVector3D(std::numeric_limits<float>::lowest(),
                      std::numeric_limits<float>::lowest(),
                      std::numeric_limits<float>::lowest());
    b.materialId = materialId;

    for (const auto &v : mesh->getVertices()) {
      QVector3D p(v.position[0], v.position[1], v.position[2]);
      QVector3D world = model.map(p);
      b.min.setX(std::min(b.min.x(), world.x()));
      b.min.setY(std::min(b.min.y(), world.y()));
      b.min.setZ(std::min(b.min.z(), world.z()));
      b.max.setX(std::max(b.max.x(), world.x()));
      b.max.setY(std::max(b.max.y(), world.y()));
      b.max.setZ(std::max(b.max.z(), world.z()));
    }

    meshes.push_back(b);
  }

  void cylinder(const QVector3D &, const QVector3D &, float, const QVector3D &,
                float) override {}
  void selectionRing(const QMatrix4x4 &, float, float,
                     const QVector3D &) override {}
  void grid(const QMatrix4x4 &, const QVector3D &, float, float,
            float) override {}
  void selectionSmoke(const QMatrix4x4 &, const QVector3D &, float) override {}
};

// Minimal renderer that reproduces the Carthage spearman proportions and
// variation tweaks to build BodyFrames.
class TestCarthageSpearmanBase : public HumanoidRendererBase {
public:
  auto get_proportion_scaling() const -> QVector3D override {
    return {0.94F, 1.04F, 0.92F};
  }

  void adjust_variation(const DrawContext &, uint32_t,
                        VariationParams &variation) const override {
    variation.bulk_scale *= 0.90F;
    variation.stance_width *= 0.92F;
  }
};

class TestCarthageSwordsmanBase : public HumanoidRendererBase {
public:
  auto get_proportion_scaling() const -> QVector3D override {
    return {0.95F, 1.05F, 0.95F};
  }
};

struct PoseResult {
  HumanoidPose pose;
  HumanoidVariant variant;
  DrawContext ctx;
};

template <typename Renderer> class PoseBuilder : public Renderer {
public:
  auto build(uint32_t seed) -> PoseResult {
    VariationParams variation = VariationParams::fromSeed(seed);
    this->adjust_variation(DrawContext{}, seed, variation);

    const QVector3D prop_scale = this->get_proportion_scaling();
    const float combined_height_scale = prop_scale.y() * variation.height_scale;

    PoseResult result;
    result.ctx.model.scale(variation.bulk_scale, combined_height_scale, 1.0F);

    AnimationInputs inputs{};
    inputs.time = 0.0F;
    inputs.is_moving = false;
    inputs.is_attacking = false;
    inputs.is_melee = false;
    inputs.is_in_hold_mode = false;
    inputs.is_exiting_hold = false;
    inputs.hold_exit_progress = 0.0F;

    HumanoidPose pose;
    this->computeLocomotionPose(seed, inputs.time, inputs.is_moving, variation,
                                pose);

    HumanoidVariant variant;
    QVector3D team_tint(0.8F, 0.9F, 1.0F);
    variant.palette = makeHumanoidPalette(team_tint, seed);

    BoundsSubmitter sink;
    this->drawCommonBody(result.ctx, variant, pose, sink);

    result.pose = pose;
    result.variant = variant;
    return result;
  }
};

auto extractMinY(const std::vector<MeshBounds> &meshes) -> float {
  float min_y = std::numeric_limits<float>::max();
  for (const auto &m : meshes) {
    min_y = std::min(min_y, m.min.y());
  }
  return min_y;
}

} // namespace

TEST(CarthageArmorBoundsTest, LightArmorStaysNearWaist) {
  PoseBuilder<TestCarthageSpearmanBase> renderer;
  auto pose_result = renderer.build(/*seed=*/1337U);

  ArmorLightCarthageRenderer armor;
  HumanoidAnimationContext anim_ctx{};
  BoundsSubmitter submitter;
  armor.render(pose_result.ctx, pose_result.pose.body_frames,
               pose_result.variant.palette, anim_ctx, submitter);

  std::ostringstream debug;
  for (size_t i = 0; i < submitter.meshes.size(); ++i) {
    const auto &m = submitter.meshes[i];
    debug << "#" << i << ": [" << m.min.y() << ", " << m.max.y() << "] (mat "
          << m.materialId << ") ";
  }
  debug << "waist_r=" << pose_result.pose.body_frames.waist.radius;
  SCOPED_TRACE(debug.str());

  float const armor_min_y = extractMinY(submitter.meshes);
  float const waist_y =
      pose_result.ctx.model.map(pose_result.pose.body_frames.waist.origin).y();

  // Armor should not extend noticeably below the waist/hip line.
  EXPECT_GT(armor_min_y, waist_y - 0.05F)
      << "min_y=" << armor_min_y << " waist_y=" << waist_y;
}

TEST(CarthageArmorBoundsTest, HeavyArmorStaysNearWaist) {
  PoseBuilder<TestCarthageSwordsmanBase> renderer;
  auto pose_result = renderer.build(/*seed=*/4242U);

  ArmorHeavyCarthageRenderer armor;
  HumanoidAnimationContext anim_ctx{};
  BoundsSubmitter submitter;
  armor.render(pose_result.ctx, pose_result.pose.body_frames,
               pose_result.variant.palette, anim_ctx, submitter);

  std::ostringstream debug;
  for (size_t i = 0; i < submitter.meshes.size(); ++i) {
    const auto &m = submitter.meshes[i];
    debug << "#" << i << ": [" << m.min.y() << ", " << m.max.y() << "] (mat "
          << m.materialId << ") ";
  }
  debug << "waist_r=" << pose_result.pose.body_frames.waist.radius;
  SCOPED_TRACE(debug.str());

  float const armor_min_y = extractMinY(submitter.meshes);
  float const waist_y =
      pose_result.ctx.model.map(pose_result.pose.body_frames.waist.origin).y();

  EXPECT_GT(armor_min_y, waist_y - 0.05F)
      << "min_y=" << armor_min_y << " waist_y=" << waist_y;
}
