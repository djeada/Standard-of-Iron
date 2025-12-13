#pragma once

#include "../horse/rig.h"
#include "rig.h"
#include <QVector3D>
#include <string_view>

namespace Render::GL {

class HumanoidPoseController;
struct MountedAttachmentFrame;

enum class SpearGrip { OVERHAND, COUCHED, TWO_HANDED };

class MountedPoseController {
public:
  MountedPoseController(HumanoidPose &pose,
                        const HumanoidAnimationContext &anim_ctx);

  void mount_on_horse(const MountedAttachmentFrame &mount);
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

  void apply_pose(const MountedAttachmentFrame &mount,
                 const MountedRiderPoseRequest &request);

  void finalize_head_sync(const MountedAttachmentFrame &mount,
                        std::string_view debug_label = "final_head_sync");

private:
  HumanoidPose &m_pose;
  const HumanoidAnimationContext &m_anim_ctx;

  void attach_feet_to_stirrups(const MountedAttachmentFrame &mount);
  void positionPelvisOnSaddle(const MountedAttachmentFrame &mount);
  void translate_upper_body(const QVector3D &delta);
  void calculate_riding_knees(const MountedAttachmentFrame &mount);

  auto solve_elbow_ik(bool is_left, const QVector3D &shoulder,
                    const QVector3D &hand, const QVector3D &outward_dir,
                    float along_frac, float lateral_offset, float y_bias,
                    float outward_sign) const -> QVector3D;

  auto solve_knee_ik(bool is_left, const QVector3D &hip, const QVector3D &foot,
                   float height_scale) const -> QVector3D;

  auto get_shoulder(bool is_left) const -> const QVector3D &;
  auto get_hand(bool is_left) -> QVector3D &;
  auto get_hand(bool is_left) const -> const QVector3D &;
  auto get_elbow(bool is_left) -> QVector3D &;
  auto compute_right_axis() const -> QVector3D;
  auto compute_outward_dir(bool is_left) const -> QVector3D;

  void apply_lean(const MountedAttachmentFrame &mount, float forward_lean,
                 float side_lean);
  void apply_shield_defense(const MountedAttachmentFrame &mount, bool raised);
  void apply_shield_stowed(const MountedAttachmentFrame &mount,
                         const HorseDimensions &dims);
  void apply_sword_idle_pose(const MountedAttachmentFrame &mount,
                          const HorseDimensions &dims);
  void apply_sword_strike(const MountedAttachmentFrame &mount, float attack_phase,
                        bool keep_left_hand);
  void apply_spear_thrust(const MountedAttachmentFrame &mount,
                        float attack_phase);
  void apply_spear_guard(const MountedAttachmentFrame &mount, SpearGrip grip);
  void apply_bow_draw(const MountedAttachmentFrame &mount, float draw_phase);
  void apply_saddle_clearance(const MountedAttachmentFrame &mount,
                            const HorseDimensions &dims, float forward_bias,
                            float up_bias);
  void stabilizeUpperBody(const MountedAttachmentFrame &mount,
                          const HorseDimensions &dims);
  void apply_torso_sculpt(const MountedAttachmentFrame &mount, float compression,
                        float twist, float shoulderDip);
  void update_head_hierarchy(const MountedAttachmentFrame &mount,
                           float extra_forward_tilt, float extra_side_tilt,
                           std::string_view debug_label = "head_sync");
  void holdReinsImpl(const MountedAttachmentFrame &mount, float left_slack,
                     float right_slack, float left_tension, float right_tension,
                     bool apply_left, bool apply_right);

  void apply_fixed_head_frame(const MountedAttachmentFrame &mount,
                           std::string_view debug_label);
};

} // namespace Render::GL
