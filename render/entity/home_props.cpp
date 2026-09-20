#include "home_props.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include "render/equipment/attachment_builder.h"
#include "render/equipment/generated_equipment.h"
#include "render/equipment/humanoid_equipment_archetype.h"
#include "render/equipment/render_archetype_registry.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/humanoid/asset/bind_skeleton.h"
#include "render/humanoid/asset/humanoid_spec.h"
#include "render/humanoid/schema/skeleton_schema.h"

namespace Render::GL {
namespace {

enum BowlSlot : std::uint8_t {
  k_clay_slot = 0U,
  k_broth_slot = 1U,
};
constexpr std::uint8_t k_bowl_role_count = 2U;

auto soup_bowl_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = []() {
    const std::array<GeneratedEquipmentPrimitive, 3> primitives{{
        generated_cylinder(QVector3D(0.0F, 0.020F, 0.045F),
                           QVector3D(0.0F, 0.075F, 0.045F),
                           0.086F,
                           k_clay_slot),
        // The broth sits just under the rim, so it reads as full on the way
        // out and is conspicuously gone afterwards.
        generated_cylinder(QVector3D(0.0F, 0.064F, 0.045F),
                           QVector3D(0.0F, 0.070F, 0.045F),
                           0.074F,
                           k_broth_slot),
        generated_cylinder(QVector3D(0.0F, 0.012F, 0.045F),
                           QVector3D(0.0F, 0.024F, 0.045F),
                           0.042F,
                           k_clay_slot),
    }};
    return build_generated_equipment_archetype("home_soup_bowl", primitives);
  }();
  return archetype;
}

auto soup_bowl_attachments(std::uint8_t base_role)
    -> std::vector<Render::Creature::StaticAttachmentSpec> {
  constexpr auto k_socket = Render::Humanoid::HumanoidSocket::GripR;
  constexpr auto k_bone = Render::Humanoid::HumanoidBone::HandR;
  auto spec = Render::Equipment::build_socket_static_attachment({
      .archetype = &soup_bowl_archetype(),
      .socket_bone_index = static_cast<std::uint16_t>(k_bone),
      .bind_bone_transform =
          Render::Humanoid::humanoid_bind_palette()[static_cast<std::size_t>(k_bone)],
      .bind_socket_transform = Render::Humanoid::bind_socket_transform(k_socket),
      .mesh_from_socket = QMatrix4x4{},
  });
  spec.palette_role_remap[k_clay_slot] = base_role;
  spec.palette_role_remap[k_broth_slot] = static_cast<std::uint8_t>(base_role + 1U);
  return {spec};
}

auto bowl_role_colors(const void* variant_void,
                      QVector3D* out,
                      std::uint32_t base_count,
                      std::size_t max_count) -> std::uint32_t {
  if (variant_void == nullptr || max_count < base_count + k_bowl_role_count) {
    return base_count;
  }
  out[base_count] = QVector3D(0.55F, 0.35F, 0.24F);      // Red-brown kitchenware.
  out[base_count + 1U] = QVector3D(0.72F, 0.55F, 0.24F); // Lentil broth.
  return base_count + k_bowl_role_count;
}

} // namespace

void register_home_prop_archetypes() {
  auto& registry = RenderArchetypeRegistry::instance();
  registry.register_archetype("home_soup_puddle", [] { (void)soup_puddle_archetype(); });
  registry.register_archetype("home_lamp_glow", [] { (void)lamp_glow_archetype(); });
  registry.register_archetype("home_washing_line", [] { (void)washing_line_archetype(); });
  registry.register_archetype("home_washing_pole", [] { (void)washing_pole_archetype(); });
  registry.register_archetype("home_cloth", [] {
    for (bool indigo : {false, true}) {
      (void)cloth_strip_archetype(indigo);
      (void)cloth_strip_tail_archetype(indigo);
    }
  });
  registry.register_archetype("home_shutter", [] {
    for (bool carthage : {false, true}) {
      (void)shutter_leaf_archetype(carthage);
    }
  });
  registry.register_archetype("home_laundry", [] {
    for (int colour = 0; colour < 4; ++colour) {
      (void)laundry_piece_archetype(colour);
    }
  });
  // The bowl is humanoid equipment, so warming it also warms its contribution.
  registry.register_archetype("home_soup_bowl", [] { (void)home_props(); });
}

