#include "horse_spec.h"

#include "../creature/part_graph.h"
#include "../creature/skeleton.h"
#include "../geom/transforms.h"
#include "../gl/mesh.h"
#include "../gl/primitives.h"
#include "../submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <numbers>
#include <span>
#include <vector>

namespace Render::Horse {

namespace {

using Render::Creature::BoneDef;
using Render::Creature::BoneIndex;
using Render::Creature::CreatureLOD;
using Render::Creature::kLodMinimal;
using Render::Creature::kLodReduced;
using Render::Creature::PartGraph;
using Render::Creature::PrimitiveInstance;
using Render::Creature::PrimitiveParams;
using Render::Creature::PrimitiveShape;
using Render::Creature::SkeletonTopology;

constexpr std::array<BoneDef, kHorseBoneCount> kHorseBones = {{
    {"Root", Render::Creature::kInvalidBone},
    {"Body", 0},
    {"FootFL", 0},
    {"FootFR", 0},
    {"FootBL", 0},
    {"FootBR", 0},
    {"NeckTop", 0},
    {"Head", 0},
}};

constexpr std::array<Render::Creature::SocketDef, 0> kHorseSockets{};

constexpr SkeletonTopology kHorseTopology{
    std::span<const BoneDef>(kHorseBones),
    std::span<const Render::Creature::SocketDef>(kHorseSockets),
};

constexpr std::uint8_t kRoleCoat = 1;
constexpr std::uint8_t kRoleCoatDark = 2;
constexpr std::uint8_t kRoleCoatReducedLeg = 3;
constexpr std::uint8_t kRoleHoof = 4;
constexpr std::uint8_t kRoleMane = 5;
constexpr std::uint8_t kRoleTail = 6;
constexpr std::uint8_t kRoleMuzzle = 7;
constexpr std::uint8_t kRoleEye = 8;
constexpr std::size_t kHorseRoleCount = 8;

constexpr float kHorseBodyVisualWidthScale = 1.24F;
constexpr float kHorseBodyVisualHeightScale = 1.16F;

[[nodiscard]] auto
translation_matrix(const QVector3D &origin) noexcept -> QMatrix4x4 {
  QMatrix4x4 m;
  m.setToIdentity();
  m.setColumn(3, QVector4D(origin, 1.0F));
  return m;
}

struct primitive_instance_desc {
  std::string_view debug_name{};
  PrimitiveShape shape{PrimitiveShape::Sphere};
  BoneIndex anchor_bone{0};
  BoneIndex tail_bone{0};
  QVector3D head_offset{};
  QVector3D tail_offset{};
  QVector3D half_extents{};
  float radius{0.0F};
  std::uint8_t color_role{0};
  int material_id{0};
  std::uint32_t lod_mask{0};
  Render::GL::Mesh *custom_mesh{nullptr};
};

struct torso_ring {
  float z{0.0F};
  float y{0.0F};
  float half_width{0.0F};
  float top{0.0F};
  float bottom{0.0F};
};

auto make_torso_vertex(const QVector3D &pos, const QVector3D &normal, float u,
                       float v) -> Render::GL::Vertex {
  QVector3D n = normal;
  if (n.lengthSquared() < 1.0e-8F) {
    n = QVector3D(0.0F, 1.0F, 0.0F);
  } else {
    n.normalize();
  }
  return {{pos.x(), pos.y(), pos.z()}, {n.x(), n.y(), n.z()}, {u, v}};
}

auto create_faceted_horse_torso_mesh() -> std::unique_ptr<Render::GL::Mesh> {
  Render::GL::HorseDimensions const dims =
      Render::GL::make_horse_dimensions(0U);
  float const bw = dims.body_width * kHorseBodyVisualWidthScale;
  float const bh = dims.body_height * kHorseBodyVisualHeightScale;
  float const bl = dims.body_length;

  std::array<torso_ring, 6> const rings{{
      {-0.56F, 0.06F, 0.14F, 0.22F, 0.14F},
      {-0.38F, 0.04F, 0.26F, 0.30F, 0.20F},
      {-0.12F, 0.00F, 0.31F, 0.26F, 0.22F},
      {0.16F, 0.00F, 0.30F, 0.27F, 0.22F},
      {0.38F, 0.05F, 0.28F, 0.34F, 0.20F},
      {0.56F, 0.03F, 0.17F, 0.24F, 0.18F},
  }};

  std::vector<Render::GL::Vertex> vertices;
  std::vector<unsigned int> indices;
  vertices.reserve(rings.size() * 8U + 2U);
  indices.reserve((rings.size() - 1U) * 8U * 6U + 48U);

  for (std::size_t r = 0; r < rings.size(); ++r) {
    torso_ring const &ring = rings[r];
    float const z = ring.z * bl;
    float const yc = ring.y * bh;
    float const hw = ring.half_width * bw;
    float const top = yc + ring.top * bh;
    float const bot = yc - ring.bottom * bh;
    float const upper = yc + ring.top * bh * 0.55F;
    float const lower = yc - ring.bottom * bh * 0.62F;
    float const shoulder = hw * 0.82F;
    float const rib = hw;
    float const v =
        static_cast<float>(r) / static_cast<float>(rings.size() - 1U);

    std::array<QVector3D, 8> const pts{{
        {0.0F, top, z},
        {shoulder, upper, z},
        {rib, yc, z},
        {shoulder, lower, z},
        {0.0F, bot, z},
        {-shoulder, lower, z},
        {-rib, yc, z},
        {-shoulder, upper, z},
    }};

    for (std::size_t p = 0; p < pts.size(); ++p) {
      QVector3D normal(pts[p].x() / std::max(0.001F, hw),
                       (pts[p].y() - yc) / std::max(0.001F, bh * 0.30F), 0.0F);
      vertices.push_back(
          make_torso_vertex(pts[p], normal, static_cast<float>(p) / 7.0F, v));
    }
  }

  constexpr unsigned int kRingVertexCount = 8U;
  for (unsigned int r = 0; r + 1U < rings.size(); ++r) {
    unsigned int const row = r * kRingVertexCount;
    unsigned int const next = (r + 1U) * kRingVertexCount;
    for (unsigned int p = 0; p < kRingVertexCount; ++p) {
      unsigned int const a = row + p;
      unsigned int const b = row + ((p + 1U) % kRingVertexCount);
      unsigned int const c = next + ((p + 1U) % kRingVertexCount);
      unsigned int const d = next + p;
      indices.push_back(a);
      indices.push_back(b);
      indices.push_back(c);
      indices.push_back(c);
      indices.push_back(d);
      indices.push_back(a);
    }
  }

  auto add_cap = [&](unsigned int row, QVector3D normal, bool reverse) {
    QVector3D center;
    for (unsigned int p = 0; p < kRingVertexCount; ++p) {
      auto const &v = vertices[row + p].position;
      center += QVector3D(v[0], v[1], v[2]);
    }
    center /= static_cast<float>(kRingVertexCount);
    unsigned int const ci = static_cast<unsigned int>(vertices.size());
    vertices.push_back(make_torso_vertex(center, normal, 0.5F, 0.5F));
    for (unsigned int p = 0; p < kRingVertexCount; ++p) {
      unsigned int const a = row + p;
      unsigned int const b = row + ((p + 1U) % kRingVertexCount);
      if (reverse) {
        indices.push_back(ci);
        indices.push_back(b);
        indices.push_back(a);
      } else {
        indices.push_back(ci);
        indices.push_back(a);
        indices.push_back(b);
      }
    }
  };

  add_cap(0U, QVector3D(0.0F, 0.0F, -1.0F), true);
  add_cap(static_cast<unsigned int>((rings.size() - 1U) * kRingVertexCount),
          QVector3D(0.0F, 0.0F, 1.0F), false);

  return std::make_unique<Render::GL::Mesh>(vertices, indices);
}

auto get_faceted_horse_torso_mesh() -> Render::GL::Mesh * {
  static std::unique_ptr<Render::GL::Mesh> const mesh =
      create_faceted_horse_torso_mesh();
  return mesh.get();
}

[[nodiscard]] auto make_primitive_instance(
    const primitive_instance_desc &desc) noexcept -> PrimitiveInstance {
  PrimitiveInstance primitive{};
  primitive.debug_name = desc.debug_name;
  primitive.shape = desc.shape;
  primitive.params.anchor_bone = desc.anchor_bone;
  primitive.params.tail_bone = desc.tail_bone;
  primitive.params.head_offset = desc.head_offset;
  primitive.params.tail_offset = desc.tail_offset;
  primitive.params.half_extents = desc.half_extents;
  primitive.params.radius = desc.radius;
  primitive.custom_mesh = desc.custom_mesh;
  primitive.color_role = desc.color_role;
  primitive.material_id = desc.material_id;
  primitive.lod_mask = desc.lod_mask;
  return primitive;
}

struct reduced_anatomy_profile {
  QVector3D reduced_body_half_scale{0.68F, 0.34F, 0.42F};
  QVector3D neck_base_scale{0.0F, 0.58F, 0.52F};
  float neck_rise_scale{0.80F};
  float neck_length_scale{1.00F};
  float neck_radius_scale{0.18F};
  QVector3D head_center_scale{0.0F, -0.08F, 0.78F};
  QVector3D head_half_scale{0.18F, 0.15F, 0.27F};
  QVector3D front_anchor_scale{0.0F, -0.13F, 0.42F};
  QVector3D rear_anchor_scale{0.0F, -0.20F, -0.36F};
  float front_bias_scale{0.08F};
  float rear_bias_scale{-0.12F};
  float front_shoulder_out_scale{0.96F};
  float rear_shoulder_out_scale{0.74F};
  float front_vertical_bias_scale{-0.30F};
  float rear_vertical_bias_scale{-0.44F};
  float front_longitudinal_bias_scale{0.08F};
  float rear_longitudinal_bias_scale{-0.11F};
  float front_leg_length_scale{1.14F};
  float rear_leg_length_scale{1.12F};
  float leg_radius_scale{0.22F};
  float front_upper_radius_scale{1.70F};
  float rear_upper_radius_scale{1.85F};
  float front_lower_radius_scale{0.78F};
  float rear_lower_radius_scale{0.76F};
  float front_upper_joint_height_scale{0.66F};
  float rear_upper_joint_height_scale{0.60F};
  float front_joint_z_scale{0.12F};
  float rear_joint_z_scale{-0.48F};

