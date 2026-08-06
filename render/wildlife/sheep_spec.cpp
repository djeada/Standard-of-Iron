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

// Everything below is authored against a standing shoulder height of 0.612 and holds
// the proportions a sheep is recognised by: legs 47% of that height, barrel 0.53 deep
// and 0.55 wide, 1.19 shoulder heights long, and widest across the hips.
constexpr float k_hip_y = 0.300F;
constexpr float k_knee_y = 0.164F;
constexpr float k_fetlock_y = 0.052F;
constexpr float k_fore_hip_z = 0.208F;
constexpr float k_hind_hip_z = -0.240F;
constexpr float k_swing_angle = 0.30F;
constexpr float k_knee_flex = 0.52F;

// Neck root sits at the front of the chest, not on top of the withers: that is what
// lets the muzzle reach the grass over a neck this short. A sheep carries its head
// barely above its own topline with the face angled down, which is most of what
// separates its profile from a goat's.
constexpr QVector3D k_withers(0.0F, 0.480F, 0.330F);
constexpr QVector3D k_poll_up(0.0F, 0.672F, 0.470F);
constexpr QVector3D k_graze_dir(0.0F, -0.974F, 0.226F);
constexpr float k_head_length = 0.196F;

struct BodyRing {
  float z;
  float y;
  float half_width;
  float top;
  float bottom;
};

// Flat topline, widest across the hips, a shallow tuck at the loin, then tapering up
// into the brisket in front and rounding off over the rump behind. The ring-to-ring
// jitter is the whole of the fleece texture: separate clumps laid over this surface
// intersect it in hard lines that flat shading turns into plates, so the wool has to
// be breathed into the barrel itself.
constexpr std::array<BodyRing, 17> k_body_rings{{
    {-0.386F, 0.472F, 0.022F, 0.030F, 0.030F},
    {-0.368F, 0.470F, 0.062F, 0.070F, 0.066F},
    {-0.342F, 0.464F, 0.108F, 0.108F, 0.102F},
    {-0.306F, 0.458F, 0.145F, 0.138F, 0.130F},
    {-0.262F, 0.454F, 0.168F, 0.156F, 0.150F},
    {-0.228F, 0.453F, 0.170F, 0.162F, 0.154F},
    {-0.196F, 0.454F, 0.165F, 0.157F, 0.157F},
    {-0.152F, 0.452F, 0.163F, 0.161F, 0.157F},
    {-0.106F, 0.451F, 0.157F, 0.157F, 0.159F},
    {-0.054F, 0.451F, 0.159F, 0.161F, 0.160F},
    {0.006F, 0.452F, 0.156F, 0.157F, 0.162F},
    {0.062F, 0.454F, 0.161F, 0.160F, 0.161F},
    {0.126F, 0.459F, 0.156F, 0.155F, 0.158F},
    {0.190F, 0.466F, 0.148F, 0.151F, 0.145F},
    {0.252F, 0.477F, 0.128F, 0.140F, 0.121F},
    {0.310F, 0.492F, 0.098F, 0.127F, 0.092F},
    {0.358F, 0.510F, 0.044F, 0.090F, 0.052F},
}};

struct LegPlan {
  float x;
  float z;
  float phase_offset;
  float knee_bias;
  float foot_bias;
};

