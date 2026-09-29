#include "app/orders/formation_preview_cache.h"

#include <cmath>

#include "game/core/world.h"
#include "game/formation/army_formation_registry.h"

namespace App::Controllers {

namespace {
constexpr float k_anchor_epsilon = 0.05F;
constexpr float k_facing_epsilon = 0.25F;
} // namespace

void FormationPreviewCache::invalidate_layout() {
  m_layout_valid = false;
  m_dirty = true;
}

void FormationPreviewCache::clear() {
  m_plan = Game::Formation::ArmyFormationPlan{};
  m_members.clear();
  invalidate_layout();
}

auto FormationPreviewCache::refresh(Engine::Core::World* world,
                                    Game::Formation::ArmyFormationRequest request,
                                    bool placing) -> Refresh {
  using Game::Formation::ArmyFormationPlanner;
  using Game::Formation::ArmyFormationRegistry;

  if (world == nullptr || request.members.empty() || !placing) {
    clear();
    return Refresh::Changed;
  }

  auto& registry = ArmyFormationRegistry::for_world(*world);
  request.group_id = registry.group_of(request.members.front());
  request.assign_nearest = true;

  if (!m_dirty && m_plan.valid &&
      (request.anchor - m_previewed_anchor).lengthSquared() <
          k_anchor_epsilon * k_anchor_epsilon &&
      std::abs(request.facing - m_previewed_facing) < k_facing_epsilon) {
    return Refresh::Unchanged;
  }

  if (m_members.empty()) {
    m_members = ArmyFormationPlanner::collect_members(*world, request.members);
  }

  const auto* previous_group = request.group_id != Game::Formation::k_invalid_group
                                   ? registry.find(request.group_id)
                                   : nullptr;
  auto const signature =
      ArmyFormationPlanner::layout_signature(m_members, request, previous_group);
  if (!m_layout_valid || m_layout.signature != signature) {
    m_layout = ArmyFormationPlanner::build_layout(m_members, request, previous_group);
    m_layout_valid = true;
  }

  m_plan =
      ArmyFormationPlanner::fit_to_ground(m_layout, m_members, request, previous_group);
  m_previewed_anchor = request.anchor;
  m_previewed_facing = request.facing;
  m_dirty = false;
  return Refresh::Changed;
}

} // namespace App::Controllers
