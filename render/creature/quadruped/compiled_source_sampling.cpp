#include "render/creature/quadruped/compiled_source_assets_internal.h"

namespace Render::Creature::Quadruped::compiled_source_detail {

auto source_asset() -> const SourceAsset& {
  static const SourceAsset result = parse_source(k_horse_config);
  return result;
}

auto elephant_asset() -> const SourceAsset& {
  static const SourceAsset result = parse_source(k_elephant_config);
  return result;
}

void sample_channel(AnimationChannel const& channel, float time, NodePose& pose) {
  auto upper = std::upper_bound(channel.times.begin(), channel.times.end(), time);
  std::size_t right = static_cast<std::size_t>(upper - channel.times.begin());
  if (right == 0U) {
    right = 1U;
  }
  if (right >= channel.times.size()) {
    right = channel.times.size() - 1U;
  }
  std::size_t const left = right - 1U;
  float const span = channel.times[right] - channel.times[left];
  float const alpha = channel.step || span <= 1.0e-7F
                          ? 0.0F
                          : std::clamp((time - channel.times[left]) / span, 0.0F, 1.0F);
  auto value = [&](std::size_t key, int component) {
    return channel.values[key * static_cast<std::size_t>(channel.components) +
                          static_cast<std::size_t>(component)];
  };
  if (channel.path == ChannelPath::Rotation) {
    QQuaternion const a(value(left, 3), value(left, 0), value(left, 1), value(left, 2));
    QQuaternion const b(
        value(right, 3), value(right, 0), value(right, 1), value(right, 2));
    pose.rotation = QQuaternion::slerp(a, b, alpha).normalized();
    return;
  }
  QVector3D const a(value(left, 0), value(left, 1), value(left, 2));
  QVector3D const b(value(right, 0), value(right, 1), value(right, 2));
  QVector3D const sampled = a + (b - a) * alpha;
  if (channel.path == ChannelPath::Translation) {
    pose.translation = sampled;
  } else {
    pose.scale = sampled;
  }
}

auto sample_source_clip(SourceAsset const& asset,
                        SourceConfig const& config,
                        std::string_view source_clip,
                        float normalized_phase,
                        std::span<QMatrix4x4> out) noexcept -> bool {
  if (!asset.status.loaded || out.size() < asset.bind_palette.size()) {
    return false;
  }
  auto const clip_it =
      std::find_if(asset.clips.begin(), asset.clips.end(), [&](SourceClip const& clip) {
        return clip.name == source_clip;
      });
  if (clip_it == asset.clips.end()) {
    return false;
  }
  SourceClip const& clip = *clip_it;
  float const phase = std::clamp(normalized_phase, 0.0F, 1.0F);
  float const time = phase * clip.duration;

  thread_local std::vector<NodePose> poses;
  poses.assign(asset.default_poses.begin(), asset.default_poses.end());
  for (AnimationChannel const& channel : clip.channels) {
    sample_channel(channel, time, poses[static_cast<std::size_t>(channel.node)]);
  }
  thread_local std::vector<QMatrix4x4> world;
  evaluate_world_matrices(poses, asset.parents, world);
  for (std::size_t slot = 0U; slot < asset.bind_palette.size(); ++slot) {
    out[slot] = normalize_world_matrix(
        world[static_cast<std::size_t>(asset.joint_nodes[slot])], config);
  }
  return true;
}

auto sample_source_slots(SourceAsset const& asset,
                         SourceConfig const& config,
                         std::string_view source_clip,
                         float normalized_phase,
                         std::span<const std::size_t> joint_slots,
                         std::span<QMatrix4x4> out) noexcept -> bool {
  if (!asset.status.loaded || out.size() < asset.bind_palette.size()) {
    return false;
  }
  auto const clip_it =
      std::find_if(asset.clips.begin(), asset.clips.end(), [&](SourceClip const& clip) {
        return clip.name == source_clip;
      });
  if (clip_it == asset.clips.end()) {
    return false;
  }
  SourceClip const& clip = *clip_it;
  float const phase = std::clamp(normalized_phase, 0.0F, 1.0F);
  float const time = phase * clip.duration;

  thread_local std::vector<bool> needed;
  needed.assign(asset.default_poses.size(), false);
  for (std::size_t const slot : joint_slots) {
    if (slot >= asset.joint_nodes.size()) {
      return false;
    }
    int node = asset.joint_nodes[slot];
    while (node >= 0 && !needed[static_cast<std::size_t>(node)]) {
      needed[static_cast<std::size_t>(node)] = true;
      node = asset.parents[static_cast<std::size_t>(node)];
    }
  }

  thread_local std::vector<NodePose> poses;
  poses.assign(asset.default_poses.begin(), asset.default_poses.end());
  for (AnimationChannel const& channel : clip.channels) {
    auto const node = static_cast<std::size_t>(channel.node);
    if (needed[node]) {
      sample_channel(channel, time, poses[node]);
    }
  }

  thread_local std::vector<QMatrix4x4> world;
  thread_local std::vector<bool> evaluated;
  world.resize(poses.size());
  evaluated.assign(poses.size(), false);
  auto evaluate = [&](auto&& self, std::size_t node) -> void {
    if (evaluated[node]) {
      return;
    }
    int const parent = asset.parents[node];
    if (parent >= 0) {
      self(self, static_cast<std::size_t>(parent));
      world[node] = world[static_cast<std::size_t>(parent)] * local_matrix(poses[node]);
    } else {
      world[node] = local_matrix(poses[node]);
    }
    evaluated[node] = true;
  };
  for (std::size_t const slot : joint_slots) {
    auto const node = static_cast<std::size_t>(asset.joint_nodes[slot]);
    evaluate(evaluate, node);
    out[slot] = normalize_world_matrix(world[node], config);
  }
  return true;
}

void rotate_elephant_subtree_about(std::span<QMatrix4x4> palette,
                                   std::span<const BoneDef> bones,
                                   std::size_t root,
                                   float degrees,
                                   const QVector3D& axis) noexcept {
  if (std::abs(degrees) < 1.0e-4F) {
    return;
  }
  QVector3D const origin = palette[root].column(3).toVector3D();
  QMatrix4x4 delta;
  delta.translate(origin);
  delta.rotate(degrees, axis);
  delta.translate(-origin);
  for (std::size_t bone = 0U; bone < bones.size(); ++bone) {
    std::size_t cursor = bone;
    while (cursor != root && bones[cursor].parent != Render::Creature::k_invalid_bone) {
      cursor = static_cast<std::size_t>(bones[cursor].parent);
    }
    if (cursor == root) {
      palette[bone] = delta * palette[bone];
    }
  }
}

void transform_elephant_palette(std::span<QMatrix4x4> palette,
                                std::size_t bone_count,
                                const QVector3D& pivot,
                                float degrees,
                                const QVector3D& axis) noexcept {
  if (std::abs(degrees) < 1.0e-4F) {
    return;
  }
  QMatrix4x4 delta;
  delta.translate(pivot);
  delta.rotate(degrees, axis);
  delta.translate(-pivot);
  for (QMatrix4x4& bone : palette.first(bone_count)) {
    bone = delta * bone;
  }
}

void apply_elephant_secondary(SourceAsset const& asset,
                              const Animation::ElephantSecondaryMotion& motion,
                              std::span<QMatrix4x4> out) noexcept {
  using namespace ElephantRig;
  auto const& bones = asset.bone_defs;
  rotate_elephant_subtree_about(
      out, bones, k_head, motion.head_pitch_degrees, k_pitch_axis);
  rotate_elephant_subtree_about(
      out, bones, k_head, motion.head_yaw_degrees, k_yaw_axis);
  for (std::size_t segment = 0U; segment < k_trunk.size(); ++segment) {
    rotate_elephant_subtree_about(
        out, bones, k_trunk[segment], motion.trunk_yaw_degrees[segment], k_yaw_axis);
    rotate_elephant_subtree_about(out,
                                  bones,
                                  k_trunk[segment],
                                  motion.trunk_pitch_degrees[segment],
                                  k_pitch_axis);
  }
  rotate_elephant_subtree_about(
      out, bones, k_ear_left, motion.ear_left_degrees, k_yaw_axis);
  rotate_elephant_subtree_about(
      out, bones, k_ear_right, motion.ear_right_degrees, k_yaw_axis);
  rotate_elephant_subtree_about(out, bones, k_tail, motion.tail_degrees, k_roll_axis);
  QVector3D const body_pivot = out[k_body].column(3).toVector3D();
  transform_elephant_palette(out,
                             asset.bind_palette.size(),
                             body_pivot,
                             motion.body_roll_degrees,
                             k_roll_axis);
  transform_elephant_palette(out,
                             asset.bind_palette.size(),
                             body_pivot,
                             motion.body_pitch_degrees,
                             k_pitch_axis);
}

auto synthesise_elephant_locomotion(SourceAsset const& asset,
                                    float phase,
                                    bool fast,
                                    std::span<QMatrix4x4> out) noexcept -> bool {
  if (!asset.status.loaded || out.size() < asset.bind_palette.size()) {
    return false;
  }
  using namespace ElephantRig;
  std::copy(asset.bind_palette.begin(), asset.bind_palette.end(), out.begin());
  auto const& shape =
      fast ? Animation::elephant_run_shape() : Animation::elephant_walk_shape();
  for (std::size_t leg = 0U; leg < k_upper.size(); ++leg) {
    auto const sample = Animation::sample_elephant_leg(
        shape, static_cast<Animation::ElephantLeg>(leg), phase);
    rotate_elephant_subtree_about(
        out, asset.bone_defs, k_upper[leg], sample.thigh_degrees, k_pitch_axis);
    rotate_elephant_subtree_about(
        out, asset.bone_defs, k_lower[leg], sample.knee_degrees, k_pitch_axis);
    rotate_elephant_subtree_about(
        out, asset.bone_defs, k_feet[leg], sample.foot_degrees, k_pitch_axis);
  }
  apply_elephant_secondary(
      asset, Animation::elephant_locomotion_secondary(shape, phase), out);
  return true;
}

auto synthesise_elephant_idle(SourceAsset const& asset,
                              float phase,
                              std::span<QMatrix4x4> out) noexcept -> bool {
  if (!sample_source_clip(asset, k_elephant_config, "Idle", phase, out)) {
    return false;
  }
  apply_elephant_secondary(asset, Animation::elephant_idle_secondary(phase), out);
  return true;
}

auto synthesise_elephant_stomp(SourceAsset const& asset,
                               float phase,
                               std::span<QMatrix4x4> out) noexcept -> bool {
  if (!sample_source_clip(asset, k_elephant_config, "Idle", 0.0F, out)) {
    return false;
  }
  using namespace ElephantRig;
  auto const& bones = asset.bone_defs;
  auto const pose = Animation::resolve_elephant_stomp_pose(phase);
  for (std::size_t leg = 0U; leg < 2U; ++leg) {
    rotate_elephant_subtree_about(
        out, bones, k_upper[leg], pose.front_thigh_degrees, k_pitch_axis);
    rotate_elephant_subtree_about(
        out, bones, k_lower[leg], pose.front_knee_degrees, k_pitch_axis);
    rotate_elephant_subtree_about(
        out,
        bones,
        k_feet[leg],
        -(pose.front_thigh_degrees + pose.front_knee_degrees) * 0.6F,
        k_pitch_axis);
  }
  for (std::size_t leg = 2U; leg < 4U; ++leg) {
    rotate_elephant_subtree_about(
        out, bones, k_upper[leg], -pose.hind_knee_degrees * 0.5F, k_pitch_axis);
    rotate_elephant_subtree_about(
        out, bones, k_lower[leg], pose.hind_knee_degrees, k_pitch_axis);
    rotate_elephant_subtree_about(
        out, bones, k_feet[leg], -pose.hind_knee_degrees * 0.5F, k_pitch_axis);
  }
  rotate_elephant_subtree_about(
      out, bones, k_head, pose.head_pitch_degrees, k_pitch_axis);
  for (std::size_t segment = 0U; segment < k_trunk.size(); ++segment) {
    rotate_elephant_subtree_about(
        out, bones, k_trunk[segment], pose.trunk_pitch_degrees[segment], k_pitch_axis);
  }
  rotate_elephant_subtree_about(
      out, bones, k_ear_left, pose.ear_spread_degrees, k_yaw_axis);
  rotate_elephant_subtree_about(
      out, bones, k_ear_right, -pose.ear_spread_degrees, k_yaw_axis);
  rotate_elephant_subtree_about(out, bones, k_tail, pose.tail_degrees, k_roll_axis);
  QVector3D const hind_left = out[k_feet[2]].column(3).toVector3D();
  QVector3D const hind_right = out[k_feet[3]].column(3).toVector3D();
  QVector3D const pivot = (hind_left + hind_right) * 0.5F;
  transform_elephant_palette(
      out, asset.bind_palette.size(), pivot, pose.rear_degrees, k_pitch_axis);
  return true;
}

auto sample_elephant_locomotion(SourceAsset const& asset,
                                float phase,
                                bool fast,
                                std::span<QMatrix4x4> out) noexcept -> bool {
  const std::string_view authored = fast ? "Run" : "Walk";
  if (sample_source_clip(asset, k_elephant_config, authored, phase, out)) {
    return true;
  }
  return synthesise_elephant_locomotion(asset, phase, fast, out);
}

} // namespace Render::Creature::Quadruped::compiled_source_detail
