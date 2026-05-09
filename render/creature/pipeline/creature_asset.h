#pragma once

#include "../../creature/spec.h"
#include "../../static_attachment_spec.h"
#include "../render_request.h"
#include "unit_visual_spec.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Render::Creature {
struct ArchetypeDescriptor;
}

namespace Render::Creature::Bpat {
class BpatBlob;
}

namespace Render::Creature::Pipeline {

struct CreatureVisualDefinition;

inline constexpr std::size_t kMaxCreatureBones = 24;
inline constexpr std::size_t kMaxAttachmentSetSpecs = 16;
using AttachmentSetId = std::uint32_t;
inline constexpr AttachmentSetId kInvalidAttachmentSetId = 0U;

using BindPaletteFn = std::span<const QMatrix4x4> (*)() noexcept;
using FillRoleColorsFn = std::uint32_t (*)(const void *variant, QVector3D *out,
                                           std::size_t max_roles);

inline constexpr CreatureAssetId kHumanoidAsset = 0;
inline constexpr CreatureAssetId kHorseAsset = 1;
inline constexpr CreatureAssetId kElephantAsset = 2;
inline constexpr CreatureAssetId kHumanoidSwordAsset = 3;

struct CreatureAsset {
  CreatureAssetId id{kInvalidCreatureAsset};
  std::string_view debug_name{};
  CreatureKind kind{CreatureKind::Humanoid};
  std::uint32_t bpat_species_id{0};
  const Render::Creature::CreatureSpec *spec{nullptr};
  const Render::Creature::SkeletonTopology *topology{nullptr};
  std::uint8_t role_count{0};
  std::uint8_t max_bones{0};
  BindPaletteFn bind_palette{nullptr};
  FillRoleColorsFn fill_role_colors{nullptr};
  std::uint32_t snapshot_mesh_species_id{0xFFFFFFFFu};
  std::uint8_t snapshot_mesh_lod_mask{0};
  const CreatureVisualDefinition *visual_definition{nullptr};
};

struct CreatureClipPlaybackDesc {
  std::uint16_t clip_id{0xFFFFu};
  bool snapshot{false};
  const Render::Creature::Bpat::BpatBlob *blob{nullptr};
  std::uint32_t frame_count{0U};
  std::uint32_t frame_offset{0U};
};

struct CreatureRenderAssetHandle {
  Render::Creature::CreatureRenderAssetHandleId id{
      Render::Creature::kInvalidCreatureRenderAssetHandle};
  const CreatureAsset *asset{nullptr};
  const Render::Creature::ArchetypeDescriptor *archetype{nullptr};
  std::span<const QMatrix4x4> bind_palette{};
  std::span<const Render::Creature::StaticAttachmentSpec> attachments{};
  std::uint64_t attachments_hash{0U};
  AttachmentSetId attachment_set_id{kInvalidAttachmentSetId};
  bool has_static_attachments{false};
  bool requires_prebaked_minimal_snapshot{false};
  std::array<CreatureClipPlaybackDesc,
             Render::Creature::animation_state_count()>
      playback{};

  [[nodiscard]] auto valid() const noexcept -> bool {
    return asset != nullptr && archetype != nullptr && asset->spec != nullptr &&
           asset->bind_palette != nullptr && !bind_palette.empty();
  }
};

class CreatureRenderAssetHandleRegistry {
public:
  [[nodiscard]] static auto instance() -> CreatureRenderAssetHandleRegistry &;

  [[nodiscard]] auto get_or_create(CreatureAssetId asset_id,
                                   Render::Creature::ArchetypeId archetype_id)
      -> Render::Creature::CreatureRenderAssetHandleId;

  [[nodiscard]] auto get(Render::Creature::CreatureRenderAssetHandleId id) const
      -> const CreatureRenderAssetHandle *;

  void clear();

private:
  struct Key {
    CreatureAssetId asset_id{kInvalidCreatureAsset};
    Render::Creature::ArchetypeId archetype_id{
        Render::Creature::kInvalidArchetype};

    auto operator==(const Key &other) const noexcept -> bool {
      return asset_id == other.asset_id && archetype_id == other.archetype_id;
    }
  };

  struct KeyHash {
    auto operator()(const Key &key) const noexcept -> std::size_t {
      return (static_cast<std::size_t>(key.asset_id) << 16U) ^
             static_cast<std::size_t>(key.archetype_id);
    }
  };

  CreatureRenderAssetHandleRegistry() = default;

  struct AttachmentSetRecord {
    std::uint64_t hash{0U};
    std::array<Render::Creature::StaticAttachmentSpec, kMaxAttachmentSetSpecs>
        attachments{};
    std::uint8_t attachment_count{0U};
    AttachmentSetId id{kInvalidAttachmentSetId};
  };

  [[nodiscard]] auto acquire_attachment_set_id(
      std::span<const Render::Creature::StaticAttachmentSpec> attachments,
      std::uint64_t attachments_hash) -> AttachmentSetId;

  std::unordered_map<Key, Render::Creature::CreatureRenderAssetHandleId,
                     KeyHash>
      lookup_{};
  std::deque<CreatureRenderAssetHandle> handles_{};
  std::vector<AttachmentSetRecord> attachment_sets_{};
  AttachmentSetId next_attachment_set_id_{1U};
};

class CreatureAssetRegistry {
public:
  [[nodiscard]] static auto
  instance() noexcept -> const CreatureAssetRegistry &;

  [[nodiscard]] auto
  get(CreatureAssetId id) const noexcept -> const CreatureAsset *;
  [[nodiscard]] auto
  resolve(const UnitVisualSpec &spec) const noexcept -> const CreatureAsset *;

  [[nodiscard]] auto
  for_species(CreatureKind kind) const noexcept -> const CreatureAsset *;

private:
  CreatureAssetRegistry();

  CreatureAsset m_humanoid{};
  CreatureAsset m_horse{};
  CreatureAsset m_elephant{};
  CreatureAsset m_humanoid_sword{};
};

[[nodiscard]] auto resolve_creature_render_asset_handle(
    CreatureAssetId asset_id,
    Render::Creature::ArchetypeId archetype_id) -> CreatureRenderAssetHandle;

} // namespace Render::Creature::Pipeline
