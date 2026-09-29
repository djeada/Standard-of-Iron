#include "formation_structure_facade.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>

#include "../../core/component.h"
#include "formation_local_frame.h"
#include "target_rules.h"

namespace Game::Systems::Combat {

auto build_structure_facade(
    Engine::Core::Entity& attacker,
    const FormationCombat::FormationLayout& layout,
    const Engine::Core::TransformComponent* actor_transform,
    const Engine::Core::UnitTraversalLayoutStateComponent* traversal,
    Engine::Core::Entity* display_opponent,
    bool outgoing_melee) -> StructureFacadeFrame {
  StructureFacadeFrame frame;
  frame.attacks_structure =
      outgoing_melee && display_opponent != nullptr && is_building(display_opponent);
  if (frame.attacks_structure && actor_transform != nullptr) {
    QVector3D const root(actor_transform->position.x,
                         actor_transform->position.y,
                         actor_transform->position.z);
    frame.facade = closest_structure_surface(*display_opponent, root);
    for (auto const& slot : layout.live_slots) {
      QVector3D const anchor(slot.world_x, actor_transform->position.y, slot.world_z);
      QVector3D const from_facade = anchor - frame.facade.point;
      frame.closest_gap =
          std::min(frame.closest_gap,
                   QVector3D::dotProduct(from_facade, frame.facade.outward_normal));
    }
    if (std::isfinite(frame.closest_gap)) {
      float const desired_gap = structure_attack_profile(&attacker).contact_clearance;
      frame.render_shift = std::max(0.0F, desired_gap - frame.closest_gap);

      QVector3D const world_shift = frame.facade.outward_normal * frame.render_shift;
      auto const [shift_local_x, shift_local_z] =
          world_vector_to_local(*actor_transform, world_shift.x(), world_shift.z());
      frame.shift_local_x = shift_local_x;
      frame.shift_local_z = shift_local_z;
    }
  }

  if (display_opponent != nullptr && actor_transform != nullptr &&
      is_building(display_opponent)) {
    for (auto const& slot : layout.live_slots) {
      float anchor_x = slot.local_x;
      float anchor_z = slot.local_z;
      if (traversal != nullptr) {
        if (auto const* moved = traversal->slot_for(slot.index); moved != nullptr) {
          anchor_x = moved->current_local_x;
          anchor_z = moved->current_local_z;
        }
      }
      frame.nearest_anchor = std::min(
          frame.nearest_anchor,
          closest_structure_surface(
              *display_opponent, local_to_world(*actor_transform, anchor_x, anchor_z))
              .distance);
    }
  }
  return frame;
}

} // namespace Game::Systems::Combat
