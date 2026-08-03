#include <QVector3D>

#include <cmath>
#include <gtest/gtest.h>

#include "render/draw_queue.h"
#include "render/effects_submitter.h"
#include "render/gl/backend/spark_orientation.h"

namespace {

using Render::GL::DrawQueue;
using Render::GL::EffectBatchCmd;
using Render::GL::EffectBatchCmdIndex;
using Render::GL::EffectsSubmitter;
using Render::GL::BackendPipelines::k_spark_across_squash;
using Render::GL::BackendPipelines::k_spark_along_stretch;
using Render::GL::BackendPipelines::spark_model_matrix;

auto only_effect(DrawQueue& queue) -> EffectBatchCmd {
  EXPECT_EQ(queue.size(), 1U);
  queue.sort_for_batching();
  return std::get<EffectBatchCmdIndex>(queue.get_sorted(0));
}

} // namespace

TEST(SparkDirection, ASparkWithNothingBehindItStaysIsotropic) {
  DrawQueue queue;
  EffectsSubmitter submitter;
  submitter.metal_spark(&queue,
                        QVector3D(1.0F, 2.0F, 3.0F),
                        QVector3D(1.0F, 1.0F, 1.0F),
                        0.2F,
                        2.0F,
                        0.05F);

  EXPECT_EQ(only_effect(queue).direction, QVector3D(0.0F, 0.0F, 0.0F));

  QMatrix4x4 const model =
      spark_model_matrix(QVector3D(1.0F, 2.0F, 3.0F), 0.2F, QVector3D());

  EXPECT_NEAR(
      model.map(QVector3D(1.0F, 0.0F, 0.0F)).distanceToPoint(model.map(QVector3D())),
      0.2F,
      1.0e-5F);
  EXPECT_NEAR(
      model.map(QVector3D(0.0F, 1.0F, 0.0F)).distanceToPoint(model.map(QVector3D())),
      0.2F,
      1.0e-5F);
}

TEST(SparkDirection, TheBurstIsCarriedOntoTheCommand) {
  DrawQueue queue;
  EffectsSubmitter submitter;
  QVector3D const travel(0.0F, 0.0F, -1.0F);
  submitter.metal_spark(
      &queue, QVector3D(), QVector3D(1.0F, 0.8F, 0.4F), 0.1F, 2.0F, 0.0F, travel);

  EXPECT_EQ(only_effect(queue).direction, travel);
}

TEST(SparkDirection, TheFanIsLaidAlongTheBlowAndSqueezedAcrossIt) {
  float const radius = 0.5F;
  QVector3D const travel(3.0F, 0.0F, 4.0F);
  QMatrix4x4 const model =
      spark_model_matrix(QVector3D(2.0F, 1.0F, 0.0F), radius, travel);

  QVector3D const origin = model.map(QVector3D());
  EXPECT_NEAR(origin.distanceToPoint(QVector3D(2.0F, 1.0F, 0.0F)), 0.0F, 1.0e-5F);

  QVector3D const along = model.map(QVector3D(1.0F, 0.0F, 0.0F)) - origin;
  EXPECT_NEAR(along.length(), radius * k_spark_along_stretch, 1.0e-5F);
  EXPECT_NEAR(
      QVector3D::dotProduct(along.normalized(), travel.normalized()), 1.0F, 1.0e-5F);

  QVector3D const across = model.map(QVector3D(0.0F, 0.0F, 1.0F)) - origin;
  EXPECT_NEAR(across.length(), radius * k_spark_across_squash, 1.0e-5F);
  EXPECT_LT(across.length(), along.length());
}

TEST(SparkDirection, AStraightUpBlowStillProducesAFiniteBasis) {
  QMatrix4x4 const model =
      spark_model_matrix(QVector3D(), 0.3F, QVector3D(0.0F, 1.0F, 0.0F));

  QVector3D const origin = model.map(QVector3D());
  QVector3D const along = model.map(QVector3D(1.0F, 0.0F, 0.0F)) - origin;
  QVector3D const across = model.map(QVector3D(0.0F, 0.0F, 1.0F)) - origin;

  ASSERT_TRUE(std::isfinite(along.x()) && std::isfinite(along.y()) &&
              std::isfinite(along.z()));
  ASSERT_TRUE(std::isfinite(across.x()) && std::isfinite(across.y()) &&
              std::isfinite(across.z()));
  EXPECT_NEAR(along.normalized().y(), 1.0F, 1.0e-5F);
  EXPECT_NEAR(
      QVector3D::dotProduct(along.normalized(), across.normalized()), 0.0F, 1.0e-5F);
}
