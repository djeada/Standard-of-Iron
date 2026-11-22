#include "render/equipment/weapons/sword_renderer_internal.h"

namespace Render::GL {

using namespace sword_renderer_detail;

namespace {

auto held_prop_attachment(const RenderArchetype& archetype)
    -> Render::Creature::StaticAttachmentSpec {
  constexpr auto k_bone = Render::Humanoid::HumanoidBone::HandR;
  QMatrix4x4 const bind_bone =
      Render::Humanoid::humanoid_bind_palette()[static_cast<std::size_t>(k_bone)];
  auto const& bind_grip = Render::Humanoid::humanoid_bind_body_frames().grip_r;
  return Render::Equipment::build_socket_static_attachment({
      .archetype = &archetype,
      .socket_bone_index = static_cast<std::uint16_t>(k_bone),
      .bind_bone_transform = bind_bone,
      .bind_socket_transform = hand_basis_transform(QMatrix4x4{}, bind_grip),
      .mesh_from_socket = sword_local_pose(k_sword_blade_axis_in_grip),
  });
}

auto scroll_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = [] {
    RenderArchetypeBuilder builder{"held_scroll"};
    builder.add_palette_mesh(
        get_unit_cylinder(),
        Render::Geom::cylinder_between(
            QVector3D(0.0F, -0.12F, 0.0F), QVector3D(0.0F, 0.12F, 0.0F), 0.034F),
        0U);
    for (float y : {-0.13F, 0.13F}) {
      builder.add_palette_mesh(
          get_unit_sphere(),
          Render::Geom::sphere_at(QVector3D(0.0F, y, 0.0F), 0.022F),
          1U);
    }
    return std::move(builder).build();
  }();
  return archetype;
}

auto wax_tablet_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = [] {
    RenderArchetypeBuilder builder{"held_wax_tablet"};
    builder.add_palette_box(
        QVector3D(0.0F, 0.07F, 0.03F), QVector3D(0.012F, 0.11F, 0.085F), 1U);
    builder.add_palette_box(
        QVector3D(0.008F, 0.07F, 0.03F), QVector3D(0.008F, 0.095F, 0.07F), 0U);
    return std::move(builder).build();
  }();
  return archetype;
}

auto cuff_archetype(bool left) -> RenderArchetype {
  using Render::Humanoid::HumanoidBone;
  auto const palette = Render::Humanoid::humanoid_bind_palette();
  QVector3D const hand = palette[static_cast<std::size_t>(left ? HumanoidBone::HandL
                                                               : HumanoidBone::HandR)]
                             .column(3)
                             .toVector3D();
  QVector3D const forearm =
      palette[static_cast<std::size_t>(left ? HumanoidBone::ForearmL
                                            : HumanoidBone::ForearmR)]
          .column(3)
          .toVector3D();
  QVector3D const axis = forearm - hand;
  RenderArchetypeBuilder builder{left ? "manacle_l" : "manacle_r"};
  builder.add_palette_mesh(
      get_unit_cylinder(),
      Render::Geom::cylinder_between(hand + axis * 0.10F, hand + axis * 0.24F, 0.05F),
      0U);
  QVector3D const ring = hand + axis * 0.17F + QVector3D(0.0F, -0.045F, 0.0F);
  for (int link = 0; link < 3; ++link) {
    builder.add_palette_mesh(
        get_unit_sphere(),
        Render::Geom::sphere_at(
            ring + QVector3D(0.0F, -0.03F * static_cast<float>(link), 0.0F), 0.018F),
        0U);
  }
  return std::move(builder).build();
}

} // namespace

auto held_scroll_make_static_attachment(std::uint8_t base_role_byte)
    -> Render::Creature::StaticAttachmentSpec {
  auto spec = held_prop_attachment(scroll_archetype());
  spec.palette_role_remap[0] = base_role_byte;
  spec.palette_role_remap[1] = static_cast<std::uint8_t>(base_role_byte + 1U);
  return spec;
}

auto held_wax_tablet_make_static_attachment(std::uint8_t base_role_byte)
    -> Render::Creature::StaticAttachmentSpec {
  auto spec = held_prop_attachment(wax_tablet_archetype());
  spec.palette_role_remap[0] = base_role_byte;
  spec.palette_role_remap[1] = static_cast<std::uint8_t>(base_role_byte + 1U);
  return spec;
}

auto manacles_make_static_attachments(std::uint8_t base_role_byte)
    -> std::vector<Render::Creature::StaticAttachmentSpec> {
  using Render::Humanoid::HumanoidBone;
  static const RenderArchetype left = cuff_archetype(true);
  static const RenderArchetype right = cuff_archetype(false);
  std::vector<Render::Creature::StaticAttachmentSpec> out;
  for (auto const& [archetype, bone] : {std::pair{&left, HumanoidBone::HandL},
                                        std::pair{&right, HumanoidBone::HandR}}) {
    auto spec = Render::Equipment::build_static_attachment({
        .archetype = archetype,
        .socket_bone_index = static_cast<std::uint16_t>(bone),
    });
    spec.palette_role_remap[0] = base_role_byte;
    out.push_back(spec);
  }
  return out;
}

auto held_prop_fill_role_colors(bool tablet,
                                QVector3D* out,
                                std::size_t max) -> std::uint32_t {
  if (max < 2U) {
    return 0U;
  }
  out[0] = tablet ? QVector3D(0.17F, 0.13F, 0.09F) : QVector3D(0.86F, 0.79F, 0.62F);
  out[1] = tablet ? QVector3D(0.45F, 0.30F, 0.16F) : QVector3D(0.34F, 0.21F, 0.11F);
  return 2U;
}

auto manacles_fill_role_colors(QVector3D* out, std::size_t max) -> std::uint32_t {
  if (max < 1U) {
    return 0U;
  }
  out[0] = QVector3D(0.22F, 0.22F, 0.24F);
  return 1U;
}

} // namespace Render::GL
