#include "elephant_manifest.h"

#include "../creature/bpat/bpat_format.h"
#include "../creature/quadruped/skeleton_factory.h"
#include "../gl/primitives.h"
#include "dimensions.h"
#include "elephant_gait.h"
#include "elephant_spec.h"

#include <QMatrix4x4>

#include <array>
#include <cmath>
#include <numbers>
#include <vector>

namespace Render::Elephant {

namespace {

constexpr std::uint8_t kRoleSkin = 1;
constexpr std::uint8_t kRoleSkinTrunk = 5;
constexpr std::uint8_t kRoleTusk = 6;
constexpr std::uint8_t kRoleEye = 7;
constexpr int kElephantMaterialId = 6;
constexpr float kElephantVisualScale = 1.85F;
constexpr float kElephantBodyLengthScale = 0.56F;
constexpr float kElephantBodyWidthScale = 0.66F;
constexpr float kElephantBodyHeightScale = 1.02F;
constexpr float kElephantHeadScale = 1.55F;
constexpr float kElephantTorsoWidthScale = 1.20F;
constexpr float kElephantTorsoHeightScale = 1.10F;
constexpr int kElephantLegRadialSegments = 6;

struct ElephantClipSpec {
  Render::Creature::BakeClipDescriptor desc;
  bool is_moving;
  Render::GL::ElephantGait gait;
  bool is_fighting{false};
  float bob_scale{0.0F};
};

const std::array<ElephantClipSpec, 4> kElephantClips{{
    {{"idle", 24U, 24.0F, true},
     false,
     Render::GL::ElephantGait{2.0F, 0.0F, 0.0F, 0.02F, 0.01F},
     false,
     0.0F},
    {{"walk", 24U, 24.0F, true},
     true,
     Render::GL::ElephantGait{1.2F, 0.25F, 0.0F, 0.30F, 0.10F},
     false,
     0.62F},
    {{"run", 16U, 24.0F, true},
     true,
     Render::GL::ElephantGait{0.6F, 0.5F, 0.5F, 0.70F, 0.25F},
     false,
     0.75F},
    {{"fight", 24U, 24.0F, true},
     false,
     Render::GL::ElephantGait{1.15F, 0.0F, 0.0F, 0.30F, 0.06F},
     true,
     0.0F},
}};

const std::array<Render::Creature::BakeClipDescriptor, kElephantClips.size()>
    kElephantClipDescs{{
        kElephantClips[0].desc,
        kElephantClips[1].desc,
        kElephantClips[2].desc,
        kElephantClips[3].desc,
    }};

auto build_elephant_whole_nodes()
    -> std::vector<Render::Creature::Quadruped::MeshNode> {
  using namespace Render::Creature;
  using namespace Render::Creature::Quadruped;

  Render::GL::ElephantDimensions const dims =
      Render::GL::make_elephant_dimensions(0U);
  float const bw =
      dims.body_width * kElephantBodyWidthScale * kElephantVisualScale;
  float const bh =
      dims.body_height * kElephantBodyHeightScale * kElephantVisualScale;
  float const bl =
      dims.body_length * kElephantBodyLengthScale * kElephantVisualScale;
  float const hw = dims.head_width * kElephantVisualScale;
  float const hh = dims.head_height * kElephantVisualScale;
  float const hl = dims.head_length * kElephantVisualScale;

  std::vector<MeshNode> nodes;
  nodes.reserve(24);

  BarrelNode body;
  body.rings = {
      {-bl * 0.70F, bh * 0.00F, bw * 0.34F * kElephantTorsoWidthScale,
       bh * 0.12F * kElephantTorsoHeightScale,
       bh * 0.12F * kElephantTorsoHeightScale},
      {-bl * 0.56F, bh * 0.04F, bw * 0.70F * kElephantTorsoWidthScale,
       bh * 0.32F * kElephantTorsoHeightScale,
       bh * 0.28F * kElephantTorsoHeightScale},
      {-bl * 0.36F, bh * 0.04F, bw * 0.88F * kElephantTorsoWidthScale,
       bh * 0.38F * kElephantTorsoHeightScale,
       bh * 0.40F * kElephantTorsoHeightScale},
      {-bl * 0.14F, bh * 0.02F, bw * 0.96F * kElephantTorsoWidthScale,
       bh * 0.42F * kElephantTorsoHeightScale,
       bh * 0.46F * kElephantTorsoHeightScale},
      {bl * 0.10F, bh * 0.02F, bw * 0.96F * kElephantTorsoWidthScale,
       bh * 0.42F * kElephantTorsoHeightScale,
       bh * 0.46F * kElephantTorsoHeightScale},
      {bl * 0.32F, bh * 0.06F, bw * 0.86F * kElephantTorsoWidthScale,
       bh * 0.40F * kElephantTorsoHeightScale,
       bh * 0.36F * kElephantTorsoHeightScale},
      {bl * 0.50F, bh * 0.10F, bw * 0.74F * kElephantTorsoWidthScale,
       bh * 0.38F * kElephantTorsoHeightScale,
       bh * 0.28F * kElephantTorsoHeightScale},
      {bl * 0.62F, bh * 0.14F, bw * 0.50F * kElephantTorsoWidthScale,
       bh * 0.24F * kElephantTorsoHeightScale,
       bh * 0.14F * kElephantTorsoHeightScale},
      {bl * 0.70F, bh * 0.13F, bw * 0.28F * kElephantTorsoWidthScale,
       bh * 0.10F * kElephantTorsoHeightScale,
       bh * 0.04F * kElephantTorsoHeightScale},
  };
  nodes.push_back({"elephant.body", static_cast<BoneIndex>(ElephantBone::Body),
                   kRoleSkin, kLodAll, 0, body});

  EllipsoidNode head;
  head.center = QVector3D(0.0F, bh * 0.28F, bl * 0.72F);
  head.radii = QVector3D(hw * 0.41F * kElephantHeadScale,
                         hh * 0.51F * kElephantHeadScale,
                         hl * 0.48F * kElephantHeadScale);
  head.ring_count = 5U;
  head.ring_vertices = 8U;
  nodes.push_back({"elephant.head", static_cast<BoneIndex>(ElephantBone::Head),
                   kRoleSkin, kLodAll, 0, head});

  float const head_front_z = head.center.z() + head.radii.z();
  float const trunk_base_z = bl * 1.16F;
  float const eye_radius_z = hl * 0.035F;
  float const head_eye_z = head_front_z - 3 * eye_radius_z;

  EllipsoidNode trunk_bridge;
  trunk_bridge.center = QVector3D(0.0F, bh * 0.08F, head_front_z - hl * 0.075F);
  trunk_bridge.radii = QVector3D(hw * 0.16F, hh * 0.14F, hl * 0.095F);
  trunk_bridge.ring_count = 4U;
  trunk_bridge.ring_vertices = 6U;
  nodes.push_back({"elephant.trunk.bridge",
                   static_cast<BoneIndex>(ElephantBone::Head), kRoleSkin,
                   kLodAll, 0, trunk_bridge});

  auto add_eye = [&](float side) {
    EllipsoidNode eye;
    eye.center = QVector3D(side * hw * 0.34F, bh * 0.24F, head_eye_z);
    eye.radii = QVector3D(hw * 0.055F, hh * 0.055F, eye_radius_z);
    eye.ring_count = 4U;
    eye.ring_vertices = 6U;
    nodes.push_back({"elephant.eye", static_cast<BoneIndex>(ElephantBone::Head),
                     kRoleEye, kLodAll, 0, eye});
  };
  add_eye(1.0F);
  add_eye(-1.0F);

  auto add_leg = [&](std::string_view thigh_name, std::string_view calf_name,
                     std::string_view hoof_name, float x, float z,
                     BoneIndex shoulder_bone, BoneIndex knee_bone,
                     BoneIndex foot_bone, bool front) {
    float const top_y = -bh * 0.30F;
    float const sole_y = -dims.leg_length * kElephantVisualScale * 2.04F;
    float const leg_drop = sole_y - top_y;
    float const knee_y = top_y + leg_drop * (front ? 0.36F : 0.34F);
    float const hoof_height = dims.foot_radius * kElephantVisualScale * 0.86F;
    float const hoof_top_y = sole_y + hoof_height;
    float const calf_end_y =
        hoof_top_y + dims.leg_radius * kElephantVisualScale * 0.08F;
    float const bend_z = (front ? 1.0F : -1.0F) * dims.body_length *
                         kElephantVisualScale * 0.16F;
    float const ankle_z = z + (front ? 1.0F : -1.0F) * dims.body_length *
                                  kElephantVisualScale * 0.06F;
    float const foot_scale = dims.foot_radius * kElephantVisualScale;
    float const leg_scale =
        dims.leg_radius * kElephantVisualScale * (front ? 2.15F : 2.02F);
    float const hoof_radius_y = hoof_height * 0.5F;
    float const knee_x = x;
    float const ankle_x = x;

    QVector3D const shoulder_local(x, top_y, z);
    QVector3D const knee_local(knee_x, knee_y, z + bend_z);
    QVector3D const calf_end(ankle_x, calf_end_y, ankle_z);

    QVector3D thigh_axis = knee_local - shoulder_local;
    if (thigh_axis.lengthSquared() > 1.0e-6F) {
      thigh_axis.normalize();
    }
    QVector3D calf_axis = calf_end - knee_local;
    if (calf_axis.lengthSquared() > 1.0e-6F) {
      calf_axis.normalize();
    }
    float const knee_gap = leg_scale * 0.34F;
    float const ankle_gap = foot_scale * 0.24F;

    TubeNode thigh;
    thigh.start = shoulder_local;
    thigh.end = knee_local - thigh_axis * knee_gap;
    thigh.start_radius = leg_scale * 0.44F;
    thigh.end_radius = leg_scale * 0.20F;
    thigh.segment_count = 4U;
    thigh.ring_vertices = 6U;
    nodes.push_back({thigh_name, shoulder_bone, kRoleSkin, kLodAll, 0, thigh});

    TubeNode calf;
    calf.start = knee_local + calf_axis * knee_gap;
    calf.end = calf_end + calf_axis * ankle_gap;
    calf.start_radius = leg_scale * 0.16F;
    calf.end_radius = leg_scale * 0.10F;
    calf.segment_count = 4U;
    calf.ring_vertices = 6U;
    nodes.push_back({calf_name, knee_bone, kRoleSkin, kLodAll, 0, calf});

    EllipsoidNode hoof;
    hoof.center = QVector3D(ankle_x, hoof_top_y - hoof_radius_y, ankle_z);
    hoof.radii = QVector3D(foot_scale * (front ? 1.22F : 1.14F), hoof_radius_y,
                           foot_scale * (front ? 1.42F : 1.28F));
    hoof.ring_count = 4U;
    hoof.ring_vertices = 6U;
    nodes.push_back({hoof_name, foot_bone, kRoleSkin, kLodAll, 0, hoof});
  };
  add_leg("elephant.leg.fl.thigh", "elephant.leg.fl.calf",
          "elephant.leg.fl.hoof", bw * 0.40F, bl * 0.42F,
          static_cast<BoneIndex>(ElephantBone::ShoulderFL),
          static_cast<BoneIndex>(ElephantBone::KneeFL),
          static_cast<BoneIndex>(ElephantBone::FootFL), true);
  add_leg("elephant.leg.fr.thigh", "elephant.leg.fr.calf",
          "elephant.leg.fr.hoof", -bw * 0.40F, bl * 0.42F,
          static_cast<BoneIndex>(ElephantBone::ShoulderFR),
          static_cast<BoneIndex>(ElephantBone::KneeFR),
          static_cast<BoneIndex>(ElephantBone::FootFR), true);
  add_leg("elephant.leg.bl.thigh", "elephant.leg.bl.calf",
          "elephant.leg.bl.hoof", bw * 0.38F, -bl * 0.44F,
          static_cast<BoneIndex>(ElephantBone::ShoulderBL),
          static_cast<BoneIndex>(ElephantBone::KneeBL),
          static_cast<BoneIndex>(ElephantBone::FootBL), false);
  add_leg("elephant.leg.br.thigh", "elephant.leg.br.calf",
          "elephant.leg.br.hoof", -bw * 0.38F, -bl * 0.44F,
          static_cast<BoneIndex>(ElephantBone::ShoulderBR),
          static_cast<BoneIndex>(ElephantBone::KneeBR),
          static_cast<BoneIndex>(ElephantBone::FootBR), false);

  SnoutNode trunk;
  trunk.start = QVector3D(0.0F, bh * 0.06F, trunk_base_z);
  trunk.end = QVector3D(0.0F, -bh * 1.02F, bl * 1.04F);
  trunk.base_radius = dims.trunk_base_radius * kElephantVisualScale * 0.304F;
  trunk.tip_radius = dims.trunk_tip_radius * kElephantVisualScale * 0.25F;
  trunk.sag = -bh * 0.16F;
  trunk.segment_count = 9U;
  trunk.ring_vertices = 6U;
  nodes.push_back({"elephant.trunk",
                   static_cast<BoneIndex>(ElephantBone::TrunkTip),
                   kRoleSkinTrunk, kLodAll, 0, trunk});

  auto add_tusk = [&](float side) {
    ConeNode tusk;
    tusk.base_center = QVector3D(side * hw * 0.32F, bh * 0.02F, bl * 0.96F);
    tusk.tip =
        QVector3D(side * hw * 0.42F, -bh * 0.34F,
                  bl * 1.10F + dims.tusk_length * kElephantVisualScale * 0.24F);
    tusk.base_radius = dims.tusk_radius * kElephantVisualScale * 1.0F;
    tusk.ring_vertices = 6U;
    nodes.push_back({"elephant.tusk",
                     static_cast<BoneIndex>(ElephantBone::Head), kRoleTusk,
                     kLodAll, 0, tusk});
  };
  add_tusk(1.0F);
  add_tusk(-1.0F);

  auto make_ear = [&](float side) {
    constexpr float kElephantEarScale = 1.0F / 1.5F;
    FlatFanNode ear;
    QVector3D const ear_root(side * (hw * ((0.48F + 0.34F + 0.40F) / 3.0F)),
                             bh * ((0.82F + 0.08F + 0.58F) / 3.0F),
                             bl * ((0.56F + 0.62F + 0.58F) / 3.0F));
    auto scale_from_root = [&](const QVector3D &point) {
      return ear_root + (point - ear_root) * kElephantEarScale;
    };
    ear.outline = {
        scale_from_root({side * (hw * 0.48F), bh * 0.82F, bl * 0.56F}),
        scale_from_root({side * (hw * 1.34F), bh * 0.70F, bl * 0.48F}),
        scale_from_root({side * (hw * 1.72F), bh * 0.26F, bl * 0.44F}),
        scale_from_root({side * (hw * 1.54F), -bh * 0.24F, bl * 0.44F}),
        scale_from_root({side * (hw * 1.04F), -bh * 0.48F, bl * 0.50F}),
        scale_from_root({side * (hw * 0.46F), -bh * 0.34F, bl * 0.60F}),
        scale_from_root({side * (hw * 0.34F), bh * 0.08F, bl * 0.62F}),
        scale_from_root({side * (hw * 0.40F), bh * 0.58F, bl * 0.58F}),
    };
    ear.thickness_axis = QVector3D(0.0F, 0.0F, 1.0F);
    ear.thickness =
        dims.ear_thickness * kElephantVisualScale * 1.15F * kElephantEarScale;
    nodes.push_back({"elephant.ear", static_cast<BoneIndex>(ElephantBone::Head),
                     kRoleSkin, kLodAll, 0, ear});
  };
  make_ear(1.0F);
  make_ear(-1.0F);

  TubeNode tail;
  tail.start = QVector3D(0.0F, bh * 0.11F, -bl * 0.50F);
  tail.end = QVector3D(0.0F, -bh * 0.48F, -bl * 1.00F);
  tail.start_radius = dims.tail_length * kElephantVisualScale * 0.03F;
  tail.end_radius = dims.tail_length * kElephantVisualScale * 0.04F;
  tail.segment_count = 4U;
  tail.ring_vertices = 6U;
  tail.sag = -bh * 0.05F;
  nodes.push_back({"elephant.tail", static_cast<BoneIndex>(ElephantBone::Body),
                   kRoleSkin, kLodAll, 0, tail});

  return nodes;
}

const auto kElephantWholeNodes = build_elephant_whole_nodes();
const std::array<std::string_view, 1> kElephantFullExcludedPrefixes{
    "elephant.leg."};

auto elephant_leg_segment_mesh() noexcept -> Render::GL::Mesh * {
  static Render::GL::Mesh *const mesh =
      Render::GL::get_unit_cylinder(kElephantLegRadialSegments);
  return mesh;
}

auto build_elephant_full_leg_overlays()
    -> std::array<Render::Creature::PrimitiveInstance, 12> {
  using Render::Creature::PrimitiveInstance;
  Render::GL::ElephantDimensions const dims =
      Render::GL::make_elephant_dimensions(0U);
  std::array<PrimitiveInstance, 12> out{};
  std::size_t idx = 0U;

  auto add_leg = [&](std::string_view thigh_name, std::string_view calf_name,
                     std::string_view hoof_name,
                     Render::Creature::BoneIndex shoulder_bone,
                     Render::Creature::BoneIndex knee_bone,
                     Render::Creature::BoneIndex foot_bone, bool front) {
    PrimitiveInstance &thigh = out[idx++];
    thigh.debug_name = thigh_name;
    thigh.shape = Render::Creature::PrimitiveShape::Cylinder;
    thigh.params.anchor_bone = shoulder_bone;
    thigh.params.tail_bone = knee_bone;
    thigh.params.head_offset = QVector3D(0.0F, dims.leg_length * 0.34F, 0.0F);
    thigh.params.tail_offset = QVector3D(0.0F, -dims.leg_length * 0.06F, 0.0F);
    thigh.params.radius = dims.leg_radius * (front ? 1.60F : 1.50F);
    thigh.custom_mesh = elephant_leg_segment_mesh();
    thigh.color_role = kRoleSkin;
    thigh.material_id = kElephantMaterialId;
    thigh.lod_mask = Render::Creature::kLodFull;

    PrimitiveInstance &calf = out[idx++];
    calf.debug_name = calf_name;
    calf.shape = Render::Creature::PrimitiveShape::Cylinder;
    calf.params.anchor_bone = knee_bone;
    calf.params.tail_bone = foot_bone;
    calf.params.head_offset = QVector3D(0.0F, dims.leg_length * 0.10F, 0.0F);
    calf.params.tail_offset =
        QVector3D(0.0F, dims.foot_radius * (front ? 0.06F : 0.04F), 0.0F);
    calf.params.radius = dims.leg_radius * (front ? 0.90F : 0.85F);
    calf.custom_mesh = elephant_leg_segment_mesh();
    calf.color_role = kRoleSkin;
    calf.material_id = kElephantMaterialId;
    calf.lod_mask = Render::Creature::kLodFull;

    PrimitiveInstance &hoof = out[idx++];
    hoof.debug_name = hoof_name;
    hoof.shape = Render::Creature::PrimitiveShape::Box;
    hoof.params.anchor_bone = foot_bone;
    hoof.params.head_offset =
        QVector3D(0.0F, dims.foot_radius * (0.56F / 3.0F),
                  (front ? 1.0F : -1.0F) * dims.foot_radius * (0.10F / 3.0F));
    hoof.params.half_extents =
        QVector3D(dims.foot_radius * (front ? 0.88F / 3.0F : 0.82F / 3.0F),
                  dims.foot_radius * (0.56F / 3.0F),
                  dims.foot_radius * (front ? 1.18F / 3.0F : 1.02F / 3.0F));
    hoof.color_role = kRoleSkin;
    hoof.material_id = kElephantMaterialId;
    hoof.lod_mask = Render::Creature::kLodFull;
  };

  add_leg("elephant.leg.fl.thigh", "elephant.leg.fl.calf",
          "elephant.leg.fl.hoof",
          static_cast<Render::Creature::BoneIndex>(ElephantBone::ShoulderFL),
          static_cast<Render::Creature::BoneIndex>(ElephantBone::KneeFL),
          static_cast<Render::Creature::BoneIndex>(ElephantBone::FootFL), true);
  add_leg("elephant.leg.fr.thigh", "elephant.leg.fr.calf",
          "elephant.leg.fr.hoof",
          static_cast<Render::Creature::BoneIndex>(ElephantBone::ShoulderFR),
          static_cast<Render::Creature::BoneIndex>(ElephantBone::KneeFR),
          static_cast<Render::Creature::BoneIndex>(ElephantBone::FootFR), true);
  add_leg(
      "elephant.leg.bl.thigh", "elephant.leg.bl.calf", "elephant.leg.bl.hoof",
      static_cast<Render::Creature::BoneIndex>(ElephantBone::ShoulderBL),
      static_cast<Render::Creature::BoneIndex>(ElephantBone::KneeBL),
      static_cast<Render::Creature::BoneIndex>(ElephantBone::FootBL), false);
  add_leg(
      "elephant.leg.br.thigh", "elephant.leg.br.calf", "elephant.leg.br.hoof",
      static_cast<Render::Creature::BoneIndex>(ElephantBone::ShoulderBR),
      static_cast<Render::Creature::BoneIndex>(ElephantBone::KneeBR),
      static_cast<Render::Creature::BoneIndex>(ElephantBone::FootBR), false);
  return out;
}

const auto kElephantFullLegOverlays = build_elephant_full_leg_overlays();

auto elephant_topology_storage() noexcept
    -> const Render::Creature::Quadruped::TopologyStorage & {
  static const auto storage = [] {
    Render::Creature::Quadruped::TopologyOptions options;
    options.include_neck_top = false;
    options.include_appendage_tip = true;
    options.appendage_tip_name = "TrunkTip";
    return Render::Creature::Quadruped::make_topology(options);
  }();
  return storage;
}

auto elephant_topology_ref() noexcept
    -> const Render::Creature::SkeletonTopology & {
  static const auto topology = elephant_topology_storage().topology();
  return topology;
}

void bake_elephant_manifest_clip_palettes(
    std::size_t clip_index, std::uint32_t frame_index,
    std::vector<QMatrix4x4> &out_palettes) {
  auto const &clip = kElephantClips[clip_index];
  Render::GL::ElephantDimensions const dims =
      Render::GL::make_elephant_dimensions(0U);
  float const phase =
      static_cast<float>(frame_index) /
      static_cast<float>(std::max<std::uint32_t>(clip.desc.frame_count, 1U));

  Render::Elephant::ElephantPoseMotion motion{};
  motion.phase = phase;
  motion.is_moving = clip.is_moving;
  motion.is_fighting = clip.is_fighting;
  motion.anim_time = phase * clip.gait.cycle_time;
  if (!clip.is_fighting) {
    float const amp =
        clip.is_moving ? dims.move_bob_amplitude : dims.idle_bob_amplitude;
    float const scale = clip.is_moving ? clip.bob_scale : 0.8F;
    motion.bob =
        std::sin(phase * 2.0F * std::numbers::pi_v<float>) * amp * scale;
  }

  Render::Elephant::ElephantSpecPose pose{};
  Render::Elephant::make_elephant_spec_pose_animated(dims, clip.gait, motion,
                                                     pose);

  Render::Elephant::BonePalette palette{};
  Render::Elephant::evaluate_elephant_skeleton(pose, palette);
  out_palettes.insert(out_palettes.end(), palette.begin(), palette.end());
}

} // namespace

auto elephant_manifest() noexcept -> const Render::Creature::SpeciesManifest & {
  static const Render::Creature::SpeciesManifest manifest = [] {
    Render::Creature::SpeciesManifest m;
    m.species_name = "elephant";
    m.species_id = Render::Creature::Bpat::kSpeciesElephant;
    m.bpat_file_name = "elephant.bpat";
    m.minimal_snapshot_file_name = "elephant_minimal.bpsm";
    m.topology = &elephant_topology_ref();
    m.lod_full.primitive_name = "elephant.full.body";
    m.lod_full.anchor_bone =
        static_cast<Render::Creature::BoneIndex>(ElephantBone::Root);
    m.lod_full.mesh_skinning = Render::Creature::MeshSkinning::ElephantWhole;
    m.lod_full.color_role = kRoleSkin;
    m.lod_full.material_id = kElephantMaterialId;
    m.lod_full.lod_mask = Render::Creature::kLodFull;
    m.lod_full.mesh_nodes =
        std::span<const Render::Creature::Quadruped::MeshNode>(
            kElephantWholeNodes);
    m.lod_full.excluded_node_name_prefixes = kElephantFullExcludedPrefixes;
    m.lod_full.overlay_primitives = kElephantFullLegOverlays;
    m.lod_minimal.primitive_name = "elephant.minimal.whole";
    m.lod_minimal.anchor_bone =
        static_cast<Render::Creature::BoneIndex>(ElephantBone::Root);
    m.lod_minimal.mesh_skinning = Render::Creature::MeshSkinning::ElephantWhole;
    m.lod_minimal.color_role = kRoleSkin;
    m.lod_minimal.material_id = kElephantMaterialId;
    m.lod_minimal.lod_mask = Render::Creature::kLodMinimal;
    m.lod_minimal.mesh_nodes =
        std::span<const Render::Creature::Quadruped::MeshNode>(
            kElephantWholeNodes);
    m.clips = std::span<const Render::Creature::BakeClipDescriptor>(
        kElephantClipDescs);
    m.bind_palette = &elephant_bind_palette;
    m.creature_spec = &elephant_creature_spec;
    m.bake_clip_palette = &bake_elephant_manifest_clip_palettes;
    return m;
  }();
  return manifest;
}

} // namespace Render::Elephant
