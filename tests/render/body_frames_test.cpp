#include "render/humanoid/humanoid_specs.h"
#include "render/humanoid/rig.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <cmath>
#include <gtest/gtest.h>

using namespace Render::GL;

class BodyFramesTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Initialize a basic pose
    pose.headPos = QVector3D(0.0F, 1.70F, 0.0F);
    pose.headR = 0.10F;
    pose.neck_base = QVector3D(0.0F, 1.49F, 0.0F);
    pose.shoulderL = QVector3D(-0.21F, 1.45F, 0.0F);
    pose.shoulderR = QVector3D(0.21F, 1.45F, 0.0F);
    pose.pelvisPos = QVector3D(0.0F, 0.95F, 0.0F);
    pose.handL = QVector3D(-0.25F, 1.20F, 0.30F);
    pose.hand_r = QVector3D(0.25F, 1.20F, 0.30F);
    pose.elbowL = QVector3D(-0.23F, 1.30F, 0.15F);
    pose.elbowR = QVector3D(0.23F, 1.30F, 0.15F);
    pose.footL = QVector3D(-0.14F, 0.022F, 0.06F);
    pose.foot_r = QVector3D(0.14F, 0.022F, -0.06F);
  }

  HumanoidPose pose;

  bool approxEqual(const QVector3D &a, const QVector3D &b,
                   float epsilon = 0.01F) {
    return std::abs(a.x() - b.x()) < epsilon &&
           std::abs(a.y() - b.y()) < epsilon &&
           std::abs(a.z() - b.z()) < epsilon;
  }

  bool approxEqual(float a, float b, float epsilon = 0.01F) {
    return std::abs(a - b) < epsilon;
  }
};

TEST_F(BodyFramesTest, AttachmentFrameStructHasCorrectFields) {
  AttachmentFrame frame;
  EXPECT_EQ(frame.origin, QVector3D(0.0F, 0.0F, 0.0F));
  EXPECT_EQ(frame.right, QVector3D(1.0F, 0.0F, 0.0F));
  EXPECT_EQ(frame.up, QVector3D(0.0F, 1.0F, 0.0F));
  EXPECT_EQ(frame.forward, QVector3D(0.0F, 0.0F, 1.0F));
  EXPECT_EQ(frame.radius, 0.0F);
}

TEST_F(BodyFramesTest, HeadFrameIsAliasForAttachmentFrame) {
  // Verify that HeadFrame is an alias and can be used interchangeably
  HeadFrame headFrame;
  AttachmentFrame attachFrame;

  headFrame.origin = QVector3D(1.0F, 2.0F, 3.0F);
  headFrame.radius = 0.5F;

  attachFrame = headFrame;
  EXPECT_EQ(attachFrame.origin, QVector3D(1.0F, 2.0F, 3.0F));
  EXPECT_EQ(attachFrame.radius, 0.5F);
}

TEST_F(BodyFramesTest, BodyFramesHasAllRequiredFrames) {
  BodyFrames frames;

  // Verify all frames exist and are initialized to default
  EXPECT_EQ(frames.head.origin, QVector3D(0.0F, 0.0F, 0.0F));
  EXPECT_EQ(frames.torso.origin, QVector3D(0.0F, 0.0F, 0.0F));
  EXPECT_EQ(frames.back.origin, QVector3D(0.0F, 0.0F, 0.0F));
  EXPECT_EQ(frames.waist.origin, QVector3D(0.0F, 0.0F, 0.0F));
  EXPECT_EQ(frames.shoulderL.origin, QVector3D(0.0F, 0.0F, 0.0F));
  EXPECT_EQ(frames.shoulderR.origin, QVector3D(0.0F, 0.0F, 0.0F));
  EXPECT_EQ(frames.handL.origin, QVector3D(0.0F, 0.0F, 0.0F));
  EXPECT_EQ(frames.handR.origin, QVector3D(0.0F, 0.0F, 0.0F));
  EXPECT_EQ(frames.footL.origin, QVector3D(0.0F, 0.0F, 0.0F));
  EXPECT_EQ(frames.footR.origin, QVector3D(0.0F, 0.0F, 0.0F));
}

