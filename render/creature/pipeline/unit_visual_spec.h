

#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>
#include <span>
#include <string_view>

#include "render/creature/archetype_variant_table.h"
#include "render/creature/part_graph.h"
#include "render/creature/render_request.h"
#include "render/creature/skeleton.h"
#include "render/humanoid/schema/pose_policy.h"
#include "render/humanoid/schema/visual_capabilities.h"

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
  Sheep = 4,
  Wolf = 5,
};

using PaletteId = std::uint32_t;
inline constexpr PaletteId k_default_palette = 0u;

using SpecId = std::uint32_t;
inline constexpr SpecId k_invalid_spec = static_cast<SpecId>(0xFFFFFFFFu);

using CreatureAssetId = std::uint16_t;
inline constexpr CreatureAssetId k_invalid_creature_asset =
    static_cast<CreatureAssetId>(0xFFFFu);

struct HumanoidAnimationManifest {
  const Render::Creature::ArchetypeVariantTable* variant_table{nullptr};

  Render::Humanoid::HumanoidPosePolicy pose_policy{
      Render::Humanoid::HumanoidPosePolicy::None};

  std::uint16_t melee_clip_override{0xFFFFU};
};

struct ProportionScaling {
  float x{1.0F};
  float y{1.0F};
  float z{1.0F};

  [[nodiscard]] constexpr auto as_vector() const -> QVector3D { return {x, y, z}; }
};

struct MountedSpec;

struct UnitVisualSpec {
  std::string_view debug_name{};
  CreatureKind kind{CreatureKind::Humanoid};
  CreatureAssetId creature_asset_id{k_invalid_creature_asset};
  PaletteId palette_id{k_default_palette};
  HumanoidAnimationManifest animation_manifest{};
  ProportionScaling scaling{};
  const CreatureVisualDefinition* creature_definition{nullptr};

  bool skip_default_facial_hair_archetype{false};

  Render::Creature::ArchetypeId archetype_id{Render::Creature::k_invalid_archetype};

  Render::Humanoid::HumanoidCapabilities capabilities{};

  const MountedSpec* mounted{nullptr};
};

struct MountedSpec {
  UnitVisualSpec rider{};
  UnitVisualSpec mount{};
  SocketIndex mount_socket{k_invalid_socket};
};

} // namespace Render::Creature::Pipeline