auto home_props() -> const HomeProps& {
  static const HomeProps props = []() {
    HomeProps result{};
    auto& registry = EquipmentRegistry::instance();
    registry.register_equipment_id(EquipmentCategory::Weapon, "home_soup_bowl");
    result.soup_bowl =
        registry.resolve_handle(EquipmentCategory::Weapon, "home_soup_bowl");
    if (result.soup_bowl != k_invalid_equipment_handle) {
      register_humanoid_equipment_contribution(
          result.soup_bowl,
          {.build_attachments = &soup_bowl_attachments,
           .append_role_colors = &bowl_role_colors,
           .role_count = k_bowl_role_count});
    }
    return result;
  }();
  return props;
}

auto soup_puddle_archetype() -> const RenderArchetype& {
  // Discs, not flattened spheres: flat ellipsoids shade black in this shader.
  // Several small overlapping lobes read as a splash; one big disc reads as a
  // painted circle.
  static const RenderArchetype archetype = []() {
    RenderArchetypeBuilder builder{"home_soup_puddle"};
    const QVector3D broth(0.42F, 0.30F, 0.15F);
    const QVector3D thin = broth * 0.82F;
    struct Lobe {
      QVector3D at;
      float radius;
      QVector3D color;
    };
    const std::array<Lobe, 5> lobes{{
        {QVector3D(0.0F, 0.0F, 0.0F), 0.165F, broth},
        {QVector3D(0.13F, 0.0F, 0.07F), 0.095F, broth},
        {QVector3D(-0.10F, 0.0F, -0.08F), 0.078F, thin},
        {QVector3D(0.19F, 0.0F, -0.11F), 0.046F, thin},
        {QVector3D(-0.17F, 0.0F, 0.13F), 0.038F, thin},
    }};
    for (const auto& lobe : lobes) {
      builder.add_cylinder(lobe.at + QVector3D(0.0F, 0.004F, 0.0F),
                           lobe.at + QVector3D(0.0F, 0.011F, 0.0F),
                           lobe.radius,
                           lobe.color);
    }
    return std::move(builder).build();
  }();
  return archetype;
}

namespace {
auto build_cloth_link(const QVector3D& cloth, bool tail) -> RenderArchetype {
  RenderArchetypeBuilder builder{tail ? "home_cloth_tail" : "home_cloth_strip"};
  // Authored hanging from the origin so a rotation about x/z at the top edge
  // swings the free end, the way a hung cloth actually moves.
  const float length = k_cloth_link_length;
  builder.add_box(QVector3D(0.0F, -length * 0.5F, 0.0F),
                  QVector3D(0.055F, length * 0.5F + 0.004F, 0.008F),
                  tail ? cloth * 0.94F : cloth);
  if (tail) {
    // A heavier hem, which is what gives the swing its weight.
    builder.add_box(QVector3D(0.0F, -length - 0.008F, 0.0F),
                    QVector3D(0.052F, 0.012F, 0.010F),
                    cloth * 0.76F);
  }
  return std::move(builder).build();
}
constexpr QVector3D k_punic_cloth{0.21F, 0.24F, 0.46F};
constexpr QVector3D k_roman_cloth{0.53F, 0.17F, 0.14F};
} // namespace

auto cloth_strip_archetype(bool indigo) -> const RenderArchetype& {
  static const RenderArchetype punic = build_cloth_link(k_punic_cloth, false);
  static const RenderArchetype roman = build_cloth_link(k_roman_cloth, false);
  return indigo ? punic : roman;
}

auto cloth_strip_tail_archetype(bool indigo) -> const RenderArchetype& {
  static const RenderArchetype punic = build_cloth_link(k_punic_cloth, true);
  static const RenderArchetype roman = build_cloth_link(k_roman_cloth, true);
  return indigo ? punic : roman;
}

