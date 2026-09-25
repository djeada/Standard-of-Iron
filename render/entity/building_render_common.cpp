#include "building_render_common.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_map>

#include "game/core/component_core.h"
#include "game/systems/nation_id.h"
#include "game/visuals/building_asset_key.h"
#include "render/geom/transforms.h"
#include "render/gl/primitives.h"
#include "render/gl/resources.h"

namespace Render::GL {
namespace {

constexpr std::uint64_t k_building_cache_prune_interval = 256;
constexpr std::uint64_t k_building_cache_max_age = 2048;

thread_local std::uint64_t s_building_submit_tick{0};

struct BuildingStateMemory {
  BuildingState state{BuildingState::Normal};
  float changed_at{0.0F};
  bool switched{false};
  std::uint64_t last_seen_tick{0};
};

thread_local std::unordered_map<std::uint32_t, BuildingStateMemory>
    s_building_state_memory;

void prune_building_state_memory(std::uint64_t current_tick) {
  if ((current_tick % k_building_cache_prune_interval) != 0) {
    return;
  }
  std::erase_if(s_building_state_memory, [current_tick](auto const& entry) {
    return current_tick - entry.second.last_seen_tick > k_building_cache_max_age;
  });
}

auto building_unit(const DrawContext& ctx) -> Engine::Core::UnitComponent* {
  return (ctx.entity != nullptr)
             ? ctx.entity->get_component<Engine::Core::UnitComponent>()
             : nullptr;
}

auto building_white_texture(const DrawContext& ctx) -> Texture* {
  return (ctx.resources != nullptr) ? ctx.resources->white() : nullptr;
}

} // namespace

auto resolve_building_health_ratio(const DrawContext& ctx) -> float {
  auto* unit = building_unit(ctx);
  if (unit == nullptr) {
    return 0.0F;
  }

  return std::clamp(unit->health / float(std::max(1, unit->max_health)), 0.0F, 1.0F);
}

auto resolve_building_state(const DrawContext& ctx) -> BuildingState {
  auto* unit = building_unit(ctx);
  if (unit == nullptr) {
    return BuildingState::Normal;
  }
  float const ratio = resolve_building_health_ratio(ctx);
  if (ctx.template_prewarm || ctx.entity == nullptr) {
    return get_building_state(ratio);
  }

  auto const entity_id = static_cast<std::uint32_t>(ctx.entity->get_id());
  auto [it, inserted] = s_building_state_memory.try_emplace(entity_id);
  auto& memory = it->second;
  if (inserted ||
      s_building_submit_tick - memory.last_seen_tick > k_building_cache_max_age) {
    memory.state = get_building_state(ratio);
    memory.changed_at = 0.0F;
    memory.switched = false;
  } else {
    BuildingState const next = get_building_state(ratio, memory.state);
    if (next != memory.state) {
      memory.state = next;
      memory.changed_at = ctx.animation_time;
      memory.switched = true;
    }
  }
  memory.last_seen_tick = s_building_submit_tick;
  return memory.state;
}

auto building_state_transition_age(std::uint32_t entity_id, float now) -> float {
  auto const it = s_building_state_memory.find(entity_id);
  if (it == s_building_state_memory.end() || !it->second.switched) {
    return -1.0F;
  }
  return now - it->second.changed_at;
}

auto building_renderer_key(std::string_view nation_slug,
                           std::string_view building_type) -> std::string {
  return Game::Visuals::building_asset_key(nation_slug, building_type);
}

auto building_renderer_key(Game::Systems::NationID nation_id,
                           std::string_view building_type) -> std::string {
  return Game::Visuals::building_asset_key(nation_id, building_type);
}

auto canonicalize_building_renderer_key(std::string_view renderer_key)
    -> std::string_view {
  return Game::Visuals::canonicalize_building_asset_key(renderer_key);
}

auto resolve_building_renderer_key(std::string_view renderer_key,
                                   std::string_view building_type,
                                   Game::Systems::NationID nation_id) -> std::string {
  return Game::Visuals::resolve_building_asset_key(
      renderer_key, building_type, nation_id);
}

void submit_building_instance(ISubmitter& out,
                              const DrawContext& ctx,
                              const RenderArchetype& archetype,
                              std::span<const QVector3D> palette) {
  RenderInstance instance;
  instance.archetype = &archetype;
  instance.world = ctx.model;
  instance.palette = palette;
  instance.default_texture = building_white_texture(ctx);
  instance.alpha_multiplier = ctx.alpha_multiplier;
  if (ctx.entity != nullptr) {
    ++s_building_submit_tick;
    prune_building_state_memory(s_building_submit_tick);
    if (!ctx.template_prewarm) {
      instance.static_id = static_cast<std::uint32_t>(ctx.entity->get_id());
    }
    switch (resolve_building_state(ctx)) {
    case BuildingState::Damaged:
      instance.damage_material_id = 10;
      break;
    case BuildingState::Destroyed:
      instance.damage_material_id = 20;
      break;
    case BuildingState::Normal:
    default:
      break;
    }
  }
  out.render_instance(instance);
}

void submit_building_box(ISubmitter& out,
                         Mesh* mesh,
                         Texture* texture,
                         const QMatrix4x4& model,
                         const QVector3D& pos,
                         const QVector3D& size,
                         const QVector3D& color,
                         float alpha) {
  if (mesh == nullptr) {
    return;
  }

  QMatrix4x4 local = model;
  local.translate(pos);
  local.scale(size);
  out.mesh(mesh, local, color, texture, alpha);
}

void submit_building_cylinder(ISubmitter& out,
                              const QMatrix4x4& model,
                              const QVector3D& start,
                              const QVector3D& end,
                              float radius,
                              const QVector3D& color,
                              Texture* texture,
                              float alpha) {
  out.mesh(get_unit_cylinder(),
           model * Render::Geom::cylinder_between(start, end, radius),
           color,
           texture,
           alpha);
}

void draw_building_selection_overlay(ISubmitter& out,
                                     const DrawContext& ctx,
                                     const BuildingSelectionStyle& style) {
  QMatrix4x4 model;
  QVector3D const pos = ctx.model.column(3).toVector3D();
  model.translate(pos.x(), 0.0F, pos.z());
  model.scale(style.scale_x, 1.0F, style.scale_z);

  if (ctx.selected) {
    out.selection_smoke(model, QVector3D(0.2F, 0.85F, 0.2F), 0.35F);
  } else if (ctx.hovered) {
    out.selection_smoke(model, QVector3D(0.95F, 0.92F, 0.25F), 0.22F);
  }
}

auto select_nation_variant_renderer_key(std::string_view roman_key,
                                        std::string_view carthage_key,
                                        Game::Systems::NationID nation_id,
                                        std::string_view sepulcher_key)
    -> std::string_view {
  switch (nation_id) {
  case Game::Systems::NationID::Carthage:
    return carthage_key;
  case Game::Systems::NationID::IronSepulcher:
    return sepulcher_key.empty() ? roman_key : sepulcher_key;
  case Game::Systems::NationID::RomanRepublic:
  default:
    return roman_key;
  }
}

void register_nation_variant_renderer(EntityRendererRegistry& registry,
                                      const std::string& public_key,
                                      std::string roman_key,
                                      std::string carthage_key,
                                      std::string sepulcher_key) {
  registry.register_renderer(
      public_key,
      [&registry,
       roman_key = std::move(roman_key),
       carthage_key = std::move(carthage_key),
       sepulcher_key = std::move(sepulcher_key)](const DrawContext& ctx,
                                                 ISubmitter& out) {
        auto* unit = building_unit(ctx);
        if (unit == nullptr) {
          return;
        }

        std::string const renderer_key(select_nation_variant_renderer_key(
            roman_key, carthage_key, unit->nation_id, sepulcher_key));
        auto renderer = registry.get(renderer_key);
        if (renderer) {
          renderer(ctx, out);
        }
      });
}

void register_building_renderer(EntityRendererRegistry& registry,
                                std::string_view nation_slug,
                                std::string_view building_type,
                                RenderFunc func) {
  const std::string canonical_key = building_renderer_key(nation_slug, building_type);
  registry.register_renderer(canonical_key, std::move(func));
}

} // namespace Render::GL
