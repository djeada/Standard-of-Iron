#include "creature_asset_init.h"

#include <sstream>

#include "animation/bpat/bpat_format.h"
#include "animation/bpat/bpat_reader.h"
#include "render/bone_palette_arena.h"
#include "render/creature/archetype_registry.h"
#include "render/rigged_mesh_cache.h"

namespace Render::Creature::Pipeline {

auto rigged_asset_key(const CreatureRenderAssetHandle& handle,
                      Render::Creature::CreatureLOD lod,
                      std::uint32_t skin_species_id) noexcept
    -> Render::GL::RiggedMeshCache::Key {
  return Render::GL::RiggedMeshCache::Key{
      .spec = handle.asset != nullptr ? handle.asset->spec : nullptr,
      .lod = lod,
      .skin_species_id = skin_species_id,
      .attachment_set_id = handle.attachment_set_id,
      .attachments_hash = handle.attachments_hash};
}

auto describe_rigged_asset(const CreatureRenderAssetHandle& handle,
                           Render::Creature::CreatureLOD lod) -> std::string {
  std::ostringstream out;
  out << "asset="
      << static_cast<std::uint32_t>(handle.asset != nullptr ? handle.asset->id
                                                            : k_invalid_creature_asset)
      << " archetype="
      << static_cast<std::uint32_t>(handle.archetype != nullptr
                                        ? handle.archetype->id
                                        : static_cast<Render::Creature::ArchetypeId>(
                                              Render::Creature::k_invalid_archetype))
      << " lod=" << static_cast<int>(lod)
      << " attachment_set_id=" << handle.attachment_set_id << " attachments_hash=0x"
      << std::hex << handle.attachments_hash;
  return out.str();
}

auto create_creature_render_asset(Render::GL::RiggedMeshCache& cache,
                                  const CreatureRenderAssetHandle& handle,
                                  Render::Creature::CreatureLOD lod,
                                  const Render::Creature::Bpat::BpatBlob& blob,
                                  std::uint16_t variant_bucket,
                                  bool upload_skin_ubo)
    -> const Render::GL::RiggedMeshEntry* {
  if (!handle.valid() || lod == Render::Creature::CreatureLOD::Culled) {
    return nullptr;
  }

  const auto key = rigged_asset_key(handle, lod, blob.species_id());
  const auto* entry = cache.create_rigged_asset(
      key, handle.bind_palette, handle.attachments, variant_bucket);
  if (entry == nullptr) {
    return nullptr;
  }

  const auto* atlas = entry->skin_atlas.get();
  const bool had_atlas = atlas != nullptr && atlas->frame_total == blob.frame_total() &&
                         atlas->bone_count != 0U && !atlas->palettes.empty();
  Render::GL::rigged_entry_ensure_skin_atlas_from_blob(*entry, blob);
  atlas = entry->skin_atlas.get();
  if (!had_atlas && atlas != nullptr && atlas->frame_total == blob.frame_total() &&
      atlas->bone_count != 0U && !atlas->palettes.empty()) {
    cache.record_skin_atlas_build();
  }

  if (!upload_skin_ubo) {
    return entry;
  }

  const bool had_ubo =
      entry->skin_atlas != nullptr && entry->skin_atlas->palette_ubo != 0U;
  Render::GL::rigged_entry_ensure_skin_ubo(*entry);
  if (!had_ubo && entry->skin_atlas != nullptr &&
      entry->skin_atlas->palette_ubo != 0U) {
    const auto bytes = static_cast<std::uint64_t>(entry->skin_atlas->frame_total) *
                       Render::GL::BonePaletteArena::k_palette_bytes;
    cache.record_skin_ubo_upload(bytes);
  } else if (entry->skin_atlas != nullptr && entry->skin_atlas->palette_ubo == 0U &&
             !entry->skin_atlas->palettes.empty() &&
             entry->skin_atlas->frame_total != 0U &&
             entry->skin_atlas->bone_count != 0U) {
    cache.mark_skin_ubo_upload_pending();
  }
  return entry;
}

} // namespace Render::Creature::Pipeline