TEST_F(BodyFramesTest, FrameLocalPositionComputesCorrectly) {
  AttachmentFrame frame;
  frame.origin = QVector3D(1.0F, 2.0F, 3.0F);
  frame.right = QVector3D(1.0F, 0.0F, 0.0F);
  frame.up = QVector3D(0.0F, 1.0F, 0.0F);
  frame.forward = QVector3D(0.0F, 0.0F, 1.0F);
  frame.radius = 0.5F;

  // Test frame-local position computation
  QVector3D local(1.0F, 0.0F, 0.0F); // Right
  QVector3D world = HumanoidRendererBase::frameLocalPosition(frame, local);

  // Expected: origin + right * (1.0 * radius)
  QVector3D expected = QVector3D(1.5F, 2.0F, 3.0F);
  EXPECT_TRUE(approxEqual(world, expected));
}

TEST_F(BodyFramesTest, FrameLocalPositionWithMultipleAxes) {
  AttachmentFrame frame;
  frame.origin = QVector3D(0.0F, 0.0F, 0.0F);
  frame.right = QVector3D(1.0F, 0.0F, 0.0F);
  frame.up = QVector3D(0.0F, 1.0F, 0.0F);
  frame.forward = QVector3D(0.0F, 0.0F, 1.0F);
  frame.radius = 1.0F;

  // Test diagonal position
  QVector3D local(1.0F, 1.0F, 1.0F);
  QVector3D world = HumanoidRendererBase::frameLocalPosition(frame, local);

  // Expected: origin + right*1 + up*1 + forward*1
  QVector3D expected = QVector3D(1.0F, 1.0F, 1.0F);
  EXPECT_TRUE(approxEqual(world, expected));
}

TEST_F(BodyFramesTest, MakeFrameLocalTransformCreatesValidMatrix) {
  AttachmentFrame frame;
  frame.origin = QVector3D(1.0F, 2.0F, 3.0F);
  frame.right = QVector3D(1.0F, 0.0F, 0.0F);
  frame.up = QVector3D(0.0F, 1.0F, 0.0F);
  frame.forward = QVector3D(0.0F, 0.0F, 1.0F);
  frame.radius = 0.5F;

  QMatrix4x4 parent; // Identity matrix
  QVector3D localOffset(0.0F, 0.0F, 0.0F);
  float uniformScale = 1.0F;

  QMatrix4x4 result = HumanoidRendererBase::makeFrameLocalTransform(
      parent, frame, localOffset, uniformScale);

  // Verify the translation component
  QVector3D translation = result.map(QVector3D(0.0F, 0.0F, 0.0F));
  EXPECT_TRUE(approxEqual(translation, frame.origin));
}

TEST_F(BodyFramesTest, LegacyHeadFunctionsStillWork) {
  HeadFrame headFrame;
  headFrame.origin = QVector3D(0.0F, 1.7F, 0.0F);
  headFrame.right = QVector3D(1.0F, 0.0F, 0.0F);
  headFrame.up = QVector3D(0.0F, 1.0F, 0.0F);
  headFrame.forward = QVector3D(0.0F, 0.0F, 1.0F);
  headFrame.radius = 0.1F;

  // Test legacy headLocalPosition function
  QVector3D local(1.0F, 0.0F, 0.0F);
  QVector3D world = HumanoidRendererBase::headLocalPosition(headFrame, local);
  QVector3D expected = QVector3D(0.1F, 1.7F, 0.0F);
  EXPECT_TRUE(approxEqual(world, expected));

  // Test legacy makeHeadLocalTransform function
  QMatrix4x4 parent;
  QVector3D localOffset(0.0F, 0.0F, 0.0F);
  float uniformScale = 1.0F;

  QMatrix4x4 result = HumanoidRendererBase::makeHeadLocalTransform(
      parent, headFrame, localOffset, uniformScale);

  QVector3D translation = result.map(QVector3D(0.0F, 0.0F, 0.0F));
  EXPECT_TRUE(approxEqual(translation, headFrame.origin));
}

TEST_F(BodyFramesTest, PoseHasBothHeadFrameAndBodyFrames) {
  // Verify that the pose has both the legacy headFrame and new bodyFrames
  EXPECT_TRUE(true); // Just verify it compiles

  // Set headFrame
  pose.headFrame.origin = QVector3D(0.0F, 1.7F, 0.0F);
  pose.headFrame.radius = 0.1F;

  // Set bodyFrames.head
  pose.bodyFrames.head.origin = QVector3D(0.0F, 1.7F, 0.0F);
  pose.bodyFrames.head.radius = 0.1F;

  // Verify both can be accessed
  EXPECT_EQ(pose.headFrame.origin, QVector3D(0.0F, 1.7F, 0.0F));
  EXPECT_EQ(pose.bodyFrames.head.origin, QVector3D(0.0F, 1.7F, 0.0F));
}
