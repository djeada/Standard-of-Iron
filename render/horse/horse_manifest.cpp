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

constexpr std::uint8_t kRoleCoat = 1;
constexpr std::uint8_t kRoleHoof = 4;
constexpr int kHorseMaterialId = 6;
constexpr float kHorseVisualScale = 1.32F;
constexpr float kHorseBodyVisualWidthScale = 1.55F;
constexpr float kHorseBodyVisualHeightScale = 1.06F;
constexpr float kHorseTorsoWidthScale = 1.55F;
constexpr float kHorseTorsoHeightScale = 1.36F;
constexpr float kHorseNeckWidthScale = 0.95F;
constexpr float kHorseNeckHeightScale = 0.87F;
constexpr float kHorseHeadLengthScale = 1.00F;
constexpr float kHorseLegLengthScale = 1.12F;
constexpr float kHorseLegThicknessScale = 1.95F;
constexpr int kHorseLegRadialSegments = 6;

struct HorseClipSpec {
  Render::Creature::BakeClipDescriptor desc;
  Render::GL::GaitType gait;
  bool is_moving;
  bool is_fighting{false};
  float bob_scale{0.0F};
};

const std::array<HorseClipSpec, 6> kHorseClips{{
    {{"idle", 24U, 24.0F, true}, Render::GL::GaitType::IDLE, false, false,
     0.0F},
    {{"walk", 24U, 24.0F, true}, Render::GL::GaitType::WALK, true, false,
     0.50F},
    {{"trot", 16U, 24.0F, true}, Render::GL::GaitType::TROT, true, false,
     0.85F},
    {{"canter", 16U, 24.0F, true}, Render::GL::GaitType::CANTER, true, false,
     1.00F},
    {{"gallop", 12U, 24.0F, true}, Render::GL::GaitType::GALLOP, true, false,
     1.12F},
    {{"fight", 24U, 24.0F, true}, Render::GL::GaitType::IDLE, false, true,
     0.0F},
}};

const std::array<Render::Creature::BakeClipDescriptor, kHorseClips.size()>
    kHorseClipDescs{{
        kHorseClips[0].desc,
        kHorseClips[1].desc,
        kHorseClips[2].desc,
        kHorseClips[3].desc,
        kHorseClips[4].desc,
        kHorseClips[5].desc,
    }};

