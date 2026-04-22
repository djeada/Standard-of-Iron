

#pragma once

#include "../part_graph.h"
#include "../skeleton.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <cstdint>
#include <functional>
#include <span>
#include <string_view>

namespace Render::GL {
struct DrawContext;
struct BodyFrames;
struct HumanoidVariant;
struct HumanoidPose;
struct HumanoidPalette;
struct HumanoidAnimationContext;
struct HorseBodyFrames;
struct HorseVariant;
struct HorseAnimationContext;
struct HorseProfile;
struct MountedAttachmentFrame;
struct ReinState;
struct HorseMotionSample;
using HorseLOD = ::Render::Creature::CreatureLOD;
struct VariationParams;
struct EquipmentBatch;
class ISubmitter;
class Mesh;
struct Material;
} // namespace Render::GL

namespace Render::Creature::Pipeline {

enum class CreatureKind : std::uint8_t {
  Humanoid = 0,
  Horse = 1,
  Elephant = 2,
  Mounted = 3,
};

using PaletteId = std::uint32_t;
inline constexpr PaletteId kDefaultPalette = 0u;

using SpecId = std::uint32_t;
inline constexpr SpecId kInvalidSpec = static_cast<SpecId>(0xFFFFFFFFu);

using CreatureAssetId = std::uint16_t;
inline constexpr CreatureAssetId kInvalidCreatureAsset =
    static_cast<CreatureAssetId>(0xFFFFu);

using MaterialId = std::uint16_t;
inline constexpr MaterialId kDefaultMaterial = 0u;

enum class TintRole : std::uint8_t {
  None = 0,
  Cloth = 1,
  Skin = 2,
  Leather = 3,
  LeatherDark = 4,
  Wood = 5,
  Metal = 6,
  TeamTint = 7,
};

struct EquipmentSize {
  float length{1.0F};
  float radius{1.0F};
  float extra{0.0F};
};

struct EquipmentMeshArgs {
  const Render::GL::DrawContext *ctx{nullptr};
  const QMatrix4x4 *socket_transform{nullptr};
  std::uint32_t seed{0};
  EquipmentSize size{};
};

using MeshProviderFn = Render::GL::Mesh *(*)(const EquipmentMeshArgs &args);

struct LegacyEquipmentSubmitArgs {
  const Render::GL::DrawContext *ctx{nullptr};
  const Render::GL::HumanoidVariant *variant{nullptr};
  const Render::GL::HumanoidPose *pose{nullptr};
  const Render::GL::HumanoidAnimationContext *anim{nullptr};
  const QMatrix4x4 *world_matrix{nullptr};
  std::uint32_t seed{0};

  const Render::GL::HorseBodyFrames *horse_frames{nullptr};
  const Render::GL::HorseVariant *horse_variant{nullptr};
  const Render::GL::HorseAnimationContext *horse_anim{nullptr};
};

using LegacyEquipmentSubmitFn = void (*)(void *user,
                                         const LegacyEquipmentSubmitArgs &args,
                                         Render::GL::ISubmitter &out);

struct EquipmentSubmitContext {
  std::uint32_t entity_id{0};
  std::uint32_t seed{0};

  const Render::GL::DrawContext *ctx{nullptr};
  const Render::GL::BodyFrames *frames{nullptr};
  const Render::GL::HumanoidPalette *palette{nullptr};
  const Render::GL::HumanoidAnimationContext *anim{nullptr};

  const Render::GL::HumanoidPose *pose{nullptr};

  const Render::GL::HorseBodyFrames *horse_frames{nullptr};
  const Render::GL::HorseVariant *horse_variant{nullptr};
  const Render::GL::HorseAnimationContext *horse_anim{nullptr};

  const Render::GL::HorseProfile *horse_profile{nullptr};
  const Render::GL::MountedAttachmentFrame *mount_frame{nullptr};
  const Render::GL::ReinState *rein_state{nullptr};
  const Render::GL::HorseMotionSample *horse_motion{nullptr};
  Render::GL::HorseLOD horse_lod{};
};

using EquipmentDispatchFn = std::function<void(const EquipmentSubmitContext &,
                                               Render::GL::EquipmentBatch &)>;

struct EquipmentRecord {

  SocketIndex socket{kInvalidSocket};

  Render::GL::Mesh *static_mesh{nullptr};
  MeshProviderFn mesh_provider{nullptr};

  MaterialId material_id{kDefaultMaterial};
  TintRole tint_role{TintRole::None};
  EquipmentSize size{};

  QVector3D override_color{};

  QMatrix4x4 local_offset{};

  EquipmentDispatchFn dispatch{};

  LegacyEquipmentSubmitFn legacy_submit{nullptr};
  void *legacy_user{nullptr};
};

using EquipmentLoadout = std::span<const EquipmentRecord>;

using PoseHookFn = void (*)(const Render::GL::DrawContext &ctx,
                            const Render::GL::HumanoidAnimationContext &anim,
                            std::uint32_t seed,
                            Render::GL::HumanoidPose &io_pose);

using VariantHookFn = void (*)(const Render::GL::DrawContext &ctx,
                               std::uint32_t seed,
                               Render::GL::VariationParams &io_variation);

struct ProportionScaling {
  float x{1.0F};
  float y{1.0F};
  float z{1.0F};

  [[nodiscard]] constexpr auto as_vector() const -> QVector3D {
    return {x, y, z};
  }
};

enum class LegacySlotMask : std::uint8_t {
  None = 0,
  Helmet = 1U << 0U,
  Armor = 1U << 1U,
  ArmorOverlay = 1U << 2U,
  ShoulderDecorations = 1U << 3U,
  FacialHair = 1U << 4U,
  Attachments = 1U << 5U,

  AllHumanoid = Helmet | Armor | ArmorOverlay | ShoulderDecorations |
                FacialHair | Attachments,

  HorseAttachments = Attachments,
  ElephantHowdah = Attachments,
};

[[nodiscard]] constexpr auto
operator|(LegacySlotMask a, LegacySlotMask b) noexcept -> LegacySlotMask {
  return static_cast<LegacySlotMask>(static_cast<std::uint8_t>(a) |
                                     static_cast<std::uint8_t>(b));
}

[[nodiscard]] constexpr auto
operator&(LegacySlotMask a, LegacySlotMask b) noexcept -> LegacySlotMask {
  return static_cast<LegacySlotMask>(static_cast<std::uint8_t>(a) &
                                     static_cast<std::uint8_t>(b));
}

[[nodiscard]] constexpr auto owns_slot(LegacySlotMask mask,
                                       LegacySlotMask slot) noexcept -> bool {
  return (static_cast<std::uint8_t>(mask) & static_cast<std::uint8_t>(slot)) !=
         0U;
}

struct MountedSpec;

struct UnitVisualSpec {
  std::string_view debug_name{};
  CreatureKind kind{CreatureKind::Humanoid};
  CreatureAssetId creature_asset_id{kInvalidCreatureAsset};
  PaletteId palette_id{kDefaultPalette};
  SpecId equipment_registry_id{kInvalidSpec};
  EquipmentLoadout equipment{};
  PoseHookFn pose_hook{nullptr};
  VariantHookFn variant_hook{nullptr};
  ProportionScaling scaling{};

  LegacySlotMask owned_legacy_slots{LegacySlotMask::None};

  const MountedSpec *mounted{nullptr};
};

struct MountedSpec {
  UnitVisualSpec rider{};
  UnitVisualSpec mount{};
  SocketIndex mount_socket{kInvalidSocket};
};

} // namespace Render::Creature::Pipeline
