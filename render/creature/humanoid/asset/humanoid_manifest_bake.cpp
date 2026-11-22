#include "render/creature/humanoid/asset/humanoid_manifest_internal.h"

namespace Render::Humanoid {

namespace humanoid_manifest_detail {

void bake_hold_pose(BakeProfile profile,
                    BakerHoldType hold_type,
                    float sample_phase,
                    Render::GL::HumanoidPose& pose) {
  Render::GL::HumanoidAnimationContext anim_ctx{};
  anim_ctx.gait = hold_gait_descriptor();
  anim_ctx.gait.state = Render::GL::HumanoidMotionState::Hold;
  anim_ctx.inputs.is_in_hold_mode = true;
  anim_ctx.inputs.hold_entry_progress = 1.0F;
  anim_ctx.inputs.time = std::clamp(sample_phase, 0.0F, 1.0F) * 1.8F;

  Render::GL::HumanoidPoseController ctrl(pose, anim_ctx);

  float kneel_depth = 0.875F;
  bool kneels = true;
  if (is_defensive_shield_hold(hold_type)) {
    bool const carthaginian = hold_type == BakerHoldType::CarthageShieldWallFront ||
                              hold_type == BakerHoldType::CarthageShieldWallLeft ||
                              hold_type == BakerHoldType::CarthageShieldWallRight;
    kneels = !carthaginian;
    kneel_depth = hold_type == BakerHoldType::TestudoFront ? 0.70F : 0.95F;
  } else if (profile == BakeProfile::SwordReady || profile == BakeProfile::Skeleton) {
    kneel_depth = 0.825F;
  } else if (hold_type == BakerHoldType::Bow) {
    kneel_depth = 1.125F;
  }

  if (hold_type == BakerHoldType::ResourceCarry) {
    ctrl.carry_resource_load();
    return;
  }
  if (hold_type == BakerHoldType::Wade) {
    ctrl.wade_arms_raised(profile == BakeProfile::SwordReady ||
                                  profile == BakeProfile::Skeleton
                              ? Animation::HumanoidHeldPoseKind::WadeSwordShield
                          : profile == BakeProfile::SpearReady
                              ? Animation::HumanoidHeldPoseKind::WadeSpear
                              : Animation::HumanoidHeldPoseKind::WadeHandsHigh);
    return;
  }
  if (kneels) {
    ctrl.kneel(kneel_depth);
  }
  if (is_defensive_shield_hold(hold_type)) {
    ctrl.guard_sword_and_shield_formation(defensive_shield_pose(hold_type), 1.0F);
  } else if (profile == BakeProfile::SwordReady || profile == BakeProfile::Skeleton) {
    ctrl.guard_sword_and_shield_for_defense();
  } else if (hold_type == BakerHoldType::Bow) {
    ctrl.hold_bow_ready();
  } else {
    ctrl.brace_spear_for_hold();
  }
}

void bake_death_pose(Animation::HumanoidDeathCollapse collapse,
                     float blend,
                     Render::GL::HumanoidPose& pose) {
  Render::GL::HumanoidAnimationContext anim_ctx{};
  anim_ctx.gait.state = Render::GL::HumanoidMotionState::Idle;
  Render::GL::HumanoidPoseController ctrl(pose, anim_ctx);
  ctrl.apply_death_collapse(collapse, std::clamp(blend, 0.0F, 1.0F));
}

[[nodiscard]] auto
to_ambient_idle_type(BakerAmbientIdleType t) noexcept -> Render::GL::AmbientIdleType {
  switch (t) {
  case BakerAmbientIdleType::SitDown:
    return Render::GL::AmbientIdleType::SitDown;
  case BakerAmbientIdleType::Jump:
    return Render::GL::AmbientIdleType::Jump;
  case BakerAmbientIdleType::RaiseWeapon:
    return Render::GL::AmbientIdleType::RaiseWeapon;
  case BakerAmbientIdleType::ShiftWeight:
    return Render::GL::AmbientIdleType::ShiftWeight;
  case BakerAmbientIdleType::PlantFlag:
    return Render::GL::AmbientIdleType::PlantFlag;
  case BakerAmbientIdleType::None:
    break;
  }
  return Render::GL::AmbientIdleType::None;
}

[[nodiscard]] auto ready_weapon_for_profile(BakeProfile profile) noexcept
    -> Animation::HumanoidReadyWeapon {
  switch (profile) {
  case BakeProfile::SwordReady:
  case BakeProfile::Skeleton:
    return Animation::HumanoidReadyWeapon::SwordAndShield;
  case BakeProfile::SpearReady:
    return Animation::HumanoidReadyWeapon::Spear;
  case BakeProfile::Default:
  case BakeProfile::Caster:
  case BakeProfile::StaveCaster:
    break;
  }
  return Animation::HumanoidReadyWeapon::None;
}

auto apply_ground_stance_for_profile(Render::GL::HumanoidPoseController& ctrl,
                                     BakeProfile profile,
                                     bool moving) -> bool {
  switch (profile) {
  case BakeProfile::SwordReady:
  case BakeProfile::Skeleton:
    ctrl.carry_sword_and_shield();
    return true;
  case BakeProfile::SpearReady:
    ctrl.hold_spear_idle();
    return true;
  case BakeProfile::Caster:

    if (!moving) {
      ctrl.channel_spell_idle();
    }
    break;
  case BakeProfile::StaveCaster:
    ctrl.carry_stave();
    break;
  case BakeProfile::Default:
    break;
  }
  return false;
}

void restore_leg_lengths(Render::GL::HumanoidPose& pose, bool knees_over_toes) {
  using HP = Render::GL::HumanProportions;
  constexpr float k_length_tolerance = 0.03F;
  auto restore = [&](QVector3D& knee, const QVector3D& foot, float lateral_sign) {
    QVector3D const hip = Render::Humanoid::PosePrimitives::hip_position(
        pose.pelvis_pos,
        lateral_sign < 0.0F ? Render::GL::Side::Left : Render::GL::Side::Right);
    auto lengths_hold = [&]() {
      float const thigh = (knee - hip).length() / HP::UPPER_LEG_LEN;
      float const shin = (foot - knee).length() / HP::LOWER_LEG_LEN;
      return std::abs(thigh - 1.0F) <= k_length_tolerance &&
             std::abs(shin - 1.0F) <= k_length_tolerance;
    };
    auto hinges_forward = [&]() {
      QVector3D const line = foot - hip;
      float const t = std::clamp(QVector3D::dotProduct(knee - hip, line) /
                                     std::max(1.0e-6F, line.lengthSquared()),
                                 0.0F,
                                 1.0F);
      QVector3D const off = knee - (hip + line * t);
      constexpr float k_straight_leg_slack = 0.08F;
      return off.z() >= -0.02F &&
             off.x() * lateral_sign <= std::max(k_straight_leg_slack, off.z() * 0.25F);
    };
    if (lengths_hold() && (!knees_over_toes || hinges_forward())) {
      return;
    }

    constexpr float k_min_forward_bend = 0.04F;
    constexpr float k_max_outward_per_forward = 0.25F;
    QVector3D bend = knee - (hip + foot) * 0.5F;
    float const forward = bend.z();
    float const outward = bend.x() * lateral_sign;
    if (!knees_over_toes) {
      if (bend.lengthSquared() < 1.0e-6F) {
        bend = QVector3D(lateral_sign * 0.24F, 0.0F, 0.95F);
      }
    } else if (forward < k_min_forward_bend) {
      bend = QVector3D(lateral_sign * 0.24F, 0.0F, 0.95F);
    } else {
      float const limit = forward * k_max_outward_per_forward;
      bend.setX(lateral_sign * std::clamp(outward, -limit, limit));

      bend.setY(0.0F);
    }
    knee = Render::Humanoid::PosePrimitives::solve_knee_ik(
        hip,
        foot,
        {.upper_leg_len = HP::UPPER_LEG_LEN,
         .lower_leg_len = HP::LOWER_LEG_LEN,
         .knee_floor = HP::GROUND_Y + pose.foot_y_offset * 0.5F,
         .bend_preference = bend});
  };
  restore(pose.knee_l, pose.foot_l, -1.0F);
  restore(pose.knee_r, pose.foot_r, 1.0F);
}

auto shield_raised_to_front(BakeProfile profile, const HumanoidClipSpec& clip) -> bool {
  if (clip.combat_pose_type == BakerCombatPoseType::ReadyStance ||
      clip.combat_pose_type == BakerCombatPoseType::ReactBlock) {
    return true;
  }
  if (is_defensive_shield_hold(clip.hold_type) || is_rpg_sword_clip(clip) ||
      clip.attack_type == BakerAttackType::Sword ||
      clip.riding_type == BakerRidingType::SwordStrike) {
    return true;
  }
  return (profile == BakeProfile::SwordReady || profile == BakeProfile::Skeleton) &&
         clip.hold_type == BakerHoldType::Spear;
}

auto hold_shield_upright(BakeProfile profile, Render::GL::HumanoidPose& pose) -> bool {
  if (profile != BakeProfile::SwordReady && profile != BakeProfile::Skeleton) {
    return false;
  }
  if (pose.grip_axis_l.lengthSquared() > 1.0e-6F) {
    return false;
  }
  QVector3D torso_up = pose.neck_base - pose.pelvis_pos;
  torso_up = torso_up.lengthSquared() > 1.0e-6F ? torso_up.normalized()
                                                : QVector3D(0.0F, 1.0F, 0.0F);
  QVector3D right = pose.shoulder_r - pose.shoulder_l;
  right = right - torso_up * QVector3D::dotProduct(right, torso_up);
  right = right.lengthSquared() > 1.0e-6F ? right.normalized()
                                          : QVector3D(1.0F, 0.0F, 0.0F);
  QVector3D const forward = QVector3D::crossProduct(right, torso_up).normalized();

  pose.grip_axis_l = (right * -0.10F + torso_up * 0.98F + forward * 0.17F).normalized();
  return true;
}

void bake_humanoid_clip_frame(BakeProfile profile,
                              const HumanoidClipSpec& clip,
                              std::uint32_t frame_index,
                              std::vector<QMatrix4x4>& out_palettes,
                              std::vector<QMatrix4x4>& out_sockets) {
  Render::GL::VariationParams variation{};
  variation.height_scale = 1.0F;
  variation.bulk_scale = 1.0F;
  variation.stance_width = 1.0F;
  variation.arm_swing_amp = 1.0F;
  variation.walk_speed_mult = 1.0F;
  variation.posture_slump = 0.0F;
  variation.shoulder_tilt = 0.0F;

  float const phase =
      (!clip.loops && clip.frames > 1U)
          ? static_cast<float>(frame_index) / static_cast<float>(clip.frames - 1U)
          : static_cast<float>(frame_index) / static_cast<float>(clip.frames);

  Render::GL::HumanoidPose pose{};
  bool grip_oriented_by_stance = false;

  if (clip.death_collapse != Animation::HumanoidDeathCollapse::None) {

    Render::GL::HumanoidGaitDescriptor gait{};
    gait.state = Render::GL::HumanoidMotionState::Idle;
    gait.cycle_time = 1.6F;
    gait.cycle_phase = 0.0F;
    gait.speed = 0.0F;
    gait.normalized_speed = 0.0F;
    Render::GL::HumanoidRendererBase::compute_locomotion_pose(
        0U, 0.0F, gait, variation, pose);

    float const death_blend = clip.loops ? 1.0F : phase;
    bake_death_pose(clip.death_collapse, death_blend, pose);
  } else if (clip.attack_type != BakerAttackType::None) {

    Render::GL::HumanoidGaitDescriptor hold_gait{};
    hold_gait.state = Render::GL::HumanoidMotionState::Hold;
    hold_gait.cycle_time = 1.8F;
    hold_gait.cycle_phase = 0.0F;
    hold_gait.speed = 0.0F;
    hold_gait.normalized_speed = 0.0F;
    Render::GL::HumanoidRendererBase::compute_locomotion_pose(
        0U, 0.0F, hold_gait, variation, pose);

    Render::GL::HumanoidAnimationContext anim_ctx{};
    anim_ctx.gait = hold_gait;
    anim_ctx.gait.state = Render::GL::HumanoidMotionState::Attacking;
    anim_ctx.attack_phase = phase;
    anim_ctx.jitter_seed = 0U;
    anim_ctx.inputs.is_attacking = true;
    anim_ctx.inputs.is_melee = !is_ranged_attack_type(clip.attack_type);
    anim_ctx.inputs.attack_variant = clip.attack_variant;

    Render::GL::HumanoidPoseController ctrl(pose, anim_ctx);
    float const sword_reach_scale = profile == BakeProfile::Skeleton ? 0.88F : 1.0F;
    switch (clip.attack_type) {
    case BakerAttackType::Unarmed:
      ctrl.unarmed_strike(phase, clip.attack_variant);
      break;
    case BakerAttackType::Sword:
      if (is_rpg_sword_clip(clip)) {
        apply_authored_rpg_sword_pose(ctrl, clip.attack_variant, phase, pose);
      } else if (profile == BakeProfile::SwordReady ||
                 profile == BakeProfile::Skeleton) {
        ctrl.combat_sword_slash_variant(phase, clip.attack_variant, sword_reach_scale);
      } else {
        ctrl.sword_slash_variant(phase, clip.attack_variant, sword_reach_scale);
      }
      break;
    case BakerAttackType::Spear:
      if (is_rpg_spear_clip(clip)) {
        apply_authored_rpg_spear_pose(ctrl, clip.attack_variant, phase, pose);
      } else {
        ctrl.spear_thrust_variant(phase, clip.attack_variant);
      }
      break;
    case BakerAttackType::Sling:
    case BakerAttackType::Javelin: {

      bool const sling = clip.attack_type == BakerAttackType::Sling;
      float const windup = std::clamp(phase / 0.36F, 0.0F, 1.0F);
      float const release = std::clamp((phase - 0.36F) / 0.10F, 0.0F, 1.0F);
      float const recover = std::clamp((phase - 0.60F) / 0.40F, 0.0F, 1.0F);
      float const angle = windup * 4.0F * std::numbers::pi_v<float>;
      QVector3D const rest(0.10F, -0.30F, 0.18F);
      QVector3D const loaded = sling ? QVector3D(0.20F + 0.12F * std::cos(angle),
                                                 0.20F,
                                                 0.12F * std::sin(angle))
                                     : QVector3D(0.13F, 0.15F, -0.30F);
      QVector3D const extended(0.02F, 0.02F, 0.48F);
      QVector3D const cocked = rest * (1.0F - windup) + loaded * windup;
      QVector3D const thrown = cocked * (1.0F - release) + extended * release;
      pose.hand_r = pose.shoulder_r + thrown * (1.0F - recover) + rest * recover;
      pose.elbow_r = pose.shoulder_r +
                     QVector3D(0.23F, 0.02F, -0.06F) * (windup * (1.0F - recover)) +
                     QVector3D(0.08F, -0.18F, 0.10F);
      pose.hand_l = pose.shoulder_l + QVector3D(-0.04F, -0.22F, 0.27F);
      pose.elbow_l = pose.shoulder_l + QVector3D(-0.12F, -0.17F, 0.10F);
      pose.grip_axis_r =
          sling ? QVector3D(std::cos(angle), 0.12F, std::sin(angle)).normalized()
                : QVector3D(0.0F, 0.20F - release * 0.15F, 1.0F).normalized();
      pose.grip_axis_l = QVector3D(0.0F, 1.0F, 0.0F);
      ctrl.tilt_torso(-0.10F * windup * (1.0F - recover),
                      (0.16F * windup - 0.30F * release) * (1.0F - recover));
      break;
    }
    case BakerAttackType::Bow:
      ctrl.aim_bow(phase);
      break;
    case BakerAttackType::BowMelee:
      ctrl.bow_melee_strike(phase, clip.attack_variant);
      break;
    case BakerAttackType::SpearFromHold:
      ctrl.kneel(0.875F);
      ctrl.spear_thrust_from_hold(phase, 0.875F);
      break;
    case BakerAttackType::BowFromHold:
      ctrl.kneel(1.125F);
      ctrl.aim_bow(phase);
      break;
    default:
      break;
    }
  } else if (clip.combat_pose_type != BakerCombatPoseType::None) {
    Render::GL::HumanoidGaitDescriptor hold_gait{};
    hold_gait.state = Render::GL::HumanoidMotionState::Hold;
    hold_gait.cycle_time = 1.8F;
    hold_gait.cycle_phase = 0.0F;
    hold_gait.speed = 0.0F;
    hold_gait.normalized_speed = 0.0F;
    Render::GL::HumanoidRendererBase::compute_locomotion_pose(
        0U, 0.0F, hold_gait, variation, pose);

    Render::GL::HumanoidAnimationContext anim_ctx{};
    anim_ctx.gait = hold_gait;
    anim_ctx.gait.state = Render::GL::HumanoidMotionState::Attacking;
    anim_ctx.attack_phase = 0.0F;
    anim_ctx.jitter_seed = 0U;
    anim_ctx.inputs.is_attacking = true;
    anim_ctx.inputs.is_melee = true;
    anim_ctx.inputs.attack_variant = 0U;

    Render::GL::HumanoidPoseController ctrl(pose, anim_ctx);
    float const sword_reach_scale = profile == BakeProfile::Skeleton ? 0.88F : 1.0F;
    auto const weapon = ready_weapon_for_profile(profile);
    if (weapon == Animation::HumanoidReadyWeapon::None) {
      static_cast<void>(apply_ground_stance_for_profile(ctrl, profile));
    }
    float const pose_phase =
        clip.loops ? phase : transition_phase(frame_index, clip.frames);
    switch (clip.combat_pose_type) {
    case BakerCombatPoseType::ReadyStance:
      ctrl.combat_ready_stance(pose_phase, weapon, sword_reach_scale);
      break;
    case BakerCombatPoseType::ReactFlinch:
      ctrl.melee_reaction(Animation::HumanoidReactionKind::Flinch,
                          pose_phase,
                          weapon,
                          sword_reach_scale);
      break;
    case BakerCombatPoseType::ReactBlock:
      ctrl.melee_reaction(Animation::HumanoidReactionKind::Block,
                          pose_phase,
                          weapon,
                          sword_reach_scale);
      break;
    case BakerCombatPoseType::ReactEvade:
      ctrl.melee_reaction(Animation::HumanoidReactionKind::Evade,
                          pose_phase,
                          weapon,
                          sword_reach_scale);
      break;
    case BakerCombatPoseType::ReactStagger:
      ctrl.melee_reaction(Animation::HumanoidReactionKind::Stagger,
                          pose_phase,
                          weapon,
                          sword_reach_scale);
      break;
    case BakerCombatPoseType::None:
      break;
    }
  } else if (clip.riding_type != BakerRidingType::None) {
    Render::GL::HumanoidGaitDescriptor hold_gait{};
    hold_gait.state = Render::GL::HumanoidMotionState::Hold;
    hold_gait.cycle_time = 1.8F;
    hold_gait.cycle_phase = 0.0F;
    hold_gait.speed = 0.0F;
    hold_gait.normalized_speed = 0.0F;
    Render::GL::HumanoidRendererBase::compute_locomotion_pose(
        0U, 0.0F, hold_gait, variation, pose);

    Render::GL::HumanoidAnimationContext anim_ctx_r{};
    anim_ctx_r.variation = variation;
    anim_ctx_r.gait = hold_gait;
    anim_ctx_r.gait.state = Render::GL::HumanoidMotionState::Hold;
    anim_ctx_r.inputs.movement_state =
        (clip.riding_type != BakerRidingType::Idle)
            ? Render::Creature::MovementAnimationState::Walk
            : Render::Creature::MovementAnimationState::Idle;

    auto horse_profile = Render::GL::make_horse_profile(0U, {}, {});
    auto mount = Render::GL::compute_mount_frame(horse_profile);
    Render::GL::tune_mounted_swordsman_frame(horse_profile.dims, mount);

    Render::GL::MountedPoseController ctrl(pose, anim_ctx_r);
    if ((profile == BakeProfile::SwordReady || profile == BakeProfile::Skeleton) &&
        clip.riding_type != BakerRidingType::BowShot &&
        clip.riding_type != BakerRidingType::SwordStrike &&
        clip.riding_type != BakerRidingType::SpearThrust) {
      Render::GL::MountedPoseController::MountedRiderPoseRequest request{};
      request.dims = horse_profile.dims;
      request.weapon_pose =
          Render::GL::MountedPoseController::MountedWeaponPose::SwordIdle;
      request.shield_pose = Render::GL::MountedPoseController::MountedShieldPose::Guard;
      request.left_hand_on_reins = false;
      request.right_hand_on_reins = false;

      switch (clip.riding_type) {
      case BakerRidingType::Idle:
        request.seat_pose = Render::GL::MountedPoseController::MountedSeatPose::Neutral;
        break;
      case BakerRidingType::Charge:
        request.seat_pose = Render::GL::MountedPoseController::MountedSeatPose::Forward;
        request.forward_bias = 0.30F;
        request.clearance_forward = 1.15F;
        break;
      case BakerRidingType::Reining:
        request.seat_pose =
            Render::GL::MountedPoseController::MountedSeatPose::Defensive;
        request.forward_bias = -0.15F;
        request.rein_tension_left = 0.55F;
        request.rein_tension_right = 0.55F;
        break;
      case BakerRidingType::BowShot:
      default:
        break;
      }

      ctrl.apply_pose(mount, request);
      ctrl.finalize_head_sync(mount, "sword_ready_riding");
    } else {
      ctrl.mount_on_horse(mount);

      switch (clip.riding_type) {
      case BakerRidingType::Idle:
        ctrl.riding_idle(mount);
        break;
      case BakerRidingType::Charge:
        ctrl.riding_charging(mount, 1.0F);
        break;
      case BakerRidingType::Reining:
        ctrl.riding_reining(mount, 0.7F, 0.7F);
        break;
      case BakerRidingType::BowShot:
        ctrl.riding_bow_shot(mount, phase);
        break;
      case BakerRidingType::SwordStrike:
        ctrl.riding_melee_strike(mount, phase);
        break;
      case BakerRidingType::SpearThrust:
        ctrl.riding_spear_thrust(mount, phase);
        break;
      default:
        break;
      }

      if (profile == BakeProfile::SpearReady) {
        switch (clip.riding_type) {
        case BakerRidingType::Idle:
        case BakerRidingType::Reining:
          ctrl.grip_spear(mount, Render::GL::SpearGrip::UPRIGHT);
          grip_oriented_by_stance = true;
          break;
        case BakerRidingType::Charge:
          ctrl.grip_spear(mount, Render::GL::SpearGrip::COUCHED);
          grip_oriented_by_stance = true;
          break;
        default:
          break;
        }
      }
    }
    if (clip.riding_type == BakerRidingType::Idle) {

      Render::GL::HumanoidPoseController breath_ctrl(pose, anim_ctx_r);
      breath_ctrl.apply_idle_breath(phase, true);
    }
  } else {
    Render::GL::HumanoidGaitDescriptor gait{};
    gait.state = clip.state;
    gait.cycle_time = clip.cycle_time;

    gait.travel_lateral = clip.travel_lateral;
    gait.travel_alignment =
        std::sqrt(std::max(0.0F, 1.0F - (clip.travel_lateral * clip.travel_lateral)));

    switch (clip.state) {
    case Render::GL::HumanoidMotionState::Idle:
      gait.speed = 0.0F;
      gait.normalized_speed = 0.0F;
      break;
    case Render::GL::HumanoidMotionState::Walk:
      gait.speed = 1.5F;
      gait.normalized_speed = 1.0F;
      break;
    case Render::GL::HumanoidMotionState::Run:
      gait.speed = 4.0F;
      gait.normalized_speed = 1.0F;
      break;
    case Render::GL::HumanoidMotionState::Hold:
    default:
      gait.speed = 0.0F;
      gait.normalized_speed = 0.0F;
      break;
    }

    if (clip.work_type == BakerWorkType::CrewPush) {
      gait.speed = k_crew_push_bake_speed;
      gait.cycle_phase = phase;
      Render::GL::HumanoidRendererBase::compute_locomotion_pose(
          0U, phase * clip.cycle_time, gait, variation, pose);
      Render::GL::HumanoidAnimationContext anim_ctx{};
      anim_ctx.gait = gait;
      anim_ctx.inputs.movement_state = Render::Creature::MovementAnimationState::Walk;
      anim_ctx.inputs.is_constructing = true;
      Render::GL::HumanoidPoseController ctrl(pose, anim_ctx);
      ctrl.construction_pose(Animation::HumanoidConstructionPoseKind::CrewPush, phase);
    } else if (clip.work_type != BakerWorkType::None) {
      gait.cycle_phase = 0.0F;
      Render::GL::HumanoidRendererBase::compute_locomotion_pose(
          0U, 0.0F, gait, variation, pose);
      Render::GL::HumanoidAnimationContext anim_ctx{};
      anim_ctx.gait = gait;
      anim_ctx.gait.state = Render::GL::HumanoidMotionState::Idle;
      anim_ctx.inputs.is_constructing = true;
      Render::GL::HumanoidPoseController ctrl(pose, anim_ctx);
      switch (clip.work_type) {
      case BakerWorkType::Hammer:
        ctrl.construction_hammer(phase);
        break;
      case BakerWorkType::Saw:
        ctrl.construction_saw(phase);
        break;
      case BakerWorkType::Chisel:
        ctrl.construction_chisel(phase, false);
        break;
      case BakerWorkType::KneelingChisel:
        ctrl.kneel(0.875F);
        ctrl.construction_chisel(phase, true);
        break;
      case BakerWorkType::Reap:
        ctrl.construction_reap(phase);
        break;
      case BakerWorkType::CrewCrank:
        ctrl.construction_pose(Animation::HumanoidConstructionPoseKind::CrewCrank,
                               phase);
        break;
      case BakerWorkType::CrewHeave:
        ctrl.construction_pose(Animation::HumanoidConstructionPoseKind::CrewHeave,
                               phase);
        break;
      case BakerWorkType::Climb:
        ctrl.climb_ladder(phase);
        break;
      case BakerWorkType::CrewPush:
      case BakerWorkType::None:
        break;
      }
    } else if (clip.showcase_type != BakerShowcaseType::None) {
      gait.cycle_phase = 0.0F;
      Render::GL::HumanoidRendererBase::compute_locomotion_pose(
          0U, 0.0F, gait, variation, pose);
      Render::GL::HumanoidAnimationContext anim_ctx{};
      anim_ctx.gait = gait;
      anim_ctx.gait.state = Render::GL::HumanoidMotionState::Idle;
      Render::GL::HumanoidPoseController ctrl(pose, anim_ctx);
      ctrl.apply_showcase_move(to_showcase_move(clip.showcase_type),
                               transition_phase(frame_index, clip.frames));
    } else if (clip.hold_type != BakerHoldType::None) {
      gait.cycle_phase = 0.0F;
      Render::GL::HumanoidRendererBase::compute_locomotion_pose(
          0U, 0.0F, gait, variation, pose);
      bake_hold_pose(profile, clip.hold_type, phase, pose);
    } else if (clip.ambient_idle_type != BakerAmbientIdleType::None) {
      gait.cycle_phase = 0.0F;
      Render::GL::HumanoidRendererBase::compute_locomotion_pose(
          0U, 0.0F, gait, variation, pose);
      float const ambient_phase = transition_phase(frame_index, clip.frames);
      Render::GL::HumanoidAnimationContext anim_ctx{};
      anim_ctx.gait = gait;
      anim_ctx.gait.state = Render::GL::HumanoidMotionState::Idle;
      Render::GL::HumanoidPoseController ctrl(pose, anim_ctx);

      grip_oriented_by_stance = apply_ground_stance_for_profile(ctrl, profile);

      ctrl.apply_idle_breath(ambient_phase * clip.cycle_time /
                                 Animation::k_humanoid_idle_breath_cycle_time,
                             false);
      ctrl.apply_ambient_idle_explicit(to_ambient_idle_type(clip.ambient_idle_type),
                                       ambient_phase);
    } else {
      gait.cycle_phase = phase;
      Render::GL::HumanoidRendererBase::compute_locomotion_pose(
          0U, phase * clip.cycle_time, gait, variation, pose);
      Render::GL::HumanoidAnimationContext anim_ctx{};
      anim_ctx.gait = gait;
      anim_ctx.gait.state = clip.state;
      if (clip.state == Render::GL::HumanoidMotionState::Run) {
        anim_ctx.inputs.movement_state = Render::Creature::MovementAnimationState::Run;
      } else if (gait.speed > 0.1F) {
        anim_ctx.inputs.movement_state = Render::Creature::MovementAnimationState::Walk;
      } else {
        anim_ctx.inputs.movement_state = Render::Creature::MovementAnimationState::Idle;
      }
      Render::GL::HumanoidPoseController ctrl(pose, anim_ctx);
      grip_oriented_by_stance = apply_ground_stance_for_profile(
          ctrl, profile, clip.state != Render::GL::HumanoidMotionState::Idle);
      if (std::string_view{clip.name} == "idle_bow_rest") {
        ctrl.rest_bow_idle(phase);
        grip_oriented_by_stance = true;
      }
      if (clip.state == Render::GL::HumanoidMotionState::Idle) {
        ctrl.apply_idle_breath(phase, false);
      }
    }
  }

  if (clip.death_collapse == Animation::HumanoidDeathCollapse::None &&
      clip.riding_type == BakerRidingType::None &&
      clip.showcase_type == BakerShowcaseType::None) {

    restore_leg_lengths(pose, is_rpg_sword_clip(clip) || is_rpg_spear_clip(clip));
  }

  bool const shield_axis_applied = hold_shield_upright(profile, pose);
  pose.shield_faces_outward =
      (profile == BakeProfile::SwordReady || profile == BakeProfile::Skeleton) &&
      !shield_raised_to_front(profile, clip);

  if (grip_oriented_by_stance || shield_axis_applied ||
      clip.showcase_type != BakerShowcaseType::None || is_rpg_sword_clip(clip) ||
      is_rpg_spear_clip(clip) || clip.work_type != BakerWorkType::None ||
      clip.combat_pose_type != BakerCombatPoseType::None ||
      clip.attack_type == BakerAttackType::Unarmed ||
      clip.attack_type == BakerAttackType::Sword ||
      clip.attack_type == BakerAttackType::BowMelee ||
      clip.attack_type == BakerAttackType::Spear ||
      clip.attack_type == BakerAttackType::SpearFromHold ||
      clip.attack_type == BakerAttackType::Bow ||
      clip.attack_type == BakerAttackType::BowFromHold ||
      clip.attack_type == BakerAttackType::Sling ||
      clip.attack_type == BakerAttackType::Javelin ||
      clip.hold_type == BakerHoldType::Spear || clip.hold_type == BakerHoldType::Bow ||
      clip.hold_type == BakerHoldType::Wade ||
      clip.riding_type == BakerRidingType::SwordStrike ||
      clip.riding_type == BakerRidingType::SpearThrust) {
    Render::Humanoid::rebuild_humanoid_frames(pose, QVector3D(1.0F, 1.0F, 1.0F), 1.0F);
  }

  Render::Humanoid::BonePalette palette{};
  Render::Humanoid::evaluate_skeleton(pose, QVector3D(1.0F, 0.0F, 0.0F), palette);

  for (std::size_t b = 0; b < Render::Humanoid::k_bone_count; ++b) {
    out_palettes.push_back(palette[b]);
  }

  Render::GL::AnimationInputs socket_inputs{};
  if (clip.attack_type != BakerAttackType::None) {
    socket_inputs.is_attacking = true;
    socket_inputs.is_melee = !is_ranged_attack_type(clip.attack_type);
    socket_inputs.attack_variant = clip.attack_variant;
  }
  if (clip.combat_pose_type != BakerCombatPoseType::None &&
      ready_weapon_for_profile(profile) != Animation::HumanoidReadyWeapon::None) {
    socket_inputs.is_attacking = true;
    socket_inputs.is_melee = true;
    socket_inputs.attack_variant = 0U;
  }
  if (clip.riding_type == BakerRidingType::SpearThrust) {

    socket_inputs.is_mounted = true;
    socket_inputs.is_attacking = true;
    socket_inputs.is_melee = true;
  }
  if (clip.hold_type != BakerHoldType::None) {
    socket_inputs.is_in_hold_mode = true;
    socket_inputs.hold_entry_progress = 1.0F;
  }

  for (auto const& spec : k_humanoid_sockets) {
    switch (spec.kind) {
    case HumanoidSocketSpec::Kind::GripFrame:
      if (spec.socket == Render::Humanoid::HumanoidSocket::GripR &&
          pose.body_frames.grip_r.radius > 0.0F) {
        out_sockets.push_back(attachment_frame_matrix(pose.body_frames.grip_r));
        continue;
      }
      if (spec.socket == Render::Humanoid::HumanoidSocket::GripL &&
          pose.body_frames.grip_l.radius > 0.0F) {
        out_sockets.push_back(attachment_frame_matrix(pose.body_frames.grip_l));
        continue;
      }
      break;
    case HumanoidSocketSpec::Kind::SwordBladeBase:
      if (pose.body_frames.grip_r.radius > 0.0F) {
        out_sockets.push_back(
            baked_sword_blade_socket_matrix(pose.body_frames.grip_r, false));
        continue;
      }
      break;
    case HumanoidSocketSpec::Kind::SwordBladeTip:
      if (pose.body_frames.grip_r.radius > 0.0F) {
        out_sockets.push_back(
            baked_sword_blade_socket_matrix(pose.body_frames.grip_r, true));
        continue;
      }
      break;
    case HumanoidSocketSpec::Kind::SpearShaftBase:
    case HumanoidSocketSpec::Kind::SpearShaftTip:
    case HumanoidSocketSpec::Kind::SpearHeadTip:
      if (pose.body_frames.grip_r.radius > 0.0F) {
        out_sockets.push_back(baked_spear_socket_matrix(
            pose.body_frames.grip_r,
            socket_inputs,
            clip.combat_pose_type != BakerCombatPoseType::None ? 0.03F : phase,
            spec.kind));
        continue;
      }
      break;
    case HumanoidSocketSpec::Kind::TopologySocket:
      break;
    }
    out_sockets.push_back(Render::Humanoid::socket_transform(palette, spec.socket));
  }
}

} // namespace humanoid_manifest_detail

} // namespace Render::Humanoid