auto build_horse_whole_nodes() -> std::vector<Render::Creature::Quadruped::MeshNode> {
  using namespace Render::Creature;
  using namespace Render::Creature::Quadruped;

  Render::GL::HorseDimensions const dims = Render::GL::make_horse_dimensions(0U);
  float const bw = dims.body_width * kHorseBodyVisualWidthScale * kHorseVisualScale;
  float const bh = dims.body_height * kHorseBodyVisualHeightScale * kHorseVisualScale;
  float const bl = dims.body_length * kHorseVisualScale;
  float const hw = dims.head_width * kHorseVisualScale;
  float const hh = dims.head_height * kHorseVisualScale;
  float const hl = dims.head_length * kHorseVisualScale;
  float const torso_lift = bh * 0.14F;

  std::vector<MeshNode> nodes;
  nodes.reserve(14);

  BarrelNode rear;
  rear.rings = {
      {-bl * 0.98F, bh * 0.20F + torso_lift,
       bw * 0.16F * kHorseTorsoWidthScale,
       bh * 0.2142F * kHorseTorsoHeightScale,
       bh * 0.1008F * kHorseTorsoHeightScale},
      {-bl * 0.78F, bh * 0.33F + torso_lift,
       bw * 0.31F * kHorseTorsoWidthScale,
       bh * 0.4032F * kHorseTorsoHeightScale,
       bh * 0.1890F * kHorseTorsoHeightScale},
      {-bl * 0.54F, bh * 0.46F + torso_lift,
       bw * 0.48F * kHorseTorsoWidthScale,
       bh * 0.5922F * kHorseTorsoHeightScale,
       bh * 0.3150F * kHorseTorsoHeightScale},
      {-bl * 0.24F, bh * 0.34F + torso_lift,
       bw * 0.40F * kHorseTorsoWidthScale,
       bh * 0.3906F * kHorseTorsoHeightScale,
       bh * 0.1638F * kHorseTorsoHeightScale},
      {-bl * 0.02F, bh * 0.24F + torso_lift,
       bw * 0.34F * kHorseTorsoWidthScale,
       bh * 0.3150F * kHorseTorsoHeightScale,
       bh * 0.1008F * kHorseTorsoHeightScale},
  };
  nodes.push_back({"horse.body.rear", static_cast<BoneIndex>(HorseBone::Body),
                   kRoleCoat, kLodAll, 0, rear});

  BarrelNode mid;
  mid.rings = {
      {-bl * 0.36F, bh * 0.28F + torso_lift,
       bw * 0.42F * kHorseTorsoWidthScale,
       bh * 0.4284F * kHorseTorsoHeightScale,
       bh * 0.3780F * kHorseTorsoHeightScale},
      {0.0F, bh * 0.28F + torso_lift,
       bw * 0.435F * kHorseTorsoWidthScale,
       bh * 0.4284F * kHorseTorsoHeightScale,
       bh * 0.3780F * kHorseTorsoHeightScale},
      {bl * 0.36F, bh * 0.30F + torso_lift,
       bw * 0.42F * kHorseTorsoWidthScale,
       bh * 0.4032F * kHorseTorsoHeightScale,
       bh * 0.3528F * kHorseTorsoHeightScale},
  };
  nodes.push_back({"horse.body.mid", static_cast<BoneIndex>(HorseBone::Body),
                   kRoleCoat, kLodAll, 0, mid});

  BarrelNode front;
  front.rings = {
      {bl * 0.04F, bh * 0.34F + torso_lift,
       bw * 0.39F * kHorseTorsoWidthScale,
       bh * 0.4536F * kHorseTorsoHeightScale,
       bh * 0.3528F * kHorseTorsoHeightScale},
      {bl * 0.40F, bh * 0.42F + torso_lift,
       bw * 0.355F * kHorseTorsoWidthScale,
       bh * 0.5544F * kHorseTorsoHeightScale,
       bh * 0.3906F * kHorseTorsoHeightScale},
      {bl * 0.76F, bh * 0.50F + torso_lift,
       bw * 0.30F * kHorseTorsoWidthScale,
       bh * 0.5544F * kHorseTorsoHeightScale,
       bh * 0.2772F * kHorseTorsoHeightScale},
  };
  nodes.push_back({"horse.body.front", static_cast<BoneIndex>(HorseBone::Body),
                   kRoleCoat, kLodAll, 0, front});

  TubeNode neck;
  neck.start = QVector3D(0.0F, bh * 0.62F, bl * 0.44F);
  neck.end = QVector3D(0.0F, bh * 1.70F, bl * 1.07F);
  neck.start_radius =
      0.5F * (bw * 0.34F * kHorseNeckWidthScale +
              bh * 0.34F * kHorseNeckHeightScale);
  neck.end_radius =
      0.5F * (bw * 0.26F * kHorseNeckWidthScale +
              bh * 0.24F * kHorseNeckHeightScale);
  neck.segment_count = 5U;
  neck.ring_vertices = 8U;
  nodes.push_back({"horse.neck", static_cast<BoneIndex>(HorseBone::NeckTop),
                   kRoleCoat, kLodAll, 0, neck});

  float const head_back_z = neck.end.z();
  float const head_mid_z = head_back_z + hl * 0.76F * kHorseHeadLengthScale;
  float const head_front_z = head_mid_z + hl * 0.40F * kHorseHeadLengthScale;

  TubeNode head_upper;
  head_upper.start = neck.end;
  head_upper.end = QVector3D(0.0F, bh * 1.24F, head_mid_z);
  head_upper.start_radius = neck.end_radius * 1.04F;
  head_upper.end_radius = 0.5F * (hw * 1.20F + hh * 0.86F);
  head_upper.segment_count = 4U;
  head_upper.ring_vertices = 8U;
  nodes.push_back({"horse.head.upper", static_cast<BoneIndex>(HorseBone::Head),
                   kRoleCoat, kLodAll, 0, head_upper});

  TubeNode nose;
  nose.start = head_upper.end;
  nose.end = QVector3D(0.0F, bh * 1.10F, head_front_z);
  nose.start_radius = head_upper.end_radius;
  nose.end_radius = 0.5F * (hw * 0.56F + hh * 0.44F);
  nose.segment_count = 3U;
  nose.ring_vertices = 8U;
  nodes.push_back({"horse.head.nose", static_cast<BoneIndex>(HorseBone::Head),
                   kRoleCoat, kLodAll, 0, nose});

  auto add_leg = [&](float x, float z, BoneIndex foot_bone) {
    ColumnLegNode leg;
    leg.top_center = QVector3D(x, -bh * (z < 0.0F ? 0.18F : 0.12F), z);
    leg.bottom_y = -dims.leg_length * kHorseVisualScale * 0.86F *
                   kHorseLegLengthScale;
    leg.top_radius_x =
        dims.body_width * kHorseVisualScale * (z < 0.0F ? 0.18F : 0.17F) *
        kHorseLegThicknessScale;
    leg.top_radius_z = leg.top_radius_x * 0.86F;
    leg.shaft_taper = 0.50F;
    leg.foot_radius_scale = 1.45F;
    nodes.push_back({"horse.leg", foot_bone, kRoleCoat, kLodAll, 0, leg});
  };
  add_leg(bw * 0.34F, bl * 0.55F, static_cast<BoneIndex>(HorseBone::FootFL));
  add_leg(-bw * 0.34F, bl * 0.55F, static_cast<BoneIndex>(HorseBone::FootFR));
  add_leg(bw * 0.28F, -bl * 0.58F, static_cast<BoneIndex>(HorseBone::FootBL));
  add_leg(-bw * 0.28F, -bl * 0.58F, static_cast<BoneIndex>(HorseBone::FootBR));

  FlatFanNode mane;
  mane.outline = {
      {0.0F, bh * 1.38F, bl * 0.86F},
      {0.0F, bh * 1.16F, bl * 0.76F},
      {0.0F, bh * 0.92F, bl * 0.62F},
      {0.0F, bh * 0.62F, bl * 0.44F},
      {0.0F, bh * 0.70F, bl * 0.56F},
      {0.0F, bh * 1.02F, bl * 0.78F},
  };
  mane.thickness_axis = QVector3D(1.0F, 0.0F, 0.0F);
  mane.thickness = bw * 0.09F;
  nodes.push_back({"horse.mane", static_cast<BoneIndex>(HorseBone::NeckTop),
                   kRoleCoat, kLodAll, 0, mane});

  FlatFanNode ear_l;
  ear_l.outline = {
      {hw * 0.14F, bh * 1.36F, head_back_z + hl * 0.04F},
      {hw * 0.24F, bh * 1.74F, head_back_z - hl * 0.02F},
      {hw * 0.06F, bh * 1.42F, head_back_z - hl * 0.14F},
  };
  ear_l.thickness_axis = QVector3D(0.0F, 0.0F, 1.0F);
  ear_l.thickness = hw * 0.03F;
  nodes.push_back({"horse.ear.l", static_cast<BoneIndex>(HorseBone::Head), kRoleCoat,
                   kLodAll, 0, ear_l});

  FlatFanNode ear_r = ear_l;
  for (QVector3D &p : ear_r.outline) {
    p.setX(-p.x());
  }
  nodes.push_back({"horse.ear.r", static_cast<BoneIndex>(HorseBone::Head), kRoleCoat,
                   kLodAll, 0, ear_r});

  TubeNode tail;
  tail.start = QVector3D(0.0F, bh * 0.24F, -bl * 0.84F);
  tail.end = QVector3D(0.0F, -bh * 0.44F, -bl * 1.02F);
  tail.start_radius = bw * 0.09F;
  tail.end_radius = bw * 0.035F;
  tail.segment_count = 4U;
  tail.ring_vertices = 6U;
  tail.sag = -bh * 0.08F;
  nodes.push_back({"horse.tail", static_cast<BoneIndex>(HorseBone::Body), kRoleCoat,
                   kLodAll, 0, tail});

  return nodes;
}

