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
    using HP = HumanProportions;
    // Initialize a basic pose
    float const head_center_y = HP::HEAD_CENTER_Y;
    float const half_shoulder = 0.5F * HP::SHOULDER_WIDTH;
    pose.head_pos = QVector3D(0.0F, head_center_y, 0.0F);
    pose.head_r = HP::HEAD_RADIUS;
    pose.neck_base = QVector3D(0.0F, HP::NECK_BASE_Y, 0.0F);
    pose.shoulder_l = QVector3D(-half_shoulder, HP::SHOULDER_Y, 0.0F);
    pose.shoulder_r = QVector3D(half_shoulder, HP::SHOULDER_Y, 0.0F);
    pose.pelvis_pos = QVector3D(0.0F, HP::WAIST_Y, 0.0F);
    pose.hand_l = QVector3D(-0.25F, 1.20F, 0.30F);
    pose.hand_r = QVector3D(0.25F, 1.20F, 0.30F);
    pose.elbow_l = QVector3D(-0.23F, 1.30F, 0.15F);
    pose.elbow_r = QVector3D(0.23F, 1.30F, 0.15F);
    pose.foot_l = QVector3D(-0.14F, 0.022F, 0.06F);
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
  EXPECT_EQ(frames.shoulder_l.origin, QVector3D(0.0F, 0.0F, 0.0F));
  EXPECT_EQ(frames.shoulder_r.origin, QVector3D(0.0F, 0.0F, 0.0F));
  EXPECT_EQ(frames.hand_l.origin, QVector3D(0.0F, 0.0F, 0.0F));
  EXPECT_EQ(frames.hand_r.origin, QVector3D(0.0F, 0.0F, 0.0F));
  EXPECT_EQ(frames.foot_l.origin, QVector3D(0.0F, 0.0F, 0.0F));
  EXPECT_EQ(frames.foot_r.origin, QVector3D(0.0F, 0.0F, 0.0F));
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
  QVector3D world = HumanoidRendererBase::frame_local_position(frame, local);

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
  QVector3D world = HumanoidRendererBase::frame_local_position(frame, local);

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

  QMatrix4x4 result = HumanoidRendererBase::make_frame_local_transform(
      parent, frame, localOffset, uniformScale);

  // Verify the translation component
  QVector3D translation = result.map(QVector3D(0.0F, 0.0F, 0.0F));
  EXPECT_TRUE(approxEqual(translation, frame.origin));
}

TEST_F(BodyFramesTest, LegacyHeadFunctionsStillWork) {
  using HP = HumanProportions;
  HeadFrame headFrame;
  float const head_center_y = HP::HEAD_CENTER_Y;
  headFrame.origin = QVector3D(0.0F, head_center_y, 0.0F);
  headFrame.right = QVector3D(1.0F, 0.0F, 0.0F);
  headFrame.up = QVector3D(0.0F, 1.0F, 0.0F);
  headFrame.forward = QVector3D(0.0F, 0.0F, 1.0F);
  headFrame.radius = HP::HEAD_RADIUS;

  // Test legacy head_local_position function
  QVector3D local(1.0F, 0.0F, 0.0F);
  QVector3D world = HumanoidRendererBase::head_local_position(headFrame, local);
  QVector3D expected = QVector3D(HP::HEAD_RADIUS, head_center_y, 0.0F);
  EXPECT_TRUE(approxEqual(world, expected));

  // Test legacy make_head_local_transform function
  QMatrix4x4 parent;
  QVector3D localOffset(0.0F, 0.0F, 0.0F);
  float uniformScale = 1.0F;

  QMatrix4x4 result = HumanoidRendererBase::make_head_local_transform(
      parent, headFrame, localOffset, uniformScale);

  QVector3D translation = result.map(QVector3D(0.0F, 0.0F, 0.0F));
  EXPECT_TRUE(approxEqual(translation, headFrame.origin));
}

TEST_F(BodyFramesTest, PoseHasBothHeadFrameAndBodyFrames) {
  using HP = HumanProportions;
  // Verify that the pose has both the legacy headFrame and new bodyFrames
  EXPECT_TRUE(true); // Just verify it compiles

  // Set headFrame
  float const head_center_y = HP::HEAD_CENTER_Y;
  pose.head_frame.origin = QVector3D(0.0F, head_center_y, 0.0F);
  pose.head_frame.radius = HP::HEAD_RADIUS;

  // Set bodyFrames.head
  pose.body_frames.head.origin = QVector3D(0.0F, head_center_y, 0.0F);
  pose.body_frames.head.radius = HP::HEAD_RADIUS;

  // Verify both can be accessed
  EXPECT_EQ(pose.head_frame.origin, QVector3D(0.0F, head_center_y, 0.0F));
  EXPECT_EQ(pose.body_frames.head.origin, QVector3D(0.0F, head_center_y, 0.0F));
}
