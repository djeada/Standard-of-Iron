#include "rigged_mesh_cache.h"

#include "bone_palette_arena.h"
#include "creature/bpat/bpat_format.h"
#include "creature/bpat/bpat_reader.h"
#include "creature/spec.h"

#include <GL/gl.h>
#include <QOpenGLContext>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLVersionFunctionsFactory>
#include <cstring>
#include <limits>
#include <vector>

namespace Render::GL {

namespace {

auto rigged_cache_gl_funcs() -> QOpenGLFunctions_3_3_Core * {
  auto *ctx = QOpenGLContext::currentContext();
  if (ctx == nullptr) {
    return nullptr;
  }
  return QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_3_3_Core>(ctx);
}

} // namespace

void rigged_entry_ensure_skin_atlas(const RiggedMeshEntry &entry,
                                    const QMatrix4x4 *bpat_palettes,
                                    std::uint32_t frame_total,
                                    std::uint32_t bone_count) {
  if (entry.skinned_frame_total == frame_total &&
      entry.skinned_bone_count == bone_count &&
      !entry.skinned_palettes.empty()) {
    return;
  }
  if (frame_total == 0 || bone_count == 0 || bpat_palettes == nullptr) {
    return;
  }
  if (bone_count > entry.inverse_bind.size()) {
    bone_count = static_cast<std::uint32_t>(entry.inverse_bind.size());
  }
  entry.skinned_palettes.assign(
      static_cast<std::size_t>(frame_total) * bone_count, QMatrix4x4{});
  for (std::uint32_t f = 0; f < frame_total; ++f) {
    const QMatrix4x4 *src =
        bpat_palettes + static_cast<std::size_t>(f) * bone_count;
    QMatrix4x4 *dst = entry.skinned_palettes.data() +
                      static_cast<std::size_t>(f) * bone_count;
    for (std::uint32_t b = 0; b < bone_count; ++b) {
      dst[b] = src[b] * entry.inverse_bind[b];
    }
  }
  entry.skinned_frame_total = frame_total;
  entry.skinned_bone_count = bone_count;
}

void rigged_entry_ensure_skin_ubo(const RiggedMeshEntry &entry) {
  if (entry.skin_palette_ubo != 0) {
    return;
  }
  if (entry.skinned_palettes.empty() || entry.skinned_frame_total == 0 ||
      entry.skinned_bone_count == 0) {
    return;
  }
  auto *fn = rigged_cache_gl_funcs();
  if (fn == nullptr) {
    return;
  }

  const std::size_t stride = BonePaletteArena::kPaletteBytes;
  std::vector<float> staging(
      static_cast<std::size_t>(entry.skinned_frame_total) *
          BonePaletteArena::kPaletteFloats,
      0.0F);
  for (std::uint32_t f = 0; f < entry.skinned_frame_total; ++f) {
    const QMatrix4x4 *frame_src =
        entry.skinned_palettes.data() +
        static_cast<std::size_t>(f) * entry.skinned_bone_count;

    float *frame_dst = staging.data() + static_cast<std::size_t>(f) *
                                            BonePaletteArena::kPaletteFloats;
    for (std::uint32_t b = 0; b < entry.skinned_bone_count; ++b) {
      std::memcpy(frame_dst + b * BonePaletteArena::kMatrixFloats,
                  frame_src[b].constData(),
                  sizeof(float) * BonePaletteArena::kMatrixFloats);
    }

    for (std::uint32_t b = entry.skinned_bone_count;
         b < BonePaletteArena::kPaletteWidth; ++b) {
      QMatrix4x4 ident;
      std::memcpy(frame_dst + b * BonePaletteArena::kMatrixFloats,
                  ident.constData(),
                  sizeof(float) * BonePaletteArena::kMatrixFloats);
    }
  }
  GLuint ubo = 0;
  fn->glGenBuffers(1, &ubo);
  if (ubo == 0) {
    return;
  }
  fn->glBindBuffer(GL_UNIFORM_BUFFER, ubo);
  fn->glBufferData(GL_UNIFORM_BUFFER,
                   static_cast<GLsizeiptr>(staging.size() * sizeof(float)),
                   staging.data(), GL_STATIC_DRAW);
  fn->glBindBuffer(GL_UNIFORM_BUFFER, 0);
  entry.skin_palette_ubo = ubo;
  entry.skin_palette_frame_stride_bytes = stride;
}

auto RiggedMeshCache::get_or_bake(
    const Render::Creature::CreatureSpec &spec,
    Render::Creature::CreatureLOD lod, std::span<const QMatrix4x4> rest_palette,
    std::uint16_t variant_bucket,
    std::span<const Render::Creature::StaticAttachmentSpec> attachments)
    -> const RiggedMeshEntry * {
  return get_or_bake(spec, lod, rest_palette, variant_bucket, attachments, 0);
}

auto RiggedMeshCache::get_or_bake(
    const Render::Creature::CreatureSpec &spec,
    Render::Creature::CreatureLOD lod, std::span<const QMatrix4x4> rest_palette,
    std::uint16_t variant_bucket,
    std::span<const Render::Creature::StaticAttachmentSpec> attachments,
    std::uint32_t skin_species_id) -> const RiggedMeshEntry * {
  Key const key{&spec, lod, variant_bucket, skin_species_id,
                Render::Creature::static_attachments_hash(attachments.data(),
                                                          attachments.size())};
  if (auto it = m_entries.find(key); it != m_entries.end()) {
    return &it->second;
  }

  RiggedMeshEntry entry;

  Render::Creature::BakeInput input{};
  input.graph = &Render::Creature::part_graph_for(spec, lod);
  input.bind_pose = rest_palette;
  input.attachments = attachments;

  entry.mesh = Render::Creature::bake_rigged_mesh(input);

  entry.inverse_bind.reserve(rest_palette.size());
  for (const auto &m : rest_palette) {
    entry.inverse_bind.push_back(m.inverted());
  }

  auto [it, _] = m_entries.emplace(key, std::move(entry));
  return &it->second;
}

RiggedMeshCache::~RiggedMeshCache() {
  auto *fn = rigged_cache_gl_funcs();
  if (fn == nullptr) {
    return;
  }
  for (auto &kv : m_entries) {
    if (kv.second.skin_palette_ubo != 0) {
      fn->glDeleteBuffers(1, &kv.second.skin_palette_ubo);
      kv.second.skin_palette_ubo = 0;
    }
  }
}

namespace {
auto matrix_from_row_major(std::span<const float> row) -> QMatrix4x4 {
  if (row.size() < 16) {
    return QMatrix4x4{};
  }
  return QMatrix4x4(row.data());
}
} // namespace

void rigged_entry_ensure_skin_atlas_from_blob(
    const RiggedMeshEntry &entry,
    const Render::Creature::Bpat::BpatBlob &blob) {
  if (!blob.loaded()) {
    return;
  }
  const std::uint32_t frame_total = blob.frame_total();
  std::uint32_t bone_count = blob.bone_count();
  if (entry.skinned_frame_total == frame_total &&
      entry.skinned_bone_count == bone_count &&
      !entry.skinned_palettes.empty()) {
    return;
  }
  if (frame_total == 0 || bone_count == 0) {
    return;
  }
  if (bone_count > entry.inverse_bind.size()) {
    bone_count = static_cast<std::uint32_t>(entry.inverse_bind.size());
  }
  entry.skinned_palettes.assign(
      static_cast<std::size_t>(frame_total) * bone_count, QMatrix4x4{});
  for (std::uint32_t f = 0; f < frame_total; ++f) {
    QMatrix4x4 *dst = entry.skinned_palettes.data() +
                      static_cast<std::size_t>(f) * bone_count;
    for (std::uint32_t b = 0; b < bone_count; ++b) {
      auto row = blob.palette_matrix(f, b);
      const QMatrix4x4 raw = matrix_from_row_major(row);
      dst[b] = raw * entry.inverse_bind[b];
    }
  }
  entry.skinned_frame_total = frame_total;
  entry.skinned_bone_count = bone_count;
}

} // namespace Render::GL