const auto kHorseWholeNodes = build_horse_whole_nodes();
const std::array<std::string_view, 1> kHorseFullExcludedPrefixes{"horse.leg"};

auto horse_thigh_segment_mesh(bool front) noexcept -> Render::GL::Mesh * {
  static Render::GL::Mesh *const front_mesh =
      Render::GL::get_unit_tapered_cylinder(1.28F, 0.56F,
                                            kHorseLegRadialSegments);
  static Render::GL::Mesh *const rear_mesh =
      Render::GL::get_unit_tapered_cylinder(1.34F, 0.60F,
                                            kHorseLegRadialSegments);
  return front ? front_mesh : rear_mesh;
}

auto horse_calf_segment_mesh(bool front) noexcept -> Render::GL::Mesh * {
  static Render::GL::Mesh *const front_mesh =
      Render::GL::get_unit_tapered_cylinder(1.10F, 0.78F,
                                            kHorseLegRadialSegments);
  static Render::GL::Mesh *const rear_mesh =
      Render::GL::get_unit_tapered_cylinder(1.14F, 0.82F,
                                            kHorseLegRadialSegments);
  return front ? front_mesh : rear_mesh;
}

auto horse_hoof_center_y(bool front, Render::GL::HorseDimensions const &dims)
    -> float {
  return dims.hoof_height * (front ? 0.33F : 0.31F);
}

auto horse_hoof_half_extents(bool front, Render::GL::HorseDimensions const &dims)
    -> QVector3D {
  return QVector3D(dims.body_width * (front ? 0.062F : 0.058F),
                   dims.hoof_height * 0.33F,
                   dims.body_width * (front ? 0.110F : 0.098F));
}