  QVector3D body_offset_scale{0.0F, 0.03F, -0.02F};
  QVector3D body_half_extents_scale{0.50F, 0.22F, 0.44F};
  QVector3D chest_offset_scale{0.0F, 0.03F, 0.50F};
  QVector3D chest_half_extents_scale{0.36F, 0.32F, 0.14F};
  QVector3D belly_offset_scale{0.0F, -0.18F, -0.04F};
  QVector3D belly_half_extents_scale{0.36F, 0.08F, 0.34F};
  QVector3D rump_offset_scale{0.0F, 0.07F, -0.42F};
  QVector3D rump_half_extents_scale{0.46F, 0.28F, 0.24F};

  QVector3D withers_offset_scale{0.0F, 0.58F, 0.18F};
  QVector3D withers_tail_offset_scale{0.0F, 0.34F, 0.18F};
  float withers_radius_scale{0.12F};
  QVector3D cranium_offset_scale{0.0F, 0.04F, -0.18F};
  QVector3D cranium_half_extents_scale{0.22F, 0.18F, 0.26F};
  QVector3D muzzle_head_scale{0.0F, -0.06F, 0.12F};
  QVector3D muzzle_tail_scale{0.0F, -0.16F, 0.80F};
  float muzzle_radius_scale{0.10F};
  QVector3D jaw_offset_scale{0.0F, -0.20F, 0.20F};
  QVector3D jaw_half_extents_scale{0.10F, 0.05F, 0.20F};
  QVector3D ear_head_scale{0.10F, 0.12F, -0.28F};
  QVector3D ear_tail_scale{0.11F, 0.34F, -0.36F};
  float ear_radius_scale{0.05F};
  QVector3D tail_head_scale{0.0F, 0.16F, -0.42F};
  QVector3D tail_tail_scale{0.0F, -0.36F, -0.56F};
  float tail_radius_scale{0.20F};
  QVector3D hoof_scale{0.30F, 0.88F, 0.50F};
};

[[nodiscard]] auto
make_reduced_anatomy_profile() noexcept -> reduced_anatomy_profile {
  return {};
}

} // namespace

auto horse_topology() noexcept -> const SkeletonTopology & {
  return kHorseTopology;
}

void evaluate_horse_skeleton(const HorseSpecPose &pose,
                             BonePalette &out_palette) noexcept {
  out_palette[static_cast<std::size_t>(HorseBone::Root)] =
      translation_matrix(pose.barrel_center);

  QMatrix4x4 body;
  body.setColumn(0, QVector4D(pose.body_ellipsoid_x, 0.0F, 0.0F, 0.0F));
  body.setColumn(1, QVector4D(0.0F, pose.body_ellipsoid_y, 0.0F, 0.0F));
  body.setColumn(2, QVector4D(0.0F, 0.0F, pose.body_ellipsoid_z, 0.0F));
  body.setColumn(3, QVector4D(pose.barrel_center, 1.0F));
  out_palette[static_cast<std::size_t>(HorseBone::Body)] = body;

  out_palette[static_cast<std::size_t>(HorseBone::FootFL)] =
      translation_matrix(pose.foot_fl);
  out_palette[static_cast<std::size_t>(HorseBone::FootFR)] =
      translation_matrix(pose.foot_fr);
  out_palette[static_cast<std::size_t>(HorseBone::FootBL)] =
      translation_matrix(pose.foot_bl);
  out_palette[static_cast<std::size_t>(HorseBone::FootBR)] =
      translation_matrix(pose.foot_br);

  out_palette[static_cast<std::size_t>(HorseBone::NeckTop)] =
      translation_matrix(pose.neck_top);
  out_palette[static_cast<std::size_t>(HorseBone::Head)] =
      translation_matrix(pose.head_center);
}

void fill_horse_role_colors(const Render::GL::HorseVariant &variant,
                            std::array<QVector3D, 8> &out_roles) noexcept {
  QVector3D const coat = variant.coat_color;
  out_roles[0] = coat;
  out_roles[1] = coat * 0.62F + QVector3D(0.080F, 0.068F, 0.050F);
  out_roles[2] = coat * 0.70F + QVector3D(0.060F, 0.050F, 0.040F);
  out_roles[3] = variant.hoof_color;
  out_roles[4] = variant.mane_color;
  out_roles[5] = variant.tail_color;
  out_roles[6] = variant.muzzle_color;
  out_roles[7] = QVector3D(0.04F, 0.03F, 0.03F);
}

void make_horse_spec_pose(const Render::GL::HorseDimensions &dims, float bob,
                          HorseSpecPose &out_pose) noexcept {
  QVector3D const center(0.0F, dims.barrel_center_y + bob, 0.0F);
  out_pose.barrel_center = center;

  out_pose.body_ellipsoid_x = dims.body_width * 1.52F;
  out_pose.body_ellipsoid_y = dims.body_height * 0.56F + dims.neck_rise * 0.05F;
  out_pose.body_ellipsoid_z =
      dims.body_length * 1.02F + dims.head_length * 0.12F;

  float const front_shoulder_dx = dims.body_width * 0.92F;
  float const rear_hip_dx = dims.body_width * 0.62F;
  float const front_shoulder_dy = -dims.body_height * 0.10F;
  float const rear_hip_dy = -dims.body_height * 0.20F;
  float const front_dz = dims.body_length * 0.48F;
  float const rear_dz = -dims.body_length * 0.42F;

  out_pose.shoulder_offset_fl =
      QVector3D(front_shoulder_dx, front_shoulder_dy, front_dz);
  out_pose.shoulder_offset_fr =
      QVector3D(-front_shoulder_dx, front_shoulder_dy, front_dz);
  out_pose.shoulder_offset_bl = QVector3D(rear_hip_dx, rear_hip_dy, rear_dz);
  out_pose.shoulder_offset_br = QVector3D(-rear_hip_dx, rear_hip_dy, rear_dz);

  float const front_drop = -dims.leg_length * 0.80F;
  float const rear_drop = -dims.leg_length * 0.79F;
  out_pose.foot_fl = center + out_pose.shoulder_offset_fl +
                     QVector3D(0.0F, front_drop, dims.body_length * 0.03F);
  out_pose.foot_fr = center + out_pose.shoulder_offset_fr +
                     QVector3D(0.0F, front_drop, dims.body_length * 0.03F);
  out_pose.foot_bl = center + out_pose.shoulder_offset_bl +
                     QVector3D(0.0F, rear_drop, -dims.body_length * 0.10F);
  out_pose.foot_br = center + out_pose.shoulder_offset_br +
                     QVector3D(0.0F, rear_drop, -dims.body_length * 0.10F);

  out_pose.leg_radius = dims.body_width * 0.108F;
}

namespace {

struct reduced_leg_sample {
  QVector3D shoulder;
  QVector3D foot;
};

auto compute_reduced_leg(
    const Render::GL::HorseDimensions &dims, const Render::GL::HorseGait &gait,
    const reduced_anatomy_profile &profile, float phase_base,
    float phase_offset, float lateral_sign, bool is_rear, float forward_bias,
    bool is_moving,
    const QVector3D &anchor_offset) noexcept -> reduced_leg_sample {
  auto ease_in_out = [](float t) {
    t = std::clamp(t, 0.0F, 1.0F);
    return t * t * (3.0F - 2.0F * t);
  };
  float const leg_phase = std::fmod(phase_base + phase_offset, 1.0F);
  float stride = 0.0F;
  float lift = 0.0F;
  float shoulder_compress = 0.0F;
  if (is_moving) {
    float const stance_fraction =
        gait.cycle_time > 0.9F ? 0.60F
                               : (gait.cycle_time > 0.5F ? 0.44F : 0.34F);
    float const stride_extent = gait.stride_swing * 0.56F;
    if (leg_phase < stance_fraction) {
      float const t = ease_in_out(leg_phase / stance_fraction);
      stride = forward_bias + stride_extent * (0.40F - 1.00F * t);
      shoulder_compress =
          std::sin(t * std::numbers::pi_v<float>) * gait.stride_lift * 0.08F;
    } else {
      float const t =
          ease_in_out((leg_phase - stance_fraction) / (1.0F - stance_fraction));
      stride = forward_bias - stride_extent * 0.60F + stride_extent * 1.00F * t;
      lift = std::sin(t * std::numbers::pi_v<float>) * gait.stride_lift * 0.84F;
    }
  }

  float const shoulder_out =
      dims.body_width * (is_rear ? profile.rear_shoulder_out_scale
                                 : profile.front_shoulder_out_scale);

  float const vertical_bias =
      dims.body_height * (is_rear ? profile.rear_vertical_bias_scale
                                  : profile.front_vertical_bias_scale);
  float const longitudinal_bias =
      dims.body_length * (is_rear ? profile.rear_longitudinal_bias_scale
                                  : profile.front_longitudinal_bias_scale);
  QVector3D const shoulder =
      anchor_offset +
      QVector3D(lateral_sign * shoulder_out,
                vertical_bias + lift * 0.05F - shoulder_compress,
                stride + longitudinal_bias);

  float const leg_length =
      dims.leg_length * (is_rear ? profile.rear_leg_length_scale
                                 : profile.front_leg_length_scale);
  QVector3D const foot =
      shoulder + QVector3D(0.0F, -leg_length + lift,
                           dims.body_length * (is_rear ? -0.02F : 0.01F));

  return {shoulder, foot};
}

} // namespace

void make_horse_spec_pose_reduced(const Render::GL::HorseDimensions &dims,
                                  const Render::GL::HorseGait &gait,
                                  HorseReducedMotion motion,
                                  HorseSpecPose &out_pose) noexcept {

  make_horse_spec_pose(dims, motion.bob, out_pose);
  reduced_anatomy_profile const profile = make_reduced_anatomy_profile();

  QVector3D const center = out_pose.barrel_center;

  out_pose.reduced_body_half =
      QVector3D(dims.body_width * profile.reduced_body_half_scale.x(),
                dims.body_height * profile.reduced_body_half_scale.y(),
                dims.body_length * profile.reduced_body_half_scale.z());

  float const head_height_scale = 1.0F + gait.head_height_jitter;
  out_pose.neck_base =
      center + QVector3D(dims.body_width * profile.neck_base_scale.x(),
                         dims.body_height * profile.neck_base_scale.y() *
                             head_height_scale,
                         dims.body_length * profile.neck_base_scale.z());
  out_pose.neck_top =
      out_pose.neck_base +
      QVector3D(0.0F,
                dims.neck_rise * profile.neck_rise_scale * head_height_scale,
                dims.neck_length * profile.neck_length_scale);
  out_pose.neck_radius = dims.body_width * profile.neck_radius_scale;

  out_pose.head_center =
      out_pose.neck_top +
      QVector3D(dims.head_width * profile.head_center_scale.x(),
                dims.head_height * profile.head_center_scale.y(),
                dims.head_length * profile.head_center_scale.z());
  out_pose.head_half =
      QVector3D(dims.head_width * profile.head_half_scale.x(),
                dims.head_height * profile.head_half_scale.y(),
                dims.head_length * profile.head_half_scale.z());

  QVector3D const front_anchor =
      QVector3D(dims.body_width * profile.front_anchor_scale.x(),
                dims.body_height * profile.front_anchor_scale.y(),
                dims.body_length * profile.front_anchor_scale.z());
  QVector3D const rear_anchor =
      QVector3D(dims.body_width * profile.rear_anchor_scale.x(),
                dims.body_height * profile.rear_anchor_scale.y(),
                dims.body_length * profile.rear_anchor_scale.z());

  float const front_bias = dims.body_length * profile.front_bias_scale;
  float const rear_bias = dims.body_length * profile.rear_bias_scale;

  Render::GL::HorseGait jittered = gait;
  jittered.stride_swing *= (1.0F + gait.stride_jitter);
  float const front_lat =
      gait.lateral_lead_front == 0.0F ? 0.44F : gait.lateral_lead_front;
  float const rear_lat =
      gait.lateral_lead_rear == 0.0F ? 0.50F : gait.lateral_lead_rear;

  auto fl = compute_reduced_leg(
      dims, jittered, profile, motion.phase, gait.front_leg_phase, 1.0F, false,
      front_bias, motion.is_moving,
      front_anchor + QVector3D(dims.body_width * 0.10F, 0.0F, 0.0F));
  auto fr = compute_reduced_leg(
      dims, jittered, profile, motion.phase, gait.front_leg_phase + front_lat,
      -1.0F, false, front_bias, motion.is_moving,
      front_anchor + QVector3D(-dims.body_width * 0.10F, 0.0F, 0.0F));
  auto bl = compute_reduced_leg(
      dims, jittered, profile, motion.phase, gait.rear_leg_phase, 1.0F, true,
      rear_bias, motion.is_moving,
      rear_anchor + QVector3D(-dims.body_width * 0.10F, 0.0F, 0.0F));
  auto br = compute_reduced_leg(
      dims, jittered, profile, motion.phase, gait.rear_leg_phase + rear_lat,
      -1.0F, true, rear_bias, motion.is_moving,
      rear_anchor + QVector3D(dims.body_width * 0.10F, 0.0F, 0.0F));

  out_pose.shoulder_offset_reduced_fl = fl.shoulder;
  out_pose.shoulder_offset_reduced_fr = fr.shoulder;
  out_pose.shoulder_offset_reduced_bl = bl.shoulder;
  out_pose.shoulder_offset_reduced_br = br.shoulder;

  out_pose.foot_fl = center + fl.foot;
  out_pose.foot_fr = center + fr.foot;
  out_pose.foot_bl = center + bl.foot;
  out_pose.foot_br = center + br.foot;

  out_pose.leg_radius_reduced = dims.body_width * profile.leg_radius_scale;

  out_pose.hoof_scale = QVector3D(dims.body_width * profile.hoof_scale.x(),
                                  dims.hoof_height * profile.hoof_scale.y(),
                                  dims.body_width * profile.hoof_scale.z());
}

namespace {

auto build_minimal_primitives(const HorseSpecPose &pose,
                              std::array<PrimitiveInstance, 5> &out) noexcept
    -> std::size_t {
  {
    PrimitiveInstance &p = out[0];
    p.debug_name = "horse.body";
    p.shape = PrimitiveShape::Mesh;
    p.custom_mesh = Render::GL::get_unit_sphere();
    p.params.anchor_bone = static_cast<BoneIndex>(HorseBone::Body);
    p.params.half_extents = QVector3D(0.82F, 1.08F, 0.92F);
    p.color_role = kRoleCoat;
    p.material_id = 6;
    p.lod_mask = kLodMinimal;
  }

  struct minimal_leg_desc {
    std::string_view name;
    HorseBone foot_bone;
    QVector3D shoulder_offset;
  };
  std::array<minimal_leg_desc, 4> const legs{{
      {"horse.leg.fl", HorseBone::FootFL, pose.shoulder_offset_fl},
      {"horse.leg.fr", HorseBone::FootFR, pose.shoulder_offset_fr},
      {"horse.leg.bl", HorseBone::FootBL, pose.shoulder_offset_bl},
      {"horse.leg.br", HorseBone::FootBR, pose.shoulder_offset_br},
  }};

  for (std::size_t i = 0; i < 4; ++i) {
    PrimitiveInstance &p = out[1 + i];
    p.debug_name = legs[i].name;
    p.shape = PrimitiveShape::Cylinder;
    p.params.anchor_bone = static_cast<BoneIndex>(HorseBone::Root);
    p.params.head_offset = legs[i].shoulder_offset;
    p.params.tail_bone = static_cast<BoneIndex>(legs[i].foot_bone);
    p.params.tail_offset = QVector3D();
    p.params.radius = pose.leg_radius * 1.08F;
    p.color_role = kRoleCoatReducedLeg;
    p.material_id = 6;
    p.lod_mask = kLodMinimal;
  }
  return 5;
}

constexpr std::size_t kHorseReducedPartCount = 29;
constexpr std::size_t kHorseReducedLegStart = 13;

auto build_reduced_primitives(
    const HorseSpecPose &pose, const Render::GL::HorseDimensions &dims,
    std::array<PrimitiveInstance, kHorseReducedPartCount> &out) noexcept
    -> std::size_t {

  reduced_anatomy_profile const profile = make_reduced_anatomy_profile();
  float const bh = dims.body_height;
  float const bl = dims.body_length;
  float const bw = dims.body_width;
  BoneIndex const root_bone = static_cast<BoneIndex>(HorseBone::Root);
  BoneIndex const neck_top_bone = static_cast<BoneIndex>(HorseBone::NeckTop);
  BoneIndex const head_bone = static_cast<BoneIndex>(HorseBone::Head);

  std::array<primitive_instance_desc,
             kHorseReducedLegStart> const torso_primitives{{
      {"horse.body.ribcage.reduced", PrimitiveShape::Mesh, root_bone, root_bone,
       QVector3D(bw * profile.body_offset_scale.x(),
                 bh * profile.body_offset_scale.y(),
                 bl * profile.body_offset_scale.z()),
       QVector3D(), QVector3D(1.0F, 1.0F, 1.0F), 0.0F, kRoleCoat, 6,
       kLodReduced, get_faceted_horse_torso_mesh()},
      {"horse.body.chest.reduced", PrimitiveShape::OrientedSphere, root_bone,
       root_bone,
       QVector3D(bw * profile.chest_offset_scale.x(),
                 bh * profile.chest_offset_scale.y(),
                 bl * profile.chest_offset_scale.z()),
       QVector3D(),
       QVector3D(bw * profile.chest_half_extents_scale.x(),
                 bh * profile.chest_half_extents_scale.y(),
                 bl * profile.chest_half_extents_scale.z()),
       0.0F, kRoleCoat, 6, kLodReduced, nullptr},
      {"horse.body.pelvis.reduced", PrimitiveShape::OrientedSphere, root_bone,
       root_bone,
       QVector3D(bw * profile.rump_offset_scale.x(),
                 bh * profile.rump_offset_scale.y(),
                 bl * profile.rump_offset_scale.z()),
       QVector3D(),
       QVector3D(bw * profile.rump_half_extents_scale.x(),
                 bh * profile.rump_half_extents_scale.y(),
                 bl * profile.rump_half_extents_scale.z()),
       0.0F, kRoleCoat, 6, kLodReduced, nullptr},
      {"horse.body.belly.reduced", PrimitiveShape::OrientedSphere, root_bone,
       root_bone,
       QVector3D(bw * profile.belly_offset_scale.x(),
                 bh * profile.belly_offset_scale.y(),
                 bl * profile.belly_offset_scale.z()),
       QVector3D(),
       QVector3D(bw * profile.belly_half_extents_scale.x(),
                 bh * profile.belly_half_extents_scale.y(),
                 bl * profile.belly_half_extents_scale.z()),
       0.0F, kRoleCoatDark, 6, kLodReduced, nullptr},
      {"horse.withers.reduced", PrimitiveShape::Cone, root_bone, root_bone,
       QVector3D(bw * profile.withers_tail_offset_scale.x(),
                 bh * profile.withers_tail_offset_scale.y(),
                 bl * profile.withers_tail_offset_scale.z()),
       QVector3D(bw * profile.withers_offset_scale.x(),
                 bh * profile.withers_offset_scale.y(),
                 bl * profile.withers_offset_scale.z()),
       QVector3D(), bw * profile.withers_radius_scale, kRoleCoatDark, 6,
       kLodReduced, nullptr},
      {"horse.neck.reduced", PrimitiveShape::Capsule, root_bone, neck_top_bone,
       pose.neck_base - pose.barrel_center, QVector3D(), QVector3D(),
       pose.neck_radius * 0.92F, kRoleCoat, 0, kLodReduced, nullptr},
      {"horse.mane.reduced", PrimitiveShape::Capsule, root_bone, neck_top_bone,
       pose.neck_base - pose.barrel_center +
           QVector3D(bw * 0.03F, pose.neck_radius * 0.88F,
                     -pose.head_half.z() * 0.20F),
       QVector3D(bw * 0.02F, pose.head_half.y() * 0.30F,
                 -pose.head_half.z() * 0.20F),
       QVector3D(), pose.neck_radius * 0.30F, kRoleMane, 0, kLodReduced,
       nullptr},
      {"horse.head.cranium.reduced", PrimitiveShape::Box, head_bone, head_bone,
       QVector3D(dims.head_width * profile.cranium_offset_scale.x(),
                 dims.head_height * profile.cranium_offset_scale.y(),
                 dims.head_length * profile.cranium_offset_scale.z()),
       QVector3D(),
       QVector3D(dims.head_width * profile.cranium_half_extents_scale.x(),
                 dims.head_height * profile.cranium_half_extents_scale.y(),
                 dims.head_length * profile.cranium_half_extents_scale.z()),
       0.0F, kRoleCoat, 6, kLodReduced, nullptr},
      {"horse.head.muzzle.reduced", PrimitiveShape::Cone, head_bone, head_bone,
       QVector3D(dims.head_width * profile.muzzle_head_scale.x(),
                 dims.head_height * profile.muzzle_head_scale.y(),
                 dims.head_length * profile.muzzle_head_scale.z()),
       QVector3D(dims.head_width * profile.muzzle_tail_scale.x(),
                 dims.head_height * profile.muzzle_tail_scale.y(),
                 dims.head_length * profile.muzzle_tail_scale.z()),
       QVector3D(), dims.head_width * profile.muzzle_radius_scale, kRoleMuzzle,
       0, kLodReduced, nullptr},
      {"horse.head.jaw.reduced", PrimitiveShape::Box, head_bone, head_bone,
       QVector3D(dims.head_width * profile.jaw_offset_scale.x(),
                 dims.head_height * profile.jaw_offset_scale.y(),
                 dims.head_length * profile.jaw_offset_scale.z()),
       QVector3D(),
       QVector3D(dims.head_width * profile.jaw_half_extents_scale.x(),
                 dims.head_height * profile.jaw_half_extents_scale.y(),
                 dims.head_length * profile.jaw_half_extents_scale.z()),
       0.0F, kRoleMuzzle, 0, kLodReduced, nullptr},
      {"horse.head.ear.l.reduced", PrimitiveShape::Cone, head_bone, head_bone,
       QVector3D(dims.head_width * profile.ear_head_scale.x(),
                 dims.head_height * profile.ear_head_scale.y(),
                 dims.head_length * profile.ear_head_scale.z()),
       QVector3D(dims.head_width * profile.ear_tail_scale.x(),
                 dims.head_height * profile.ear_tail_scale.y(),
                 dims.head_length * profile.ear_tail_scale.z()),
       QVector3D(), dims.head_width * profile.ear_radius_scale, kRoleCoat, 0,
       kLodReduced, nullptr},
      {"horse.head.ear.r.reduced", PrimitiveShape::Cone, head_bone, head_bone,
       QVector3D(-dims.head_width * profile.ear_head_scale.x(),
                 dims.head_height * profile.ear_head_scale.y(),
                 dims.head_length * profile.ear_head_scale.z()),
       QVector3D(-dims.head_width * profile.ear_tail_scale.x(),
                 dims.head_height * profile.ear_tail_scale.y(),
                 dims.head_length * profile.ear_tail_scale.z()),
       QVector3D(), dims.head_width * profile.ear_radius_scale, kRoleCoat, 0,
       kLodReduced, nullptr},
      {"horse.tail.reduced", PrimitiveShape::Capsule, root_bone, root_bone,
       QVector3D(bw * profile.tail_head_scale.x(),
                 bh * profile.tail_head_scale.y(),
                 bl * profile.tail_head_scale.z()),
       QVector3D(bw * profile.tail_tail_scale.x(),
                 bh * profile.tail_tail_scale.y(),
                 bl * profile.tail_tail_scale.z()),
       QVector3D(), bw * profile.tail_radius_scale * 0.375F, kRoleTail, 0,
       kLodReduced, nullptr},
  }};
  for (std::size_t torso_index = 0; torso_index < torso_primitives.size();
       ++torso_index) {
    out[torso_index] = make_primitive_instance(torso_primitives[torso_index]);
  }

  struct reduced_leg_desc {
    std::string_view upper_name;
    std::string_view joint_name;
    std::string_view lower_name;
    std::string_view hoof_name;
    HorseBone foot_bone;
    QVector3D shoulder_offset;
    bool is_rear;
  };
  std::array<reduced_leg_desc, 4> const legs{{
      {"horse.leg.fl.upper.r", "horse.leg.fl.knee.r", "horse.leg.fl.lower.r",
       "horse.hoof.fl.r", HorseBone::FootFL, pose.shoulder_offset_reduced_fl,
       false},
      {"horse.leg.fr.upper.r", "horse.leg.fr.knee.r", "horse.leg.fr.lower.r",
       "horse.hoof.fr.r", HorseBone::FootFR, pose.shoulder_offset_reduced_fr,
       false},
      {"horse.leg.bl.upper.r", "horse.leg.bl.hock.r", "horse.leg.bl.lower.r",
       "horse.hoof.bl.r", HorseBone::FootBL, pose.shoulder_offset_reduced_bl,
       true},
      {"horse.leg.br.upper.r", "horse.leg.br.hock.r", "horse.leg.br.lower.r",
       "horse.hoof.br.r", HorseBone::FootBR, pose.shoulder_offset_reduced_br,
       true},
  }};
  for (std::size_t i = 0; i < 4; ++i) {
    BoneIndex const foot_bone = static_cast<BoneIndex>(legs[i].foot_bone);

    float const upper_radius =
        pose.leg_radius_reduced * (legs[i].is_rear
                                       ? profile.rear_upper_radius_scale
                                       : profile.front_upper_radius_scale);
    float const lower_radius =
        pose.leg_radius_reduced * (legs[i].is_rear
                                       ? profile.rear_lower_radius_scale
                                       : profile.front_lower_radius_scale);

    float const lateral_sign =
        legs[i].shoulder_offset.x() >= 0.0F ? 1.0F : -1.0F;
    QVector3D const upper_tail =
        legs[i].is_rear
            ? QVector3D(lateral_sign * pose.reduced_body_half.x() * 0.06F,
                        dims.leg_length * profile.rear_upper_joint_height_scale,
                        pose.reduced_body_half.z() * profile.rear_joint_z_scale)
            : QVector3D(
                  lateral_sign * pose.reduced_body_half.x() * 0.04F,
                  dims.leg_length * profile.front_upper_joint_height_scale,
                  pose.reduced_body_half.z() * profile.front_joint_z_scale);
    QVector3D const lower_head =
        legs[i].is_rear
            ? QVector3D(lateral_sign * pose.reduced_body_half.x() * 0.05F,
                        dims.leg_length * profile.rear_upper_joint_height_scale,
                        pose.reduced_body_half.z() *
                            (profile.rear_joint_z_scale * 0.78F))
            : QVector3D(lateral_sign * pose.reduced_body_half.x() * 0.03F,
                        dims.leg_length *
                            profile.front_upper_joint_height_scale,
                        pose.reduced_body_half.z() *
                            (profile.front_joint_z_scale * 0.86F));
    QVector3D const lower_tail =
        QVector3D(lateral_sign * pose.reduced_body_half.x() * 0.06F,
                  pose.hoof_scale.y() * 0.82F,
                  legs[i].is_rear ? -pose.reduced_body_half.z() * 0.03F
                                  : pose.reduced_body_half.z() * 0.03F);

    out[kHorseReducedLegStart + (i * 4)] = make_primitive_instance(
        {legs[i].upper_name, PrimitiveShape::Cone, root_bone, foot_bone,
         legs[i].shoulder_offset, upper_tail, QVector3D(), upper_radius,
         kRoleCoatReducedLeg, 0, kLodReduced, nullptr});
    out[kHorseReducedLegStart + 1 + (i * 4)] = make_primitive_instance(
        {legs[i].joint_name, PrimitiveShape::Sphere, foot_bone, foot_bone,
         upper_tail, QVector3D(), QVector3D(),
         pose.leg_radius_reduced * (legs[i].is_rear ? 1.55F : 1.40F),
         kRoleCoatReducedLeg, 0, kLodReduced, nullptr});
    out[kHorseReducedLegStart + 2 + (i * 4)] = make_primitive_instance(
        {legs[i].lower_name, PrimitiveShape::Cylinder, foot_bone, foot_bone,
         lower_head, lower_tail, QVector3D(), lower_radius, kRoleCoatDark, 0,
         kLodReduced, nullptr});
    out[kHorseReducedLegStart + 3 + (i * 4)] = make_primitive_instance(
        {legs[i].hoof_name, PrimitiveShape::Mesh, foot_bone, foot_bone,
         QVector3D(), QVector3D(),
         QVector3D(pose.hoof_scale.x(), pose.hoof_scale.y(),
                   pose.hoof_scale.z() * 0.94F),
         0.0F, kRoleHoof, 8, kLodReduced, Render::GL::get_unit_cylinder()});
  }

  return kHorseReducedPartCount;
}

} // namespace

} // namespace Render::Horse