auto shutter_leaf_archetype(bool carthage) -> const RenderArchetype& {
  // A unit leaf: one wide, one tall, hinged along the origin's vertical axis.
  // Two thin battens across it so it reads as boards rather than a plank.
  static const auto build = [](const QVector3D& wood, const QVector3D& batten) {
    RenderArchetypeBuilder builder{"home_shutter_leaf"};
    builder.add_box(QVector3D(0.0F, 0.0F, 0.5F), QVector3D(0.012F, 0.5F, 0.5F), wood);
    for (const float y : {-0.28F, 0.28F}) {
      builder.add_box(
          QVector3D(0.014F, y, 0.5F), QVector3D(0.006F, 0.05F, 0.44F), batten);
    }
    return std::move(builder).build();
  };
  static const RenderArchetype roman =
      build(QVector3D(0.36F, 0.24F, 0.14F), QVector3D(0.30F, 0.19F, 0.11F));
  static const RenderArchetype punic =
      build(QVector3D(0.30F, 0.22F, 0.15F), QVector3D(0.24F, 0.17F, 0.12F));
  return carthage ? punic : roman;
}

auto laundry_piece_archetype(int colour) -> const RenderArchetype& {
  // Undyed linen, ochre, indigo and madder: what a Mediterranean household
  // actually hung out. A unit piece, hung from the origin, scaled per piece.
  static const auto build = [](const QVector3D& cloth) {
    RenderArchetypeBuilder builder{"home_laundry_piece"};
    builder.add_box(QVector3D(0.0F, -0.5F, 0.0F), QVector3D(0.5F, 0.5F, 0.006F), cloth);
    // Folded over the line.
    builder.add_box(
        QVector3D(0.0F, -0.02F, 0.0F), QVector3D(0.5F, 0.024F, 0.014F), cloth * 0.9F);
    return std::move(builder).build();
  };
  static const std::array<RenderArchetype, k_laundry_colours> pieces{{
      build(QVector3D(0.80F, 0.74F, 0.60F)),
      build(QVector3D(0.66F, 0.46F, 0.22F)),
      build(QVector3D(0.22F, 0.25F, 0.46F)),
      build(QVector3D(0.55F, 0.19F, 0.15F)),
  }};
  return pieces[static_cast<std::size_t>(std::clamp(colour, 0, k_laundry_colours - 1))];
}

auto washing_line_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = []() {
    RenderArchetypeBuilder builder{"home_washing_line"};
    builder.add_cylinder(QVector3D(0.0F, 0.0F, 0.0F),
                         QVector3D(1.0F, 0.0F, 0.0F),
                         0.007F,
                         QVector3D(0.62F, 0.56F, 0.44F));
    return std::move(builder).build();
  }();
  return archetype;
}

auto washing_pole_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = []() {
    RenderArchetypeBuilder builder{"home_washing_pole"};
    builder.add_cylinder(QVector3D(0.0F, 0.0F, 0.0F),
                         QVector3D(0.0F, 1.0F, 0.0F),
                         0.022F,
                         QVector3D(0.44F, 0.33F, 0.20F));
    return std::move(builder).build();
  }();
  return archetype;
}

auto lamp_glow_archetype() -> const RenderArchetype& {
  // Discs on the ground rather than a billboard: the effect shader is unlit
  // and a glowing quad in mid-air reads as a bug at this camera angle.
  static const RenderArchetype archetype = []() {
    RenderArchetypeBuilder builder{"home_lamp_glow"};
    const QVector3D warm(0.92F, 0.66F, 0.32F);
    builder.add_cylinder(
        QVector3D(0, 0.004F, 0), QVector3D(0, 0.010F, 0), 0.62F, warm * 0.55F);
    builder.add_cylinder(
        QVector3D(0, 0.006F, 0), QVector3D(0, 0.012F, 0), 0.36F, warm * 0.85F);
    builder.add_cylinder(QVector3D(0, 0.008F, 0), QVector3D(0, 0.014F, 0), 0.17F, warm);
    return std::move(builder).build();
  }();
  return archetype;
}

} // namespace Render::GL