auto build_horse_full_leg_overlays()
    -> std::array<Render::Creature::PrimitiveInstance, 12> {
  using Render::Creature::PrimitiveInstance;
  Render::GL::HorseDimensions const dims = Render::GL::make_horse_dimensions(0U);
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
        QVector3D(0.0F, dims.body_height * (front ? 0.72F : 0.78F), 0.0F);
    thigh.params.tail_offset =
        QVector3D(0.0F, dims.leg_length * (front ? 0.020F : 0.018F), 0.0F);
    thigh.params.radius = dims.body_width * (front ? 0.430F : 0.455F);
    thigh.custom_mesh = horse_thigh_segment_mesh(front);
    thigh.color_role = kRoleCoat;
    thigh.material_id = kHorseMaterialId;
    thigh.lod_mask = Render::Creature::kLodFull;

    PrimitiveInstance &calf = out[idx++];
    calf.debug_name = calf_name;
    calf.shape = Render::Creature::PrimitiveShape::Cylinder;
    calf.params.anchor_bone = knee_bone;
    calf.params.tail_bone = foot_bone;
    calf.params.head_offset =
        QVector3D(0.0F, -dims.leg_length * (front ? 0.004F : 0.003F), 0.0F);
    calf.params.tail_offset = QVector3D(0.0F, dims.hoof_height * 0.10F, 0.0F);
    calf.params.radius = dims.body_width * (front ? 0.225F : 0.240F);
    calf.custom_mesh = horse_calf_segment_mesh(front);
    calf.color_role = kRoleCoat;
    calf.material_id = kHorseMaterialId;
    calf.lod_mask = Render::Creature::kLodFull;

    PrimitiveInstance &hoof = out[idx++];
    hoof.debug_name = hoof_name;
    hoof.shape = Render::Creature::PrimitiveShape::Box;
    hoof.params.anchor_bone = foot_bone;
    hoof.params.head_offset = QVector3D(
        0.0F, horse_hoof_center_y(front, dims),
        (front ? 1.0F : -1.0F) * dims.body_width * 0.010F);
    hoof.params.half_extents = horse_hoof_half_extents(front, dims);
    hoof.color_role = kRoleHoof;
    hoof.material_id = kHorseMaterialId;
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

const auto kHorseFullLegOverlays = build_horse_full_leg_overlays();

auto horse_topology_storage() noexcept
    -> const Render::Creature::Quadruped::TopologyStorage & {
  static const auto storage = Render::Creature::Quadruped::make_topology();
  return storage;
}

auto horse_topology_ref() noexcept -> const Render::Creature::SkeletonTopology & {
  static const auto topology = horse_topology_storage().topology();
  return topology;
}

void bake_horse_manifest_clip_palettes(std::size_t clip_index,
                                       std::uint32_t frame_index,
                                       std::vector<QMatrix4x4> &out_palettes) {
  auto const &clip = kHorseClips[clip_index];
  Render::GL::HorseDimensions const dims = Render::GL::make_horse_dimensions(0U);
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
    m.lod_full.color_role = kRoleCoat;
    m.lod_full.material_id = kHorseMaterialId;
    m.lod_full.lod_mask = Render::Creature::kLodFull;
    m.lod_full.mesh_nodes =
        std::span<const Render::Creature::Quadruped::MeshNode>(kHorseWholeNodes);
    m.lod_full.excluded_node_name_prefixes = kHorseFullExcludedPrefixes;
    m.lod_full.overlay_primitives = kHorseFullLegOverlays;
    m.lod_minimal.primitive_name = "horse.minimal.whole";
    m.lod_minimal.anchor_bone =
        static_cast<Render::Creature::BoneIndex>(HorseBone::Root);
    m.lod_minimal.mesh_skinning = Render::Creature::MeshSkinning::HorseWhole;
    m.lod_minimal.color_role = kRoleCoat;
    m.lod_minimal.material_id = kHorseMaterialId;
    m.lod_minimal.lod_mask = Render::Creature::kLodMinimal;
    m.lod_minimal.mesh_nodes =
        std::span<const Render::Creature::Quadruped::MeshNode>(kHorseWholeNodes);
    m.clips = std::span<const Render::Creature::BakeClipDescriptor>(kHorseClipDescs);
    m.bind_palette = &horse_bind_palette;
    m.creature_spec = &horse_creature_spec;
    m.bake_clip_palette = &bake_horse_manifest_clip_palettes;
    return m;
  }();
  return manifest;
}

} // namespace Render::Horse