#include "../bone_palette_arena.h"
#include "../rigged_mesh_cache.h"
#include "../scene_renderer.h"

namespace Render::Horse {

namespace {

auto build_baseline_pose() noexcept -> HorseSpecPose {
  Render::GL::HorseDimensions const dims =
      Render::GL::make_horse_dimensions(0U);
  Render::GL::HorseGait gait{};
  HorseSpecPose pose{};
  make_horse_spec_pose_reduced(dims, gait, HorseReducedMotion{}, pose);
  return pose;
}

auto baseline_pose() noexcept -> const HorseSpecPose & {
  static const HorseSpecPose pose = build_baseline_pose();
  return pose;
}

auto build_static_reduced_parts() noexcept
    -> std::array<PrimitiveInstance, kHorseReducedPartCount> {
  Render::GL::HorseDimensions const dims =
      Render::GL::make_horse_dimensions(0U);
  std::array<PrimitiveInstance, kHorseReducedPartCount> out{};
  build_reduced_primitives(baseline_pose(), dims, out);
  return out;
}

auto static_reduced_parts() noexcept
    -> const std::array<PrimitiveInstance, kHorseReducedPartCount> & {
  static const auto parts = build_static_reduced_parts();
  return parts;
}

auto build_static_minimal_parts() noexcept -> std::array<PrimitiveInstance, 5> {
  std::array<PrimitiveInstance, 5> out{};
  build_minimal_primitives(baseline_pose(), out);
  return out;
}

auto static_minimal_parts() noexcept
    -> const std::array<PrimitiveInstance, 5> & {
  static const auto parts = build_static_minimal_parts();
  return parts;
}

constexpr std::size_t kHorseFullPartCount = 43;

auto build_static_full_parts() noexcept
    -> std::array<PrimitiveInstance, kHorseFullPartCount> {
  HorseSpecPose const &pose = baseline_pose();
  Render::GL::HorseDimensions const dims =
      Render::GL::make_horse_dimensions(0U);
  std::array<PrimitiveInstance, kHorseFullPartCount> out{};

  using Render::Creature::kLodFull;

  auto root = static_cast<BoneIndex>(HorseBone::Root);
  auto head_bone = static_cast<BoneIndex>(HorseBone::Head);
  auto neck_top_bone = static_cast<BoneIndex>(HorseBone::NeckTop);

  auto ell = [&](PrimitiveInstance &p, std::string_view name, BoneIndex anchor,
                 const QVector3D &half_extents, const QVector3D &offset,
                 std::uint8_t role = kRoleCoat, int material_id = 6) {
    p.debug_name = name;
    p.shape = PrimitiveShape::OrientedSphere;
    p.params.anchor_bone = anchor;
    p.params.head_offset = offset;
    p.params.half_extents = half_extents;
    p.color_role = role;
    p.material_id = material_id;
    p.lod_mask = kLodFull;
  };

  auto box = [&](PrimitiveInstance &p, std::string_view name, BoneIndex anchor,
                 const QVector3D &half_extents, const QVector3D &offset,
                 std::uint8_t role = kRoleCoat, int material_id = 6) {
    p.debug_name = name;
    p.shape = PrimitiveShape::Box;
    p.params.anchor_bone = anchor;
    p.params.head_offset = offset;
    p.params.half_extents = half_extents;
    p.color_role = role;
    p.material_id = material_id;
    p.lod_mask = kLodFull;
  };

  auto sph = [&](PrimitiveInstance &p, std::string_view name, BoneIndex anchor,
                 const QVector3D &offset, float radius, std::uint8_t role,
                 int material_id = 0) {
    p.debug_name = name;
    p.shape = PrimitiveShape::Sphere;
    p.params.anchor_bone = anchor;
    p.params.head_offset = offset;
    p.params.radius = radius;
    p.color_role = role;
    p.material_id = material_id;
    p.lod_mask = kLodFull;
  };

  auto cyl = [&](PrimitiveInstance &p, std::string_view name, BoneIndex anchor,
                 const QVector3D &head_off, BoneIndex tail,
                 const QVector3D &tail_off, float radius, std::uint8_t role,
                 int material_id = 0) {
    p.debug_name = name;
    p.shape = PrimitiveShape::Cylinder;
    p.params.anchor_bone = anchor;
    p.params.head_offset = head_off;
    p.params.tail_bone = tail;
    p.params.tail_offset = tail_off;
    p.params.radius = radius;
    p.color_role = role;
    p.material_id = material_id;
    p.lod_mask = kLodFull;
  };

  auto cap = [&](PrimitiveInstance &p, std::string_view name, BoneIndex anchor,
                 const QVector3D &head_off, BoneIndex tail,
                 const QVector3D &tail_off, float radius, std::uint8_t role,
                 int material_id = 0) {
    p.debug_name = name;
    p.shape = PrimitiveShape::Capsule;
    p.params.anchor_bone = anchor;
    p.params.head_offset = head_off;
    p.params.tail_bone = tail;
    p.params.tail_offset = tail_off;
    p.params.radius = radius;
    p.color_role = role;
    p.material_id = material_id;
    p.lod_mask = kLodFull;
  };

  auto cone_p = [&](PrimitiveInstance &p, std::string_view name,
                    BoneIndex anchor, const QVector3D &head_off, BoneIndex tail,
                    const QVector3D &tail_off, float radius, std::uint8_t role,
                    int material_id = 0) {
    p.debug_name = name;
    p.shape = PrimitiveShape::Cone;
    p.params.anchor_bone = anchor;
    p.params.head_offset = head_off;
    p.params.tail_bone = tail;
    p.params.tail_offset = tail_off;
    p.params.radius = radius;
    p.color_role = role;
    p.material_id = material_id;
    p.lod_mask = kLodFull;
  };

  float const bw = dims.body_width;
  float const bh = dims.body_height;
  float const bl = dims.body_length;
  float const hl = dims.head_length;
  float const bw_body = bw * kHorseBodyVisualWidthScale;
  float const bh_body = bh * kHorseBodyVisualHeightScale;

  float const bw_v = bw_body * 0.98F;
  float const bh_v = bh_body * 0.95F;

  std::size_t i = 0;

  PrimitiveInstance &ribcage = out[i++];
  ribcage.debug_name = "horse.full.ribcage";
  ribcage.shape = PrimitiveShape::Mesh;
  ribcage.params.anchor_bone = root;
  ribcage.params.head_offset = QVector3D(0.0F, 0.0F, 0.0F);
  ribcage.params.half_extents = QVector3D(1.0F, 1.0F, 1.0F);
  ribcage.custom_mesh = get_faceted_horse_torso_mesh();
  ribcage.color_role = kRoleCoat;
  ribcage.material_id = 6;
  ribcage.lod_mask = kLodFull;
  cone_p(out[i++], "horse.full.withers", root,
         QVector3D(0.0F, bh_body * 0.34F, bl * 0.14F), root,
         QVector3D(0.0F, bh_body * 0.64F, bl * 0.10F), bw_body * 0.09F,
         kRoleCoatDark, 6);
  ell(out[i++], "horse.full.belly", root,
      QVector3D(bw_body * 0.26F, bh_body * 0.08F, bl * 0.27F),
      QVector3D(0.0F, -bh_body * 0.20F, -bl * 0.07F), kRoleCoatDark);
  ell(out[i++], "horse.full.chest", root,
      QVector3D(bw_body * 0.30F, bh_body * 0.25F, bl * 0.12F),
      QVector3D(0.0F, bh_body * 0.03F, bl * 0.47F));
  ell(out[i++], "horse.full.sternum", root,
      QVector3D(bw_body * 0.18F, bh_body * 0.10F, bl * 0.09F),
      QVector3D(0.0F, -bh_body * 0.30F, bl * 0.40F));
  ell(out[i++], "horse.full.croup", root,
      QVector3D(bw_body * 0.36F, bh_body * 0.19F, bl * 0.16F),
      QVector3D(0.0F, bh_body * 0.23F, -bl * 0.42F));
  ell(out[i++], "horse.full.hq.l", root,
      QVector3D(bw_body * 0.18F, bh_body * 0.34F, bl * 0.21F),
      QVector3D(bw_body * 0.22F, bh_body * 0.03F, -bl * 0.37F));
  ell(out[i++], "horse.full.hq.r", root,
      QVector3D(bw_body * 0.18F, bh_body * 0.34F, bl * 0.21F),
      QVector3D(-bw_body * 0.22F, bh_body * 0.03F, -bl * 0.37F));
  ell(out[i++], "horse.full.shoulder.l", root,
      QVector3D(bw_body * 0.10F, bh_body * 0.32F, bl * 0.14F),
      QVector3D(bw_body * 0.21F, bh_body * 0.06F, bl * 0.33F));
  ell(out[i++], "horse.full.shoulder.r", root,
      QVector3D(bw_body * 0.10F, bh_body * 0.32F, bl * 0.14F),
      QVector3D(-bw_body * 0.21F, bh_body * 0.06F, bl * 0.33F));

  cap(out[i++], "horse.full.neck.crest", root,
      pose.neck_base - pose.barrel_center, neck_top_bone,
      QVector3D(0.0F, 0.0F, 0.0F), pose.neck_radius * 1.18F, kRoleCoat);

  ell(out[i++], "horse.full.neck.throat", root,
      QVector3D(bw_v * 0.11F, bh_v * 0.12F, bl * 0.12F),
      (pose.neck_base - pose.barrel_center) +
          QVector3D(0.0F, -bh_v * 0.11F, bl * 0.07F));

  for (int m = 0; m < 5; ++m) {
    float const t = static_cast<float>(m) / 4.0F;
    QVector3D const along = (pose.neck_base - pose.barrel_center) * (1.0F - t) +
                            (pose.neck_top - pose.barrel_center) * t;
    float const size = 1.0F - 0.16F * t;
    ell(out[i++],
        m == 0   ? "horse.full.mane.0"
        : m == 1 ? "horse.full.mane.1"
        : m == 2 ? "horse.full.mane.2"
        : m == 3 ? "horse.full.mane.3"
                 : "horse.full.mane.4",
        root,
        QVector3D(bw_v * 0.04F * size, bh_v * 0.06F * size, bl * 0.045F * size),
        along + QVector3D(bw_v * 0.025F, bh_v * (0.17F - 0.06F * t),
                          -bl * 0.02F * t),
        kRoleMane, 0);
  }

  ell(out[i++], "horse.full.forelock", head_bone,
      QVector3D(dims.head_width * 0.14F, dims.head_height * 0.08F, hl * 0.07F),
      QVector3D(0.0F, dims.head_height * 0.25F, -hl * 0.20F), kRoleMane, 0);

  box(out[i++], "horse.full.head.cranium", head_bone,
      QVector3D(dims.head_width * 0.24F, dims.head_height * 0.23F, hl * 0.30F),
      QVector3D(0.0F, dims.head_height * 0.06F, -hl * 0.21F));

  cap(out[i++], "horse.full.head.muzzle", head_bone,
      QVector3D(0.0F, -dims.head_height * 0.06F, hl * 0.10F), head_bone,
      QVector3D(0.0F, -dims.head_height * 0.18F, hl * 0.76F),
      dims.head_width * 0.11F, kRoleMuzzle);

  box(out[i++], "horse.full.head.jaw", head_bone,
      QVector3D(dims.head_width * 0.13F, dims.head_height * 0.07F, hl * 0.22F),
      QVector3D(0.0F, -dims.head_height * 0.22F, hl * 0.19F), kRoleMuzzle);

  sph(out[i++], "horse.full.head.cheek.l", head_bone,
      QVector3D(dims.head_width * 0.20F, -dims.head_height * 0.03F,
                -hl * 0.04F),
      dims.head_width * 0.14F, kRoleCoat);
  sph(out[i++], "horse.full.head.cheek.r", head_bone,
      QVector3D(-dims.head_width * 0.20F, -dims.head_height * 0.03F,
                -hl * 0.04F),
      dims.head_width * 0.14F, kRoleCoat);

  cone_p(
      out[i++], "horse.full.head.ear.l", head_bone,
      QVector3D(dims.head_width * 0.10F, dims.head_height * 0.20F, -hl * 0.30F),
      head_bone,
      QVector3D(dims.head_width * 0.11F, dims.head_height * 0.34F, -hl * 0.40F),
      dims.head_width * 0.045F, kRoleCoat);
  cone_p(out[i++], "horse.full.head.ear.r", head_bone,
         QVector3D(-dims.head_width * 0.10F, dims.head_height * 0.20F,
                   -hl * 0.30F),
         head_bone,
         QVector3D(-dims.head_width * 0.11F, dims.head_height * 0.34F,
                   -hl * 0.40F),
         dims.head_width * 0.045F, kRoleCoat);

  sph(out[i++], "horse.full.head.eye.l", head_bone,
      QVector3D(dims.head_width * 0.28F, dims.head_height * 0.15F, -hl * 0.06F),
      dims.head_width * 0.08F, kRoleEye, 0);
  sph(out[i++], "horse.full.head.eye.r", head_bone,
      QVector3D(-dims.head_width * 0.28F, dims.head_height * 0.15F,
                -hl * 0.06F),
      dims.head_width * 0.08F, kRoleEye, 0);

  QVector3D const tail_root_off(0.0F, bh_v * 0.28F, -bl * 0.46F);
  ell(out[i++], "horse.full.tail.dock", root,
      QVector3D(bw_v * 0.05F, bh_v * 0.06F, bl * 0.07F), tail_root_off,
      kRoleCoat);

  for (int t_i = 0; t_i < 3; ++t_i) {
    float const tt = static_cast<float>(t_i + 1) / 3.0F;
    QVector3D const off =
        tail_root_off + QVector3D(0.0F, -bh_v * 0.22F * tt - bh_v * 0.04F,
                                  -bl * 0.08F * tt - bl * 0.02F);
    ell(out[i++],
        t_i == 0   ? "horse.full.tail.switch.0"
        : t_i == 1 ? "horse.full.tail.switch.1"
                   : "horse.full.tail.switch.2",
        root,
        QVector3D(bw_v * (0.08F - 0.02F * tt), bh_v * (0.14F + 0.04F * tt),
                  bl * 0.08F),
        off, kRoleTail, 0);
  }

  struct full_leg_desc {
    std::string_view upper_name;
    std::string_view cannon_name;
    std::string_view hoof_name;
    HorseBone foot_bone;
    QVector3D shoulder_offset;
    bool is_rear;
  };
  std::array<full_leg_desc, 4> const legs{{
      {"horse.full.leg.fl.upper", "horse.full.leg.fl.cannon",
       "horse.full.hoof.fl", HorseBone::FootFL, pose.shoulder_offset_fl, false},
      {"horse.full.leg.fr.upper", "horse.full.leg.fr.cannon",
       "horse.full.hoof.fr", HorseBone::FootFR, pose.shoulder_offset_fr, false},
      {"horse.full.leg.bl.upper", "horse.full.leg.bl.cannon",
       "horse.full.hoof.bl", HorseBone::FootBL, pose.shoulder_offset_bl, true},
      {"horse.full.leg.br.upper", "horse.full.leg.br.cannon",
       "horse.full.hoof.br", HorseBone::FootBR, pose.shoulder_offset_br, true},
  }};
  float const leg_len = dims.leg_length;

  for (auto const &leg : legs) {
    auto foot_b = static_cast<BoneIndex>(leg.foot_bone);
    float const lateral = leg.shoulder_offset.x() >= 0.0F ? 1.0F : -1.0F;

    float const upper_r =
        pose.leg_radius_reduced * (leg.is_rear ? 1.92F : 2.05F);
    float const cannon_r =
        pose.leg_radius_reduced * (leg.is_rear ? 0.62F : 0.72F);

    QVector3D const upper_tail =
        leg.is_rear
            ? QVector3D(lateral * bw_body * 0.05F, leg_len * 0.66F, -bl * 0.30F)
            : QVector3D(lateral * bw_body * 0.04F, leg_len * 0.70F, bl * 0.08F);
    QVector3D const cannon_head =
        leg.is_rear
            ? QVector3D(lateral * bw_body * 0.04F, leg_len * 0.66F, -bl * 0.25F)
            : QVector3D(lateral * bw_body * 0.03F, leg_len * 0.70F, bl * 0.06F);
    QVector3D const cannon_tail =
        QVector3D(lateral * bw_body * 0.03F, dims.hoof_height * 0.90F,
                  leg.is_rear ? -bl * 0.06F : bl * 0.06F);

    cone_p(out[i++], leg.upper_name, root, leg.shoulder_offset, foot_b,
           upper_tail, upper_r, kRoleCoat);

    cyl(out[i++], leg.cannon_name, foot_b, cannon_head, foot_b, cannon_tail,
        cannon_r, kRoleCoatDark);

    PrimitiveInstance &hoof = out[i++];
    hoof.debug_name = leg.hoof_name;
    hoof.shape = PrimitiveShape::Mesh;
    hoof.custom_mesh = Render::GL::get_unit_cylinder();
    hoof.params.anchor_bone = foot_b;
    hoof.params.half_extents = pose.hoof_scale;
    hoof.color_role = kRoleHoof;
    hoof.material_id = 8;
    hoof.lod_mask = kLodFull;
  }

  (void)i;

  return out;
}

auto static_full_parts() noexcept
    -> const std::array<PrimitiveInstance, kHorseFullPartCount> & {
  static const auto parts = build_static_full_parts();
  return parts;
}

auto build_horse_bind_palette() noexcept
    -> std::array<QMatrix4x4, kHorseBoneCount> {
  std::array<QMatrix4x4, kHorseBoneCount> out{};
  BonePalette tmp{};
  evaluate_horse_skeleton(baseline_pose(), tmp);
  for (std::size_t i = 0; i < kHorseBoneCount; ++i) {
    out[i] = tmp[i];
  }
  return out;
}

} // namespace

auto horse_creature_spec() noexcept -> const Render::Creature::CreatureSpec & {
  static const Render::Creature::CreatureSpec spec = [] {
    Render::Creature::CreatureSpec s;
    s.species_name = "horse";
    s.topology = horse_topology();
    s.lod_minimal = PartGraph{
        std::span<const PrimitiveInstance>(static_minimal_parts().data(),
                                           static_minimal_parts().size()),
    };
    s.lod_reduced = PartGraph{
        std::span<const PrimitiveInstance>(static_reduced_parts().data(),
                                           static_reduced_parts().size()),
    };
    s.lod_full = PartGraph{
        std::span<const PrimitiveInstance>(static_full_parts().data(),
                                           static_full_parts().size()),
    };
    return s;
  }();
  return spec;
}

auto compute_horse_bone_palette(const HorseSpecPose &pose,
                                std::span<QMatrix4x4> out_bones) noexcept
    -> std::uint32_t {
  if (out_bones.size() < kHorseBoneCount) {
    return 0U;
  }
  BonePalette tmp{};
  evaluate_horse_skeleton(pose, tmp);
  for (std::size_t i = 0; i < kHorseBoneCount; ++i) {
    out_bones[i] = tmp[i];
  }
  return static_cast<std::uint32_t>(kHorseBoneCount);
}

auto horse_bind_palette() noexcept -> std::span<const QMatrix4x4> {
  static const std::array<QMatrix4x4, kHorseBoneCount> palette =
      build_horse_bind_palette();
  return std::span<const QMatrix4x4>(palette.data(), palette.size());
}

} // namespace Render::Horse
