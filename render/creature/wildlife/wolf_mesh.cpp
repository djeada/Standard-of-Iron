#include "render/creature/wildlife/wolf_spec_internal.h"

namespace Render::Wildlife {

namespace wolf_spec_detail {

bool g_minimal_tessellation = false;

auto ellipsoid(std::string_view name,
               Bone bone,
               std::uint8_t role,
               const QVector3D& center,
               const QVector3D& radii,
               std::uint8_t lod_mask) -> MeshNode {
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
  } else {
    data.ring_count = 7U;
    data.ring_vertices = 10U;
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
          std::uint8_t lod_mask) -> MeshNode {
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
  } else {
    data.ring_vertices = 10U;
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
          std::uint8_t lod_mask) -> MeshNode {
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
  nodes.reserve(80U);

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
                            {0.0F, 0.612F, 0.214F},
                            {0.076F, 0.056F, 0.120F}));
  nodes.push_back(ellipsoid("wolf.chest",
                            Bone::Body,
                            k_wolf_role_pale,
                            {0.0F, 0.326F, 0.186F},
                            {0.066F, 0.034F, 0.150F}));
  nodes.push_back(ellipsoid("wolf.belly",
                            Bone::Body,
                            k_wolf_role_pale,
                            {0.0F, 0.404F, -0.090F},
                            {0.058F, 0.030F, 0.150F}));
  nodes.push_back(ellipsoid("wolf.blaze",
                            Bone::Body,
                            k_wolf_role_cream,
                            {0.0F, 0.408F, 0.350F},
                            {0.072F, 0.084F, 0.050F}));

  QVector3D const neck_axis = (bind.poll - bind.withers).normalized();
  QVector3D const neck_mid = (bind.withers + bind.poll) * 0.5F;
  nodes.push_back(tube("wolf.neck.lower",
                       Bone::NeckTop,
                       k_wolf_role_fur,
                       bind.withers - (neck_axis * 0.030F),
                       neck_mid,
                       0.126F,
                       0.106F));
  nodes.push_back(tube("wolf.neck.upper",
                       Bone::NeckTop,
                       k_wolf_role_fur,
                       neck_mid,
                       bind.poll,
                       0.106F,
                       0.088F));

  nodes.push_back(ellipsoid("wolf.ruff",
                            Bone::NeckTop,
                            k_wolf_role_fur,
                            neck_mid - QVector3D(0.0F, 0.016F, 0.012F),
                            {0.138F, 0.128F, 0.118F}));
  nodes.push_back(ellipsoid("wolf.mane",
                            Bone::NeckTop,
                            k_wolf_role_saddle,
                            neck_mid + QVector3D(0.0F, 0.026F, -0.054F),
                            {0.086F, 0.064F, 0.112F}));
  nodes.push_back(ellipsoid("wolf.throat",
                            Bone::NeckTop,
                            k_wolf_role_cream,
                            neck_mid + QVector3D(0.0F, -0.066F, 0.040F),
                            {0.076F, 0.062F, 0.076F}));

  QVector3D const facing = (bind.muzzle - bind.poll).normalized();
  QVector3D const head_up =
      QVector3D::crossProduct(facing, QVector3D(1.0F, 0.0F, 0.0F)).normalized();
  QVector3D const side = QVector3D::crossProduct(head_up, facing).normalized();

