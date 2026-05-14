

#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>
#include <span>
#include <string_view>

#include "../archetype_variant_table.h"
#include "../part_graph.h"
#include "../render_request.h"
#include "../skeleton.h"

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

struct CreatureVisualDefinition;

enum class CreatureKind : std::uint8_t {
  Humanoid = 0,
  Horse = 1,
  Elephant = 2,
  Mounted = 3,
};

using PaletteId = std::uint32_t;
inline constexpr PaletteId k_default_palette = 0u;

using SpecId = std::uint32_t;
inline constexpr SpecId k_invalid_spec = static_cast<SpecId>(0xFFFFFFFFu);

using CreatureAssetId = std::uint16_t;
inline constexpr CreatureAssetId k_invalid_creature_asset =
    static_cast<CreatureAssetId>(0xFFFFu);

using PoseHookFn = void (*)(const Render::GL::DrawContext& ctx,
                            const Render::GL::HumanoidAnimationContext& anim,
                            std::uint32_t seed,
                            Render::GL::HumanoidPose& io_pose);

using VariantHookFn = void (*)(const Render::GL::DrawContext& ctx,
                               std::uint32_t seed,
                               Render::GL::VariationParams& io_variation);

struct ProportionScaling {
  float x{1.0F};
  float y{1.0F};
  float z{1.0F};

  [[nodiscard]] constexpr auto as_vector() const -> QVector3D { return {x, y, z}; }
};

enum class LegacySlotMask : std::uint8_t {
  None = 0,
  Helmet = 1U << 0U,
  Armor = 1U << 1U,
  ArmorOverlay = 1U << 2U,
  ShoulderDecorations = 1U << 3U,
  FacialHair = 1U << 4U,
  Attachments = 1U << 5U,

  AllHumanoid =
      Helmet | Armor | ArmorOverlay | ShoulderDecorations | FacialHair | Attachments,

  HorseAttachments = Attachments,
  ElephantHowdah = Attachments,
};

[[nodiscard]] constexpr auto operator|(LegacySlotMask a,
                                       LegacySlotMask b) noexcept -> LegacySlotMask {
  return static_cast<LegacySlotMask>(static_cast<std::uint8_t>(a) |
                                     static_cast<std::uint8_t>(b));
}

[[nodiscard]] constexpr auto operator&(LegacySlotMask a,
                                       LegacySlotMask b) noexcept -> LegacySlotMask {
  return static_cast<LegacySlotMask>(static_cast<std::uint8_t>(a) &
                                     static_cast<std::uint8_t>(b));
}

[[nodiscard]] constexpr auto owns_slot(LegacySlotMask mask,
                                       LegacySlotMask slot) noexcept -> bool {
  return (static_cast<std::uint8_t>(mask) & static_cast<std::uint8_t>(slot)) != 0U;
}

struct MountedSpec;

struct UnitVisualSpec {
  std::string_view debug_name{};
  CreatureKind kind{CreatureKind::Humanoid};
  CreatureAssetId creature_asset_id{k_invalid_creature_asset};
  PaletteId palette_id{k_default_palette};
  PoseHookFn pose_hook{nullptr};
  VariantHookFn variant_hook{nullptr};
  const Render::Creature::ArchetypeVariantTable* variant_table{nullptr};
  ProportionScaling scaling{};
  const CreatureVisualDefinition* creature_definition{nullptr};

  LegacySlotMask owned_legacy_slots{LegacySlotMask::None};
  bool skip_default_facial_hair_archetype{false};

  Render::Creature::ArchetypeId archetype_id{Render::Creature::k_invalid_archetype};

  const MountedSpec* mounted{nullptr};
};

struct MountedSpec {
  UnitVisualSpec rider{};
  UnitVisualSpec mount{};
  SocketIndex mount_socket{k_invalid_socket};
};

} // namespace Render::Creature::Pipeline
