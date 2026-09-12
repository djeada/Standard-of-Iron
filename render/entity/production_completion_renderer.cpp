#include "production_completion_renderer.h"

#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <numbers>

#include "game/core/component_presentation.h"
#include "game/core/ownership_constants.h"
#include "game/core/world.h"
#include "game/map/render_visibility_rules.h"
#include "game/map/visibility_service.h"
#include "render/draw_commands.h"
#include "render/local_lighting.h"
#include "render/scene_renderer.h"
#include "render/world_view.h"

namespace Render::GL {

namespace {

constexpr float k_fade_in_seconds = 0.12F;
constexpr float k_flare_seconds = 0.45F;
constexpr float k_ring_seconds = 0.85F;
constexpr float k_ring_delay_seconds = 0.26F;
constexpr int k_ring_count = 2;

constexpr float k_spark_cycle_seconds = 0.28F;
constexpr int k_glint_count = 12;
constexpr QVector3D k_gold{1.0F, 0.78F, 0.30F};
constexpr QVector3D k_pale_gold{1.0F, 0.94F, 0.72F};

void submit_ground_disc(Renderer* renderer,
                        const QVector3D& ground,
                        float radius,
                        float alpha) {
  if (alpha <= 0.01F) {
    return;
  }
  GroundMarkerCmd disc;
  disc.center = QVector3D(ground.x(), ground.y() + 0.05F, ground.z());
  disc.outer_radius = radius * 0.95F;
  disc.thickness = disc.outer_radius;
  disc.color = k_pale_gold;
  disc.alpha = alpha;
  renderer->ground_marker(disc);
}

void submit_ground_ring(Renderer* renderer,
                        const QVector3D& ground,
                        float radius,
                        float ring_age,
                        float alpha_scale) {
  if (ring_age < 0.0F || ring_age >= k_ring_seconds || alpha_scale <= 0.01F) {
    return;
  }
  const float travel = ring_age / k_ring_seconds;
  const float fade = (1.0F - travel) * (1.0F - travel);
  GroundMarkerCmd ring;
  ring.center = QVector3D(ground.x(), ground.y() + 0.07F, ground.z());
  ring.outer_radius = radius * (0.55F + 1.10F * travel);
  ring.thickness = std::max(0.08F, radius * 0.16F * (1.0F - 0.5F * travel));
  ring.color = k_gold;
  ring.alpha = alpha_scale * fade;
  renderer->ground_marker(ring);
}

void submit_glow_light(Renderer* renderer,
                       const QVector3D& position,
                       float radius,
                       float intensity,
                       float flare) {
  Render::LocalLight glow;
  glow.position = position + QVector3D(0.0F, radius * 0.75F, 0.0F);
  glow.color = k_gold;
  glow.radius = std::clamp(radius * 2.6F, 4.0F, 9.0F);
  glow.intensity = std::min(1.05F, 0.5F * intensity + 0.7F * flare);
  renderer->local_light(glow);
}

} // namespace

void render_production_completions(Renderer* renderer,
                                   Engine::Core::World* world,
                                   int local_owner_id,
                                   bool reduced_motion) {
  if (renderer == nullptr || world == nullptr) {
    return;
  }

  static const bool trace = qEnvironmentVariableIsSet("SOI_FX_TRACE");

  const auto& world_view = renderer->world_view();
  const auto visibility = world_view.has_visibility()
                              ? world_view.visibility()->snapshot_ptr()
                              : Game::Map::VisibilityService::SnapshotPtr{};
  const bool fog_applies = visibility != nullptr && visibility->initialized &&
                           !Game::Core::is_neutral_owner(local_owner_id);

  for (auto [entity, effect, transform, unit] :
       world->entity_view<Engine::Core::ProductionCompletionComponent,
                          Engine::Core::TransformComponent,
                          Engine::Core::UnitComponent>()) {
    if (unit.health <= 0 ||
        entity.has_component<Engine::Core::PendingRemovalComponent>() ||
        effect.remaining <= 0.0F) {
      continue;
    }
    if (fog_applies && unit.owner_id != local_owner_id &&
        Game::Map::classify_world_visibility(
            *visibility, transform.position.x, transform.position.z) !=
            Game::Map::RenderVisibilityState::Visible) {
      continue;
    }

    const float age =
        Engine::Core::ProductionCompletionComponent::k_duration - effect.remaining;
    const float progress = std::clamp(
        age / Engine::Core::ProductionCompletionComponent::k_duration, 0.0F, 1.0F);
    const float fade_in = std::clamp(age / k_fade_in_seconds, 0.0F, 1.0F);
    const float fade_out = 1.0F - progress * progress * (3.0F - 2.0F * progress);
    const float intensity = fade_in * fade_out;
    const float flare = fade_in * std::clamp(1.0F - age / k_flare_seconds, 0.0F, 1.0F);
    const QVector3D ground(
        transform.position.x, transform.position.y, transform.position.z);
    const QVector3D position(ground.x(), ground.y() + 0.08F, ground.z());
    const float time = reduced_motion ? 0.0F : age;
    const float radius =
        effect.radius * (reduced_motion ? 1.0F : 1.0F + 0.18F * progress);
    if (trace) {
      qWarning("FXTRACE completion p%d age %.2f radius %.2f at %.1f %.1f",
               unit.owner_id,
               static_cast<double>(age),
               static_cast<double>(effect.radius),
               static_cast<double>(ground.x()),
               static_cast<double>(ground.z()));
    }

    submit_ground_disc(
        renderer, ground, effect.radius, 0.16F * intensity * intensity + 0.22F * flare);
    submit_glow_light(renderer, ground, effect.radius, intensity, flare);

    renderer->healer_aura(position, k_gold, radius, intensity, time);
    renderer->healer_aura(
        position, k_pale_gold, radius * 0.78F, intensity * 0.9F, time);
    renderer->healer_aura(
        position, k_pale_gold, radius * 0.52F, intensity * 0.8F, time);
    if (reduced_motion) {
      submit_ground_ring(
          renderer, ground, effect.radius, k_ring_seconds * 0.35F, 0.55F * intensity);
      continue;
    }

    for (int ring = 0; ring < k_ring_count; ++ring) {
      submit_ground_ring(renderer,
                         ground,
                         effect.radius,
                         age - static_cast<float>(ring) * k_ring_delay_seconds,
                         0.60F - 0.18F * static_cast<float>(ring));
    }

    for (int i = 0; i < k_glint_count; ++i) {
      const float phase = static_cast<float>(i) / k_glint_count;
      const float angle = phase * 2.0F * std::numbers::pi_v<float> + age * 0.65F;
      const float orbit = radius * (0.70F + 0.18F * std::sin(phase * 19.0F));
      const float height = effect.radius * (0.12F + progress * 0.85F + phase * 0.30F);
      const QVector3D outward(std::cos(angle), 0.0F, std::sin(angle));
      const QVector3D glint =
          position + outward * orbit + QVector3D(0.0F, height, 0.0F);

      const float spark_age =
          std::fmod(age + phase * k_spark_cycle_seconds, k_spark_cycle_seconds);
      const float shimmer = 0.75F + 0.25F * std::sin(age * 7.0F + phase * 23.0F);
      renderer->metal_spark(
          glint,
          i % 3 == 0 ? k_pale_gold : k_gold,
          0.09F + 0.035F * effect.radius,
          (intensity + flare) * shimmer * 1.3F,
          spark_age,
          (outward * 0.5F + QVector3D(0.0F, 1.0F, 0.0F)).normalized());
    }
  }
}

} // namespace Render::GL
