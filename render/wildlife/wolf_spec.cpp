#include "wolf_spec.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "../creature/species_manifest.h"
#include "wolf_manifest.h"

namespace Render::Wildlife {

namespace {

using Render::Creature::Quadruped::ConeNode;
using Render::Creature::Quadruped::EllipsoidNode;
using Render::Creature::Quadruped::MeshNode;
using Render::Creature::Quadruped::SnoutNode;
using Render::Creature::Quadruped::TubeNode;

constexpr float k_two_pi = 6.28318530718F;
constexpr float k_half_pi = 1.57079633F;

constexpr float k_fore_swing = 0.40F;
constexpr float k_hind_swing = 0.36F;
constexpr float k_fore_flex = 0.60F;
constexpr float k_hind_flex = 0.72F;
constexpr float k_fore_half_width = 0.076F;
constexpr float k_hind_half_width = 0.084F;

struct LegPlan {
  float x;
  float phase_offset;
  bool hind;
};

constexpr std::array<LegPlan, k_leg_count> k_leg_plans{{
    {-k_fore_half_width, 0.0F, false},
    {k_fore_half_width, 0.5F, false},
    {-k_hind_half_width, 0.5F, true},
    {k_hind_half_width, 0.0F, true},
}};

auto swung(const QVector3D& point, const QVector3D& pivot, float angle) -> QVector3D {
  float const dy = point.y() - pivot.y();
  float const dz = point.z() - pivot.z();
  float const c = std::cos(angle);
  float const s = std::sin(angle);
  return {point.x(), pivot.y() + (dy * c) + (dz * s), pivot.z() + (dz * c) - (dy * s)};
}

auto lerp(const QVector3D& a, const QVector3D& b, float t) -> QVector3D {
  return a + ((b - a) * std::clamp(t, 0.0F, 1.0F));
}

auto bezier(const QVector3D& p0,
            const QVector3D& p1,
            const QVector3D& p2,
            float t) -> QVector3D {
  float const inv = 1.0F - t;
  return (p0 * (inv * inv)) + (p1 * (2.0F * inv * t)) + (p2 * (t * t));
}

void fill_legs(RigPose& pose, const WolfDrive& drive) {
  for (std::size_t i = 0; i < k_leg_count; ++i) {
    const LegPlan& plan = k_leg_plans[i];
    float const cycle = (drive.stride_phase + plan.phase_offset) * k_two_pi;
    float const swing = plan.hind ? k_hind_swing : k_fore_swing;
    float const flex_scale = plan.hind ? k_hind_flex : k_fore_flex;
    float const angle = std::sin(cycle) * swing * drive.speed_ratio;
    float const flex =
        std::max(0.0F, std::sin(cycle + k_half_pi)) * flex_scale * drive.speed_ratio;

    QVector3D shoulder;
    QVector3D knee;
    QVector3D foot;
    QVector3D toe;
    if (plan.hind) {
      shoulder = {plan.x, 0.430F, -0.205F};
      knee = {plan.x, 0.295F, -0.126F};
      foot = {plan.x, 0.172F, -0.240F};
      toe = {plan.x, 0.034F, -0.198F};
    } else {
      shoulder = {plan.x, 0.398F, 0.168F};
      knee = {plan.x, 0.278F, 0.150F};
      foot = {plan.x, 0.164F, 0.170F};
      toe = {plan.x, 0.034F, 0.176F};
    }

    knee = swung(knee, shoulder, angle);
    foot = swung(swung(foot, shoulder, angle), knee, -flex);
    toe = swung(swung(toe, shoulder, angle), knee, -flex);

    pose.legs[i].shoulder = shoulder;
    pose.legs[i].knee = knee;
    pose.legs[i].foot = foot;
    pose.legs[i].toe = toe;
  }
}

void fill_head(RigPose& pose, const WolfDrive& drive) {
  float const lower = std::clamp(drive.crouch + (drive.lunge * 0.35F), 0.0F, 1.0F);
  QVector3D const root(0.0F, 0.600F, 0.268F);
  QVector3D const poll =
      lerp(QVector3D(0.0F, 0.648F, 0.438F), QVector3D(0.0F, 0.508F, 0.470F), lower);
  QVector3D const control =
      lerp(QVector3D(0.0F, 0.674F, 0.348F), QVector3D(0.0F, 0.588F, 0.378F), lower);

  pose.withers = root;
  pose.poll = poll;

  QVector3D const facing = (poll - bezier(root, control, poll, 0.74F)).normalized();
  QVector3D const head_up =
      QVector3D::crossProduct(facing, QVector3D(1.0F, 0.0F, 0.0F)).normalized();
  QVector3D const side = QVector3D::crossProduct(head_up, facing).normalized();
  QVector3D const muzzle_dir = (facing - (head_up * 0.16F)).normalized();
  pose.muzzle = poll + (facing * 0.062F) + (muzzle_dir * 0.104F);

  for (int sign_index = 0; sign_index < 2; ++sign_index) {
    float const sign = sign_index == 0 ? -1.0F : 1.0F;
    QVector3D const base =
        poll + (head_up * 0.048F) + (side * (sign * 0.048F)) - (facing * 0.014F);
    QVector3D const erect =
        ((head_up * 0.90F) + (side * (sign * 0.30F)) + (facing * 0.14F)).normalized();
    QVector3D const pinned =
        ((head_up * 0.30F) + (side * (sign * 0.52F)) - (facing * 0.80F)).normalized();
    QVector3D const dir = lerp(erect, pinned, drive.ear_pin).normalized();
    QVector3D const tip = base + (dir * (0.118F - (drive.ear_pin * 0.022F)));
    if (sign_index == 0) {
      pose.ear_base_l = base;
      pose.ear_tip_l = tip;
    } else {
      pose.ear_base_r = base;
      pose.ear_tip_r = tip;
    }
  }
}

auto make_pose(const WolfDrive& drive) -> RigPose {
  RigPose pose;
  float const crouch = drive.crouch * 0.048F;
  float const bob =
      (std::sin(drive.stride_phase * k_two_pi * 2.0F) * 0.016F * drive.speed_ratio) -
      crouch;
  pose.root = QVector3D(0.0F, bob, 0.0F);
  pose.body_rear = QVector3D(0.0F, 0.510F + bob, -0.360F);
  pose.body_front = QVector3D(0.0F, 0.510F + bob, 0.346F);

  fill_legs(pose, drive);
  fill_head(pose, drive);

  float const sway = std::sin(drive.stride_phase * k_two_pi) * 0.040F *
                     (0.35F + (drive.speed_ratio * 0.65F));
  float const lift = drive.crouch;
  pose.tail_base = QVector3D(0.0F, 0.530F, -0.394F);
  pose.tail_mid = QVector3D(sway * 0.5F, 0.400F + (lift * 0.150F), -0.520F);
  pose.tail_tip = QVector3D(sway, 0.238F + (lift * 0.300F), -0.616F);
  return pose;
}

bool g_minimal_tessellation = false;

auto ellipsoid(std::string_view name,
               Bone bone,
               std::uint8_t role,
               const QVector3D& center,
               const QVector3D& radii,
               std::uint8_t lod_mask = Render::Creature::k_lod_all) -> MeshNode {
  MeshNode node;
  node.debug_name = name;
  node.anchor_bone = bone_index(bone);
  node.color_role = role;
  node.lod_mask = lod_mask;
  EllipsoidNode data;
  data.center = center;
  data.radii = radii;
  if (g_minimal_tessellation) {
    data.ring_count = 3U;
    data.ring_vertices = 6U;
  }
  node.data = data;
  return node;
}

auto barrel(std::string_view name,
            Bone bone,
            std::uint8_t role,
            std::vector<Render::Creature::Quadruped::BarrelRing> rings) -> MeshNode {
  MeshNode node;
  node.debug_name = name;
  node.anchor_bone = bone_index(bone);
  node.color_role = role;
  Render::Creature::Quadruped::BarrelNode data;
  data.rings = std::move(rings);
  node.data = data;
  return node;
}

auto tube(std::string_view name,
          Bone bone,
          std::uint8_t role,
          const QVector3D& start,
          const QVector3D& end,
          float start_radius,
          float end_radius,
          std::uint8_t lod_mask = Render::Creature::k_lod_all) -> MeshNode {
  MeshNode node;
  node.debug_name = name;
  node.anchor_bone = bone_index(bone);
  node.color_role = role;
  node.lod_mask = lod_mask;
  TubeNode data;
  data.start = start;
  data.end = end;
  data.start_radius = start_radius;
  data.end_radius = end_radius;
  if (g_minimal_tessellation) {
    data.segment_count = 2U;
    data.ring_vertices = 5U;
  }
  node.data = data;
  return node;
}

auto cone(std::string_view name,
          Bone bone,
          std::uint8_t role,
          const QVector3D& base_center,
          const QVector3D& tip,
          float base_radius,
          std::uint8_t lod_mask = Render::Creature::k_lod_all) -> MeshNode {
  MeshNode node;
  node.debug_name = name;
  node.anchor_bone = bone_index(bone);
  node.color_role = role;
  node.lod_mask = lod_mask;
  ConeNode data;
  data.base_center = base_center;
  data.tip = tip;
  data.base_radius = base_radius;
  node.data = data;
  return node;
}

auto build_mesh_nodes(std::uint8_t wanted_lod) -> std::vector<MeshNode> {
  g_minimal_tessellation = wanted_lod == Render::Creature::k_lod_minimal;
  const RigPose& bind = wolf_bind_pose();
  std::vector<MeshNode> nodes;
  nodes.reserve(56U);

  constexpr std::uint8_t k_full = Render::Creature::k_lod_full;

  nodes.push_back(barrel("wolf.body",
                         Bone::Body,
                         k_wolf_role_fur,
                         {{0.360F, 0.500F, 0.082F, 0.086F, 0.074F},
                          {0.268F, 0.504F, 0.128F, 0.124F, 0.124F},
                          {0.168F, 0.510F, 0.148F, 0.132F, 0.146F},
                          {0.060F, 0.512F, 0.132F, 0.116F, 0.126F},
                          {-0.046F, 0.514F, 0.116F, 0.104F, 0.090F},
                          {-0.150F, 0.516F, 0.122F, 0.108F, 0.086F},
                          {-0.246F, 0.520F, 0.142F, 0.124F, 0.114F},
                          {-0.336F, 0.524F, 0.112F, 0.102F, 0.096F},
                          {-0.398F, 0.530F, 0.054F, 0.050F, 0.046F}}));
  nodes.push_back(barrel("wolf.saddle",
                         Bone::Body,
                         k_wolf_role_saddle,
                         {{0.286F, 0.508F, 0.052F, 0.116F, 0.090F},
                          {0.196F, 0.512F, 0.086F, 0.146F, 0.110F},
                          {0.096F, 0.514F, 0.094F, 0.142F, 0.116F},
                          {-0.020F, 0.514F, 0.082F, 0.126F, 0.110F},
                          {-0.140F, 0.516F, 0.080F, 0.130F, 0.108F},
                          {-0.250F, 0.520F, 0.092F, 0.144F, 0.114F},
                          {-0.340F, 0.526F, 0.070F, 0.118F, 0.096F},
                          {-0.394F, 0.532F, 0.034F, 0.070F, 0.056F}}));
  nodes.push_back(ellipsoid("wolf.withers",
                            Bone::Body,
                            k_wolf_role_saddle,
                            {0.0F, 0.616F, 0.196F},
                            {0.076F, 0.050F, 0.120F}));
  nodes.push_back(ellipsoid("wolf.chest",
                            Bone::Body,
                            k_wolf_role_pale,
                            {0.0F, 0.394F, 0.170F},
                            {0.070F, 0.038F, 0.160F}));
  nodes.push_back(ellipsoid("wolf.belly",
                            Bone::Body,
                            k_wolf_role_pale,
                            {0.0F, 0.442F, -0.080F},
                            {0.058F, 0.032F, 0.182F}));
  nodes.push_back(ellipsoid("wolf.blaze",
                            Bone::Body,
                            k_wolf_role_cream,
                            {0.0F, 0.452F, 0.356F},
                            {0.048F, 0.066F, 0.046F}));

  QVector3D const neck_mid = (bind.withers + bind.poll) * 0.5F;
  nodes.push_back(tube("wolf.neck.lower",
                       Bone::NeckTop,
                       k_wolf_role_fur,
                       bind.withers,
                       neck_mid,
                       0.098F,
                       0.078F));
  nodes.push_back(tube("wolf.neck.upper",
                       Bone::NeckTop,
                       k_wolf_role_fur,
                       neck_mid,
                       bind.poll,
                       0.078F,
                       0.062F));
  nodes.push_back(ellipsoid("wolf.ruff",
                            Bone::NeckTop,
                            k_wolf_role_fur,
                            neck_mid - QVector3D(0.0F, 0.012F, 0.030F),
                            {0.134F, 0.124F, 0.106F}));
  nodes.push_back(ellipsoid("wolf.mane",
                            Bone::NeckTop,
                            k_wolf_role_saddle,
                            neck_mid + QVector3D(0.0F, 0.046F, -0.018F),
                            {0.106F, 0.080F, 0.106F}));
  nodes.push_back(ellipsoid("wolf.throat",
                            Bone::NeckTop,
                            k_wolf_role_cream,
                            neck_mid + QVector3D(0.0F, -0.058F, 0.014F),
                            {0.070F, 0.050F, 0.062F}));

  QVector3D const facing = (bind.muzzle - bind.poll).normalized();
  QVector3D const head_up =
      QVector3D::crossProduct(facing, QVector3D(1.0F, 0.0F, 0.0F)).normalized();
  QVector3D const side = QVector3D::crossProduct(head_up, facing).normalized();

  nodes.push_back(ellipsoid("wolf.skull",
                            Bone::Head,
                            k_wolf_role_fur,
                            bind.poll + (facing * 0.026F),
                            {0.066F, 0.066F, 0.082F}));
  nodes.push_back(ellipsoid("wolf.crown",
                            Bone::Head,
                            k_wolf_role_saddle,
                            bind.poll + (facing * 0.014F) + (head_up * 0.030F),
                            {0.062F, 0.032F, 0.070F}));
  {
    MeshNode node;
    node.debug_name = "wolf.muzzle";
    node.anchor_bone = bone_index(Bone::Head);
    node.color_role = k_wolf_role_pale;
    SnoutNode data;
    data.start = bind.poll + (facing * 0.062F);
    data.end = bind.muzzle;
    data.base_radius = 0.050F;
    data.tip_radius = 0.030F;
    node.data = data;
    nodes.push_back(node);
  }
  nodes.push_back(tube("wolf.jaw",
                       Bone::Head,
                       k_wolf_role_pale,
                       bind.poll + (facing * 0.062F) - (head_up * 0.026F),
                       bind.muzzle - (head_up * 0.018F),
                       0.034F,
                       0.022F));
  nodes.push_back(tube("wolf.bridge",
                       Bone::Head,
                       k_wolf_role_saddle,
                       bind.poll + (facing * 0.058F) + (head_up * 0.026F),
                       bind.muzzle + (head_up * 0.016F),
                       0.032F,
                       0.019F));
  nodes.push_back(ellipsoid("wolf.nose",
                            Bone::Head,
                            k_wolf_role_nose,
                            bind.muzzle + (facing * 0.008F),
                            {0.026F, 0.026F, 0.026F}));
  for (float sign : {-1.0F, 1.0F}) {
    nodes.push_back(ellipsoid("wolf.cheek",
                              Bone::Head,
                              k_wolf_role_pale,
                              bind.poll + (facing * 0.036F) + (side * (sign * 0.048F)) -
                                  (head_up * 0.014F),
                              {0.028F, 0.040F, 0.060F},
                              k_full));
    nodes.push_back(ellipsoid("wolf.eye",
                              Bone::Head,
                              k_wolf_role_eye,
                              bind.poll + (facing * 0.052F) + (head_up * 0.030F) +
                                  (side * (sign * 0.043F)),
                              {0.0145F, 0.0145F, 0.0145F},
                              k_full));
    nodes.push_back(ellipsoid("wolf.iris",
                              Bone::Head,
                              k_wolf_role_iris,
                              bind.poll + (facing * 0.060F) + (head_up * 0.030F) +
                                  (side * (sign * 0.049F)),
                              {0.009F, 0.009F, 0.009F},
                              k_full));
  }

  nodes.push_back(cone("wolf.ear_l",
                       Bone::EarL,
                       k_wolf_role_saddle,
                       bind.ear_base_l,
                       bind.ear_tip_l,
                       0.044F));
  nodes.push_back(cone("wolf.ear_r",
                       Bone::EarR,
                       k_wolf_role_saddle,
                       bind.ear_base_r,
                       bind.ear_tip_r,
                       0.044F));
  nodes.push_back(cone("wolf.ear_inner_l",
                       Bone::EarL,
                       k_wolf_role_pale,
                       bind.ear_base_l + (facing * 0.012F),
                       bind.ear_tip_l + (facing * 0.010F),
                       0.026F,
                       k_full));
  nodes.push_back(cone("wolf.ear_inner_r",
                       Bone::EarR,
                       k_wolf_role_pale,
                       bind.ear_base_r + (facing * 0.012F),
                       bind.ear_tip_r + (facing * 0.010F),
                       0.026F,
                       k_full));

  constexpr std::array<Bone, k_leg_count> k_shoulders{
      Bone::ShoulderFL, Bone::ShoulderFR, Bone::ShoulderBL, Bone::ShoulderBR};
  constexpr std::array<Bone, k_leg_count> k_knees{
      Bone::KneeFL, Bone::KneeFR, Bone::KneeBL, Bone::KneeBR};
  constexpr std::array<Bone, k_leg_count> k_feet{
      Bone::FootFL, Bone::FootFR, Bone::FootBL, Bone::FootBR};

  for (std::size_t i = 0; i < k_leg_count; ++i) {
    const LegJoints& joints = bind.legs[i];
    bool const hind = k_leg_plans[i].hind;
    nodes.push_back(tube("wolf.leg.upper",
                         k_shoulders[i],
                         k_wolf_role_fur,
                         joints.shoulder,
                         joints.knee,
                         hind ? 0.054F : 0.048F,
                         0.036F));
    nodes.push_back(tube("wolf.leg.mid",
                         k_knees[i],
                         k_wolf_role_limb,
                         joints.knee,
                         joints.foot,
                         0.034F,
                         0.027F));
    nodes.push_back(tube("wolf.leg.lower",
                         k_feet[i],
                         k_wolf_role_limb,
                         joints.foot,
                         joints.toe,
                         0.027F,
                         0.023F));
    nodes.push_back(ellipsoid(
        "wolf.paw", k_feet[i], k_wolf_role_paw, joints.toe, {0.034F, 0.026F, 0.046F}));
    nodes.push_back(ellipsoid(
        "wolf.limb_mass",
        k_shoulders[i],
        k_wolf_role_fur,
        hind ? QVector3D(k_leg_plans[i].x * 0.86F, 0.410F, -0.194F)
             : QVector3D(k_leg_plans[i].x * 0.84F, 0.420F, 0.166F),
        hind ? QVector3D(0.054F, 0.098F, 0.100F) : QVector3D(0.046F, 0.080F, 0.080F)));
  }

  nodes.push_back(tube("wolf.tail.base",
                       Bone::TailBase,
                       k_wolf_role_fur,
                       bind.tail_base,
                       bind.tail_mid,
                       0.042F,
                       0.070F));
  nodes.push_back(ellipsoid("wolf.tail.brush",
                            Bone::TailBase,
                            k_wolf_role_fur,
                            (bind.tail_base * 0.24F) + (bind.tail_mid * 0.76F),
                            {0.062F, 0.070F, 0.070F}));
  nodes.push_back(tube("wolf.tail.tip",
                       Bone::TailTip,
                       k_wolf_role_saddle,
                       bind.tail_mid,
                       bind.tail_tip,
                       0.074F,
                       0.026F));

  std::erase_if(nodes, [wanted_lod](const MeshNode& node) {
    return (node.lod_mask & wanted_lod) == 0U;
  });
  g_minimal_tessellation = false;
  return nodes;
}

auto static_full_parts() noexcept -> const Render::Creature::CompiledWholeMeshLod& {
  static const auto compiled =
      Render::Creature::compile_whole_mesh_lod(wolf_manifest().lod_full);
  return compiled;
}

auto static_minimal_parts() noexcept -> const Render::Creature::CompiledWholeMeshLod& {
  static const auto compiled =
      Render::Creature::compile_whole_mesh_lod(wolf_manifest().lod_minimal);
  return compiled;
}

} // namespace

auto wolf_bind_pose() noexcept -> const RigPose& {
  static const RigPose pose = make_pose(WolfDrive{});
  return pose;
}

auto wolf_pose(const WolfDrive& drive) noexcept -> RigPose {
  return make_pose(drive);
}

auto wolf_bind_palette() noexcept -> std::span<const QMatrix4x4> {
  static const BonePalette palette = [] {
    BonePalette out{};
    evaluate_wildlife_skeleton(wolf_bind_pose(), out);
    return out;
  }();
  return {palette.data(), palette.size()};
}

auto wolf_mesh_nodes() noexcept
    -> std::span<const Render::Creature::Quadruped::MeshNode> {
  static const std::vector<MeshNode> nodes =
      build_mesh_nodes(Render::Creature::k_lod_full);
  return {nodes.data(), nodes.size()};
}

auto wolf_minimal_mesh_nodes() noexcept
    -> std::span<const Render::Creature::Quadruped::MeshNode> {
  static const std::vector<MeshNode> nodes =
      build_mesh_nodes(Render::Creature::k_lod_minimal);
  return {nodes.data(), nodes.size()};
}

auto wolf_creature_spec() noexcept -> const Render::Creature::CreatureSpec& {
  static const Render::Creature::CreatureSpec spec = [] {
    Render::Creature::CreatureSpec s;
    s.species_name = "wolf";
    s.topology = wildlife_topology();
    s.lod_full = static_full_parts().part_graph();
    s.lod_minimal = static_minimal_parts().part_graph();
    return s;
  }();
  return spec;
}

} // namespace Render::Wildlife
