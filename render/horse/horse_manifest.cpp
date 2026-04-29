#include "horse_manifest.h"

#include "../creature/bpat/bpat_format.h"
#include "../creature/quadruped/skeleton_factory.h"
#include "../gl/primitives.h"
#include "../horse/dimensions.h"
#include "horse_gait.h"
#include "horse_spec.h"

#include <QMatrix4x4>

#include <array>
#include <cmath>
#include <numbers>
#include <vector>

namespace Render::Horse {

namespace {

constexpr std::uint8_t k_role_coat = 1;
constexpr std::uint8_t k_role_hoof = 4;
constexpr int k_horse_material_id = 6;
constexpr float k_horse_visual_scale = 1.32F;
constexpr float k_horse_body_visual_width_scale = 1.55F;
constexpr float k_horse_body_visual_height_scale = 1.06F;
constexpr float k_horse_torso_width_scale = 1.55F;
constexpr float k_horse_torso_height_scale = 1.36F;
constexpr float k_horse_neck_width_scale = 0.95F;
constexpr float k_horse_neck_height_scale = 0.87F;
constexpr float k_horse_head_length_scale = 1.00F;
constexpr float k_horse_leg_length_scale = 1.12F;
constexpr float k_horse_leg_thickness_scale = 1.95F;
constexpr int k_horse_leg_radial_segments = 6;

struct HorseClipSpec {
  Render::Creature::BakeClipDescriptor desc;
  Render::GL::GaitType gait;
  bool is_moving;
  bool is_fighting{false};
  float bob_scale{0.0F};
};

const std::array<HorseClipSpec, 6> k_horse_clips{{
    {{"idle", 24U, 24.0F, true},
     Render::GL::GaitType::IDLE,
     false,
     false,
     0.0F},
    {{"walk", 24U, 24.0F, true},
     Render::GL::GaitType::WALK,
     true,
     false,
     0.50F},
    {{"trot", 16U, 24.0F, true},
     Render::GL::GaitType::TROT,
     true,
     false,
     0.85F},
    {{"canter", 16U, 24.0F, true},
     Render::GL::GaitType::CANTER,
     true,
     false,
     1.00F},
    {{"gallop", 12U, 24.0F, true},
     Render::GL::GaitType::GALLOP,
     true,
     false,
     1.12F},
    {{"fight", 24U, 24.0F, true},
     Render::GL::GaitType::IDLE,
     false,
     true,
     0.0F},
}};

const std::array<Render::Creature::BakeClipDescriptor, k_horse_clips.size()>
    k_horse_clip_descs{{
        k_horse_clips[0].desc,
        k_horse_clips[1].desc,
        k_horse_clips[2].desc,
        k_horse_clips[3].desc,
        k_horse_clips[4].desc,
        k_horse_clips[5].desc,
    }};

auto build_horse_whole_nodes()
    -> std::vector<Render::Creature::Quadruped::MeshNode> {
  using namespace Render::Creature;
  using namespace Render::Creature::Quadruped;

  Render::GL::HorseDimensions const dims =
      Render::GL::make_horse_dimensions(0U);
  float const bw =
      dims.body_width * k_horse_body_visual_width_scale * k_horse_visual_scale;
  float const bh = dims.body_height * k_horse_body_visual_height_scale *
                   k_horse_visual_scale;
  float const bl = dims.body_length * k_horse_visual_scale;
  float const hw = dims.head_width * k_horse_visual_scale;
  float const hh = dims.head_height * k_horse_visual_scale;
  float const hl = dims.head_length * k_horse_visual_scale;
  float const torso_lift = bh * 0.14F;

  std::vector<MeshNode> nodes;
  nodes.reserve(20);

  BarrelNode rear;
  rear.horse_rump_profile = true;
  rear.rings = {
      {-bl * 0.78F, bh * 0.34F + torso_lift,
       bw * 0.30F * k_horse_torso_width_scale,
       bh * 0.2970F * k_horse_torso_height_scale,
       bh * 0.1760F * k_horse_torso_height_scale},
      {-bl * 0.66F, bh * 0.43F + torso_lift,
       bw * 0.46F * k_horse_torso_width_scale,
       bh * 0.4400F * k_horse_torso_height_scale,
       bh * 0.3190F * k_horse_torso_height_scale},
      {-bl * 0.48F, bh * 0.45F + torso_lift,
       bw * 0.50F * k_horse_torso_width_scale,
       bh * 0.5005F * k_horse_torso_height_scale,
       bh * 0.3795F * k_horse_torso_height_scale},
      {-bl * 0.26F, bh * 0.40F + torso_lift,
       bw * 0.46F * k_horse_torso_width_scale,
       bh * 0.4620F * k_horse_torso_height_scale,
       bh * 0.3025F * k_horse_torso_height_scale},
      {-bl * 0.06F, bh * 0.34F + torso_lift,
       bw * 0.38F * k_horse_torso_width_scale,
       bh * 0.3905F * k_horse_torso_height_scale,
       bh * 0.1925F * k_horse_torso_height_scale},
  };
  nodes.push_back({"horse.body.rear", static_cast<BoneIndex>(HorseBone::Body),
                   k_role_coat, kLodAll, 0, rear});

  BarrelNode mid;
  mid.rings = {
      {-bl * 0.36F, bh * 0.28F + torso_lift,
       bw * 0.42F * k_horse_torso_width_scale,
       bh * 0.4284F * k_horse_torso_height_scale,
       bh * 0.3780F * k_horse_torso_height_scale},
      {0.0F, bh * 0.28F + torso_lift, bw * 0.435F * k_horse_torso_width_scale,
       bh * 0.4284F * k_horse_torso_height_scale,
       bh * 0.3780F * k_horse_torso_height_scale},
      {bl * 0.36F, bh * 0.30F + torso_lift,
       bw * 0.42F * k_horse_torso_width_scale,
       bh * 0.4032F * k_horse_torso_height_scale,
       bh * 0.3528F * k_horse_torso_height_scale},
  };
  nodes.push_back({"horse.body.mid", static_cast<BoneIndex>(HorseBone::Body),
                   k_role_coat, kLodAll, 0, mid});

  BarrelNode front;
  front.rings = {
      {bl * 0.04F, bh * 0.34F + torso_lift,
       bw * 0.39F * k_horse_torso_width_scale,
       bh * 0.4536F * k_horse_torso_height_scale,
       bh * 0.3528F * k_horse_torso_height_scale},
      {bl * 0.40F, bh * 0.42F + torso_lift,
       bw * 0.355F * k_horse_torso_width_scale,
       bh * 0.5544F * k_horse_torso_height_scale,
       bh * 0.3906F * k_horse_torso_height_scale},
      {bl * 0.76F, bh * 0.50F + torso_lift,
       bw * 0.30F * k_horse_torso_width_scale,
       bh * 0.5544F * k_horse_torso_height_scale,
       bh * 0.2772F * k_horse_torso_height_scale},
  };
  nodes.push_back({"horse.body.front", static_cast<BoneIndex>(HorseBone::Body),
                   k_role_coat, kLodAll, 0, front});

  TubeNode neck;
  neck.start = QVector3D(0.0F, bh * 0.62F, bl * 0.44F);
  neck.end = QVector3D(0.0F, bh * 1.56F, bl * 0.98F);
  neck.start_radius = 0.5F * (bw * 0.34F * k_horse_neck_width_scale +
                              bh * 0.34F * k_horse_neck_height_scale);
  neck.end_radius = 0.5F * (bw * 0.20F * k_horse_neck_width_scale +
                            bh * 0.19F * k_horse_neck_height_scale);
  neck.segment_count = 5U;
  neck.ring_vertices = 8U;
  nodes.push_back({"horse.neck", static_cast<BoneIndex>(HorseBone::NeckTop),
                   k_role_coat, kLodAll, 0, neck});

  float const head_back_z = neck.end.z();
  float const head_mid_z = head_back_z + hl * 0.44F * k_horse_head_length_scale;
  float const head_front_z =
      head_back_z + hl * 0.88F * k_horse_head_length_scale;

  EllipsoidNode head;
  head.center = QVector3D(0.0F, bh * 1.62F, head_back_z + hl * 0.34F);
  head.radii = QVector3D(hw * 1.05F, hh * 0.78F, hl * 0.62F);
  head.ring_count = 5U;
  head.ring_vertices = 8U;
  nodes.push_back({"horse.head.upper", static_cast<BoneIndex>(HorseBone::Head),
                   k_role_coat, kLodAll, 0, head});

  BarrelNode muzzle;
  muzzle.horse_muzzle_profile = true;
  muzzle.rings = {
      {head_back_z + hl * 0.22F, bh * 1.54F, hw * 0.42F, hh * 0.20F,
       hh * 0.16F},
      {head_back_z + hl * 0.46F, bh * 1.46F, hw * 0.39F, hh * 0.19F,
       hh * 0.15F},
      {head_mid_z + hl * 0.16F, bh * 1.34F, hw * 0.31F, hh * 0.15F, hh * 0.12F},
      {head_front_z - hl * 0.04F, bh * 1.22F, hw * 0.24F, hh * 0.11F,
       hh * 0.10F},
  };
  nodes.push_back({"horse.head.nose", static_cast<BoneIndex>(HorseBone::Head),
                   k_role_coat, kLodAll, 0, muzzle});

  EllipsoidNode eye_l;
  eye_l.center = QVector3D(hw * 0.60F, bh * 1.44F, head_back_z + hl * 0.42F);
  eye_l.radii = QVector3D(hw * 0.060F, hh * 0.050F, hl * 0.040F);
  eye_l.ring_count = 4U;
  eye_l.ring_vertices = 6U;
  nodes.push_back({"horse.eye.l", static_cast<BoneIndex>(HorseBone::Head),
                   k_role_hoof, kLodAll, 0, eye_l});
  EllipsoidNode eye_r = eye_l;
  eye_r.center.setX(-eye_l.center.x());
  nodes.push_back({"horse.eye.r", static_cast<BoneIndex>(HorseBone::Head),
                   k_role_hoof, kLodAll, 0, eye_r});

  auto add_leg = [&](float x, float z, BoneIndex foot_bone) {
    ColumnLegNode leg;
    leg.top_center = QVector3D(x, -bh * (z < 0.0F ? 0.18F : 0.12F), z);
    leg.bottom_y = -dims.leg_length * k_horse_visual_scale * 0.86F *
                   k_horse_leg_length_scale;
    leg.top_radius_x = dims.body_width * k_horse_visual_scale *
                       (z < 0.0F ? 0.18F : 0.17F) * k_horse_leg_thickness_scale;
    leg.top_radius_z = leg.top_radius_x * 0.86F;
    leg.shaft_taper = 0.50F;
    leg.foot_radius_scale = 1.45F;
    nodes.push_back({"horse.leg", foot_bone, k_role_coat, kLodAll, 0, leg});
  };
  add_leg(bw * 0.34F, bl * 0.55F, static_cast<BoneIndex>(HorseBone::FootFL));
  add_leg(-bw * 0.34F, bl * 0.55F, static_cast<BoneIndex>(HorseBone::FootFR));
  add_leg(bw * 0.28F, -bl * 0.58F, static_cast<BoneIndex>(HorseBone::FootBL));
  add_leg(-bw * 0.28F, -bl * 0.58F, static_cast<BoneIndex>(HorseBone::FootBR));

  FlatFanNode mane;
  mane.outline = {
      {0.0F, bh * 1.38F, bl * 0.86F}, {0.0F, bh * 1.16F, bl * 0.76F},
      {0.0F, bh * 0.92F, bl * 0.62F}, {0.0F, bh * 0.62F, bl * 0.44F},
      {0.0F, bh * 0.70F, bl * 0.56F}, {0.0F, bh * 1.02F, bl * 0.78F},
  };
  mane.thickness_axis = QVector3D(1.0F, 0.0F, 0.0F);
  mane.thickness = bw * 0.09F;
  nodes.push_back({"horse.mane", static_cast<BoneIndex>(HorseBone::NeckTop),
                   k_role_coat, kLodAll, 0, mane});

  FlatFanNode ear_l;
  ear_l.outline = {
      {hw * 0.22F, bh * 1.74F, head_back_z + hl * 0.08F},
      {hw * 0.44F, bh * 2.16F, head_back_z - hl * 0.02F},
      {hw * 0.08F, bh * 1.86F, head_back_z - hl * 0.24F},
  };
  ear_l.thickness_axis = QVector3D(0.0F, 0.0F, 1.0F);
  ear_l.thickness = hw * 0.065F;
  nodes.push_back({"horse.ear.l", static_cast<BoneIndex>(HorseBone::Head),
                   k_role_coat, kLodAll, 0, ear_l});

  FlatFanNode ear_r = ear_l;
  for (QVector3D &p : ear_r.outline) {
    p.setX(-p.x());
  }
  nodes.push_back({"horse.ear.r", static_cast<BoneIndex>(HorseBone::Head),
                   k_role_coat, kLodAll, 0, ear_r});

  TubeNode tail;
  tail.start = QVector3D(0.0F, bh * 0.34F, -bl * 0.82F);
  tail.end = QVector3D(0.0F, -bh * 0.40F, -bl * 1.18F);
  tail.start_radius = bw * 0.09F;
  tail.end_radius = bw * 0.035F;
  tail.segment_count = 4U;
  tail.ring_vertices = 6U;
  tail.sag = -bh * 0.08F;
  nodes.push_back({"horse.tail", static_cast<BoneIndex>(HorseBone::Body),
                   k_role_coat, kLodAll, 0, tail});

  return nodes;
}

const auto k_horse_whole_nodes = build_horse_whole_nodes();
const std::array<std::string_view, 1> k_horse_full_excluded_prefixes{
    "horse.leg"};

auto horse_thigh_segment_mesh(bool front) noexcept -> Render::GL::Mesh * {
  static Render::GL::Mesh *const front_mesh =
      Render::GL::get_unit_tapered_cylinder(1.28F, 0.56F,
                                            k_horse_leg_radial_segments);
  static Render::GL::Mesh *const rear_mesh =
      Render::GL::get_unit_tapered_cylinder(1.34F, 0.60F,
                                            k_horse_leg_radial_segments);
  return front ? front_mesh : rear_mesh;
}

auto horse_calf_segment_mesh(bool front) noexcept -> Render::GL::Mesh * {
  static Render::GL::Mesh *const front_mesh =
      Render::GL::get_unit_tapered_cylinder(1.10F, 0.78F,
                                            k_horse_leg_radial_segments);
  static Render::GL::Mesh *const rear_mesh =
      Render::GL::get_unit_tapered_cylinder(1.14F, 0.82F,
                                            k_horse_leg_radial_segments);
  return front ? front_mesh : rear_mesh;
}

auto horse_hoof_center_y(bool front,
                         Render::GL::HorseDimensions const &dims) -> float {
  return dims.hoof_height * (front ? 0.33F : 0.31F);
}

auto horse_hoof_half_extents(
    bool front, Render::GL::HorseDimensions const &dims) -> QVector3D {
  return QVector3D(dims.body_width * (front ? 0.062F : 0.058F),
                   dims.hoof_height * 0.33F,
                   dims.body_width * (front ? 0.110F : 0.098F));
}

auto build_horse_full_leg_overlays()
    -> std::array<Render::Creature::PrimitiveInstance, 12> {
  using Render::Creature::PrimitiveInstance;
  Render::GL::HorseDimensions const dims =
      Render::GL::make_horse_dimensions(0U);
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
    thigh.params.head_offset =
        QVector3D(0.0F, dims.body_height * (front ? 0.88F : 1.04F),
                  dims.body_length * (front ? 0.055F : -0.075F));
    thigh.params.tail_offset =
        QVector3D(0.0F, dims.leg_length * (front ? 0.085F : 0.090F),
                  dims.body_length * (front ? 0.040F : 0.040F));
    thigh.params.radius = dims.body_width * (front ? 0.430F : 0.455F);
    thigh.custom_mesh = horse_thigh_segment_mesh(front);
    thigh.color_role = k_role_coat;
    thigh.material_id = k_horse_material_id;
    thigh.lod_mask = Render::Creature::kLodFull;

    PrimitiveInstance &calf = out[idx++];
    calf.debug_name = calf_name;
    calf.shape = Render::Creature::PrimitiveShape::Cylinder;
    calf.params.anchor_bone = knee_bone;
    calf.params.tail_bone = foot_bone;
    calf.params.head_offset =
        QVector3D(0.0F, dims.leg_length * (front ? 0.030F : 0.008F),
                  dims.body_length * (front ? 0.012F : -0.010F));
    calf.params.tail_offset =
        QVector3D(0.0F, dims.hoof_height * (front ? 0.02F : 0.08F),
                  dims.body_length * (front ? 0.008F : -0.010F));
    calf.params.radius = dims.body_width * (front ? 0.225F : 0.240F);
    calf.custom_mesh = horse_calf_segment_mesh(front);
    calf.color_role = k_role_coat;
    calf.material_id = k_horse_material_id;
    calf.lod_mask = Render::Creature::kLodFull;

    PrimitiveInstance &hoof = out[idx++];
    hoof.debug_name = hoof_name;
    hoof.shape = Render::Creature::PrimitiveShape::Box;
    hoof.params.anchor_bone = foot_bone;
    hoof.params.head_offset =
        QVector3D(0.0F, horse_hoof_center_y(front, dims),
                  (front ? 1.0F : -1.0F) * dims.body_width * 0.010F);
    hoof.params.half_extents = horse_hoof_half_extents(front, dims);
    hoof.color_role = k_role_hoof;
    hoof.material_id = k_horse_material_id;
    hoof.lod_mask = Render::Creature::kLodFull;
  };

