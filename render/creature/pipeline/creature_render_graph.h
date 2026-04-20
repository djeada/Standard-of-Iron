#pragma once

/// @file creature_render_graph.h
/// @brief Declarative creature render graph system.
///
/// This header defines the frame-graph style data preparation stages for
/// creature rendering. Instead of assembling render behavior procedurally
/// at submission time, all necessary render data is prepared ahead of time
/// as structured, stable descriptions.
///
/// The render graph prepares:
/// - LOD selection (via decide_creature_lod)
/// - Pose and animation state
/// - Equipment/attachment configuration
/// - Palette inputs for rigged animation
/// - Material and shader variant selection
/// - Render pass intent (main pass, shadow pass)
///
/// Species-specific differences are expressed through data builders rather
/// than ad hoc branching inside renderer code.

#include "creature_frame.h"
#include "creature_render_state.h"
#include "lod_decision.h"
#include "unit_visual_spec.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

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

/// @brief Input parameters for creature graph node evaluation.
struct CreatureGraphInputs {
  const Render::GL::DrawContext *ctx{nullptr};
  const Render::GL::AnimationInputs *anim{nullptr};
  Engine::Core::Entity *entity{nullptr};
  Engine::Core::UnitComponent *unit{nullptr};
  Engine::Core::TransformComponent *transform{nullptr};

  /// Frame index for temporal LOD skipping.
  std::uint32_t frame_index{0};

  /// Camera distance for LOD selection.
  float camera_distance{0.0F};

  /// Whether a camera is present for LOD calculation.
  bool has_camera{true};

  /// Optional forced LOD override.
  std::optional<CreatureLOD> forced_lod{};

  /// Visibility budget grant (true = allow Full LOD).
  bool budget_grant_full{true};
};

/// @brief Evaluated creature render state from graph nodes.
///
/// This struct contains all the precomputed data needed to render a
/// creature without re-deriving gameplay or animation logic locally.
struct CreatureGraphOutput {
  /// Selected LOD tier.
  CreatureLOD lod{CreatureLOD::Full};

  /// Whether this creature should be culled (temporal skip or billboard).
  bool culled{false};

  /// Reason for culling, if culled.
  CullReason cull_reason{CullReason::None};

  /// Render pass intent (Main or Shadow).
  RenderPassIntent pass_intent{RenderPassIntent::Main};

  /// Instance seed for deterministic variation.
  std::uint32_t seed{0};

  /// World transform matrix.
  QMatrix4x4 world_matrix{};

  /// Entity ID for tracking.
  EntityId entity_id{0};

  /// The visual specification to use.
  UnitVisualSpec spec{};
};

/// @brief Configuration for LOD distance thresholds.
///
/// These thresholds can be configured per-species or globally.
struct CreatureLodConfig {
  LodDistanceThresholds thresholds{};
  TemporalSkipParams temporal{};
  bool apply_visibility_budget{false};
};

/// @brief Default LOD configuration for humanoids.
[[nodiscard]] auto humanoid_lod_config() noexcept -> CreatureLodConfig;

/// @brief Default LOD configuration for horses.
[[nodiscard]] auto horse_lod_config() noexcept -> CreatureLodConfig;

/// @brief Default LOD configuration for elephants.
[[nodiscard]] auto elephant_lod_config() noexcept -> CreatureLodConfig;

/// @brief Create LOD config for humanoids using current GraphicsSettings.
[[nodiscard]] auto humanoid_lod_config_from_settings() noexcept -> CreatureLodConfig;

/// @brief Create LOD config for horses using current GraphicsSettings.
[[nodiscard]] auto horse_lod_config_from_settings() noexcept -> CreatureLodConfig;

/// @brief Create LOD config for elephants using current GraphicsSettings.
[[nodiscard]] auto elephant_lod_config_from_settings() noexcept -> CreatureLodConfig;

/// @brief Evaluate LOD for a creature based on inputs and config.
///
/// This is the primary LOD decision node in the render graph.
///
/// @param inputs The creature graph inputs.
/// @param config The LOD configuration to use.
/// @return The LOD decision result.
[[nodiscard]] auto evaluate_creature_lod(
    const CreatureGraphInputs &inputs,
    const CreatureLodConfig &config) noexcept -> CreatureLodDecision;

/// @brief Build base creature graph output from inputs.
///
/// This builds the common graph output state that is shared across all
/// species. Species-specific builders extend this base output.
///
/// @param inputs The creature graph inputs.
/// @param lod_decision The LOD decision from evaluate_creature_lod.
/// @return The base graph output.
[[nodiscard]] auto build_base_graph_output(
    const CreatureGraphInputs &inputs,
    const CreatureLodDecision &lod_decision) noexcept -> CreatureGraphOutput;

/// @brief A batch of creature render state ready for submission.
///
/// This is the output of the render graph's prepare phase and input to
/// the submit phase.
class CreatureRenderBatch {
public:
  /// Clear all entries.
  void clear() noexcept;

  /// Reserve capacity for n entries.
  void reserve(std::size_t n);

  /// @brief Add a humanoid creature to the batch.
  void add_humanoid(const CreatureGraphOutput &output,
                    const Render::GL::HumanoidPose &pose,
                    const Render::GL::HumanoidVariant &variant,
                    const Render::GL::HumanoidAnimationContext &anim,
                    const Render::GL::DrawContext *legacy_ctx = nullptr);

  /// @brief Add a horse creature to the batch.
  void add_horse(const CreatureGraphOutput &output,
                 const Render::Horse::HorseSpecPose &pose,
                 const Render::GL::HorseVariant &variant);

  /// @brief Add an elephant creature to the batch.
  void add_elephant(const CreatureGraphOutput &output,
                    const Render::Elephant::ElephantSpecPose &pose,
                    const Render::GL::ElephantVariant &variant);

  /// @brief Get the prepared rows for submission.
  [[nodiscard]] auto
  rows() const noexcept -> std::span<const PreparedCreatureRenderRow>;

  /// @brief Get count of entries.
  [[nodiscard]] auto size() const noexcept -> std::size_t;

  /// @brief Check if batch is empty.
  [[nodiscard]] auto empty() const noexcept -> bool;

private:
  std::vector<PreparedCreatureRenderRow> rows_{};
};

/// @brief Callback type for post-body equipment/attachment draws.
using PostBodyDrawFn = std::function<void(Render::GL::ISubmitter &)>;

/// @brief A preparation result that includes both body rows and equipment.
struct CreaturePreparationResult {
  CreatureRenderBatch bodies;
  std::vector<PostBodyDrawFn> post_body_draws;

  void clear() noexcept {
    bodies.clear();
    post_body_draws.clear();
  }
};

} // namespace Render::Creature::Pipeline
