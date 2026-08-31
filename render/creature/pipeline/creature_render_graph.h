#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "animation/bpat/bpat_playback.h"
#include "creature_render_state.h"
#include "humanoid_animation_selection.h"
#include "lod_decision.h"
#include "render/creature/render_request.h"
#include "shadow_batch.h"
#include "unit_visual_spec.h"

namespace Engine::Core {
class Entity;
class UnitComponent;
class TransformComponent;
} // namespace Engine::Core

namespace Render::GL {
struct DrawContext;
struct AnimationInputs;
struct HumanoidPose;
struct HumanoidVariant;
struct HumanoidAnimationContext;
struct HorseProfile;
struct HorseVariant;
struct MountedAttachmentFrame;
struct HorseMotionSample;
struct ReinState;
struct ElephantProfile;
struct ElephantVariant;
struct HowdahAttachmentFrame;
struct ElephantMotionSample;
} // namespace Render::GL

namespace Render::Creature::Pipeline {

struct PreparedHumanoidBodyState;
struct PreparedHorseBodyState;
struct PreparedElephantBodyState;
struct PreparedWildlifeBodyState;

struct CreatureGraphInputs {
  const Render::GL::DrawContext* ctx{nullptr};
  const Render::GL::AnimationInputs* anim{nullptr};
  Engine::Core::Entity* entity{nullptr};
  Engine::Core::UnitComponent* unit{nullptr};
  Engine::Core::TransformComponent* transform{nullptr};

  float camera_distance{0.0F};

  bool has_camera{true};

  std::optional<CreatureLOD> forced_lod{};

  bool budget_grant_full{true};
};

struct CreatureGraphOutput {

  CreatureLOD lod{CreatureLOD::Full};

  bool culled{false};

  CullReason cull_reason{CullReason::None};

  RenderPassIntent pass_intent{RenderPassIntent::Main};

  std::uint32_t seed{0};

  QMatrix4x4 world_matrix{};
  bool world_already_grounded{true};

  EntityId entity_id{0};
  std::uint16_t instance_index{0U};

  UnitVisualSpec spec{};
  std::optional<HumanoidAnimationSelection> humanoid_selection{};
};

struct CreatureLodConfig {
  LodDistanceThresholds thresholds{};
  bool apply_visibility_budget{false};
};

[[nodiscard]] auto humanoid_lod_config() noexcept -> CreatureLodConfig;

[[nodiscard]] auto horse_lod_config() noexcept -> CreatureLodConfig;

[[nodiscard]] auto elephant_lod_config() noexcept -> CreatureLodConfig;

[[nodiscard]] auto humanoid_lod_config_from_settings() noexcept -> CreatureLodConfig;

[[nodiscard]] auto horse_lod_config_from_settings() noexcept -> CreatureLodConfig;

[[nodiscard]] auto elephant_lod_config_from_settings() noexcept -> CreatureLodConfig;

[[nodiscard]] auto
quadruped_lod_from_settings(CreatureKind kind,
                            float distance) noexcept -> Render::Creature::CreatureLOD;

[[nodiscard]] auto
evaluate_creature_lod(const CreatureGraphInputs& inputs,
                      const CreatureLodConfig& config) noexcept -> CreatureLodDecision;

[[nodiscard]] auto build_base_graph_output(
    const CreatureGraphInputs& inputs,
    const CreatureLodDecision& lod_decision) noexcept -> CreatureGraphOutput;

class CreatureRenderBatch {
public:
  void clear() noexcept;

  void reserve(std::size_t n);

  void add_humanoid(const CreatureGraphOutput& output,
                    const Render::GL::HumanoidPose& pose,
                    const Render::GL::HumanoidVariant& variant,
                    const Render::GL::HumanoidAnimationContext& anim);

  void add_humanoid(const PreparedHumanoidBodyState& state);

  void add_quadruped(const CreatureGraphOutput& output,
                     const Render::GL::HorseVariant& variant,
                     Render::Creature::AnimationStateId state,
                     float phase,
                     std::uint32_t clip_variant = 0U);

  void add_quadruped(const PreparedHorseBodyState& state);

  void add_quadruped(const CreatureGraphOutput& output,
                     const Render::GL::ElephantVariant& variant,
                     Render::Creature::AnimationStateId state,
                     float phase,
                     std::uint32_t clip_variant = 0U);

  void add_quadruped(const PreparedElephantBodyState& state);

  void add_quadruped(const PreparedWildlifeBodyState& state);

  void add_request(const Render::Creature::CreatureRenderRequest& request);

  [[nodiscard]] auto
  rows() const noexcept -> std::span<const PreparedCreatureRenderRow>;

  [[nodiscard]] auto
  requests() const noexcept -> std::span<const Render::Creature::CreatureRenderRequest>;

  [[nodiscard]] auto size() const noexcept -> std::size_t;

  [[nodiscard]] auto empty() const noexcept -> bool;

private:
  std::vector<PreparedCreatureRenderRow> rows_{};
  std::vector<Render::Creature::CreatureRenderRequest> requests_{};
  const void* cached_humanoid_variant_{nullptr};
  CreatureAssetId cached_humanoid_asset_{k_invalid_creature_asset};
  Render::Creature::ArchetypeId cached_humanoid_archetype_{
      Render::Creature::k_invalid_archetype};
  Render::Creature::CreatureRenderAssetHandleId cached_humanoid_handle_{
      Render::Creature::k_invalid_creature_render_asset_handle};
  std::shared_ptr<const Render::RoleColorPalette> cached_humanoid_role_colors_{};
};

struct CreaturePreparationResult {
  CreatureRenderBatch bodies;
  HumanoidShadowBatch shadow_batch;

  void clear() noexcept {
    bodies.clear();
    shadow_batch.clear();
  }
};

} // namespace Render::Creature::Pipeline
