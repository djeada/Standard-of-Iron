#include "render/creature/pipeline/preparation_common.h"

#include <QVector4D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

#include "animation/bpat/bpat_registry.h"
#include "animation/clip_manifest.h"
#include "animation/locomotion_manifest.h"
#include "animation/playback_manifest.h"
#include "animation/rig/humanoid_proportions.h"
#include "game/core/component_core.h"
#include "game/core/entity.h"
#include "game/map/terrain_service.h"
#include "render/creature/archetype_registry.h"
#include "render/creature/gpu/anim_key.h"
#include "render/creature/humanoid_clip_ids.h"
#include "render/creature/pipeline/species_contract.h"
#include "render/creature/pose_intent.h"
#include "render/frame/draw_context.h"

namespace Render::Creature::Pipeline {

auto pass_intent_from_ctx(const Render::GL::DrawContext& ctx) noexcept
    -> RenderPassIntent {
  return ctx.template_prewarm ? RenderPassIntent::Shadow : RenderPassIntent::Main;
}

auto derive_unit_seed(const Render::GL::DrawContext& ctx,
                      const Engine::Core::UnitComponent* unit) noexcept
    -> std::uint32_t {
  if (ctx.has_seed_override) {
    return ctx.seed_override;
  }
  std::uint32_t seed = 0U;
  if (unit != nullptr) {
    seed ^= static_cast<std::uint32_t>(unit->owner_id * 2654435761U);
  }
  if (ctx.entity != nullptr) {
    seed ^= ctx.entity->get_id() * 2246822519U;
  }
  return seed;
}

auto horse_clip_contact_y(std::uint16_t clip_id,
                          float phase) noexcept -> std::optional<float> {
  auto const* blob = Render::Creature::Bpat::BpatRegistry::instance().blob(
      Render::Creature::Bpat::k_species_horse);
  if (blob == nullptr || clip_id >= blob->clip_count()) {
    return std::nullopt;
  }

  auto const playback = resolve_bpat_playback(blob, clip_id, phase);
  if (!playback.valid()) {
    return std::nullopt;
  }

  auto const contacts = blob->frame_contacts();
  if (playback.global_frame >= contacts.size()) {
    return std::nullopt;
  }
  return contacts[playback.global_frame].foot_y;
}

auto bind_palette_for_kind(CreatureKind kind) noexcept -> std::span<const QMatrix4x4> {
  const SpeciesContract* contract = species_contract(kind);
  return contract != nullptr && contract->bind_palette != nullptr
             ? contract->bind_palette()
             : std::span<const QMatrix4x4>{};
}

namespace {

auto lowest_of(std::span<const std::size_t> bones,
               std::span<const QMatrix4x4> palette,
               auto&& sample) -> float {
  float lowest = std::numeric_limits<float>::max();
  for (auto const bone : bones) {
    if (bone < palette.size()) {
      lowest = std::min(lowest, sample(bone));
    }
  }
  return lowest == std::numeric_limits<float>::max() ? 0.0F : lowest;
}

auto foot_bones(CreatureKind kind) noexcept -> std::span<const std::size_t> {
  const SpeciesContract* contract = species_contract(kind);
  return contract != nullptr ? contract->foot_bones : std::span<const std::size_t>{};
}

} // namespace

auto palette_contact_y(CreatureKind kind,
                       std::span<const QMatrix4x4> skin_palette) noexcept -> float {
  if (kind == CreatureKind::Humanoid) {
    auto const bind = bind_palette_for_kind(kind);
    constexpr std::array<QVector3D, 2> k_sole_points{
        QVector3D{0.0F, -Render::GL::HumanProportions::FOOT_Y_OFFSET_DEFAULT, -0.060F},
        QVector3D{0.0F, -Render::GL::HumanProportions::FOOT_Y_OFFSET_DEFAULT, 0.165F}};
    return lowest_of(foot_bones(kind), skin_palette, [&](std::size_t bone) {
      if (bone >= bind.size()) {
        return 0.0F;
      }
      float lowest = std::numeric_limits<float>::max();
      for (auto const& local : k_sole_points) {
        QVector4D const bound = bind[bone] * QVector4D(local, 1.0F);
        float const posed = (skin_palette[bone] * bound).y();
        lowest = std::min(lowest, posed - bound.y());
      }
      return lowest;
    });
  }
  return lowest_of(foot_bones(kind), skin_palette, [&](std::size_t bone) {
    return skin_palette[bone].column(3).y();
  });
}

auto palette_foot_contact_y(
    CreatureKind kind, std::span<const QMatrix4x4> skin_palette) noexcept -> float {
  auto const bind = bind_palette_for_kind(kind);
  return lowest_of(foot_bones(kind), skin_palette, [&](std::size_t bone) {
    return bone < bind.size() ? (skin_palette[bone] * bind[bone]).column(3).y() : 0.0F;
  });
}

auto creature_kind_for_bpat_species(std::uint32_t species_id) noexcept -> CreatureKind {
  switch (species_id) {
  case Render::Creature::Bpat::k_species_horse:
    return CreatureKind::Horse;
  case Render::Creature::Bpat::k_species_elephant:
    return CreatureKind::Elephant;
  case Render::Creature::Bpat::k_species_sheep:
    return CreatureKind::Sheep;
  case Render::Creature::Bpat::k_species_wolf:
    return CreatureKind::Wolf;
  default:
    return CreatureKind::Humanoid;
  }
}

auto sample_terrain_height_or_fallback(const Game::Map::TerrainService& terrain_service,
                                       float world_x,
                                       float world_z,
                                       float fallback_y) noexcept -> float {
  return terrain_service
      .resolve_surface_world_position(world_x, world_z, 0.0F, fallback_y)
      .y();
}

auto model_world_origin(const QMatrix4x4& model) noexcept -> QVector3D {
  return model.column(3).toVector3D();
}

auto make_runtime_prewarm_ctx(const Render::GL::DrawContext& ctx) noexcept
    -> Render::GL::DrawContext {
  Render::GL::DrawContext runtime_ctx = ctx;
  runtime_ctx.template_prewarm = false;
  runtime_ctx.prewarming_via_runtime_path = true;
  runtime_ctx.allow_template_cache = false;
  runtime_ctx.suppress_animation_state_persistence = true;
  return runtime_ctx;
}

auto ground_model_contact_to_surface(const Game::Map::TerrainService& terrain,
                                     QMatrix4x4& model,
                                     float local_contact_y,
                                     float y_scale,
                                     float entity_ground_offset) noexcept -> float {
  float const world_y_offset = (entity_ground_offset + local_contact_y) * y_scale;
  return ground_model_to_terrain(terrain, model, world_y_offset);
}

auto ground_model_to_terrain(const Game::Map::TerrainService& terrain_service,
                             QMatrix4x4& model,
                             float world_y_offset) noexcept -> float {
  QVector3D const origin = model_world_origin(model);
  QVector3D const grounded_origin = terrain_service.resolve_surface_world_position(
      origin.x(), origin.z(), -world_y_offset, origin.y());
  set_model_world_y(model, grounded_origin.y());
  return grounded_origin.y() + world_y_offset;
}

void set_model_world_y(QMatrix4x4& model, float world_y) noexcept {
  QVector3D origin = model_world_origin(model);
  origin.setY(world_y);
  model.setColumn(3, QVector4D(origin, 1.0F));
}

} // namespace Render::Creature::Pipeline
