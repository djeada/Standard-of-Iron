#include "building_spawn_setup.h"

#include "../core/component.h"
#include "../core/entity.h"
#include "../visuals/team_colors.h"
#include "render/entity/building_render_common.h"

namespace Game::Units {

auto add_building_renderable(Engine::Core::Entity &entity, int owner_id,
                             Game::Systems::NationID nation_id,
                             std::string_view building_type)
    -> Engine::Core::RenderableComponent * {
  auto *renderable =
      entity.add_component<Engine::Core::RenderableComponent>("", "");
  if (renderable == nullptr) {
    return nullptr;
  }

  renderable->visible = true;
  renderable->mesh = Engine::Core::RenderableComponent::MeshKind::Cube;
  renderable->renderer_id =
      Render::GL::building_renderer_key(nation_id, building_type);

  QVector3D const team_color = Game::Visuals::team_colorForOwner(owner_id);
  renderable->color[0] = team_color.x();
  renderable->color[1] = team_color.y();
  renderable->color[2] = team_color.z();

  auto *building =
      Engine::Core::get_or_add_component<Engine::Core::BuildingComponent>(
          entity);
  if (building != nullptr) {
    building->original_nation_id = nation_id;
  }

  return renderable;
}

} // namespace Game::Units
