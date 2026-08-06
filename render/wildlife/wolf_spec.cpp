#include "wolf_spec.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "../creature/species_manifest.h"
#include "wildlife_gait.h"
#include "wolf_manifest.h"

namespace Render::Wildlife {

namespace {

using Render::Creature::Quadruped::ConeNode;
using Render::Creature::Quadruped::EllipsoidNode;
using Render::Creature::Quadruped::MeshNode;
using Render::Creature::Quadruped::SnoutNode;
using Render::Creature::Quadruped::TubeNode;

constexpr float k_two_pi = 6.28318530718F;

// Authored against a standing shoulder height of 0.646. A wolf is built the opposite
// way to the sheep it hunts: legs are half its height, the chest is deep and narrow
// rather than round, the belly tucks up hard behind the last rib, and the back is
// long - 1.23 shoulder heights from brisket to croup.
constexpr float k_fore_half_width = 0.088F;
constexpr float k_hind_half_width = 0.094F;

struct BodyRing {
  float z;
  float y;
  float half_width;
  float top;
  float bottom;
};

// Withers at 0.646, deepest at the heart girth, tucked up hard at the loin, then a
// slight rise over the croup. Plan-view sections of a CC0 reference wolf show the
// waist is as much a narrowing as a tuck: half the body's width goes at the loin,
// 0.14 against 0.22 over the ribs and hips. That pinch, the topline dip behind the
// shoulder, and the belly tuck are what read as "wolf" at any distance.
constexpr std::array<BodyRing, 14> k_body_rings{{
    {0.396F, 0.466F, 0.028F, 0.044F, 0.046F},
    {0.372F, 0.470F, 0.062F, 0.088F, 0.090F},
    {0.320F, 0.486F, 0.104F, 0.122F, 0.140F},
    {0.258F, 0.498F, 0.136F, 0.152F, 0.166F},
    {0.186F, 0.504F, 0.146F, 0.148F, 0.180F},
    {0.108F, 0.506F, 0.140F, 0.140F, 0.172F},
    {0.026F, 0.508F, 0.118F, 0.130F, 0.148F},
    {-0.056F, 0.510F, 0.096F, 0.126F, 0.114F},
    {-0.140F, 0.512F, 0.094F, 0.126F, 0.098F},
    {-0.222F, 0.516F, 0.122F, 0.128F, 0.108F},
    {-0.300F, 0.518F, 0.136F, 0.126F, 0.122F},
    {-0.368F, 0.516F, 0.110F, 0.110F, 0.106F},
    {-0.420F, 0.512F, 0.052F, 0.070F, 0.062F},
    {-0.444F, 0.510F, 0.024F, 0.034F, 0.030F},
}};

// How far down the back the dark saddle reaches, per ring. Waving it stops the fur
// boundary reading as a painted stripe.
constexpr std::array<float, k_body_rings.size()> k_saddle_reach{{0.62F,
                                                                 0.50F,
                                                                 0.30F,
                                                                 0.10F,
                                                                 0.02F,
                                                                 0.10F,
                                                                 0.04F,
                                                                 0.14F,
                                                                 0.06F,
                                                                 0.02F,
                                                                 0.10F,
                                                                 0.26F,
                                                                 0.52F,
                                                                 0.70F}};

constexpr float k_saddle_relief = 0.005F;

struct LegPlan {
  float x;
  float phase_offset;
  bool hind;
};

// Diagonal pairs move together: a wolf trots.
constexpr std::array<LegPlan, k_leg_count> k_leg_plans{{
    {-k_fore_half_width, 0.0F, false},
    {k_fore_half_width, 0.5F, false},
    {-k_hind_half_width, 0.5F, true},
    {k_hind_half_width, 0.0F, true},
}};

// Standing joints. Both limbs carry a real angle at rest - the fore at the elbow, the
// hind through the stifle and hock - because a column that stands dead straight has no
// travel left to fold into, and its paw can only swing through the ground on an arc.
struct LegRestPlan {
  float knee_y;
  float knee_z;
  float ankle_y;
  float ankle_z;
  float toe_y;
  float toe_z;
  float hip_y;
  float hip_z;
};

constexpr LegRestPlan k_fore_rest{
    0.244F, 0.272F, 0.108F, 0.216F, 0.024F, 0.236F, 0.402F, 0.222F};
constexpr LegRestPlan k_hind_rest{
    0.286F, -0.140F, 0.134F, -0.256F, 0.024F, -0.220F, 0.438F, -0.222F};

// The fore limb folds least, so it sets the stride both ends have to share: 0.30 at
// most before the paw outruns its reach and the leg locks straight. Each plan sits
// just inside that, which puts the cadence at 3.1 units/s near 4 Hz for the run and
// keeps the walk under 2.5 Hz across its whole speed band.
constexpr GaitPlan k_gait_stalk{0.130F, 0.70F, 0.014F, 0.030F};
constexpr GaitPlan k_gait_walk{0.235F, 0.62F, 0.028F, 0.060F};
constexpr GaitPlan k_gait_run{0.290F, 0.38F, 0.074F, 0.100F};

auto gait_plan(WolfGait gait) noexcept -> GaitPlan {
  switch (gait) {
  case WolfGait::Stalk:
    return k_gait_stalk;
  case WolfGait::Walk:
    return k_gait_walk;
  case WolfGait::Run:
    return k_gait_run;
  case WolfGait::Stand:
    break;
  }
  return GaitPlan{};
}

auto leg_rests() noexcept -> const std::array<LegRest, k_leg_count>& {
  static const std::array<LegRest, k_leg_count> rests = [] {
    std::array<LegRest, k_leg_count> out{};
    for (std::size_t i = 0; i < k_leg_count; ++i) {
      const LegPlan& plan = k_leg_plans[i];
      const LegRestPlan& r = plan.hind ? k_hind_rest : k_fore_rest;
      out[i] = make_leg_rest({plan.x, r.hip_y, r.hip_z},
                             {plan.x, r.knee_y, r.knee_z},
                             {plan.x, r.ankle_y, r.ankle_z},
                             {plan.x, r.toe_y, r.toe_z},
                             plan.phase_offset);
    }
    return out;
  }();
  return rests;
}

// Mirrors the cross-section make_oval_ring builds: the fraction of a ring's
// half_width the barrel actually has at a given height, with the height measured in
// units of the ring's vertical radius out from its centre.
auto oval_width_fraction(float height) -> float {
  constexpr std::array<std::pair<float, float>, 6> k_profile{{{1.00F, 0.00F},
                                                              {0.85F, 0.45F},
                                                              {0.50F, 0.85F},
                                                              {-0.05F, 1.00F},
                                                              {-0.70F, 0.65F},
                                                              {-1.00F, 0.00F}}};
  float const h = std::clamp(height, -1.0F, 1.0F);
  for (std::size_t i = 1; i < k_profile.size(); ++i) {
    const auto& [hi_h, hi_w] = k_profile[i - 1U];
    const auto& [lo_h, lo_w] = k_profile[i];
    if (h >= lo_h) {
      float const t = (h - lo_h) / (hi_h - lo_h);
      return lo_w + ((hi_w - lo_w) * t);
    }
  }
  return 0.0F;
}

// A shell that sits `k_saddle_relief` proud of the body wherever it is widest, so the
// dark back is a layer of fur over the barrel rather than a second torso volume.
auto saddle_rings() -> std::vector<Render::Creature::Quadruped::BarrelRing> {
  std::vector<Render::Creature::Quadruped::BarrelRing> rings;
  rings.reserve(k_body_rings.size());
  for (std::size_t i = 0; i < k_body_rings.size(); ++i) {
    const BodyRing& body = k_body_rings[i];
    float const centre = body.y + ((body.top - body.bottom) * 0.5F);
    float const radius = (body.top + body.bottom) * 0.5F;
    float const top_y = body.y + body.top + k_saddle_relief;
    float const bottom_y = centre + (radius * k_saddle_reach[i]);
    float const shell_centre = (top_y + bottom_y) * 0.5F;
    float const shell_radius = (top_y - bottom_y) * 0.5F;
    float const widest = shell_centre - (shell_radius * 0.05F);
    float const half_width =
        (body.half_width * oval_width_fraction((widest - centre) / radius)) +
        k_saddle_relief;
    rings.push_back({body.z,
                     shell_centre,
                     half_width,
                     top_y - shell_centre,
                     shell_centre - bottom_y});
  }
  return rings;
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
  const GaitPlan plan = gait_plan(drive.gait);
  float const weight = drive.gait == WolfGait::Stand ? 0.0F : 1.0F;
  for (std::size_t i = 0; i < k_leg_count; ++i) {
    const LegRest& rest = leg_rests()[i];
    solve_leg(rest, plan, drive.stride_phase + rest.phase_offset, weight, pose.legs[i]);
  }
}

void fill_head(RigPose& pose, const WolfDrive& drive) {
  float const lower = std::clamp(drive.crouch + (drive.lunge * 0.35F), 0.0F, 1.0F);
  QVector3D const root(0.0F, 0.560F, 0.320F);
  QVector3D const poll =
      lerp(QVector3D(0.0F, 0.628F, 0.470F), QVector3D(0.0F, 0.500F, 0.500F), lower);
  QVector3D const control =
      lerp(QVector3D(0.0F, 0.640F, 0.372F), QVector3D(0.0F, 0.560F, 0.410F), lower);

  pose.withers = root;
  pose.poll = poll;

  QVector3D const facing = (poll - bezier(root, control, poll, 0.74F)).normalized();
  QVector3D const head_up =
      QVector3D::crossProduct(facing, QVector3D(1.0F, 0.0F, 0.0F)).normalized();
  QVector3D const side = QVector3D::crossProduct(head_up, facing).normalized();
  QVector3D const muzzle_dir = (facing - (head_up * 0.16F)).normalized();
  pose.muzzle = poll + (facing * 0.068F) + (muzzle_dir * 0.134F);

  for (int sign_index = 0; sign_index < 2; ++sign_index) {
    float const sign = sign_index == 0 ? -1.0F : 1.0F;
    QVector3D const base =
        poll + (head_up * 0.050F) + (side * (sign * 0.046F)) - (facing * 0.016F);
    QVector3D const erect =
        ((head_up * 0.90F) + (side * (sign * 0.30F)) + (facing * 0.14F)).normalized();
    QVector3D const pinned =
        ((head_up * 0.30F) + (side * (sign * 0.52F)) - (facing * 0.80F)).normalized();
    QVector3D const dir = lerp(erect, pinned, drive.ear_pin).normalized();
    QVector3D const tip = base + (dir * (0.105F - (drive.ear_pin * 0.020F)));
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
  pose.body_rear = QVector3D(0.0F, 0.512F + bob, -0.380F);
  pose.body_front = QVector3D(0.0F, 0.498F + bob, 0.340F);

  fill_legs(pose, drive);
  fill_head(pose, drive);

  float const sway = std::sin(drive.stride_phase * k_two_pi) * 0.040F *
                     (0.35F + (drive.speed_ratio * 0.65F));
  float const lift = drive.crouch;

  pose.tail_base = QVector3D(0.0F, 0.556F, -0.412F);
  pose.tail_mid = QVector3D(sway * 0.5F, 0.512F + (lift * 0.140F), -0.606F);
  pose.tail_tip = QVector3D(sway, 0.392F + (lift * 0.320F), -0.782F);
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

  std::vector<Render::Creature::Quadruped::BarrelRing> body_rings;
  body_rings.reserve(k_body_rings.size());
  for (const BodyRing& ring : k_body_rings) {
    body_rings.push_back({ring.z, ring.y, ring.half_width, ring.top, ring.bottom});
  }
  nodes.push_back(
      barrel("wolf.body", Bone::Body, k_wolf_role_fur, std::move(body_rings)));
  nodes.push_back(
      barrel("wolf.saddle", Bone::Body, k_wolf_role_saddle, saddle_rings()));
  nodes.push_back(ellipsoid("wolf.withers",
                            Bone::Body,
                            k_wolf_role_saddle,
                            {0.0F, 0.596F, 0.212F},
                            {0.072F, 0.048F, 0.110F}));
  nodes.push_back(ellipsoid("wolf.chest",
                            Bone::Body,
                            k_wolf_role_pale,
                            {0.0F, 0.344F, 0.190F},
                            {0.064F, 0.032F, 0.150F}));
  nodes.push_back(ellipsoid("wolf.belly",
                            Bone::Body,
                            k_wolf_role_pale,
                            {0.0F, 0.418F, -0.100F},
                            {0.052F, 0.028F, 0.170F}));
  nodes.push_back(ellipsoid("wolf.blaze",
                            Bone::Body,
                            k_wolf_role_cream,
                            {0.0F, 0.404F, 0.352F},
                            {0.052F, 0.058F, 0.038F}));

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
  // The ruff has to stay inside the chest's width. Wider than the body it becomes a
  // collar the head is posted through.
  nodes.push_back(ellipsoid("wolf.ruff",
                            Bone::NeckTop,
                            k_wolf_role_fur,
                            neck_mid - QVector3D(0.0F, 0.008F, 0.022F),
                            {0.106F, 0.100F, 0.096F}));
  nodes.push_back(ellipsoid("wolf.mane",
                            Bone::NeckTop,
                            k_wolf_role_saddle,
                            neck_mid + QVector3D(0.0F, 0.042F, -0.016F),
                            {0.082F, 0.062F, 0.088F}));
  nodes.push_back(ellipsoid("wolf.throat",
                            Bone::NeckTop,
                            k_wolf_role_cream,
                            neck_mid + QVector3D(0.0F, -0.050F, 0.014F),
                            {0.056F, 0.040F, 0.056F}));

  QVector3D const facing = (bind.muzzle - bind.poll).normalized();
  QVector3D const head_up =
      QVector3D::crossProduct(facing, QVector3D(1.0F, 0.0F, 0.0F)).normalized();
  QVector3D const side = QVector3D::crossProduct(head_up, facing).normalized();

  nodes.push_back(ellipsoid("wolf.skull",
                            Bone::Head,
                            k_wolf_role_fur,
                            bind.poll + (facing * 0.028F),
                            {0.060F, 0.062F, 0.078F}));
  nodes.push_back(ellipsoid("wolf.crown",
                            Bone::Head,
                            k_wolf_role_saddle,
                            bind.poll + (facing * 0.014F) + (head_up * 0.030F),
                            {0.056F, 0.030F, 0.066F}));
  {
    MeshNode node;
    node.debug_name = "wolf.muzzle";
    node.anchor_bone = bone_index(Bone::Head);
    node.color_role = k_wolf_role_pale;
    SnoutNode data;
    data.start = bind.poll + (facing * 0.068F);
    data.end = bind.muzzle;
    data.base_radius = 0.046F;
    data.tip_radius = 0.026F;
    node.data = data;
    nodes.push_back(node);
  }
  nodes.push_back(tube("wolf.jaw",
                       Bone::Head,
                       k_wolf_role_pale,
                       bind.poll + (facing * 0.068F) - (head_up * 0.024F),
                       bind.muzzle - (head_up * 0.016F),
                       0.031F,
                       0.020F));
  nodes.push_back(tube("wolf.bridge",
                       Bone::Head,
                       k_wolf_role_saddle,
                       bind.poll + (facing * 0.062F) + (head_up * 0.024F),
                       bind.muzzle + (head_up * 0.014F),
                       0.030F,
                       0.018F));
  nodes.push_back(ellipsoid("wolf.nose",
                            Bone::Head,
                            k_wolf_role_nose,
                            bind.muzzle + (facing * 0.008F),
                            {0.024F, 0.024F, 0.024F}));
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
                         hind ? 0.052F : 0.046F,
                         0.032F));
    nodes.push_back(tube("wolf.leg.mid",
                         k_knees[i],
                         k_wolf_role_limb,
                         joints.knee,
                         joints.foot,
                         0.030F,
                         0.024F));
    nodes.push_back(tube("wolf.leg.lower",
                         k_feet[i],
                         k_wolf_role_limb,
                         joints.foot,
                         joints.toe,
                         0.024F,
                         0.021F));
    nodes.push_back(ellipsoid(
        "wolf.paw", k_feet[i], k_wolf_role_paw, joints.toe, {0.032F, 0.024F, 0.044F}));
    nodes.push_back(ellipsoid(
        "wolf.limb_mass",
        k_shoulders[i],
        k_wolf_role_fur,
        hind ? QVector3D(k_leg_plans[i].x * 0.86F, 0.400F, -0.222F)
             : QVector3D(k_leg_plans[i].x * 0.84F, 0.392F, 0.220F),
        hind ? QVector3D(0.056F, 0.100F, 0.096F) : QVector3D(0.048F, 0.086F, 0.078F)));
  }

  // One continuous brush. Stacking ellipsoids along the tail segmented it into a
  // caterpillar; the swell has to live in the tube radii instead.
  nodes.push_back(tube("wolf.tail.base",
                       Bone::TailBase,
                       k_wolf_role_fur,
                       bind.tail_base,
                       bind.tail_mid,
                       0.036F,
                       0.052F));
  nodes.push_back(tube("wolf.tail.tip",
                       Bone::TailTip,
                       k_wolf_role_saddle,
                       bind.tail_mid,
                       bind.tail_tip,
                       0.052F,
                       0.028F));

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

auto wolf_gait_advance(WolfGait gait) noexcept -> float {
  return gait_advance(gait_plan(gait));
}

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
