#pragma once

#include <QMatrix4x4>

#include <cstdint>

#include "render/creature/pipeline/unit_visual_spec.h"
#include "render/creature/spec.h"

namespace Render::GL {
struct DrawContext;
struct HumanoidVariant;
class ISubmitter;

// A nation's civilian look, published once by that nation's troop renderers so
// every presentation-only actor -- field workers, the villagers in the ambient
// gags -- is the same rig, proportions and palette as its real civilians.
// Nothing about their appearance is authored at the call site.
struct NationCivilianRig {
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

// One presentation-only actor. These are batched: call begin/submit around a
// run of them so a scene's actors share one pipeline submission.
struct CivilianActor {
  Render::Creature::ArchetypeId archetype{Render::Creature::k_invalid_archetype};
  std::uint16_t clip{0};
  float phase{0.0F};
  QMatrix4x4 world{};
  std::uint32_t owner_id{0};
  std::uint16_t instance{0};
  std::uint32_t seed{0};
  bool distant{false};
  // Where this actor was a moment ago. Set these and the outgoing clip is
  // blended out over `blend_weight`, so a change of task eases instead of
  // snapping mid-stride.
  std::uint16_t blend_clip{0xFFFFU};
  float blend_phase{0.0F};
  float blend_weight{0.0F};
};

void begin_civilian_actors();
void add_civilian_actor(const DrawContext& ctx,
                        const NationCivilianRig& rig,
                        const CivilianActor& actor);
void submit_civilian_actors(ISubmitter& out);

// Stable, cheap hash used to stagger every ambient schedule.
[[nodiscard]] auto ambient_hash(std::uint32_t value) noexcept -> std::uint32_t;

// Shared review hook: when the named variable is set, an ambient sequence that
// is meant to be rare fires every N seconds instead, so it can be captured.
[[nodiscard]] auto ambient_review_interval(const char* environment_variable) -> float;
} // namespace Render::GL
