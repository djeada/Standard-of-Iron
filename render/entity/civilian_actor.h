#pragma once

#include <QMatrix4x4>

#include <cstdint>

#include "render/creature/pipeline/unit_visual_spec.h"
#include "render/creature/spec.h"

namespace Render::GL {
struct DrawContext;
struct HumanoidVariant;
class ISubmitter;

struct NationCivilianRig {
  bool carthage{false};

  bool healer{false};
  bool builder{false};
  Render::Creature::Pipeline::UnitVisualSpec spec{};
  Render::Creature::ArchetypeId idle{Render::Creature::k_invalid_archetype};
  Render::Creature::ArchetypeId working{Render::Creature::k_invalid_archetype};
  void (*fill_variant)(const DrawContext& ctx,
                       std::uint32_t seed,
                       HumanoidVariant& out){nullptr};

  [[nodiscard]] auto valid() const noexcept -> bool {
    return fill_variant != nullptr && idle != Render::Creature::k_invalid_archetype &&
           working != Render::Creature::k_invalid_archetype;
  }
};

void register_nation_civilian_rig(bool carthage, NationCivilianRig rig);
[[nodiscard]] auto nation_civilian_rig(bool carthage) -> const NationCivilianRig&;
void register_nation_priest_rig(bool carthage, NationCivilianRig rig);
[[nodiscard]] auto nation_priest_rig(bool carthage) -> const NationCivilianRig&;
void register_nation_crew_rig(bool carthage, NationCivilianRig rig);
[[nodiscard]] auto nation_crew_rig(bool carthage) -> const NationCivilianRig&;

struct CivilianActor {
  Render::Creature::ArchetypeId archetype{Render::Creature::k_invalid_archetype};
  std::uint16_t clip{0};
  float phase{0.0F};
  QMatrix4x4 world{};
  std::uint32_t owner_id{0};
  std::uint16_t instance{0};
  std::uint32_t seed{0};
  bool distant{false};

  std::uint16_t blend_clip{0xFFFFU};
  float blend_phase{0.0F};
  float blend_weight{0.0F};

  std::uint16_t overlay_clip{0xFFFFU};
  float overlay_phase{0.0F};
  float overlay_weight{1.0F};
};

[[nodiscard]] auto civilian_render_scale(const DrawContext& ctx,
                                         const NationCivilianRig& rig) -> float;

// Whether actors that are small on screen may drop to Minimal detail. The
// template prewarm bakes their Minimal bodies exactly when this holds.
[[nodiscard]] auto civilian_actor_minimal_lod_allowed() -> bool;

void begin_civilian_actors();
void add_civilian_actor(const DrawContext& ctx,
                        const NationCivilianRig& rig,
                        const CivilianActor& actor);
void submit_civilian_actors(ISubmitter& out);

[[nodiscard]] auto ambient_hash(std::uint32_t value) noexcept -> std::uint32_t;

} // namespace Render::GL
