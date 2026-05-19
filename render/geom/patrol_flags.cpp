#include "patrol_flags.h"

#include <qvectornd.h>

#include <cstdint>
#include <optional>
#include <unordered_set>

#include "../../game/core/component.h"
#include "../../game/core/world.h"
#include "../../game/visuals/team_colors.h"
#include "../gl/resources.h"
#include "../scene_renderer.h"
#include "flag.h"
#include "transforms.h"

namespace Render::GL {

constexpr float k_position_grid_precision = 10.0F;
constexpr int k_position_hash_shift = 32;

void draw_flag(Renderer* renderer,
               ResourceManager* resources,
               const Geom::Flag::FlagMatrices& flag) {
  if ((renderer == nullptr) || (resources == nullptr)) {
    return;
  }

  renderer->mesh(
      get_unit_cylinder(),
      Render::Geom::cylinder_between(flag.pole_start, flag.pole_end, flag.pole_radius),
      flag.pole_color,
      resources->white(),
      1.0F);
  renderer->mesh(get_unit_cylinder(),
                 Render::Geom::cylinder_between(
                     flag.crossbeam_start, flag.crossbeam_end, flag.crossbeam_radius),
                 flag.pole_color,
                 resources->white(),
                 1.0F);

  Mesh* cloth_mesh = nullptr;
  Shader* banner_shader = nullptr;
  if (renderer->backend() != nullptr) {
    cloth_mesh = renderer->backend()->banner_mesh();
    banner_shader = renderer->backend()->banner_shader();
  }

  if (cloth_mesh != nullptr && banner_shader != nullptr) {
    Shader* const previous_shader = renderer->get_current_shader();
    renderer->set_current_shader(banner_shader);
    renderer->banner(cloth_mesh,
                     flag.pennant,
                     flag.pennant_color,
                     flag.pennant_trim_color,
                     resources->white(),
                     1.0F);
    renderer->set_current_shader(previous_shader);
  } else {
    renderer->mesh(resources->unit(),
                   flag.pennant_fallback,
                   flag.pennant_color,
                   resources->white(),
                   1.0F);
  }
}

void render_patrol_flags(Renderer* renderer,
                         ResourceManager* resources,
                         Engine::Core::World& world,
                         const std::optional<QVector3D>& preview_waypoint) {
  if ((renderer == nullptr) || (resources == nullptr)) {
    return;
  }

  std::unordered_set<uint64_t> rendered_positions;

  if (preview_waypoint.has_value()) {
    auto flag = Geom::Flag::create(preview_waypoint->x(),
                                   preview_waypoint->z(),
                                   QVector3D(0.4F, 1.0F, 0.5F),
                                   QVector3D(0.35F, 0.25F, 0.15F),
                                   1.5F);
    draw_flag(renderer, resources, flag);

    auto const grid_x =
        static_cast<int32_t>(preview_waypoint->x() * k_position_grid_precision);
    auto const grid_z =
        static_cast<int32_t>(preview_waypoint->z() * k_position_grid_precision);
    uint64_t const pos_hash = (static_cast<uint64_t>(grid_x) << k_position_hash_shift) |
                              static_cast<uint64_t>(grid_z);
    rendered_positions.insert(pos_hash);
  }

  auto patrol_entities = world.get_entities_with<Engine::Core::PatrolComponent>();

  for (auto* entity : patrol_entities) {
    auto* patrol = entity->get_component<Engine::Core::PatrolComponent>();
    if ((patrol == nullptr) || !patrol->patrolling || patrol->waypoints.empty()) {
      continue;
    }

    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if ((unit == nullptr) || unit->health <= 0) {
      continue;
    }

    for (const auto& waypoint : patrol->waypoints) {

      auto const grid_x =
          static_cast<int32_t>(waypoint.first * k_position_grid_precision);
      auto const grid_z =
          static_cast<int32_t>(waypoint.second * k_position_grid_precision);
      uint64_t const pos_hash =
          (static_cast<uint64_t>(grid_x) << k_position_hash_shift) |
          static_cast<uint64_t>(grid_z);

      if (!rendered_positions.insert(pos_hash).second) {
        continue;
      }

      auto flag = Geom::Flag::create(waypoint.first,
                                     waypoint.second,
                                     QVector3D(0.3F, 1.0F, 0.4F),
                                     QVector3D(0.35F, 0.25F, 0.15F),
                                     1.4F);
      draw_flag(renderer, resources, flag);
    }
  }
}

void render_commander_rally_flags(Renderer* renderer,
                                  ResourceManager* resources,
                                  Engine::Core::World* world,
                                  int preview_owner_id,
                                  const std::optional<QVector3D>& preview_pos) {
  if ((renderer == nullptr) || (resources == nullptr)) {
    return;
  }

  // Draw the placement preview flag (shown while picking a rally position).
  if (preview_pos.has_value()) {
    QVector3D const preview_color =
        Game::Visuals::team_colorForOwner(preview_owner_id);
    auto flag = Geom::Flag::create(preview_pos->x(),
                                   preview_pos->z(),
                                   preview_color,
                                    QVector3D(0.35F, 0.25F, 0.15F),
                                    1.6F);
    draw_flag(renderer, resources, flag);
  }

  // Draw placed rally flags from all commander components.
  if (world == nullptr) {
    return;
  }
  for (auto* entity :
       world->get_entities_with<Engine::Core::CommanderComponent>()) {
    auto* commander = entity->get_component<Engine::Core::CommanderComponent>();
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if ((commander == nullptr) || (unit == nullptr) ||
        !commander->flag_rally_flag_active) {
      continue;
    }
    QVector3D const flag_color = Game::Visuals::team_colorForOwner(unit->owner_id);
    auto flag = Geom::Flag::create(commander->flag_rally_flag_x,
                                   commander->flag_rally_flag_z,
                                   flag_color,
                                    QVector3D(0.35F, 0.25F, 0.15F),
                                    1.6F);
    draw_flag(renderer, resources, flag);
  }
}

} // namespace Render::GL
