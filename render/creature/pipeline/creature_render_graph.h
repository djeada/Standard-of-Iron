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
#include "render/creature/pipeline/creature_render_state.h"
#include "render/creature/pipeline/humanoid_animation_selection.h"
#include "render/creature/pipeline/lod_decision.h"
#include "render/creature/pipeline/shadow_batch.h"
#include "render/creature/pipeline/unit_visual_spec.h"
#include "render/creature/render_request.h"

namespace Engine::Core {
class Entity;
class UnitComponent;
class TransformComponent;
} // namespace Engine::Core

namespace Render::GL {
struct DrawContext;
struct AnimationInputs;
} // namespace Render::GL

namespace Render::Creature::Pipeline {

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

[[nodiscard]] auto creature_lod_config(CreatureKind kind) noexcept -> CreatureLodConfig;

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
  struct RequestReuseCache {
    const void* variant{nullptr};
    CreatureAssetId asset{k_invalid_creature_asset};
    Render::Creature::ArchetypeId archetype{Render::Creature::k_invalid_archetype};
    Render::Creature::CreatureRenderAssetHandleId handle{
        Render::Creature::k_invalid_creature_render_asset_handle};
    std::shared_ptr<const Render::RoleColorPalette> role_colors{};
  };

  void clear() noexcept;

  void reserve(std::size_t n);

  void add_request(const Render::Creature::CreatureRenderRequest& request);

  void add_row(PreparedCreatureRenderRow row);

  void fade_requests_from(std::size_t first, float alpha) noexcept;

  [[nodiscard]] auto
  rows() const noexcept -> std::span<const PreparedCreatureRenderRow>;

  [[nodiscard]] auto
  requests() const noexcept -> std::span<const Render::Creature::CreatureRenderRequest>;

  [[nodiscard]] auto
  mutable_requests() noexcept -> std::span<Render::Creature::CreatureRenderRequest> {
    return requests_;
  }

  [[nodiscard]] auto reuse_cache() noexcept -> RequestReuseCache& {
    return reuse_cache_;
  }

  [[nodiscard]] auto size() const noexcept -> std::size_t;

  [[nodiscard]] auto empty() const noexcept -> bool;

private:
  std::vector<PreparedCreatureRenderRow> rows_{};
  std::vector<Render::Creature::CreatureRenderRequest> requests_{};
  RequestReuseCache reuse_cache_{};
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
