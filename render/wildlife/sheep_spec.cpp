#include "sheep_spec.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>
#include <utility>
#include <vector>

#include "../creature/species_manifest.h"
#include "sheep_manifest.h"

namespace Render::Wildlife {

namespace {

using Render::Creature::Quadruped::EllipsoidNode;
using Render::Creature::Quadruped::MeshNode;
using Render::Creature::Quadruped::SnoutNode;
using Render::Creature::Quadruped::TubeNode;

constexpr float k_two_pi = 6.28318530718F;
constexpr float k_half_pi = 1.57079633F;

constexpr float k_hip_y = 0.300F;
constexpr float k_knee_y = 0.160F;
constexpr float k_fetlock_y = 0.062F;
constexpr float k_hip_half_width = 0.100F;
constexpr float k_fore_hip_z = 0.250F;
constexpr float k_hind_hip_z = -0.295F;
constexpr float k_swing_angle = 0.30F;
constexpr float k_knee_flex = 0.52F;

constexpr QVector3D k_withers(0.0F, 0.528F, 0.360F);
constexpr QVector3D k_poll_up(0.0F, 0.668F, 0.606F);
constexpr QVector3D k_graze_dir(0.0F, -0.902F, 0.432F);

struct LegPlan {
  float x;
  float z;
  float phase_offset;
  float knee_bias;
};

constexpr std::array<LegPlan, k_leg_count> k_leg_plans{{
    {-k_hip_half_width, k_fore_hip_z, 0.0F, 0.014F},
    {k_hip_half_width, k_fore_hip_z, 0.5F, 0.014F},
    {-k_hip_half_width, k_hind_hip_z, 0.5F, -0.024F},
    {k_hip_half_width, k_hind_hip_z, 0.0F, -0.024F},
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

void fill_legs(RigPose& pose, const SheepDrive& drive) {
  for (std::size_t i = 0; i < k_leg_count; ++i) {
    const LegPlan& plan = k_leg_plans[i];
    float const cycle = (drive.stride_phase + plan.phase_offset) * k_two_pi;
    float const angle = std::sin(cycle) * k_swing_angle * drive.speed_ratio;
    float const flex =
        std::max(0.0F, std::sin(cycle + k_half_pi)) * k_knee_flex * drive.speed_ratio;

    QVector3D const hip(plan.x, k_hip_y, plan.z);
    QVector3D knee(plan.x, k_knee_y, plan.z + plan.knee_bias);
    QVector3D foot(plan.x, k_fetlock_y, plan.z);
    QVector3D toe(plan.x, 0.0F, plan.z);

    knee = swung(knee, hip, angle);
    foot = swung(swung(foot, hip, angle), knee, -flex);
    toe = swung(swung(toe, hip, angle), knee, -flex);

    pose.legs[i].shoulder = hip;
    pose.legs[i].knee = knee;
    pose.legs[i].foot = foot;
    pose.legs[i].toe = toe;
  }
}

void fill_head(RigPose& pose, const SheepDrive& drive) {
  float const neck_length = (k_poll_up - k_withers).length();
  QVector3D const poll_graze = k_withers + (k_graze_dir.normalized() * neck_length);
  QVector3D const control_up(0.0F, 0.664F, 0.462F);
  QVector3D const control_graze =
      k_withers + ((poll_graze - k_withers) * 0.5F) + QVector3D(0.0F, 0.0F, 0.070F);
  QVector3D const poll = lerp(k_poll_up, poll_graze, drive.graze);
  QVector3D const control = lerp(control_up, control_graze, drive.graze);

  pose.withers = k_withers;
  pose.poll = poll;

  QVector3D const facing =
      (poll - bezier(k_withers, control, poll, 0.70F)).normalized();
  QVector3D const head_up =
      QVector3D::crossProduct(facing, QVector3D(1.0F, 0.0F, 0.0F)).normalized();
  QVector3D const side = QVector3D::crossProduct(head_up, facing).normalized();
  QVector3D const muzzle_dir =
      (facing - (head_up * (0.28F + (drive.graze * 0.55F)))).normalized();

  pose.muzzle = poll + (facing * 0.056F) + (muzzle_dir * 0.050F);

  for (int sign_index = 0; sign_index < 2; ++sign_index) {
    float const sign = sign_index == 0 ? -1.0F : 1.0F;
    QVector3D const base =
        poll - (facing * 0.024F) + (side * (sign * 0.062F)) + (head_up * 0.020F);
    QVector3D const relaxed =
        ((side * (sign * 0.88F)) - (facing * 0.26F) - (head_up * 0.30F)).normalized();
    QVector3D const alert =
        ((side * (sign * 0.74F)) + (facing * 0.22F) + (head_up * 0.28F)).normalized();
    QVector3D const dir = lerp(relaxed, alert, drive.alert).normalized();
    QVector3D const tip = base + (dir * 0.126F);
    if (sign_index == 0) {
      pose.ear_base_l = base;
      pose.ear_tip_l = tip;
    } else {
      pose.ear_base_r = base;
      pose.ear_tip_r = tip;
    }
  }
}

auto make_pose(const SheepDrive& drive) -> RigPose {
  RigPose pose;
  float const bob =
      (std::sin(drive.stride_phase * k_two_pi * 2.0F) * 0.012F * drive.speed_ratio) -
      (drive.graze * 0.030F);
  pose.root = QVector3D(0.0F, bob, 0.0F);
  pose.body_rear = QVector3D(0.0F, 0.470F + bob, -0.390F);
  pose.body_front = QVector3D(0.0F, 0.470F + bob, 0.390F);

  fill_legs(pose, drive);
  fill_head(pose, drive);

  float const wag =
      std::sin(drive.stride_phase * k_two_pi) * 0.022F * drive.speed_ratio;
  pose.tail_base = QVector3D(0.0F, 0.506F, -0.430F);
  pose.tail_mid = QVector3D(wag * 0.5F, 0.418F, -0.488F);
  pose.tail_tip = QVector3D(wag, 0.322F, -0.512F);
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

auto ear_flap(std::string_view name,
              Bone bone,
              std::uint8_t role,
              const QVector3D& base,
              const QVector3D& tip,
              const QVector3D& face_normal,
              float half_width) -> MeshNode {
  QVector3D const along = (tip - base).normalized();
  QVector3D normal = face_normal - (along * QVector3D::dotProduct(face_normal, along));
  if (normal.lengthSquared() <= 1.0e-8F) {
    normal = QVector3D(0.0F, 1.0F, 0.0F);
  }
  normal.normalize();
  QVector3D const across = QVector3D::crossProduct(normal, along).normalized();

  constexpr std::array<std::pair<float, float>, 5> k_profile{{
      {0.00F, 0.38F},
      {0.20F, 0.86F},
      {0.46F, 1.00F},
      {0.76F, 0.70F},
      {1.00F, 0.06F},
  }};

  float const length = (tip - base).length();
  std::vector<QVector3D> outline;
  outline.reserve(k_profile.size() * 2U);
  for (const auto& [t, w] : k_profile) {
    outline.push_back(base + (along * (t * length)) - (across * (w * half_width)));
  }
  for (auto it = k_profile.rbegin(); it != k_profile.rend(); ++it) {
    outline.push_back(base + (along * (it->first * length)) +
                      (across * (it->second * half_width)));
  }

  MeshNode node;
  node.debug_name = name;
  node.anchor_bone = bone_index(bone);
  node.color_role = role;
  Render::Creature::Quadruped::FlatFanNode data;
  data.outline = std::move(outline);
  data.thickness_axis = normal;
  data.thickness = 0.016F;
  node.data = data;
  return node;
}

auto build_mesh_nodes(std::uint8_t wanted_lod) -> std::vector<MeshNode> {
  g_minimal_tessellation = wanted_lod == Render::Creature::k_lod_minimal;
  const RigPose& bind = sheep_bind_pose();
  std::vector<MeshNode> nodes;
  nodes.reserve(64U);

  constexpr std::uint8_t k_full = Render::Creature::k_lod_full;

  nodes.push_back(barrel("sheep.fleece",
                         Bone::Body,
                         k_sheep_role_wool,
                         {{0.412F, 0.492F, 0.058F, 0.054F, 0.066F},
                          {0.360F, 0.484F, 0.124F, 0.120F, 0.114F},
                          {0.294F, 0.476F, 0.166F, 0.154F, 0.156F},
                          {0.212F, 0.470F, 0.188F, 0.174F, 0.180F},
                          {0.126F, 0.468F, 0.196F, 0.164F, 0.188F},
                          {0.038F, 0.467F, 0.202F, 0.182F, 0.192F},
                          {-0.050F, 0.467F, 0.199F, 0.168F, 0.190F},
                          {-0.142F, 0.469F, 0.202F, 0.184F, 0.186F},
                          {-0.232F, 0.472F, 0.192F, 0.170F, 0.174F},
                          {-0.318F, 0.477F, 0.174F, 0.178F, 0.150F},
                          {-0.396F, 0.483F, 0.128F, 0.126F, 0.108F},
                          {-0.452F, 0.490F, 0.056F, 0.050F, 0.058F}}));
  nodes.push_back(ellipsoid("sheep.belly",
                            Bone::Body,
                            k_sheep_role_wool_grubby,
                            {0.0F, 0.312F, -0.024F},
                            {0.172F, 0.070F, 0.318F}));

  nodes.push_back(ellipsoid("sheep.clump.crest_front",
                            Bone::Body,
                            k_sheep_role_wool_light,
                            {0.0F, 0.594F, 0.196F},
                            {0.152F, 0.104F, 0.156F}));
  nodes.push_back(ellipsoid("sheep.clump.crest_rear",
                            Bone::Body,
                            k_sheep_role_wool_light,
                            {0.0F, 0.598F, -0.166F},
                            {0.150F, 0.106F, 0.162F}));
  nodes.push_back(ellipsoid("sheep.clump.shoulder_l",
                            Bone::Body,
                            k_sheep_role_wool,
                            {-0.176F, 0.508F, 0.218F},
                            {0.062F, 0.108F, 0.116F}));
  nodes.push_back(ellipsoid("sheep.clump.shoulder_r",
                            Bone::Body,
                            k_sheep_role_wool,
                            {0.176F, 0.508F, 0.218F},
                            {0.062F, 0.108F, 0.116F}));
  nodes.push_back(ellipsoid("sheep.clump.hip_l",
                            Bone::Body,
                            k_sheep_role_wool,
                            {-0.172F, 0.500F, -0.256F},
                            {0.060F, 0.104F, 0.124F}));
  nodes.push_back(ellipsoid("sheep.clump.hip_r",
                            Bone::Body,
                            k_sheep_role_wool,
                            {0.172F, 0.500F, -0.256F},
                            {0.060F, 0.104F, 0.124F}));
  nodes.push_back(ellipsoid("sheep.clump.rump",
                            Bone::Body,
                            k_sheep_role_wool_light,
                            {0.0F, 0.544F, -0.412F},
                            {0.128F, 0.118F, 0.070F}));
  nodes.push_back(ellipsoid("sheep.tuft.flank_l",
                            Bone::Body,
                            k_sheep_role_wool_shade,
                            {-0.186F, 0.452F, 0.036F},
                            {0.044F, 0.078F, 0.150F},
                            k_full));
  nodes.push_back(ellipsoid("sheep.tuft.flank_r",
                            Bone::Body,
                            k_sheep_role_wool_shade,
                            {0.186F, 0.446F, -0.096F},
                            {0.044F, 0.076F, 0.144F},
                            k_full));
  nodes.push_back(ellipsoid("sheep.collar",
                            Bone::Body,
                            k_sheep_role_wool,
                            {0.0F, 0.548F, 0.336F},
                            {0.146F, 0.136F, 0.104F}));

  QVector3D const neck_mid = (bind.withers + bind.poll) * 0.5F;
  nodes.push_back(tube("sheep.neck.lower",
                       Bone::NeckTop,
                       k_sheep_role_wool,
                       bind.withers,
                       neck_mid,
                       0.124F,
                       0.092F));
  nodes.push_back(tube("sheep.neck.upper",
                       Bone::NeckTop,
                       k_sheep_role_wool_shade,
                       neck_mid,
                       bind.poll,
                       0.092F,
                       0.062F));

  QVector3D const facing = (bind.muzzle - bind.poll).normalized();
  QVector3D const head_up =
      QVector3D::crossProduct(facing, QVector3D(1.0F, 0.0F, 0.0F)).normalized();
  QVector3D const side = QVector3D::crossProduct(head_up, facing).normalized();

  nodes.push_back(ellipsoid("sheep.cranium",
                            Bone::Head,
                            k_sheep_role_face,
                            bind.poll + (facing * 0.020F),
                            {0.078F, 0.078F, 0.084F}));

  nodes.push_back(ellipsoid("sheep.poll_wool",
                            Bone::Head,
                            k_sheep_role_wool_light,
                            bind.poll - (facing * 0.026F) + (head_up * 0.046F),
                            {0.092F, 0.064F, 0.094F}));
  nodes.push_back(ellipsoid("sheep.cheek_wool",
                            Bone::NeckTop,
                            k_sheep_role_wool,
                            bind.poll - ((bind.poll - bind.withers) * 0.38F),
                            {0.102F, 0.096F, 0.092F}));

  {
    MeshNode node;
    node.debug_name = "sheep.muzzle";
    node.anchor_bone = bone_index(Bone::Head);
    node.color_role = k_sheep_role_face;
    SnoutNode data;
    data.start = bind.poll + (facing * 0.044F);
    data.end = bind.muzzle;
    data.base_radius = 0.070F;
    data.tip_radius = 0.056F;
    node.data = data;
    nodes.push_back(node);
  }
  nodes.push_back(
      ellipsoid("sheep.nose",
                Bone::Head,
                k_sheep_role_nose,
                bind.muzzle + ((bind.muzzle - bind.poll).normalized() * 0.008F),
                {0.046F, 0.042F, 0.034F}));
  for (float sign : {-1.0F, 1.0F}) {
    nodes.push_back(ellipsoid("sheep.eye",
                              Bone::Head,
                              k_sheep_role_eye,
                              bind.poll + (facing * 0.050F) + (head_up * 0.012F) +
                                  (side * (sign * 0.056F)),
                              {0.017F, 0.017F, 0.017F},
                              k_full));
  }

  nodes.push_back(ear_flap("sheep.ear_l",
                           Bone::EarL,
                           k_sheep_role_face,
                           bind.ear_base_l,
                           bind.ear_tip_l,
                           head_up,
                           0.044F));
  nodes.push_back(ear_flap("sheep.ear_r",
                           Bone::EarR,
                           k_sheep_role_face,
                           bind.ear_base_r,
                           bind.ear_tip_r,
                           head_up,
                           0.044F));

  constexpr std::array<Bone, k_leg_count> k_shoulders{
      Bone::ShoulderFL, Bone::ShoulderFR, Bone::ShoulderBL, Bone::ShoulderBR};
  constexpr std::array<Bone, k_leg_count> k_knees{
      Bone::KneeFL, Bone::KneeFR, Bone::KneeBL, Bone::KneeBR};
  constexpr std::array<Bone, k_leg_count> k_feet{
      Bone::FootFL, Bone::FootFR, Bone::FootBL, Bone::FootBR};

  for (std::size_t i = 0; i < k_leg_count; ++i) {
    const LegJoints& joints = bind.legs[i];
    nodes.push_back(ellipsoid(
        "sheep.skirt",
        Bone::Body,
        k_sheep_role_wool,
        {joints.shoulder.x() * 0.94F, k_hip_y + 0.052F, joints.shoulder.z() * 0.92F},
        {0.084F, 0.086F, 0.104F}));

    nodes.push_back(tube("sheep.leg.upper",
                         k_shoulders[i],
                         k_sheep_role_wool_shade,
                         joints.shoulder,
                         joints.knee,
                         0.058F,
                         0.038F));
    nodes.push_back(tube("sheep.leg.lower",
                         k_knees[i],
                         k_sheep_role_face,
                         joints.knee,
                         joints.foot,
                         0.036F,
                         0.028F));
    nodes.push_back(tube("sheep.leg.hoof",
                         k_feet[i],
                         k_sheep_role_hoof,
                         joints.foot,
                         joints.toe,
                         0.034F,
                         0.040F));
  }

  nodes.push_back(tube("sheep.tail.base",
                       Bone::TailBase,
                       k_sheep_role_wool,
                       bind.tail_base,
                       bind.tail_mid,
                       0.056F,
                       0.048F));
  nodes.push_back(tube("sheep.tail.tip",
                       Bone::TailTip,
                       k_sheep_role_wool_grubby,
                       bind.tail_mid,
                       bind.tail_tip,
                       0.048F,
                       0.026F));

  std::erase_if(nodes, [wanted_lod](const MeshNode& node) {
    return (node.lod_mask & wanted_lod) == 0U;
  });
  g_minimal_tessellation = false;
  return nodes;
}

auto static_full_parts() noexcept -> const Render::Creature::CompiledWholeMeshLod& {
  static const auto compiled =
      Render::Creature::compile_whole_mesh_lod(sheep_manifest().lod_full);
  return compiled;
}

auto static_minimal_parts() noexcept -> const Render::Creature::CompiledWholeMeshLod& {
  static const auto compiled =
      Render::Creature::compile_whole_mesh_lod(sheep_manifest().lod_minimal);
  return compiled;
}

} // namespace

auto sheep_bind_pose() noexcept -> const RigPose& {
  static const RigPose pose = make_pose(SheepDrive{});
  return pose;
}

auto sheep_pose(const SheepDrive& drive) noexcept -> RigPose {
  return make_pose(drive);
}

auto sheep_bind_palette() noexcept -> std::span<const QMatrix4x4> {
  static const BonePalette palette = [] {
    BonePalette out{};
    evaluate_wildlife_skeleton(sheep_bind_pose(), out);
    return out;
  }();
  return {palette.data(), palette.size()};
}

auto sheep_mesh_nodes() noexcept
    -> std::span<const Render::Creature::Quadruped::MeshNode> {
  static const std::vector<MeshNode> nodes =
      build_mesh_nodes(Render::Creature::k_lod_full);
  return {nodes.data(), nodes.size()};
}

auto sheep_minimal_mesh_nodes() noexcept
    -> std::span<const Render::Creature::Quadruped::MeshNode> {
  static const std::vector<MeshNode> nodes =
      build_mesh_nodes(Render::Creature::k_lod_minimal);
  return {nodes.data(), nodes.size()};
}

auto sheep_creature_spec() noexcept -> const Render::Creature::CreatureSpec& {
  static const Render::Creature::CreatureSpec spec = [] {
    Render::Creature::CreatureSpec s;
    s.species_name = "sheep";
    s.topology = wildlife_topology();
    s.lod_full = static_full_parts().part_graph();
    s.lod_minimal = static_minimal_parts().part_graph();
    return s;
  }();
  return spec;
}

} // namespace Render::Wildlife
