#include "cloak_renderer.h"
#include "torso_local_archetype_utils.h"

#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_spec.h"
#include "../../static_attachment_spec.h"
#include "../attachment_builder.h"
#include "../equipment_submit.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>
#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>

namespace Render::GL {

namespace {

enum CloakPaletteSlot : std::uint8_t {
  k_cloak_cloth_slot = 0U,
  k_cloak_trim_slot = 1U,
};

auto shared_cloak_meshes() -> CloakMeshes {
  static std::unique_ptr<Mesh> back_mesh = create_plane_mesh(1.0F, 1.0F, 16);
  static std::unique_ptr<Mesh> shoulder_mesh =
      create_plane_mesh(1.0F, 1.0F, 12);
  return {back_mesh.get(), shoulder_mesh.get()};
}

auto cloak_archetype(const CloakConfig &config, const CloakMeshes &meshes,
                     const QMatrix4x4 &shoulder_model,
                     const QMatrix4x4 &drape_model,
                     const QMatrix4x4 *clasp_model) -> const RenderArchetype & {
  struct CachedArchetype {
    std::string key;
    RenderArchetype archetype;
  };

  static std::deque<CachedArchetype> cache;
  std::string key = "cloak_";
  key += std::to_string(reinterpret_cast<std::uintptr_t>(meshes.shoulder));
  key.push_back('_');
  key += std::to_string(reinterpret_cast<std::uintptr_t>(meshes.back));
  key.push_back('_');
  append_quantized_key(key, config.back_material_id);
  append_quantized_key(key, config.shoulder_material_id);
  append_quantized_key(key, shoulder_model);
  append_quantized_key(key, drape_model);
  append_quantized_key(key, clasp_model != nullptr ? 1 : 0);
  if (clasp_model != nullptr) {
    append_quantized_key(key, *clasp_model);
  }

  for (const auto &entry : cache) {
    if (entry.key == key) {
      return entry.archetype;
    }
  }

  RenderArchetypeBuilder builder{key};
  builder.add_palette_mesh(meshes.shoulder, shoulder_model, k_cloak_cloth_slot,
                           nullptr, 1.0F, config.shoulder_material_id);
  builder.add_palette_mesh(meshes.back, drape_model, k_cloak_cloth_slot,
                           nullptr, 1.0F, config.back_material_id);
  if (clasp_model != nullptr) {
    builder.add_palette_mesh(get_unit_sphere(), *clasp_model, k_cloak_trim_slot,
                             nullptr, 1.0F, 1);
  }

  cache.push_back({key, std::move(builder).build()});
  return cache.back().archetype;
}

} // namespace

CloakRenderer::CloakRenderer(const CloakConfig &config) : m_config(config) {}

auto CloakRenderer::meshes() const noexcept -> CloakMeshes {
  return shared_cloak_meshes();
}

void CloakRenderer::set_config(const CloakConfig &config) { m_config = config; }

void CloakRenderer::render(const DrawContext &ctx, const BodyFrames &frames,
                           const HumanoidPalette &palette,
                           const HumanoidAnimationContext &anim,
                           EquipmentBatch &batch) {
  submit(m_config, shared_cloak_meshes(), ctx, frames, palette, anim, batch);
}

void CloakRenderer::submit(const CloakConfig &config, const CloakMeshes &meshes,
                           const DrawContext &ctx, const BodyFrames &frames,
                           const HumanoidPalette &palette,
                           const HumanoidAnimationContext &anim,
                           EquipmentBatch &batch) {
  (void)anim;

  const AttachmentFrame &torso = frames.torso;
  const AttachmentFrame &shoulder_l = frames.shoulder_l;
  const AttachmentFrame &shoulder_r = frames.shoulder_r;

  if (torso.radius <= 0.0F || meshes.back == nullptr ||
      meshes.shoulder == nullptr) {
    return;
  }

  QVector3D const cloak_color = palette.cloth;
  QVector3D const trim_color = palette.metal;

  QVector3D const up =
      safe_attachment_axis(torso.up, QVector3D(0.0F, 1.0F, 0.0F));
  QVector3D const forward =
      safe_attachment_axis(torso.forward, QVector3D(0.0F, 0.0F, 1.0F));
  QVector3D const back = -forward;
  TorsoLocalFrame const torso_local = make_torso_local_frame(ctx.model, torso);

  float const torso_r = torso.radius;

  float shoulder_span = (shoulder_r.origin - shoulder_l.origin).length();
  if (shoulder_span < 1e-4F) {
    shoulder_span = torso_r * 3.0F;
  }
  QVector3D const shoulder_mid = (shoulder_l.origin + shoulder_r.origin) * 0.5F;

  float const cape_width = shoulder_span * 1.6F * config.width_scale;
  float const cape_depth = torso_r * 1.8F;
  QVector3D const cape_anchor = shoulder_mid + up * (torso_r * 0.82F);

  QMatrix4x4 shoulder_model;
  shoulder_model.translate(torso_local.point(cape_anchor));
  shoulder_model.scale(cape_width, 1.0F, cape_depth);

  float const drape_width = shoulder_span * 1.22F * config.width_scale;
  float const drape_length = torso_r * 4.2F * config.length_scale;
  QVector3D const drape_anchor =
      shoulder_mid + up * (torso_r * 0.62F) + back * (torso_r * 0.96F);

  QMatrix4x4 drape_model;
  drape_model.translate(torso_local.point(drape_anchor));

  QMatrix4x4 drape_orient;
  drape_orient.setColumn(0, QVector4D(1.0F, 0.0F, 0.0F, 0.0F));
  drape_orient.setColumn(1, QVector4D(0.0F, 0.0F, -1.0F, 0.0F));
  drape_orient.setColumn(2, QVector4D(0.0F, -1.0F, 0.0F, 0.0F));
  drape_orient.setColumn(3, QVector4D(0.0F, 0.0F, 0.0F, 1.0F));
  drape_model = drape_model * drape_orient;
  drape_model.translate(0.0F, 0.0F, drape_length * 0.5F);

  QMatrix4x4 flare;
  flare.setToIdentity();
  flare(0, 2) = 0.35F * 0.35F;
  drape_model = drape_model * flare;
  drape_model.scale(drape_width, 1.0F, drape_length);

  std::array<QVector3D, 2> palette_slots{cloak_color, trim_color};

  if (config.show_clasp) {
    QVector3D const clasp_pos =
        shoulder_mid + up * (torso_r * 0.5F) + forward * (torso_r * 0.2F);
    QMatrix4x4 clasp_model = local_scale_model(
        torso_local.point(clasp_pos),
        QVector3D(torso_r * 0.12F, torso_r * 0.12F, torso_r * 0.12F));
    append_equipment_archetype(batch,
                               cloak_archetype(config, meshes, shoulder_model,
                                               drape_model, &clasp_model),
                               torso_local.world, palette_slots);
    return;
  }

  append_equipment_archetype(
      batch,
      cloak_archetype(config, meshes, shoulder_model, drape_model, nullptr),
      torso_local.world, palette_slots);
}

auto cloak_fill_role_colors_with_primary(const QVector3D &primary_color,
                                         const HumanoidPalette &palette,
                                         QVector3D *out,
                                         std::size_t max) -> std::uint32_t {
  if (max < kCloakRoleCount) {
    return 0U;
  }
  out[0] = primary_color;
  out[1] = palette.metal;
  return kCloakRoleCount;
}

auto cloak_make_static_attachment(
    const CloakConfig &config, const CloakMeshes &meshes,
    std::uint16_t torso_socket_bone_index,
    std::uint8_t base_role_byte) -> Render::Creature::StaticAttachmentSpec {
  const auto &bind_frames = Render::Humanoid::humanoid_bind_body_frames();
  const AttachmentFrame &torso = bind_frames.torso;
  const AttachmentFrame &shoulder_l = bind_frames.shoulder_l;
  const AttachmentFrame &shoulder_r = bind_frames.shoulder_r;

  TorsoLocalFrame const torso_local =
      make_torso_local_frame(QMatrix4x4{}, torso);

  QVector3D const up =
      safe_attachment_axis(torso.up, QVector3D(0.0F, 1.0F, 0.0F));
  QVector3D const forward =
      safe_attachment_axis(torso.forward, QVector3D(0.0F, 0.0F, 1.0F));
  QVector3D const back = -forward;

  float const torso_r = torso.radius;
  float shoulder_span = (shoulder_r.origin - shoulder_l.origin).length();
  if (shoulder_span < 1e-4F) {
    shoulder_span = torso_r * 3.0F;
  }
  QVector3D const shoulder_mid = (shoulder_l.origin + shoulder_r.origin) * 0.5F;

  float const cape_width = shoulder_span * 1.6F * config.width_scale;
  float const cape_depth = torso_r * 1.8F;
  QVector3D const cape_anchor = shoulder_mid + up * (torso_r * 0.82F);

  QMatrix4x4 shoulder_model;
  shoulder_model.translate(torso_local.point(cape_anchor));
  shoulder_model.scale(cape_width, 1.0F, cape_depth);

  float const drape_width = shoulder_span * 1.22F * config.width_scale;
  float const drape_length = torso_r * 4.2F * config.length_scale;
  QVector3D const drape_anchor =
      shoulder_mid + up * (torso_r * 0.62F) + back * (torso_r * 0.96F);

  QMatrix4x4 drape_model;
  drape_model.translate(torso_local.point(drape_anchor));

  QMatrix4x4 drape_orient;
  drape_orient.setColumn(0, QVector4D(1.0F, 0.0F, 0.0F, 0.0F));
  drape_orient.setColumn(1, QVector4D(0.0F, 0.0F, -1.0F, 0.0F));
  drape_orient.setColumn(2, QVector4D(0.0F, -1.0F, 0.0F, 0.0F));
  drape_orient.setColumn(3, QVector4D(0.0F, 0.0F, 0.0F, 1.0F));
  drape_model = drape_model * drape_orient;
  drape_model.translate(0.0F, 0.0F, drape_length * 0.5F);

  QMatrix4x4 flare;
  flare.setToIdentity();
  flare(0, 2) = 0.35F * 0.35F;
  drape_model = drape_model * flare;
  drape_model.scale(drape_width, 1.0F, drape_length);

  const QMatrix4x4 *clasp_ptr = nullptr;
  QMatrix4x4 clasp_model;
  if (config.show_clasp) {
    QVector3D const clasp_pos =
        shoulder_mid + up * (torso_r * 0.5F) + forward * (torso_r * 0.2F);
    clasp_model = local_scale_model(
        torso_local.point(clasp_pos),
        QVector3D(torso_r * 0.12F, torso_r * 0.12F, torso_r * 0.12F));
    clasp_ptr = &clasp_model;
  }

  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &cloak_archetype(config, meshes, shoulder_model, drape_model,
                                    clasp_ptr),
      .socket_bone_index = torso_socket_bone_index,
      .unit_local_pose_at_bind = torso_local.world,
  });
  spec.palette_role_remap[k_cloak_cloth_slot] =
      static_cast<std::uint8_t>(base_role_byte + 0U);
  spec.palette_role_remap[k_cloak_trim_slot] =
      static_cast<std::uint8_t>(base_role_byte + 1U);

  return spec;
}

} // namespace Render::GL
