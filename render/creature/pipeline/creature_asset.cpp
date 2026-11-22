#include "render/creature/pipeline/creature_asset.h"

#include <algorithm>
#include <limits>

#include "animation/bpat/bpat_format.h"
#include "animation/bpat/bpat_registry.h"
#include "render/cache/static_attachment_spec.h"
#include "render/creature/archetype_registry.h"
#include "render/creature/pipeline/species_contract.h"

namespace Render::Creature::Pipeline {

auto CreatureAssetRegistry::instance() noexcept -> const CreatureAssetRegistry& {
  static const CreatureAssetRegistry registry;
  return registry;
}

CreatureAssetRegistry::CreatureAssetRegistry() {
  for (const SpeciesContract* contract : registered_species()) {
    if (contract != nullptr && contract->describe_assets != nullptr) {
      contract->describe_assets(m_assets);
    }
  }
}

auto CreatureAssetRegistry::get(CreatureAssetId id) const noexcept
    -> const CreatureAsset* {
  if (id >= m_assets.size() || m_assets[id].id != id) {
    return nullptr;
  }
  return &m_assets[id];
}

auto CreatureAssetRegistry::resolve(const UnitVisualSpec& spec) const noexcept
    -> const CreatureAsset* {
  if (spec.creature_asset_id != k_invalid_creature_asset) {
    if (const auto* asset = get(spec.creature_asset_id)) {
      return asset;
    }
  }
  return for_species(spec.kind);
}

auto CreatureAssetRegistry::for_species(CreatureKind kind) const noexcept
    -> const CreatureAsset* {
  const SpeciesContract* contract = species_contract(kind);
  return contract != nullptr ? get(contract->default_asset) : nullptr;
}

auto resolve_creature_render_asset_handle(CreatureAssetId asset_id,
                                          Render::Creature::ArchetypeId archetype_id)
    -> CreatureRenderAssetHandle {
  CreatureRenderAssetHandle handle{};

  const auto& asset_registry = CreatureAssetRegistry::instance();
  const auto& archetype_registry = Render::Creature::ArchetypeRegistry::instance();

  handle.archetype = archetype_registry.get(archetype_id);
  if (handle.archetype == nullptr) {
    return handle;
  }

  handle.asset = asset_id != k_invalid_creature_asset
                     ? asset_registry.get(asset_id)
                     : asset_registry.for_species(handle.archetype->species);
  if (handle.asset == nullptr || handle.asset->bind_palette == nullptr) {
    return handle;
  }

  handle.bind_palette = handle.asset->bind_palette();
  handle.spec = handle.asset->spec;
  if (handle.archetype->body_variant != 0U &&
      handle.asset->body_variant_spec != nullptr) {
    if (const auto* variant_spec =
            handle.asset->body_variant_spec(handle.archetype->body_variant)) {
      handle.spec = variant_spec;
    }
  }
  handle.attachments = handle.archetype->attachments_view();
  handle.has_static_attachments = !handle.attachments.empty();
  handle.requires_prebaked_minimal_snapshot =
      !handle.has_static_attachments &&
      has_prebaked_snapshot_mesh(*handle.asset, Render::Creature::CreatureLOD::Minimal);
  handle.attachments_hash = Render::Creature::static_attachments_hash(
      handle.attachments.data(), handle.attachments.size());

  auto const species_id = handle.asset->bpat_species_id;
  const auto* blob = Render::Creature::Bpat::BpatRegistry::instance().blob(species_id);

  for (std::size_t i = 0; i < handle.playback.size(); ++i) {
    auto const state = static_cast<Render::Creature::AnimationStateId>(i);
    CreatureClipPlaybackDesc desc{};
    desc.clip_id = archetype_registry.bpat_clip(archetype_id, state);
    desc.snapshot = archetype_registry.is_snapshot(archetype_id, state);
    desc.blob = blob;
    if (blob != nullptr &&
        desc.clip_id != Render::Creature::ArchetypeDescriptor::k_unmapped_clip &&
        desc.clip_id < blob->clip_count()) {
      auto const clip = blob->clip(desc.clip_id);
      desc.frame_count = clip.frame_count;
      desc.frame_offset = clip.frame_offset;
    }
    handle.playback[i] = desc;
  }

  return handle;
}

auto CreatureRenderAssetHandleRegistry::instance()
    -> CreatureRenderAssetHandleRegistry& {
  static CreatureRenderAssetHandleRegistry registry;
  return registry;
}

auto CreatureRenderAssetHandleRegistry::get_or_create(
    CreatureAssetId asset_id,
    Render::Creature::ArchetypeId archetype_id,
    bool* created) -> Render::Creature::CreatureRenderAssetHandleId {
  if (created != nullptr) {
    *created = false;
  }
  const Key key{asset_id, archetype_id};
  {
    std::shared_lock const read_lock(mutex_);
    if (const auto found = lookup_.find(key); found != lookup_.end()) {
      return found->second;
    }
  }
  if (created != nullptr) {
    *created = true;
  }

  CreatureRenderAssetHandle handle =
      resolve_creature_render_asset_handle(asset_id, archetype_id);
  if (!handle.valid()) {
    return Render::Creature::k_invalid_creature_render_asset_handle;
  }
  const bool has_playback =
      std::any_of(handle.playback.begin(),
                  handle.playback.end(),
                  [](const CreatureClipPlaybackDesc& desc) {
                    return desc.blob != nullptr && desc.frame_count > 0U;
                  });
  if (!has_playback) {
    return Render::Creature::k_invalid_creature_render_asset_handle;
  }
  std::unique_lock const write_lock(mutex_);
  if (const auto found = lookup_.find(key); found != lookup_.end()) {
    if (created != nullptr) {
      *created = false;
    }
    return found->second;
  }
  handle.attachment_set_id = acquire_attachment_set_id(
      handle.archetype->attachments_view(), handle.attachments_hash);
  if (handle.attachment_set_id == k_invalid_attachment_set_id) {
    return Render::Creature::k_invalid_creature_render_asset_handle;
  }
  if (handles_.size() >= Render::Creature::k_invalid_creature_render_asset_handle) {
    return Render::Creature::k_invalid_creature_render_asset_handle;
  }

  const auto id =
      static_cast<Render::Creature::CreatureRenderAssetHandleId>(handles_.size());
  handle.id = id;
  handles_.push_back(handle);
  lookup_.emplace(key, id);
  return id;
}

auto CreatureRenderAssetHandleRegistry::get(
    Render::Creature::CreatureRenderAssetHandleId id) const
    -> const CreatureRenderAssetHandle* {
  if (id == Render::Creature::k_invalid_creature_render_asset_handle) {
    return nullptr;
  }

  std::shared_lock const read_lock(mutex_);
  const auto index = static_cast<std::size_t>(id);
  if (index >= handles_.size()) {
    return nullptr;
  }
  return &handles_[index];
}

void CreatureRenderAssetHandleRegistry::clear() {
  lookup_.clear();
  handles_.clear();
  attachment_sets_.clear();
  next_attachment_set_id_ = 1U;
}

auto CreatureRenderAssetHandleRegistry::acquire_attachment_set_id(
    std::span<const Render::Creature::StaticAttachmentSpec> attachments,
    std::uint64_t attachments_hash) -> AttachmentSetId {
  auto same_attachments =
      [](const AttachmentSetRecord& record,
         std::span<const Render::Creature::StaticAttachmentSpec> specs) {
        if (record.attachment_count != specs.size()) {
          return false;
        }
        for (std::size_t i = 0; i < specs.size(); ++i) {
          if (!Render::Creature::static_attachment_equal(record.attachments[i],
                                                         specs[i])) {
            return false;
          }
        }
        return true;
      };

  for (const auto& record : attachment_sets_) {
    if (record.hash == attachments_hash && same_attachments(record, attachments)) {
      return record.id;
    }
  }

  if (next_attachment_set_id_ == k_invalid_attachment_set_id) {
    return k_invalid_attachment_set_id;
  }
  const auto id = next_attachment_set_id_;
  if (next_attachment_set_id_ == std::numeric_limits<AttachmentSetId>::max()) {
    next_attachment_set_id_ = k_invalid_attachment_set_id;
  } else {
    ++next_attachment_set_id_;
  }
  AttachmentSetRecord record{};
  record.hash = attachments_hash;
  record.attachment_count = static_cast<std::uint8_t>(
      std::min<std::size_t>(attachments.size(), record.attachments.size()));
  for (std::size_t i = 0; i < record.attachment_count; ++i) {
    record.attachments[i] = attachments[i];
  }
  record.id = id;
  attachment_sets_.push_back(record);
  return id;
}

} // namespace Render::Creature::Pipeline