  add_leg("horse.leg.fl.thigh", "horse.leg.fl.calf", "horse.leg.fl.hoof",
          static_cast<Render::Creature::BoneIndex>(HorseBone::ShoulderFL),
          static_cast<Render::Creature::BoneIndex>(HorseBone::KneeFL),
          static_cast<Render::Creature::BoneIndex>(HorseBone::FootFL), true);
  add_leg("horse.leg.fr.thigh", "horse.leg.fr.calf", "horse.leg.fr.hoof",
          static_cast<Render::Creature::BoneIndex>(HorseBone::ShoulderFR),
          static_cast<Render::Creature::BoneIndex>(HorseBone::KneeFR),
          static_cast<Render::Creature::BoneIndex>(HorseBone::FootFR), true);
  add_leg("horse.leg.bl.thigh", "horse.leg.bl.calf", "horse.leg.bl.hoof",
          static_cast<Render::Creature::BoneIndex>(HorseBone::ShoulderBL),
          static_cast<Render::Creature::BoneIndex>(HorseBone::KneeBL),
          static_cast<Render::Creature::BoneIndex>(HorseBone::FootBL), false);
  add_leg("horse.leg.br.thigh", "horse.leg.br.calf", "horse.leg.br.hoof",
          static_cast<Render::Creature::BoneIndex>(HorseBone::ShoulderBR),
          static_cast<Render::Creature::BoneIndex>(HorseBone::KneeBR),
          static_cast<Render::Creature::BoneIndex>(HorseBone::FootBR), false);
  return out;
}

const auto k_horse_full_leg_overlays = build_horse_full_leg_overlays();

auto horse_topology_storage() noexcept
    -> const Render::Creature::Quadruped::TopologyStorage & {
  static const auto storage = Render::Creature::Quadruped::make_topology();
  return storage;
}

auto horse_topology_ref() noexcept
    -> const Render::Creature::SkeletonTopology & {
  static const auto topology = horse_topology_storage().topology();
  return topology;
}

void bake_horse_manifest_clip_palettes(std::size_t clip_index,
                                       std::uint32_t frame_index,
                                       std::vector<QMatrix4x4> &out_palettes) {
  auto const &clip = k_horse_clips[clip_index];
  Render::GL::HorseDimensions const dims =
      Render::GL::make_horse_dimensions(0U);
  float const phase =
      static_cast<float>(frame_index) /
      static_cast<float>(std::max<std::uint32_t>(clip.desc.frame_count, 1U));

  Render::GL::HorseGait base{};
  Render::GL::HorseGait const gait = Render::GL::gait_for_type(clip.gait, base);

  Render::Horse::HorsePoseMotion motion{};
  motion.phase = phase;
  motion.is_moving = clip.is_moving;
  motion.is_fighting = clip.is_fighting;
  if (!clip.is_fighting) {
    float const amp =
        clip.is_moving ? dims.move_bob_amplitude : dims.idle_bob_amplitude;
    float const scale = clip.is_moving ? clip.bob_scale : 0.8F;
    motion.bob =
        std::sin(phase * 2.0F * std::numbers::pi_v<float>) * amp * scale;
  }

  Render::Horse::HorseSpecPose pose{};
  Render::Horse::make_horse_spec_pose_animated(dims, gait, motion, pose);

  Render::Horse::BonePalette palette{};
  Render::Horse::evaluate_horse_skeleton(pose, palette);
  out_palettes.insert(out_palettes.end(), palette.begin(), palette.end());
}

} // namespace