constexpr std::array<LegPlan, k_leg_count> k_leg_plans{{
    {-0.104F, k_fore_hip_z, 0.0F, 0.008F, -0.006F},
    {0.104F, k_fore_hip_z, 0.5F, 0.008F, -0.006F},
    {-0.114F, k_hind_hip_z, 0.5F, -0.046F, 0.004F},
    {0.114F, k_hind_hip_z, 0.0F, -0.046F, 0.004F},
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
    QVector3D foot(plan.x, k_fetlock_y, plan.z + plan.foot_bias);
    QVector3D toe(plan.x, 0.0F, plan.z + plan.foot_bias);

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
  QVector3D const control_up(0.0F, 0.640F, 0.368F);
  QVector3D const control_graze =
      k_withers + ((poll_graze - k_withers) * 0.5F) + QVector3D(0.0F, 0.0F, 0.045F);
  QVector3D const poll = lerp(k_poll_up, poll_graze, drive.graze);
  QVector3D const control = lerp(control_up, control_graze, drive.graze);

  pose.withers = k_withers;
  pose.poll = poll;

  QVector3D const facing =
      (poll - bezier(k_withers, control, poll, 0.70F)).normalized();
  QVector3D const head_up =
      QVector3D::crossProduct(facing, QVector3D(1.0F, 0.0F, 0.0F)).normalized();
  QVector3D const side = QVector3D::crossProduct(head_up, facing).normalized();

  // The head hangs well off the neck axis when the sheep is up, so the face points
  // forward and down, and straightens into line with the neck as it swings down to
  // graze.
  QVector3D const muzzle_dir =
      (facing - (head_up * (1.38F - (drive.graze * 1.18F)))).normalized();

  pose.muzzle = poll + (muzzle_dir * k_head_length);

  for (int sign_index = 0; sign_index < 2; ++sign_index) {
    float const sign = sign_index == 0 ? -1.0F : 1.0F;
    QVector3D const base =
        poll - (facing * 0.014F) + (side * (sign * 0.050F)) + (head_up * 0.016F);
    QVector3D const relaxed =
        ((side * (sign * 0.90F)) - (facing * 0.22F) - (head_up * 0.32F)).normalized();
    QVector3D const alert =
        ((side * (sign * 0.78F)) + (facing * 0.20F) + (head_up * 0.24F)).normalized();
    QVector3D const dir = lerp(relaxed, alert, drive.alert).normalized();
    QVector3D const tip = base + (dir * 0.092F);
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
  pose.body_rear = QVector3D(0.0F, 0.462F + bob, -0.360F);
  pose.body_front = QVector3D(0.0F, 0.478F + bob, 0.360F);

  fill_legs(pose, drive);
  fill_head(pose, drive);

  float const wag =
      std::sin(drive.stride_phase * k_two_pi) * 0.022F * drive.speed_ratio;
  pose.tail_base = QVector3D(0.0F, 0.540F, -0.340F);
  pose.tail_mid = QVector3D(wag * 0.5F, 0.462F, -0.372F);
  pose.tail_tip = QVector3D(wag, 0.392F, -0.378F);
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
  data.thickness = 0.014F;
  node.data = data;
  return node;
}

auto build_mesh_nodes(std::uint8_t wanted_lod) -> std::vector<MeshNode> {
  g_minimal_tessellation = wanted_lod == Render::Creature::k_lod_minimal;
  const RigPose& bind = sheep_bind_pose();
  std::vector<MeshNode> nodes;
  nodes.reserve(64U);

  constexpr std::uint8_t k_full = Render::Creature::k_lod_full;

  std::vector<Render::Creature::Quadruped::BarrelRing> body_rings;
  body_rings.reserve(k_body_rings.size());
  for (const BodyRing& ring : k_body_rings) {
    body_rings.push_back({ring.z, ring.y, ring.half_width, ring.top, ring.bottom});
  }
  nodes.push_back(
      barrel("sheep.fleece", Bone::Body, k_sheep_role_wool, std::move(body_rings)));

  nodes.push_back(ellipsoid("sheep.belly",
                            Bone::Body,
                            k_sheep_role_wool_grubby,
                            {0.0F, 0.300F, -0.020F},
                            {0.136F, 0.046F, 0.250F}));
  nodes.push_back(ellipsoid("sheep.brisket",
                            Bone::Body,
                            k_sheep_role_wool,
                            {0.0F, 0.386F, 0.298F},
                            {0.092F, 0.076F, 0.070F}));
  nodes.push_back(ellipsoid("sheep.ruff",
                            Bone::Body,
                            k_sheep_role_wool,
                            {0.0F, 0.500F, 0.300F},
                            {0.124F, 0.112F, 0.090F}));

  QVector3D const neck_mid = (bind.withers + bind.poll) * 0.5F;
  nodes.push_back(tube("sheep.neck.lower",
                       Bone::NeckTop,
                       k_sheep_role_wool,
                       bind.withers,
                       neck_mid,
                       0.100F,
                       0.078F));
  nodes.push_back(tube("sheep.neck.upper",
                       Bone::NeckTop,
                       k_sheep_role_wool_shade,
                       neck_mid,
                       bind.poll,
                       0.084F,
                       0.068F));

  QVector3D const facing = (bind.muzzle - bind.poll).normalized();
  QVector3D const head_up =
      QVector3D::crossProduct(facing, QVector3D(1.0F, 0.0F, 0.0F)).normalized();
  QVector3D const side = QVector3D::crossProduct(head_up, facing).normalized();

  nodes.push_back(ellipsoid("sheep.cranium",
                            Bone::Head,
                            k_sheep_role_face,
                            bind.poll + (facing * 0.028F) + (head_up * 0.012F),
                            {0.062F, 0.058F, 0.062F}));

  // The fleece line stops at the poll. Sat any further forward this reads as a cap
  // pulled over the head rather than the end of the wool.
  nodes.push_back(ellipsoid("sheep.poll_wool",
                            Bone::Head,
                            k_sheep_role_wool_light,
                            bind.poll - (facing * 0.034F) + (head_up * 0.028F),
                            {0.066F, 0.046F, 0.056F}));
  nodes.push_back(ellipsoid("sheep.cheek_wool",
                            Bone::NeckTop,
                            k_sheep_role_wool,
                            bind.poll - ((bind.poll - bind.withers) * 0.30F),
                            {0.082F, 0.076F, 0.072F}));

  {
    MeshNode node;
    node.debug_name = "sheep.muzzle";
    node.anchor_bone = bone_index(Bone::Head);
    node.color_role = k_sheep_role_face;
    SnoutNode data;
    data.start = bind.poll + (facing * 0.044F);
    data.end = bind.muzzle;
    data.base_radius = 0.048F;
    data.tip_radius = 0.028F;
    node.data = data;
    nodes.push_back(node);
  }
  nodes.push_back(ellipsoid("sheep.nose",
                            Bone::Head,
                            k_sheep_role_nose,
                            bind.muzzle + (facing * 0.006F),
                            {0.030F, 0.026F, 0.024F}));
  for (float sign : {-1.0F, 1.0F}) {
    nodes.push_back(ellipsoid("sheep.eye",
                              Bone::Head,
                              k_sheep_role_eye,
                              bind.poll + (facing * 0.032F) + (head_up * 0.016F) +
                                  (side * (sign * 0.050F)),
                              {0.014F, 0.014F, 0.014F},
                              k_full));
  }

  nodes.push_back(ear_flap("sheep.ear_l",
                           Bone::EarL,
                           k_sheep_role_face,
                           bind.ear_base_l,
                           bind.ear_tip_l,
                           head_up,
                           0.032F));
  nodes.push_back(ear_flap("sheep.ear_r",
                           Bone::EarR,
                           k_sheep_role_face,
                           bind.ear_base_r,
                           bind.ear_tip_r,
                           head_up,
                           0.032F));

  constexpr std::array<Bone, k_leg_count> k_shoulders{
      Bone::ShoulderFL, Bone::ShoulderFR, Bone::ShoulderBL, Bone::ShoulderBR};
  constexpr std::array<Bone, k_leg_count> k_knees{
      Bone::KneeFL, Bone::KneeFR, Bone::KneeBL, Bone::KneeBR};
  constexpr std::array<Bone, k_leg_count> k_feet{
      Bone::FootFL, Bone::FootFR, Bone::FootBL, Bone::FootBR};

  for (std::size_t i = 0; i < k_leg_count; ++i) {
    const LegJoints& joints = bind.legs[i];
    bool const hind = i >= 2U;

    // Thigh and shoulder mass ride the leg bone so they swing with it; the skirt
    // belongs to the body and just closes the seam under the barrel.
    nodes.push_back(ellipsoid(hind ? "sheep.thigh" : "sheep.shoulder",
                              k_shoulders[i],
                              k_sheep_role_wool,
                              {joints.shoulder.x() * (hind ? 1.02F : 1.04F),
                               k_hip_y + (hind ? 0.068F : 0.074F),
                               joints.shoulder.z() + (hind ? 0.004F : 0.006F)},
                              hind ? QVector3D(0.062F, 0.094F, 0.086F)
                                   : QVector3D(0.056F, 0.084F, 0.078F)));
    nodes.push_back(ellipsoid(
        "sheep.skirt",
        Bone::Body,
        k_sheep_role_wool,
        {joints.shoulder.x() * 0.94F, k_hip_y + 0.016F, joints.shoulder.z() * 0.96F},
        {0.056F, 0.042F, 0.070F}));

    nodes.push_back(tube("sheep.leg.upper",
                         k_shoulders[i],
                         k_sheep_role_wool_shade,
                         joints.shoulder,
                         joints.knee,
                         0.048F,
                         0.026F));
    nodes.push_back(tube("sheep.leg.lower",
                         k_knees[i],
                         k_sheep_role_face,
                         joints.knee,
                         joints.foot,
                         0.024F,
                         0.019F));
    nodes.push_back(tube("sheep.leg.hoof",
                         k_feet[i],
                         k_sheep_role_hoof,
                         joints.foot,
                         joints.toe,
                         0.021F,
                         0.026F));
  }

  nodes.push_back(tube("sheep.tail.base",
                       Bone::TailBase,
                       k_sheep_role_wool,
                       bind.tail_base,
                       bind.tail_mid,
                       0.038F,
                       0.030F));
  nodes.push_back(tube("sheep.tail.tip",
                       Bone::TailTip,
                       k_sheep_role_wool_grubby,
                       bind.tail_mid,
                       bind.tail_tip,
                       0.030F,
                       0.014F));

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
