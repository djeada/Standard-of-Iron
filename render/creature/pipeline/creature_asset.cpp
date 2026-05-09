#include "creature_asset.h"

#include "../../elephant/elephant_spec.h"
#include "../../horse/horse_spec.h"
#include "../../humanoid/humanoid_spec.h"
#include "../../static_attachment_spec.h"
#include "../archetype_registry.h"
#include "../bpat/bpat_format.h"
#include "../bpat/bpat_registry.h"
#include "creature_visual_definition.h"

#include <algorithm>
#include <limits>

namespace Render::Creature::Pipeline {

namespace {

auto humanoid_bind() noexcept -> std::span<const QMatrix4x4> {
  return Render::Humanoid::humanoid_bind_palette();
}

auto horse_bind() noexcept -> std::span<const QMatrix4x4> {
  return Render::Horse::horse_bind_palette();
}

auto elephant_bind() noexcept -> std::span<const QMatrix4x4> {
  return Render::Elephant::elephant_bind_palette();
}

auto humanoid_fill_roles(const void *variant, QVector3D *out,
                         std::size_t max_roles) -> std::uint32_t {
  const auto &v = *static_cast<const Render::GL::HumanoidVariant *>(variant);
  std::array<QVector3D, Render::Humanoid::k_humanoid_role_count> filled{};
  Render::Humanoid::fill_humanoid_role_colors(v, filled);
  const auto n = std::min(filled.size(), max_roles);
  for (std::size_t i = 0; i < n; ++i)
    out[i] = filled[i];
  return static_cast<std::uint32_t>(n);
}

auto horse_fill_roles(const void *variant, QVector3D *out,
                      std::size_t max_roles) -> std::uint32_t {
  const auto &v = *static_cast<const Render::GL::HorseVariant *>(variant);
  std::array<QVector3D, 8> filled{};
  Render::Horse::fill_horse_role_colors(v, filled);
  const auto n = std::min<std::size_t>(filled.size(), max_roles);
  for (std::size_t i = 0; i < n; ++i)
    out[i] = filled[i];
  return static_cast<std::uint32_t>(n);
}

auto elephant_fill_roles(const void *variant, QVector3D *out,
                         std::size_t max_roles) -> std::uint32_t {
  const auto &v = *static_cast<const Render::GL::ElephantVariant *>(variant);
  std::array<QVector3D, Render::Elephant::k_elephant_role_count> filled{};
  Render::Elephant::fill_elephant_role_colors(v, filled);
  const auto n = std::min(filled.size(), max_roles);
  for (std::size_t i = 0; i < n; ++i)
    out[i] = filled[i];
  return static_cast<std::uint32_t>(n);
}

} // namespace

auto CreatureAssetRegistry::instance() noexcept
    -> const CreatureAssetRegistry & {
  static const CreatureAssetRegistry registry;
  return registry;
}

CreatureAssetRegistry::CreatureAssetRegistry() {
  m_humanoid.id = kHumanoidAsset;
  m_humanoid.debug_name = "humanoid.v1";
  m_humanoid.kind = CreatureKind::Humanoid;
  m_humanoid.bpat_species_id = Render::Creature::Bpat::kSpeciesHumanoid;
  m_humanoid.spec = &Render::Humanoid::humanoid_creature_spec();
  m_humanoid.topology = &m_humanoid.spec->topology;
  m_humanoid.role_count =
      static_cast<std::uint8_t>(Render::Humanoid::k_humanoid_role_count);
  m_humanoid.max_bones =
      static_cast<std::uint8_t>(Render::Humanoid::k_bone_count);
  m_humanoid.bind_palette = &humanoid_bind;
  m_humanoid.fill_role_colors = &humanoid_fill_roles;

  m_horse.id = kHorseAsset;
  m_horse.debug_name = "horse.v1";
  m_horse.kind = CreatureKind::Horse;
  m_horse.bpat_species_id = Render::Creature::Bpat::kSpeciesHorse;
  m_horse.spec = &Render::Horse::horse_creature_spec();
  m_horse.topology = &m_horse.spec->topology;
  m_horse.role_count = 8;
  m_horse.max_bones =
      static_cast<std::uint8_t>(Render::Horse::k_horse_bone_count);
  m_horse.bind_palette = &horse_bind;
  m_horse.fill_role_colors = &horse_fill_roles;
  m_horse.snapshot_mesh_species_id = Render::Creature::Bpat::kSpeciesHorse;
  m_horse.snapshot_mesh_lod_mask = static_cast<std::uint8_t>(
      1U << static_cast<std::uint8_t>(Render::Creature::CreatureLOD::Minimal));
  m_horse.visual_definition = &horse_creature_visual_definition();

  m_elephant.id = kElephantAsset;
  m_elephant.debug_name = "elephant.v1";
  m_elephant.kind = CreatureKind::Elephant;
  m_elephant.bpat_species_id = Render::Creature::Bpat::kSpeciesElephant;
  m_elephant.spec = &Render::Elephant::elephant_creature_spec();
  m_elephant.topology = &m_elephant.spec->topology;
  m_elephant.role_count =
      static_cast<std::uint8_t>(Render::Elephant::k_elephant_role_count);
  m_elephant.max_bones =
      static_cast<std::uint8_t>(Render::Elephant::k_elephant_bone_count);
  m_elephant.bind_palette = &elephant_bind;
  m_elephant.fill_role_colors = &elephant_fill_roles;
  m_elephant.snapshot_mesh_species_id =
      Render::Creature::Bpat::kSpeciesElephant;
  m_elephant.snapshot_mesh_lod_mask = static_cast<std::uint8_t>(
      1U << static_cast<std::uint8_t>(Render::Creature::CreatureLOD::Minimal));
  m_elephant.visual_definition = &elephant_creature_visual_definition();

  m_humanoid_sword.id = kHumanoidSwordAsset;
  m_humanoid_sword.debug_name = "humanoid.sword_ready.v1";
  m_humanoid_sword.kind = CreatureKind::Humanoid;
  m_humanoid_sword.bpat_species_id =
      Render::Creature::Bpat::kSpeciesHumanoidSword;
  m_humanoid_sword.spec = &Render::Humanoid::humanoid_creature_spec();
  m_humanoid_sword.topology = &m_humanoid_sword.spec->topology;
  m_humanoid_sword.role_count =
      static_cast<std::uint8_t>(Render::Humanoid::k_humanoid_role_count);
  m_humanoid_sword.max_bones =
      static_cast<std::uint8_t>(Render::Humanoid::k_bone_count);
  m_humanoid_sword.bind_palette = &humanoid_bind;
  m_humanoid_sword.fill_role_colors = &humanoid_fill_roles;
}

auto CreatureAssetRegistry::get(CreatureAssetId id) const noexcept
    -> const CreatureAsset * {
  switch (id) {
  case kHumanoidAsset:
    return &m_humanoid;
  case kHorseAsset:
    return &m_horse;
  case kElephantAsset:
    return &m_elephant;
  case kHumanoidSwordAsset:
    return &m_humanoid_sword;
  default:
    return nullptr;
  }
}

auto CreatureAssetRegistry::resolve(const UnitVisualSpec &spec) const noexcept
    -> const CreatureAsset * {
  if (spec.creature_asset_id != kInvalidCreatureAsset) {
    if (const auto *asset = get(spec.creature_asset_id)) {
      return asset;
    }
  }
  return for_species(spec.kind);
}

auto CreatureAssetRegistry::for_species(CreatureKind kind) const noexcept
    -> const CreatureAsset * {
  switch (kind) {
  case CreatureKind::Humanoid:
    return &m_humanoid;
  case CreatureKind::Horse:
    return &m_horse;
  case CreatureKind::Elephant:
    return &m_elephant;
  case CreatureKind::Mounted:
    return nullptr;
  }
  return nullptr;
}

auto resolve_creature_render_asset_handle(
    CreatureAssetId asset_id,
    Render::Creature::ArchetypeId archetype_id) -> CreatureRenderAssetHandle {
  CreatureRenderAssetHandle handle{};

  const auto &asset_registry = CreatureAssetRegistry::instance();
  const auto &archetype_registry =
      Render::Creature::ArchetypeRegistry::instance();

  handle.archetype = archetype_registry.get(archetype_id);
  if (handle.archetype == nullptr) {
    return handle;
  }

  handle.asset = asset_id != kInvalidCreatureAsset
                     ? asset_registry.get(asset_id)
                     : asset_registry.for_species(handle.archetype->species);
  if (handle.asset == nullptr || handle.asset->bind_palette == nullptr) {
    return handle;
  }

  handle.bind_palette = handle.asset->bind_palette();
  handle.attachments = handle.archetype->attachments_view();
  handle.has_static_attachments = !handle.attachments.empty();
  handle.requires_prebaked_minimal_snapshot =
      !handle.has_static_attachments &&
      (handle.archetype->species == CreatureKind::Horse ||
       handle.archetype->species == CreatureKind::Elephant);
  handle.attachments_hash = Render::Creature::static_attachments_hash(
      handle.attachments.data(), handle.attachments.size());

  auto const species_id = handle.asset->bpat_species_id;
  const auto *blob =
      Render::Creature::Bpat::BpatRegistry::instance().blob(species_id);

  for (std::size_t i = 0; i < handle.playback.size(); ++i) {
    auto const state = static_cast<Render::Creature::AnimationStateId>(i);
    CreatureClipPlaybackDesc desc{};
    desc.clip_id = archetype_registry.bpat_clip(archetype_id, state);
    desc.snapshot = archetype_registry.is_snapshot(archetype_id, state);
    desc.blob = blob;
    if (blob != nullptr &&
        desc.clip_id != Render::Creature::ArchetypeDescriptor::kUnmappedClip &&
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
    -> CreatureRenderAssetHandleRegistry & {
  static CreatureRenderAssetHandleRegistry registry;
  return registry;
}

auto CreatureRenderAssetHandleRegistry::get_or_create(
    CreatureAssetId asset_id, Render::Creature::ArchetypeId archetype_id)
    -> Render::Creature::CreatureRenderAssetHandleId {
  const Key key{asset_id, archetype_id};
  if (const auto found = lookup_.find(key); found != lookup_.end()) {
    return found->second;
  }

  CreatureRenderAssetHandle handle =
      resolve_creature_render_asset_handle(asset_id, archetype_id);
  if (!handle.valid()) {
    return Render::Creature::kInvalidCreatureRenderAssetHandle;
  }
  const bool has_playback =
      std::any_of(handle.playback.begin(), handle.playback.end(),
                  [](const CreatureClipPlaybackDesc &desc) {
                    return desc.blob != nullptr && desc.frame_count > 0U;
                  });
  if (!has_playback) {
    return Render::Creature::kInvalidCreatureRenderAssetHandle;
  }
  handle.attachment_set_id = acquire_attachment_set_id(
      handle.archetype->attachments_view(), handle.attachments_hash);
  if (handle.attachment_set_id == kInvalidAttachmentSetId) {
    return Render::Creature::kInvalidCreatureRenderAssetHandle;
  }
  if (handles_.size() >= Render::Creature::kInvalidCreatureRenderAssetHandle) {
    return Render::Creature::kInvalidCreatureRenderAssetHandle;
  }

  const auto id = static_cast<Render::Creature::CreatureRenderAssetHandleId>(
      handles_.size());
  handle.id = id;
  handles_.push_back(handle);
  lookup_.emplace(key, id);
  return id;
}

auto CreatureRenderAssetHandleRegistry::get(
    Render::Creature::CreatureRenderAssetHandleId id) const
    -> const CreatureRenderAssetHandle * {
  if (id == Render::Creature::kInvalidCreatureRenderAssetHandle) {
    return nullptr;
  }

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
      [](const AttachmentSetRecord &record,
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

  for (const auto &record : attachment_sets_) {
    if (record.hash == attachments_hash &&
        same_attachments(record, attachments)) {
      return record.id;
    }
  }

  if (next_attachment_set_id_ == kInvalidAttachmentSetId) {
    return kInvalidAttachmentSetId;
  }
  const auto id = next_attachment_set_id_;
  if (next_attachment_set_id_ == std::numeric_limits<AttachmentSetId>::max()) {
    next_attachment_set_id_ = kInvalidAttachmentSetId;
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
