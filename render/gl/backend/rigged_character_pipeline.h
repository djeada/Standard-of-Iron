#pragma once

// Stage 15.5b / 16.2 / 16.4 — RiggedCharacterPipeline.
//
// GPU-skinning pipeline that consumes RiggedCreatureCmd and drives the
// `character_skinned` shader. Bone palettes are uploaded once per frame
// by `BonePaletteArena::flush_to_gpu()` into pooled UBOs; this pipeline
// only binds a 4096-byte slice of one such UBO via `glBindBufferRange`
// at the kBonePaletteBindingPoint slot before each draw.
//
// Stage 16.4 adds an instanced fast path: contiguous runs of
// RiggedCreatureCmds sharing (mesh, material, no-texture) are coalesced
// into a single `glDrawElementsInstanced` against the
// `character_skinned_instanced` program, reading per-instance world,
// color/alpha and variation/material from divisor=1 vertex attribs and
// indexing the bone palette as `bones[gl_InstanceID * 64 + bone_idx]`.

#include "../shader.h"
#include "pipeline_interface.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace Render::GL {
class ShaderCache;
class Backend;
class Texture;
class Buffer;
class VertexArray;
class RiggedMesh;
struct RiggedCreatureCmd;
} // namespace Render::GL

namespace Render::GL::BackendPipelines {

class RiggedCharacterPipeline final : public IPipeline {
public:
  static constexpr std::size_t k_max_bones = 64;
  // Hard ceiling matching BonePaletteArena::kSlotsPerSlab (= 64): a
  // single instanced draw must fit inside one slab so its UBO range
  // is contiguous. The actual per-instance batch cap is set at init
  // from GL_MAX_UNIFORM_BLOCK_SIZE / (k_max_bones * sizeof(mat4)).
  static constexpr std::size_t k_instanced_batch_ceiling = 64;
  // Conservative default if querying GL_MAX_UNIFORM_BLOCK_SIZE fails;
  // matches the GL 3.3 spec floor of 16 KiB (= 4 instances * 64 bones).
  static constexpr std::size_t k_instanced_batch_floor = 4;

  RiggedCharacterPipeline(GL::Backend *backend, GL::ShaderCache *shader_cache)
      : m_backend(backend), m_shader_cache(shader_cache) {}
  ~RiggedCharacterPipeline() override { shutdown(); }

  auto initialize() -> bool override;
  void shutdown() override;
  void cache_uniforms() override;
  [[nodiscard]] auto is_initialized() const -> bool override;

  // Single-cmd dispatch (textured or 1-deep groups). Kept identical to
  // pre-16.4 behaviour.
  auto draw(const RiggedCreatureCmd &cmd, const QMatrix4x4 &view_proj) -> bool;

  // Stage 16.4 — issue ONE glDrawElementsInstanced for `count`
  // adjacent commands that all share `cmds[0].mesh`, `cmds[0].material`
  // and have `texture == nullptr`. Returns false if preconditions
  // aren't met or the instanced shader / arena allocation failed (in
  // which case the caller MUST fall back to per-cmd `draw()` for the
  // group).
  auto draw_instanced(const RiggedCreatureCmd *cmds, std::size_t count,
                      const QMatrix4x4 &view_proj) -> bool;

  [[nodiscard]] auto shader() const -> GL::Shader * { return m_shader; }
  [[nodiscard]] auto instanced_shader() const -> GL::Shader * {
    return m_instanced_shader;
  }
  [[nodiscard]] auto max_instances_per_batch() const noexcept -> std::size_t {
    return m_max_instances_per_batch;
  }

  // Stage 16.4 — debug introspection for the coalescer test. Records
  // the size of every batch issued since the last `reset_batch_stats()`
  // call. Always available (does not depend on GL).
  void reset_batch_stats() noexcept {
    m_batch_sizes.clear();
    m_last_instance_count = 0;
  }
  [[nodiscard]] auto batch_sizes_for_test() const noexcept
      -> const std::vector<std::size_t> & {
    return m_batch_sizes;
  }
  [[nodiscard]] auto last_instance_count_for_test() const noexcept
      -> std::size_t {
    return m_last_instance_count;
  }

  // Stage 16.4 — pure helper exposed for testing. Walks `cmds` and
  // emits sizes of contiguous instanceable groups (same mesh, same
  // material, texture == nullptr, capped at `cap`). Single-cmd /
  // textured / heterogeneous tail items get their own size-1 group
  // recorded in `out_groups` so the caller can map cmds 1:1 to a
  // dispatch decision.
  static void compute_groups(const RiggedCreatureCmd *cmds, std::size_t count,
                             std::size_t cap,
                             std::vector<std::size_t> &out_groups);

  struct Uniforms {
    GL::Shader::UniformHandle view_proj{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle model{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle variation_scale{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle color{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle alpha{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle use_texture{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle texture{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle material_id{GL::Shader::InvalidUniform};
  };

  [[nodiscard]] auto uniforms() const -> const Uniforms & { return m_uniforms; }

private:
  GL::Backend *m_backend = nullptr;
  GL::ShaderCache *m_shader_cache = nullptr;
  GL::Shader *m_shader = nullptr;
  GL::Shader *m_instanced_shader = nullptr;
  // Owns the instanced shader (compiled from substituted source).
  // The non-instanced `m_shader` is owned by the ShaderCache.
  std::unique_ptr<Shader> m_instanced_shader_storage;
  Uniforms m_uniforms{};

  GL::Shader::UniformHandle m_instanced_view_proj{GL::Shader::InvalidUniform};

  std::size_t m_max_instances_per_batch = 0;

  // Pipeline-owned per-instance attribute VBO. One ring buffer reused
  // across all batches in a frame; resized (orphaned via glBufferData
  // null) only when a batch needs more bytes than the current capacity.
  unsigned int m_instance_vbo = 0;
  std::size_t m_instance_vbo_capacity_bytes = 0;

  // Stage 16.4 — pipeline-owned UBO that backs the per-batch contiguous
  // bone palette range for instanced draws. Sized at init for
  // `m_max_instances_per_batch * BonePaletteArena::kPaletteBytes`. We
  // do NOT reuse the BonePaletteArena's slabs for instanced batches:
  // the per-cmd palettes the arena handed out at submit time are
  // generally non-contiguous, so the pipeline copies each batch's
  // palettes from their CPU pointers (cmd.bone_palette) into this UBO.
  unsigned int m_palette_ubo = 0;
  std::size_t m_palette_ubo_capacity_bytes = 0;
  std::vector<float> m_palette_scratch;

  // Cached instanced VAOs keyed by source RiggedMesh pointer. Each
  // VAO references the mesh's vertex/index buffers + the pipeline's
  // shared instance VBO.
  struct InstancedVaoEntry {
    unsigned int vao = 0;
  };
  std::unordered_map<void *, InstancedVaoEntry> m_instanced_vaos;

  std::vector<std::size_t> m_batch_sizes;
  std::size_t m_last_instance_count = 0;

  // Per-instance attribute layout (matches shader locations 5..10).
  // Packed: 4 vec4 columns (i_world) + vec4 (color/alpha) + vec4
  // (variation/material) = 24 floats = 96 bytes.
  struct InstanceAttrib {
    float world[16];
    float color_alpha[4];
    float variation_material[4];
  };
  std::vector<InstanceAttrib> m_instance_scratch;

  auto build_instanced_shader_source() -> bool;
  auto ensure_instance_vbo(std::size_t bytes_needed) -> bool;
  auto ensure_instanced_vao(RiggedMesh &mesh) -> unsigned int;
};

} // namespace Render::GL::BackendPipelines
