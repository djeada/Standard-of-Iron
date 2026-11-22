#include "render/creature/wildlife/sheep_spec_internal.h"

namespace Render::Wildlife {

namespace sheep_spec_detail {

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
    data.ring_vertices = 12U;
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
    data.ring_vertices = 8U;
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
  nodes.reserve(96U);

  constexpr std::uint8_t k_full = Render::Creature::k_lod_full;

  std::vector<Render::Creature::Quadruped::BarrelRing> body_rings;
  body_rings.reserve(k_body_rings.size());
  for (std::size_t i = 0; i < k_body_rings.size(); ++i) {
    const BodyRing& ring = k_body_rings[i];
    float const fi = static_cast<float>(i);

    float const clump_w =
        1.0F + (0.040F * std::sin(fi * 2.1F)) + (0.024F * std::sin((fi * 3.7F) + 1.3F));
    float const clump_t = 1.0F + (0.045F * std::sin((fi * 2.6F) + 0.7F)) +
                          (0.022F * std::sin((fi * 4.3F) + 2.1F));
    body_rings.push_back({ring.z,
                          ring.y,
                          ring.half_width * clump_w,
                          ring.top * clump_t,
                          ring.bottom * clump_w});
  }
  nodes.push_back(
      barrel("sheep.fleece", Bone::Body, k_sheep_role_wool, std::move(body_rings)));

  for (std::size_t i = 2U; i + 2U < k_body_rings.size(); i += 3U) {
    auto const& ring = k_body_rings[i];
    for (float sign : {-1.0F, 1.0F}) {
      nodes.push_back(
          ellipsoid("sheep.fleece.lock",
                    Bone::Body,
                    k_sheep_role_wool,
                    {sign * ring.half_width * 0.58F, ring.y + ring.top * 0.46F, ring.z},
                    {0.060F, 0.058F, 0.082F},
                    k_full));
    }
  }

  for (std::size_t i = 3U; i + 3U < k_body_rings.size(); i += 3U) {
    auto const& ring = k_body_rings[i];
    float const swell = 1.0F + (0.12F * std::sin(static_cast<float>(i) * 1.7F));
    for (float sign : {-1.0F, 1.0F}) {
      nodes.push_back(ellipsoid(
          "sheep.fleece.flank",
          Bone::Body,
          k_sheep_role_wool,
          {sign * ring.half_width * 0.84F, ring.y - (ring.bottom * 0.26F), ring.z},
          QVector3D(0.052F, 0.064F, 0.076F) * swell,
          k_full));
    }
  }
  for (std::size_t i = 4U; i + 4U < k_body_rings.size(); i += 3U) {
    auto const& ring = k_body_rings[i];
    float const lean = 0.022F * std::sin(static_cast<float>(i) * 2.3F);
    float const swell =
        1.0F + (0.10F * std::sin((static_cast<float>(i) * 1.3F) + 0.8F));
    nodes.push_back(ellipsoid("sheep.fleece.crest",
                              Bone::Body,
                              k_sheep_role_wool_light,
                              {lean, ring.y + (ring.top * 0.84F), ring.z},
                              QVector3D(0.074F, 0.042F, 0.080F) * swell,
                              k_full));
  }

  nodes.push_back(ellipsoid("sheep.belly",
                            Bone::Body,
                            k_sheep_role_wool_grubby,
                            {0.0F, 0.286F, -0.010F},
                            {0.092F, 0.042F, 0.186F}));
  nodes.push_back(ellipsoid("sheep.brisket",
                            Bone::Body,
                            k_sheep_role_wool,
                            {0.0F, 0.352F, 0.220F},
                            {0.076F, 0.068F, 0.062F}));
  nodes.push_back(ellipsoid("sheep.ruff",
                            Bone::Body,
                            k_sheep_role_wool,
                            {0.0F, 0.462F, 0.240F},
                            {0.098F, 0.096F, 0.084F}));

  QVector3D const neck_mid = (bind.withers + bind.poll) * 0.5F;
  nodes.push_back(tube("sheep.neck.lower",
                       Bone::NeckTop,
                       k_sheep_role_wool,
                       bind.withers,
                       neck_mid,
                       0.094F,
                       0.078F));
  nodes.push_back(tube("sheep.neck.upper",
                       Bone::NeckTop,
                       k_sheep_role_wool,
                       neck_mid,
                       bind.poll,
                       0.078F,
                       0.064F));

  QVector3D const facing = (bind.muzzle - bind.poll).normalized();
  QVector3D const head_up =
      QVector3D::crossProduct(facing, QVector3D(1.0F, 0.0F, 0.0F)).normalized();
  QVector3D const side = QVector3D::crossProduct(head_up, facing).normalized();

  nodes.push_back(ellipsoid("sheep.cranium",
                            Bone::Head,
                            k_sheep_role_face,
                            bind.poll + (facing * 0.036F) + (head_up * 0.008F),
                            {0.072F, 0.070F, 0.082F}));

  nodes.push_back(ellipsoid("sheep.poll_wool",
                            Bone::Head,
                            k_sheep_role_wool_light,
                            bind.poll - (facing * 0.040F) + (head_up * 0.014F),
                            {0.076F, 0.054F, 0.066F}));
  nodes.push_back(ellipsoid("sheep.cheek_wool",
                            Bone::NeckTop,
                            k_sheep_role_wool,
                            bind.poll - ((bind.poll - bind.withers) * 0.30F),
                            {0.084F, 0.078F, 0.072F}));
  nodes.push_back(ellipsoid("sheep.neck_wool",
                            Bone::NeckTop,
                            k_sheep_role_wool,
                            bind.poll - ((bind.poll - bind.withers) * 0.66F) +
                                QVector3D(0.0F, -0.020F, 0.024F),
                            {0.098F, 0.092F, 0.082F}));

