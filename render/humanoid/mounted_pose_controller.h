#pragma once

#include "../horse/rig.h"
#include "rig.h"
#include <QVector3D>

namespace Render::GL {

class HumanoidPoseController;
struct MountedAttachmentFrame;

enum class SpearGrip { OVERHAND, COUCHED, TWO_HANDED };

class MountedPoseController {
public:
  MountedPoseController(HumanoidPose &pose,
                        const HumanoidAnimationContext &anim_ctx);

  void mountOnHorse(const MountedAttachmentFrame &mount);
  void dismount();

  void ridingIdle(const MountedAttachmentFrame &mount);
  void ridingLeaning(const MountedAttachmentFrame &mount, float forward_lean,
                     float side_lean);
  void ridingCharging(const MountedAttachmentFrame &mount, float intensity);
  void ridingReining(const MountedAttachmentFrame &mount, float left_tension,
                     float right_tension);

  void ridingMeleeStrike(const MountedAttachmentFrame &mount,
                         float attack_phase);
  void ridingSpearThrust(const MountedAttachmentFrame &mount,
                         float attack_phase);
  void ridingBowShot(const MountedAttachmentFrame &mount, float draw_phase);
  void ridingShieldDefense(const MountedAttachmentFrame &mount, bool raised);

  void holdReins(const MountedAttachmentFrame &mount, float left_slack,
                 float right_slack, float left_tension = 0.0F,
                 float right_tension = 0.0F);
  void holdSpearMounted(const MountedAttachmentFrame &mount,
                        SpearGrip grip_style);
  void holdBowMounted(const MountedAttachmentFrame &mount);

  enum class MountedSeatPose { Neutral, Forward, Defensive };
  enum class MountedWeaponPose {
    None,
    SwordIdle,
    SwordStrike,
    SpearGuard,
    SpearThrust,
    BowDraw
  };
  enum class MountedShieldPose { None, Stowed, Guard, Raised };

  struct MountedRiderPoseRequest {
    HorseDimensions dims{};
    MountedSeatPose seatPose{MountedSeatPose::Neutral};
    MountedWeaponPose weaponPose{MountedWeaponPose::None};
    MountedShieldPose shieldPose{MountedShieldPose::None};
    float actionPhase{0.0F};
    float forwardBias{0.0F};
    float sideBias{0.0F};
    float torsoCompression{0.0F};
    float torsoTwist{0.0F};
    float shoulderDip{0.0F};
    float clearanceForward{1.0F};
    float clearanceUp{1.0F};
    float reinSlackLeft{0.20F};
    float reinSlackRight{0.20F};
    float reinTensionLeft{0.25F};
    float reinTensionRight{0.25F};
    bool leftHandOnReins{true};
    bool rightHandOnReins{true};
  };

  void applyPose(const MountedAttachmentFrame &mount,
                 const MountedRiderPoseRequest &request);

private:
  HumanoidPose &m_pose;
  const HumanoidAnimationContext &m_anim_ctx;

  void attachFeetToStirrups(const MountedAttachmentFrame &mount);
  void positionPelvisOnSaddle(const MountedAttachmentFrame &mount);
  void translateUpperBody(const QVector3D &delta);
  void calculateRidingKnees(const MountedAttachmentFrame &mount);

  auto solveElbowIK(bool is_left, const QVector3D &shoulder,
                    const QVector3D &hand, const QVector3D &outward_dir,
                    float along_frac, float lateral_offset, float y_bias,
                    float outward_sign) const -> QVector3D;

  auto solveKneeIK(bool is_left, const QVector3D &hip, const QVector3D &foot,
                   float height_scale) const -> QVector3D;

  auto getShoulder(bool is_left) const -> const QVector3D &;
  auto getHand(bool is_left) -> QVector3D &;
  auto getHand(bool is_left) const -> const QVector3D &;
  auto getElbow(bool is_left) -> QVector3D &;
  auto computeRightAxis() const -> QVector3D;
  auto computeOutwardDir(bool is_left) const -> QVector3D;

  void applyLean(const MountedAttachmentFrame &mount, float forward_lean,
                 float side_lean);
  void applyShieldDefense(const MountedAttachmentFrame &mount, bool raised);
  void applyShieldStowed(const MountedAttachmentFrame &mount,
                         const HorseDimensions &dims);
  void applySwordIdlePose(const MountedAttachmentFrame &mount,
                          const HorseDimensions &dims);
  void applySwordStrike(const MountedAttachmentFrame &mount, float attack_phase,
                        bool keep_left_hand);
  void applySpearThrust(const MountedAttachmentFrame &mount,
                        float attack_phase);
  void applySpearGuard(const MountedAttachmentFrame &mount, SpearGrip grip);
  void applyBowDraw(const MountedAttachmentFrame &mount, float draw_phase);
  void applySaddleClearance(const MountedAttachmentFrame &mount,
                            const HorseDimensions &dims, float forward_bias,
                            float up_bias);
  void stabilizeUpperBody(const MountedAttachmentFrame &mount,
                          const HorseDimensions &dims);
  void applyTorsoSculpt(const MountedAttachmentFrame &mount, float compression,
                        float twist, float shoulderDip);
  void holdReinsImpl(const MountedAttachmentFrame &mount, float left_slack,
                     float right_slack, float left_tension, float right_tension,
                     bool apply_left, bool apply_right);
};

} // namespace Render::GL