auto horse_manifest() noexcept -> const Render::Creature::SpeciesManifest & {
  static const Render::Creature::SpeciesManifest manifest = [] {
    Render::Creature::SpeciesManifest m;
    m.species_name = "horse";
    m.species_id = Render::Creature::Bpat::kSpeciesHorse;
    m.bpat_file_name = "horse.bpat";
    m.minimal_snapshot_file_name = "horse_minimal.bpsm";
    m.topology = &horse_topology_ref();
    m.lod_full.primitive_name = "horse.full.body";
    m.lod_full.anchor_bone =
        static_cast<Render::Creature::BoneIndex>(HorseBone::Root);
    m.lod_full.mesh_skinning = Render::Creature::MeshSkinning::HorseWhole;
    m.lod_full.color_role = k_role_coat;
    m.lod_full.material_id = k_horse_material_id;
    m.lod_full.lod_mask = Render::Creature::kLodFull;
    m.lod_full.mesh_nodes =
        std::span<const Render::Creature::Quadruped::MeshNode>(
            k_horse_whole_nodes);
    m.lod_full.excluded_node_name_prefixes = k_horse_full_excluded_prefixes;
    m.lod_full.overlay_primitives = k_horse_full_leg_overlays;
    m.lod_minimal.primitive_name = "horse.minimal.whole";
    m.lod_minimal.anchor_bone =
        static_cast<Render::Creature::BoneIndex>(HorseBone::Root);
    m.lod_minimal.mesh_skinning = Render::Creature::MeshSkinning::HorseWhole;
    m.lod_minimal.color_role = k_role_coat;
    m.lod_minimal.material_id = k_horse_material_id;
    m.lod_minimal.lod_mask = Render::Creature::kLodMinimal;
    m.lod_minimal.mesh_nodes =
        std::span<const Render::Creature::Quadruped::MeshNode>(
            k_horse_whole_nodes);
    m.clips = std::span<const Render::Creature::BakeClipDescriptor>(
        k_horse_clip_descs);
    m.bind_palette = &horse_bind_palette;
    m.creature_spec = &horse_creature_spec;
    m.bake_clip_palette = &bake_horse_manifest_clip_palettes;
    return m;
  }();
  return manifest;
}

} // namespace Render::Horse
