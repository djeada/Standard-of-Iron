#include <array>
#include <cstddef>
#include <span>
#include <string_view>

#include "render/creature/quadruped/compiled_source_assets_internal.h"

namespace Render::Creature::Quadruped::compiled_source_detail {

} // namespace Render::Creature::Quadruped::compiled_source_detail

using namespace Render::Creature::Quadruped::compiled_source_detail;

namespace Render::Horse {

auto horse_source_mesh_nodes() noexcept -> std::span<const MeshNode> {
  return source_asset().mesh_nodes;
}

auto horse_source_bind_palette() noexcept -> std::span<const QMatrix4x4> {
  return source_asset().bind_palette;
}

auto horse_source_bone_defs() noexcept -> std::span<const BoneDef> {
  return source_asset().bone_defs;
}

auto horse_source_sample_clip(std::string_view source_clip,
                              float normalized_phase,
                              std::span<QMatrix4x4> out) noexcept -> bool {
  return sample_source_clip(
      source_asset(), k_horse_config, source_clip, normalized_phase, out);
}

auto horse_source_pose_mount_frame(std::string_view source_clip,
                                   float normalized_phase,
                                   Render::GL::MountedAttachmentFrame& frame) noexcept
    -> bool {
  auto const sockets = Render::Horse::mounted_socket_set();
  auto const saddle = static_cast<std::size_t>(sockets.saddle);
  auto const bridle = static_cast<std::size_t>(sockets.bridle);
  auto const& bind = source_asset().bind_palette;
  if (saddle >= bind.size() || bridle >= bind.size()) {
    return false;
  }
  thread_local std::array<QMatrix4x4, k_horse_source_bone_count> pose{};
  const std::array<std::size_t, 2> mount_joints{saddle, bridle};
  if (!sample_source_slots(source_asset(),
                           k_horse_config,
                           source_clip,
                           normalized_phase,
                           mount_joints,
                           pose)) {
    return false;
  }
  static const std::array<QMatrix4x4, 2> inverse_bind{bind[saddle].inverted(),
                                                      bind[bridle].inverted()};
  auto const back =
      Render::Creature::Quadruped::BoneDelta{pose[saddle] * inverse_bind[0]};
  auto const head =
      Render::Creature::Quadruped::BoneDelta{pose[bridle] * inverse_bind[1]};

  frame.saddle_center = back.point(frame.saddle_center);
  frame.seat_position = back.point(frame.seat_position);
  frame.stirrup_attach_left = back.point(frame.stirrup_attach_left);
  frame.stirrup_attach_right = back.point(frame.stirrup_attach_right);
  frame.stirrup_bottom_left = back.point(frame.stirrup_bottom_left);
  frame.stirrup_bottom_right = back.point(frame.stirrup_bottom_right);
  frame.seat_forward = back.axis(frame.seat_forward);
  frame.seat_right = back.axis(frame.seat_right);
  frame.seat_up = back.axis(frame.seat_up);

  frame.rein_bit_left = head.point(frame.rein_bit_left);
  frame.rein_bit_right = head.point(frame.rein_bit_right);
  frame.bridle_base = head.point(frame.bridle_base);
  return true;
}

auto horse_source_asset_status() noexcept
    -> const Render::Creature::CompiledCreatureAssetStatus& {
  return source_asset().status;
}

} // namespace Render::Horse

namespace Render::Elephant {

auto elephant_source_mesh_nodes() noexcept
    -> std::span<const Render::Creature::Quadruped::MeshNode> {
  return elephant_asset().mesh_nodes;
}

auto elephant_source_bind_palette() noexcept -> std::span<const QMatrix4x4> {
  return elephant_asset().bind_palette;
}

auto elephant_source_bone_defs() noexcept
    -> std::span<const Render::Creature::BoneDef> {
  return elephant_asset().bone_defs;
}

auto elephant_source_sample_clip(std::string_view source_clip,
                                 float normalized_phase,
                                 std::span<QMatrix4x4> out) noexcept -> bool {
  auto const& asset = elephant_asset();
  if (source_clip == "Walk" || source_clip == "Run") {
    return sample_elephant_locomotion(
        asset, normalized_phase, source_clip == "Run", out);
  }
  if (source_clip == "Angry") {
    return synthesise_elephant_stomp(asset, normalized_phase, out);
  }
  if (source_clip == "Idle") {
    return synthesise_elephant_idle(asset, normalized_phase, out);
  }
  return sample_source_clip(
      asset, k_elephant_config, source_clip, normalized_phase, out);
}

auto elephant_source_pose_howdah(std::string_view source_clip,
                                 float normalized_phase,
                                 Render::GL::HowdahAttachmentFrame& frame) noexcept
    -> bool {
  std::array<QMatrix4x4, k_elephant_source_bone_count> pose{};
  if (!elephant_source_sample_clip(source_clip, normalized_phase, pose)) {
    return false;
  }
  namespace RCQ = Render::Creature::Quadruped;
  auto const back = RCQ::bone_delta(pose,
                                    elephant_source_bind_palette(),
                                    static_cast<std::size_t>(ElephantBone::MountBack));
  frame.howdah_center = back.point(frame.howdah_center);
  frame.seat_position = back.point(frame.seat_position);
  frame.seat_forward = back.axis(frame.seat_forward);
  frame.seat_right = back.axis(frame.seat_right);
  frame.seat_up = back.axis(frame.seat_up);
  return true;
}

auto elephant_source_asset_status() noexcept
    -> const Render::Creature::CompiledCreatureAssetStatus& {
  static const Render::Creature::CompiledCreatureAssetStatus status = [] {
    auto const& source = elephant_asset().status;
    return Render::Creature::CompiledCreatureAssetStatus{
        .loaded = source.loaded,
        .vertex_count = source.vertex_count,
        .triangle_count = source.triangle_count,
        .joint_count = source.joint_count,
        .clip_count = source.clip_count,
        .error = source.error};
  }();
  return status;
}

} // namespace Render::Elephant
