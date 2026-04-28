#pragma once

#include "../render_request.h"
#include "bpat_playback.h"
#include "creature_render_state.h"
#include "lod_decision.h"
#include "unit_visual_spec.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <utility>
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

struct CreatureGraphInputs {
  const Render::GL::DrawContext *ctx{nullptr};
  const Render::GL::AnimationInputs *anim{nullptr};
  Engine::Core::Entity *entity{nullptr};
  Engine::Core::UnitComponent *unit{nullptr};
  Engine::Core::TransformComponent *transform{nullptr};

  std::uint32_t frame_index{0};

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

  UnitVisualSpec spec{};
};

struct CreatureLodConfig {
  LodDistanceThresholds thresholds{};
  TemporalSkipParams temporal{};
  bool apply_visibility_budget{false};
};

[[nodiscard]] auto humanoid_lod_config() noexcept -> CreatureLodConfig;

[[nodiscard]] auto horse_lod_config() noexcept -> CreatureLodConfig;

[[nodiscard]] auto elephant_lod_config() noexcept -> CreatureLodConfig;

[[nodiscard]] auto
humanoid_lod_config_from_settings() noexcept -> CreatureLodConfig;

[[nodiscard]] auto
horse_lod_config_from_settings() noexcept -> CreatureLodConfig;

[[nodiscard]] auto
elephant_lod_config_from_settings() noexcept -> CreatureLodConfig;

[[nodiscard]] auto quadruped_lod_from_settings(CreatureKind kind,
                                               float distance) noexcept
    -> Render::Creature::CreatureLOD;

[[nodiscard]] auto evaluate_creature_lod(
    const CreatureGraphInputs &inputs,
    const CreatureLodConfig &config) noexcept -> CreatureLodDecision;

[[nodiscard]] auto build_base_graph_output(
    const CreatureGraphInputs &inputs,
    const CreatureLodDecision &lod_decision) noexcept -> CreatureGraphOutput;

class CreatureRenderBatch {
public:
  void clear() noexcept;

  void reserve(std::size_t n);

  void add_humanoid(const CreatureGraphOutput &output,
                    const Render::GL::HumanoidPose &pose,
                    const Render::GL::HumanoidVariant &variant,
                    const Render::GL::HumanoidAnimationContext &anim);

  void add_quadruped(const CreatureGraphOutput &output,
                     const Render::GL::HorseVariant &variant,
                     Render::Creature::AnimationStateId state, float phase,
                     std::uint32_t clip_variant = 0U);

  void add_quadruped(const CreatureGraphOutput &output,
                     const Render::GL::ElephantVariant &variant,
                     Render::Creature::AnimationStateId state, float phase,
                     std::uint32_t clip_variant = 0U);

  void add_request(const Render::Creature::CreatureRenderRequest &request);

  [[nodiscard]] auto
  rows() const noexcept -> std::span<const PreparedCreatureRenderRow>;

  [[nodiscard]] auto requests() const noexcept
      -> std::span<const Render::Creature::CreatureRenderRequest>;

  [[nodiscard]] auto size() const noexcept -> std::size_t;

  [[nodiscard]] auto empty() const noexcept -> bool;

private:
  std::vector<PreparedCreatureRenderRow> rows_{};
  std::vector<Render::Creature::CreatureRenderRequest> requests_{};
};

using PostBodyDrawFn = std::function<void(Render::GL::ISubmitter &)>;

struct PostBodyDrawRequest {
  RenderPassIntent pass_intent{RenderPassIntent::Main};
  PostBodyDrawFn draw{};
};

struct CreaturePreparationResult {
  CreatureRenderBatch bodies;
  std::vector<PostBodyDrawRequest> post_body_draws;

  void add_post_body_draw(RenderPassIntent pass_intent, PostBodyDrawFn draw) {
    post_body_draws.push_back(
        PostBodyDrawRequest{pass_intent, std::move(draw)});
  }

  void clear() noexcept {
    bodies.clear();
    post_body_draws.clear();
  }
};

} // namespace Render::Creature::Pipeline