  nodes.push_back(ellipsoid("wolf.skull",
                            Bone::Head,
                            k_wolf_role_fur,
                            bind.poll + (facing * 0.030F),
                            {0.094F, 0.084F, 0.106F}));
  nodes.push_back(ellipsoid("wolf.crown",
                            Bone::Head,
                            k_wolf_role_saddle,
                            bind.poll + (facing * 0.012F) + (head_up * 0.038F),
                            {0.082F, 0.040F, 0.090F}));
  {
    MeshNode node;
    node.debug_name = "wolf.muzzle";
    node.anchor_bone = bone_index(Bone::Head);
    node.color_role = k_wolf_role_pale;
    SnoutNode data;
    data.start = bind.poll + (facing * 0.074F) - (head_up * 0.008F);
    data.end = bind.muzzle;
    data.base_radius = 0.068F;
    data.tip_radius = 0.040F;
    if (!g_minimal_tessellation) {
      data.ring_vertices = 8U;
    }
    node.data = data;
    nodes.push_back(node);
  }
  nodes.push_back(tube("wolf.jaw",
                       Bone::Jaw,
                       k_wolf_role_pale,
                       bind.jaw_hinge,
                       bind.jaw_tip,
                       0.046F,
                       0.030F));
  nodes.push_back(tube("wolf.mouth",
                       Bone::Jaw,
                       k_wolf_role_nose,
                       bind.jaw_hinge + (head_up * 0.017F),
                       bind.jaw_tip + (head_up * 0.013F) - (facing * 0.016F),
                       0.029F,
                       0.016F,
                       k_full));
  nodes.push_back(tube("wolf.bridge",
                       Bone::Head,
                       k_wolf_role_saddle,
                       bind.poll + (facing * 0.066F) + (head_up * 0.032F),
                       bind.muzzle + (head_up * 0.020F) - (facing * 0.008F),
                       0.044F,
                       0.026F));
  nodes.push_back(ellipsoid("wolf.nose",
                            Bone::Head,
                            k_wolf_role_nose,
                            bind.muzzle + (facing * 0.008F) + (head_up * 0.006F),
                            {0.034F, 0.030F, 0.030F}));
  for (float sign : {-1.0F, 1.0F}) {
    nodes.push_back(ellipsoid("wolf.cheek",
                              Bone::Head,
                              k_wolf_role_pale,
                              bind.poll + (facing * 0.026F) + (side * (sign * 0.070F)) -
                                  (head_up * 0.028F),
                              {0.048F, 0.060F, 0.080F}));
    QVector3D const tuft =
        bind.poll - (facing * 0.004F) + (side * (sign * 0.086F)) - (head_up * 0.036F);
    nodes.push_back(
        cone("wolf.cheek_tuft",
             Bone::Head,
             k_wolf_role_pale,
             tuft,
             tuft + (side * (sign * 0.058F)) - (head_up * 0.062F) - (facing * 0.040F),
             0.042F,
             k_full));
    nodes.push_back(ellipsoid("wolf.eye",
                              Bone::Head,
                              k_wolf_role_eye,
                              bind.poll + (facing * 0.066F) + (head_up * 0.036F) +
                                  (side * (sign * 0.064F)),
                              {0.018F, 0.018F, 0.018F},
                              k_full));
    nodes.push_back(ellipsoid("wolf.iris",
                              Bone::Head,
                              k_wolf_role_iris,
                              bind.poll + (facing * 0.075F) + (head_up * 0.036F) +
                                  (side * (sign * 0.071F)),
                              {0.012F, 0.012F, 0.012F},
                              k_full));
  }

  for (float sign : {-1.0F, 1.0F}) {
    nodes.push_back(ellipsoid("wolf.brow",
                              Bone::Head,
                              k_wolf_role_saddle,
                              bind.poll + facing * 0.056F + head_up * 0.054F +
                                  side * (sign * 0.060F),
                              {0.030F, 0.014F, 0.042F},
                              k_full));
    QVector3D const tooth =
        bind.muzzle - facing * 0.044F + side * (sign * 0.030F) - head_up * 0.022F;
    nodes.push_back(cone("wolf.canine",
                         Bone::Head,
                         k_wolf_role_cream,
                         tooth,
                         tooth - head_up * 0.034F,
                         0.009F,
                         k_full));
  }

