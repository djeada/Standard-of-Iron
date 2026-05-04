#include "snapshot_mesh_cache.h"

#include "bone_palette_arena.h"
#include "creature/snapshot_mesh_asset.h"
#include "rigged_mesh_cache.h"
#include "snapshot_mesh_bake.h"

#include <GL/gl.h>
#include <QOpenGLContext>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLVersionFunctionsFactory>
#include <QVector3D>
#include <QVector4D>
#include <array>
#include <limits>

namespace Render::GL {

namespace {

auto snapshot_cache_gl_funcs() -> QOpenGLFunctions_3_3_Core * {
  auto *ctx = QOpenGLContext::currentContext();
  if (ctx == nullptr) {
    return nullptr;
  }
  return QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_3_3_Core>(ctx);
}

auto build_identity_palette() noexcept
    -> std::array<QMatrix4x4, BonePaletteArena::kPaletteWidth> {
  std::array<QMatrix4x4, BonePaletteArena::kPaletteWidth> arr{};
  for (auto &m : arr) {
    m.setToIdentity();
  }
  return arr;
}

} // namespace

auto SnapshotMeshCache::identity_palette() noexcept -> const QMatrix4x4 * {
  static const auto kPalette = build_identity_palette();
  return kPalette.data();
}

SnapshotMeshCache::~SnapshotMeshCache() {
  auto *fn = snapshot_cache_gl_funcs();
  if (fn == nullptr || m_identity_palette_ubo == 0) {
    return;
  }
  fn->glDeleteBuffers(1, &m_identity_palette_ubo);
  m_identity_palette_ubo = 0;
}

auto SnapshotMeshCache::identity_palette_ubo() const -> std::uint32_t {
  if (m_identity_palette_ubo != 0) {
    return m_identity_palette_ubo;
  }
  auto *fn = snapshot_cache_gl_funcs();
  if (fn == nullptr) {
    return 0U;
  }

  std::array<float, BonePaletteArena::kPaletteFloats> packed{};
  BonePaletteArena::pack_palette_for_gpu(identity_palette(), packed.data());

  fn->glGenBuffers(1, &m_identity_palette_ubo);
  if (m_identity_palette_ubo == 0) {
    return 0U;
  }

  fn->glBindBuffer(GL_UNIFORM_BUFFER, m_identity_palette_ubo);
  fn->glBufferData(GL_UNIFORM_BUFFER,
                   static_cast<GLsizeiptr>(packed.size() * sizeof(float)),
                   packed.data(), GL_STATIC_DRAW);
  fn->glBindBuffer(GL_UNIFORM_BUFFER, 0);
  return m_identity_palette_ubo;
}

namespace {

auto build_entry(std::span<const RiggedVertex> vertices,
                 std::span<const std::uint32_t> indices) -> SnapshotMeshEntry {
  SnapshotMeshEntry entry{};
  std::vector<RiggedVertex> baked(vertices.begin(), vertices.end());
  std::vector<std::uint32_t> indices_copy(indices.begin(), indices.end());
  entry.mesh =
      std::make_unique<RiggedMesh>(std::move(baked), std::move(indices_copy));
  return entry;
}

} // namespace

auto SnapshotMeshCache::get_or_bake(
    const Key &key, const RiggedMeshEntry &source,
    std::uint32_t global_frame) -> const SnapshotMeshEntry * {
  if (auto it = m_entries.find(key); it != m_entries.end()) {
    return &it->second;
  }

  if (source.mesh == nullptr || source.skinned_palettes.empty() ||
      source.skinned_bone_count == 0U ||
      global_frame >= source.skinned_frame_total) {
    return nullptr;
  }

  const auto &src_vertices = source.mesh->get_vertices();
  if (src_vertices.empty()) {
    return nullptr;
  }
  const auto &src_indices = source.mesh->get_indices();
  if (src_indices.empty()) {
    return nullptr;
  }

  const QMatrix4x4 *frame_palette =
      source.skinned_palettes.data() +
      static_cast<std::size_t>(global_frame) *
          static_cast<std::size_t>(source.skinned_bone_count);
  auto const baked = bake_snapshot_vertices(
      src_vertices,
      {frame_palette, static_cast<std::size_t>(source.skinned_bone_count)});

  SnapshotMeshEntry entry = build_entry(baked, src_indices);
  auto [it, _ok] = m_entries.emplace(key, std::move(entry));
  return &it->second;
}

auto SnapshotMeshCache::get_or_load(
    const Key &key, const Render::Creature::Snapshot::SnapshotMeshBlob &source,
    std::uint32_t global_frame) -> const SnapshotMeshEntry * {
  if (auto it = m_entries.find(key); it != m_entries.end()) {
    return &it->second;
  }

  const auto vertices = source.frame_vertices_view(global_frame);
  const auto indices = source.indices_view();
  if (vertices.empty() || indices.empty()) {
    return nullptr;
  }

  SnapshotMeshEntry entry = build_entry(vertices, indices);
  auto [it, _ok] = m_entries.emplace(key, std::move(entry));
  return &it->second;
}

} // namespace Render::GL