  {
    MeshNode node;
    node.debug_name = "sheep.muzzle";
    node.anchor_bone = bone_index(Bone::Head);
    node.color_role = k_sheep_role_face;
    SnoutNode data;
    data.start = bind.poll + (facing * 0.058F);
    data.end = bind.muzzle;
    data.base_radius = 0.056F;
    data.tip_radius = 0.037F;
    if (!g_minimal_tessellation) {
      data.ring_vertices = 8U;
    }
    node.data = data;
    nodes.push_back(node);
  }
  nodes.push_back(ellipsoid("sheep.nose_bridge",
                            Bone::Head,
                            k_sheep_role_face,
                            bind.poll + (facing * 0.096F) + (head_up * 0.016F),
                            {0.038F, 0.038F, 0.050F}));
  nodes.push_back(ellipsoid("sheep.jowl",
                            Bone::Head,
                            k_sheep_role_face,
                            bind.poll + (facing * 0.070F) - (head_up * 0.026F),
                            {0.042F, 0.040F, 0.048F}));
  nodes.push_back(ellipsoid("sheep.nose",
                            Bone::Head,
                            k_sheep_role_nose,
                            bind.muzzle + (facing * 0.006F),
                            {0.035F, 0.029F, 0.027F}));
  nodes.push_back(tube("sheep.jaw",
                       Bone::Jaw,
                       k_sheep_role_face,
                       bind.jaw_hinge,
                       bind.jaw_tip,
                       0.034F,
                       0.025F));
  for (float sign : {-1.0F, 1.0F}) {
    nodes.push_back(ellipsoid("sheep.eye",
                              Bone::Head,
                              k_sheep_role_eye,
                              bind.poll + (facing * 0.052F) + (head_up * 0.020F) +
                                  (side * (sign * 0.066F)),
                              {0.016F, 0.017F, 0.018F},
                              k_full));
  }

  for (float sign : {-1.0F, 1.0F}) {
    nodes.push_back(ellipsoid("sheep.nostril",
                              Bone::Head,
                              k_sheep_role_eye,
                              bind.muzzle + facing * 0.018F + side * (sign * 0.022F) +
                                  head_up * 0.012F,
                              {0.008F, 0.007F, 0.009F},
                              k_full));
  }

  nodes.push_back(ear_flap("sheep.ear_l",
                           Bone::EarL,
                           k_sheep_role_face,
                           bind.ear_base_l,
                           bind.ear_tip_l,
                           head_up,
                           0.043F));
  nodes.push_back(ear_flap("sheep.ear_r",
                           Bone::EarR,
                           k_sheep_role_face,
                           bind.ear_base_r,
                           bind.ear_tip_r,
                           head_up,
                           0.043F));

  constexpr std::array<Bone, k_leg_count> k_shoulders{
      Bone::ShoulderFL, Bone::ShoulderFR, Bone::ShoulderBL, Bone::ShoulderBR};
  constexpr std::array<Bone, k_leg_count> k_knees{
      Bone::KneeFL, Bone::KneeFR, Bone::KneeBL, Bone::KneeBR};
  constexpr std::array<Bone, k_leg_count> k_feet{
      Bone::FootFL, Bone::FootFR, Bone::FootBL, Bone::FootBR};

  for (std::size_t i = 0; i < k_leg_count; ++i) {
    const LegJoints& joints = bind.legs[i];
    bool const hind = i >= 2U;

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
                         0.052F,
                         0.029F));
    nodes.push_back(ellipsoid("sheep.leg.knee",
                              k_knees[i],
                              k_sheep_role_face,
                              joints.knee,
                              {0.027F, 0.029F, 0.029F},
                              k_full));
    nodes.push_back(tube("sheep.leg.lower",
                         k_knees[i],
                         k_sheep_role_face,
                         joints.knee,
                         joints.foot,
                         0.026F,
                         0.021F));
    nodes.push_back(tube("sheep.leg.hoof",
                         k_feet[i],
                         k_sheep_role_hoof,
                         joints.foot,
                         joints.toe,
                         0.023F,
                         0.028F));
    for (float sign : {-1.0F, 1.0F}) {
      nodes.push_back(ellipsoid("sheep.cloven_hoof",
                                k_feet[i],
                                k_sheep_role_hoof,
                                joints.toe + QVector3D(sign * 0.014F, 0.016F, 0.012F),
                                {0.012F, 0.020F, 0.030F},
                                k_full));
    }
  }

  nodes.push_back(tube("sheep.tail.base",
                       Bone::TailBase,
                       k_sheep_role_wool,
                       bind.tail_base,
                       bind.tail_mid,
                       0.044F,
                       0.038F));
  nodes.push_back(tube("sheep.tail.tip",
                       Bone::TailTip,
                       k_sheep_role_wool,
                       bind.tail_mid,
                       bind.tail_tip,
                       0.038F,
                       0.022F));
  nodes.push_back(ellipsoid("sheep.tail.tuft",
                            Bone::TailTip,
                            k_sheep_role_wool_grubby,
                            bind.tail_tip + QVector3D(0.0F, -0.006F, 0.0F),
                            {0.024F, 0.030F, 0.024F},
                            k_full));

  std::erase_if(nodes, [wanted_lod](const MeshNode& node) {
    return (node.lod_mask & wanted_lod) == 0U;
  });
  for (MeshNode& node : nodes) {
    node.smooth_shading = true;
  }
  g_minimal_tessellation = false;
  return nodes;
}

} // namespace sheep_spec_detail

} // namespace Render::Wildlife