  nodes.push_back(cone("wolf.ear_l",
                       Bone::EarL,
                       k_wolf_role_saddle,
                       bind.ear_base_l - (head_up * 0.016F),
                       bind.ear_tip_l,
                       0.060F));
  nodes.push_back(cone("wolf.ear_r",
                       Bone::EarR,
                       k_wolf_role_saddle,
                       bind.ear_base_r - (head_up * 0.016F),
                       bind.ear_tip_r,
                       0.060F));
  nodes.push_back(cone("wolf.ear_inner_l",
                       Bone::EarL,
                       k_wolf_role_pale,
                       bind.ear_base_l + (facing * 0.018F),
                       bind.ear_tip_l + (facing * 0.010F) - (head_up * 0.014F),
                       0.036F,
                       k_full));
  nodes.push_back(cone("wolf.ear_inner_r",
                       Bone::EarR,
                       k_wolf_role_pale,
                       bind.ear_base_r + (facing * 0.018F),
                       bind.ear_tip_r + (facing * 0.010F) - (head_up * 0.014F),
                       0.036F,
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
                         hind ? 0.070F : 0.062F,
                         hind ? 0.045F : 0.043F));
    nodes.push_back(ellipsoid("wolf.leg.joint",
                              k_knees[i],
                              k_wolf_role_fur,
                              joints.knee,
                              hind ? QVector3D(0.042F, 0.045F, 0.047F)
                                   : QVector3D(0.040F, 0.043F, 0.043F),
                              k_full));
    nodes.push_back(tube("wolf.leg.mid",
                         k_knees[i],
                         k_wolf_role_limb,
                         joints.knee,
                         joints.foot,
                         hind ? 0.041F : 0.039F,
                         0.032F));
    nodes.push_back(ellipsoid("wolf.leg.hock",
                              k_feet[i],
                              k_wolf_role_limb,
                              joints.foot,
                              {0.032F, 0.035F, 0.036F},
                              k_full));
    nodes.push_back(tube("wolf.leg.lower",
                         k_feet[i],
                         k_wolf_role_limb,
                         joints.foot,
                         joints.toe,
                         0.032F,
                         0.030F));
    nodes.push_back(ellipsoid("wolf.paw",
                              k_feet[i],
                              k_wolf_role_paw,
                              joints.toe + QVector3D(0.0F, 0.004F, 0.004F),
                              {0.045F, 0.031F, 0.060F}));
    for (float toe_side : {-1.0F, 1.0F}) {
      nodes.push_back(
          ellipsoid("wolf.paw.toes",
                    k_feet[i],
                    k_wolf_role_paw,
                    joints.toe + QVector3D(toe_side * 0.021F, 0.000F, 0.034F),
                    {0.020F, 0.022F, 0.032F},
                    k_full));
    }
    nodes.push_back(ellipsoid(
        "wolf.limb_mass",
        k_shoulders[i],
        k_wolf_role_fur,
        hind ? QVector3D(k_leg_plans[i].x * 0.90F, 0.412F, -0.214F)
             : QVector3D(k_leg_plans[i].x * 0.90F, 0.414F, 0.226F),
        hind ? QVector3D(0.072F, 0.126F, 0.120F) : QVector3D(0.064F, 0.120F, 0.098F)));
  }

  QVector3D const tail_axis = (bind.tail_tip - bind.tail_mid).normalized();
  nodes.push_back(tube("wolf.tail.base",
                       Bone::TailBase,
                       k_wolf_role_fur,
                       bind.tail_base,
                       bind.tail_mid,
                       0.048F,
                       0.082F));
  nodes.push_back(tube("wolf.tail.tip",
                       Bone::TailTip,
                       k_wolf_role_fur,
                       bind.tail_mid,
                       bind.tail_tip,
                       0.082F,
                       0.060F));
  nodes.push_back(ellipsoid("wolf.tail.bloom",
                            Bone::TailBase,
                            k_wolf_role_fur,
                            bind.tail_mid,
                            {0.079F, 0.079F, 0.079F}));
  nodes.push_back(ellipsoid("wolf.tail.taper",
                            Bone::TailTip,
                            k_wolf_role_fur,
                            bind.tail_tip,
                            {0.057F, 0.057F, 0.057F}));
  nodes.push_back(ellipsoid("wolf.tail.root",
                            Bone::TailBase,
                            k_wolf_role_fur,
                            bind.tail_base,
                            {0.046F, 0.046F, 0.046F}));
  nodes.push_back(cone("wolf.tail.brush",
                       Bone::TailTip,
                       k_wolf_role_saddle,
                       bind.tail_tip - (tail_axis * 0.010F),
                       bind.tail_tip + (tail_axis * 0.125F),
                       0.060F));

  std::erase_if(nodes, [wanted_lod](const MeshNode& node) {
    return (node.lod_mask & wanted_lod) == 0U;
  });
  for (MeshNode& node : nodes) {
    node.smooth_shading = true;
  }
  g_minimal_tessellation = false;
  return nodes;
}

} // namespace wolf_spec_detail

} // namespace Render::Wildlife
