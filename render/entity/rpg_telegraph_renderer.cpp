#include "render/entity/rpg_telegraph_renderer.h"

#include <QMatrix4x4>

#include <algorithm>
#include <cmath>
#include <numbers>

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/systems/combat_actions/combat_action_definition.h"
#include "game/systems/rpg_combat_system/rpg_targeting.h"
#include "render/entity/building_collapse.h"
#include "render/frame/frame_submitter.h"
namespace {

using Engine::Core::CombatAnimationState;

constexpr float k_telegraph_range = 12.0F;
constexpr float k_ring_y_offset = 0.025F;
constexpr float k_marker_thickness_ratio = 0.13F;
constexpr float k_marker_min_thickness = 0.05F;

constexpr QVector3D k_danger_red{1.0F, 0.08F, 0.02F};
constexpr QVector3D k_warning_orange{1.0F, 0.55F, 0.08F};

constexpr QVector3D k_stagger_violet{0.72F, 0.42F, 1.0F};
constexpr QVector3D k_flash_white{1.0F, 0.85F, 0.45F};
constexpr QVector3D k_lock_gold{1.0F, 0.82F, 0.10F};
constexpr QVector3D k_aim_cyan{0.16F, 0.92F, 1.0F};
constexpr QVector3D k_contact_red{1.0F, 0.18F, 0.08F};

inline float pulse(float t, float hz, float phase = 0.0F) {
  return 0.5F + 0.5F * std::sin(t * 6.2832F * hz + phase);
}

enum class RpgMarkerRole : std::uint8_t {
  Stagger,
  Aim,
  Lock,
  Telegraph,
  StrikeFlash,
  ContactHit,
};

struct RpgMarkerStyle {
  Game::Accessibility::TeamPattern pattern;
  bool focused;
};

inline auto marker_style(RpgMarkerRole role) -> RpgMarkerStyle {
  switch (role) {
  case RpgMarkerRole::Stagger:
    return {Game::Accessibility::TeamPattern::Dotted, false};
  case RpgMarkerRole::Aim:
    return {Game::Accessibility::TeamPattern::Dashed, false};
  case RpgMarkerRole::Lock:
    return {Game::Accessibility::TeamPattern::DoubleRing, true};
  case RpgMarkerRole::Telegraph:
    return {Game::Accessibility::TeamPattern::Chevron, false};
  case RpgMarkerRole::StrikeFlash:
    return {Game::Accessibility::TeamPattern::Solid, false};
  case RpgMarkerRole::ContactHit:
    return {Game::Accessibility::TeamPattern::Notched, false};
  }
  return {Game::Accessibility::TeamPattern::Solid, false};
}

inline void submit_marker(Render::GL::FrameSubmitter* renderer,
                          RpgMarkerRole role,
                          float px,
                          float py,
                          float pz,
                          float radius,
                          float alpha,
                          const QVector3D& color) {
  const auto style = marker_style(role);

  Render::GL::GroundMarkerCmd marker;
  marker.center = QVector3D(px, py + k_ring_y_offset, pz);
  marker.outer_radius = radius;
  marker.thickness =
      std::max(k_marker_min_thickness, radius * k_marker_thickness_ratio);
  marker.color = color;
  marker.alpha = alpha;
  marker.pattern = style.pattern;
  marker.focused = style.focused;
  renderer->ground_marker(marker);
}

enum class BodyRingPriority : std::uint8_t {
  Stagger = 0,
  Aim = 1,
  Lock = 2,
  Telegraph = 3,
};

struct BodyRing {
  Engine::Core::EntityID entity_id{0};
  std::uint16_t soldier_slot{
      Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot};
  BodyRingPriority priority{BodyRingPriority::Stagger};
  RpgMarkerRole role{RpgMarkerRole::Stagger};
  float x{0.0F};
  float y{0.0F};
  float z{0.0F};
  float radius{0.5F};
  float alpha{0.0F};
  QVector3D color;
};

class BodyRingSet {
public:
  void add(BodyRing ring) {
    for (auto& existing : m_rings) {
      if (existing.entity_id != ring.entity_id ||
          existing.soldier_slot != ring.soldier_slot) {
        continue;
      }
      if (ring.priority > existing.priority) {
        existing = ring;
      }
      return;
    }
    m_rings.push_back(ring);
  }

  void submit(Render::GL::FrameSubmitter* renderer) const {
    for (auto const& ring : m_rings) {
      submit_marker(renderer,
                    ring.role,
                    ring.x,
                    ring.y,
                    ring.z,
                    ring.radius,
                    ring.alpha,
                    ring.color);
    }
  }

private:
  std::vector<BodyRing> m_rings;
};

} // namespace

namespace Render::GL {

void RpgTelegraphRenderer::clear() {
  m_cache.clear();
  m_strike_flashes.clear();
}

void RpgTelegraphRenderer::render(FrameSubmitter* renderer,
                                  Engine::Core::World* world,
                                  Engine::Core::EntityID commander_id,
                                  Engine::Core::EntityID locked_target_id,
                                  float anim_time) {
  using Engine::Core::CombatStateComponent;
  using Engine::Core::StaggerComponent;
  using Engine::Core::TransformComponent;

  auto* commander_ent = world->get_entity(commander_id);
  if (commander_ent == nullptr) {
    return;
  }
  auto* cmd_tf = commander_ent->get_component<TransformComponent>();
  if (cmd_tf == nullptr) {
    return;
  }
  const float cx = cmd_tf->position.x;
  const float cz = cmd_tf->position.z;

  std::vector<Engine::Core::EntityID> seen;
  seen.reserve(16);

  for (auto* entity : world->collect_entities_with<CombatStateComponent>()) {
    const auto id = entity->get_id();
    if (id == commander_id) {
      continue;
    }
    auto* csc = entity->get_component<CombatStateComponent>();
    auto* tf = entity->get_component<TransformComponent>();
    if (csc == nullptr || tf == nullptr) {
      continue;
    }
    auto const visible_attacker =
        Game::Systems::RpgCombat::resolve_damage_carrier(*entity, commander_id);
    const float ex =
        visible_attacker.has_value() ? visible_attacker->position.x() : tf->position.x;
    const float ez =
        visible_attacker.has_value() ? visible_attacker->position.z() : tf->position.z;
    const float ey =
        visible_attacker.has_value() ? visible_attacker->position.y() : tf->position.y;
    const float dx = ex - cx;
    const float dz = ez - cz;
    if (dx * dx + dz * dz > k_telegraph_range * k_telegraph_range) {
      continue;
    }

    const auto cur_state = csc->animation_state;
    auto it = m_cache.find(id);

    std::uint16_t const carrier_slot =
        visible_attacker.has_value()
            ? visible_attacker->soldier_slot
            : Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot;

    if (cur_state == CombatAnimationState::WindUp) {
      seen.push_back(id);
      if (it == m_cache.end()) {
        m_cache[id] =
            TelegraphEntry{ex, ez, ey, carrier_slot, CombatAnimationState::WindUp};
      } else {

        constexpr float k_pos_epsilon = 0.01F;
        if (std::abs(it->second.last_pos_x - ex) > k_pos_epsilon ||
            std::abs(it->second.last_pos_z - ez) > k_pos_epsilon) {
          it->second.last_pos_x = ex;
          it->second.last_pos_z = ez;
          it->second.base_y = ey;
        }
        it->second.soldier_slot = carrier_slot;
        it->second.prev_state = CombatAnimationState::WindUp;
      }
    } else if (cur_state == CombatAnimationState::Strike && it != m_cache.end() &&
               it->second.prev_state == CombatAnimationState::WindUp) {

      m_strike_flashes.push_back(StrikeFlash{{ex, ey, ez}, anim_time});
      m_cache.erase(it);
    }
  }

  for (auto it = m_cache.begin(); it != m_cache.end();) {
    const bool still_seen =
        std::find(seen.begin(), seen.end(), it->first) != seen.end();
    if (!still_seen) {
      it = m_cache.erase(it);
    } else {
      ++it;
    }
  }

  m_strike_flashes.erase(std::remove_if(m_strike_flashes.begin(),
                                        m_strike_flashes.end(),
                                        [anim_time](const StrikeFlash& f) {
                                          return (anim_time - f.start_time) >=
                                                 StrikeFlash::k_duration;
                                        }),
                         m_strike_flashes.end());

  BodyRingSet body_rings;

  for (const auto& [id, entry] : m_cache) {
    auto* entity = world->get_entity(id);
    if (entity == nullptr) {
      continue;
    }
    auto* csc = entity->get_component<CombatStateComponent>();
    if (csc == nullptr) {
      continue;
    }
    const float progress =
        (csc->state_duration > 0.0F)
            ? std::clamp(csc->state_time / csc->state_duration, 0.0F, 1.0F)
            : 0.0F;

    bool unblockable = false;
    if (auto const* action =
            entity->get_component<Engine::Core::RpgCommanderActionComponent>()) {
      auto const* definition =
          Game::Systems::CombatActions::find_combat_action_definition(
              static_cast<Game::Systems::CombatActions::CombatActionId>(
                  action->combat_action_id));
      unblockable = definition != nullptr && definition->damage.unblockable;
    }

    const float ring_r =
        (unblockable ? 0.74F : 0.50F) + (unblockable ? 0.30F : 0.18F) * progress;
    const float pulse_speed =
        (unblockable ? 7.0F : 4.0F) + (unblockable ? 9.0F : 6.0F) * progress;
    const float ring_alpha =
        (unblockable ? 0.78F : 0.62F) +
        (unblockable ? 0.22F : 0.25F) * pulse(anim_time, pulse_speed);
    QVector3D const ring_color =
        unblockable ? k_danger_red
                    : k_warning_orange * (1.0F - progress) + k_danger_red * progress;

    body_rings.add({.entity_id = id,
                    .soldier_slot = entry.soldier_slot,
                    .priority = BodyRingPriority::Telegraph,
                    .role = RpgMarkerRole::Telegraph,
                    .x = entry.last_pos_x,
                    .y = entry.base_y,
                    .z = entry.last_pos_z,
                    .radius = ring_r,
                    .alpha = ring_alpha,
                    .color = ring_color});
  }

  for (auto* entity : world->collect_entities_with<StaggerComponent>()) {
    if (entity->get_id() == commander_id) {
      continue;
    }
    auto* tf = entity->get_component<TransformComponent>();
    auto* sc = entity->get_component<StaggerComponent>();
    if (tf == nullptr || sc == nullptr) {
      continue;
    }

    auto const carrier =
        Game::Systems::RpgCombat::resolve_damage_carrier(*entity, commander_id);
    const float ex = carrier.has_value() ? carrier->position.x() : tf->position.x;
    const float ey = carrier.has_value() ? carrier->position.y() : tf->position.y;
    const float ez = carrier.has_value() ? carrier->position.z() : tf->position.z;
    const float dx = ex - cx;
    const float dz = ez - cz;
    if (dx * dx + dz * dz > k_telegraph_range * k_telegraph_range) {
      continue;
    }

    const float fade = std::clamp(sc->remaining / 0.5F, 0.0F, 1.0F);
    const float stagger_alpha = 0.70F * fade * (0.7F + 0.3F * pulse(anim_time, 6.0F));
    body_rings.add(
        {.entity_id = entity->get_id(),
         .soldier_slot =
             carrier.has_value()
                 ? carrier->soldier_slot
                 : Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot,
         .priority = BodyRingPriority::Stagger,
         .role = RpgMarkerRole::Stagger,
         .x = ex,
         .y = ey,
         .z = ez,
         .radius = carrier.has_value() ? std::max(0.58F, carrier->body_radius * 1.24F)
                                       : 0.90F,
         .alpha = stagger_alpha,
         .color = k_stagger_violet});
  }

  for (const auto& flash : m_strike_flashes) {
    const float elapsed = anim_time - flash.start_time;
    const float t = elapsed / StrikeFlash::k_duration;

    const float flash_r = 0.30F + 0.26F * t;
    const float flash_alpha = (1.0F - t) * (1.0F - t) * 0.62F;
    submit_marker(renderer,
                  RpgMarkerRole::StrikeFlash,
                  flash.pos.x(),
                  flash.pos.y(),
                  flash.pos.z(),
                  flash_r,
                  flash_alpha,
                  k_flash_white);

    if (elapsed < 0.08F) {
      QVector3D const spark_pos(flash.pos.x(), flash.pos.y() + 0.4F, flash.pos.z());
      QVector3D const spark_color(1.0F, 0.85F, 0.4F);
      renderer->metal_spark(spark_pos, spark_color, 0.24F, 1.15F, elapsed);
    }
  }

  auto const* targets =
      commander_ent->get_component<Engine::Core::RpgCommanderTargetComponent>();
  auto resolve_target = [world](Engine::Core::EntityID entity_id,
                                std::uint16_t soldier_slot)
      -> std::optional<Game::Systems::RpgCombat::SoldierTarget> {
    auto* entity = world->get_entity(entity_id);
    return entity != nullptr
               ? Game::Systems::RpgCombat::resolve_soldier_target(*entity, soldier_slot)
               : std::nullopt;
  };

  Engine::Core::EntityID const resolved_lock_id =
      targets != nullptr ? targets->explicit_lock_target_id : locked_target_id;
  std::uint16_t const resolved_lock_slot =
      targets != nullptr ? targets->explicit_lock_soldier_slot
                         : Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot;
  bool aim_ring_active = false;
  if (targets != nullptr && targets->aim_candidate_in_range &&
      targets->aim_candidate_id != 0) {
    auto const aim =
        resolve_target(targets->aim_candidate_id, targets->aim_candidate_soldier_slot);
    if (aim.has_value()) {
      bool const aim_is_lock =
          targets->aim_candidate_id == resolved_lock_id &&
          targets->aim_candidate_soldier_slot == resolved_lock_slot;
      aim_ring_active = true;
      float const alpha = 0.78F + 0.18F * pulse(anim_time, 4.0F);
      float const radius =
          std::max(0.54F, aim->body_radius * (aim_is_lock ? 1.30F : 1.22F));
      body_rings.add(
          {.entity_id = targets->aim_candidate_id,
           .soldier_slot = targets->aim_candidate_soldier_slot,
           .priority = aim_is_lock ? BodyRingPriority::Lock : BodyRingPriority::Aim,
           .role = aim_is_lock ? RpgMarkerRole::Lock : RpgMarkerRole::Aim,
           .x = aim->position.x(),
           .y = aim->position.y(),
           .z = aim->position.z(),
           .radius = radius,
           .alpha = alpha,
           .color = aim_is_lock ? k_lock_gold : k_aim_cyan});
    }
  }

  if (resolved_lock_id != 0 && !aim_ring_active) {
    auto const lock = resolve_target(resolved_lock_id, resolved_lock_slot);
    if (lock.has_value()) {
      float const lock_alpha = 0.72F + 0.22F * pulse(anim_time, 3.0F);
      float const radius = std::max(0.62F, lock->body_radius * 1.36F);
      body_rings.add({.entity_id = resolved_lock_id,
                      .soldier_slot = resolved_lock_slot,
                      .priority = BodyRingPriority::Lock,
                      .role = RpgMarkerRole::Lock,
                      .x = lock->position.x(),
                      .y = lock->position.y(),
                      .z = lock->position.z(),
                      .radius = radius,
                      .alpha = lock_alpha,
                      .color = k_lock_gold});
    }
  }

  body_rings.submit(renderer);

  if (targets != nullptr && targets->recent_hit_target_id != 0 &&
      targets->recent_hit_timer > 0.0F) {
    auto const hit =
        resolve_target(targets->recent_hit_target_id, targets->recent_hit_soldier_slot);
    if (hit.has_value()) {
      float const life = std::clamp(targets->recent_hit_timer / 0.28F, 0.0F, 1.0F);
      float const radius = std::max(0.50F, hit->body_radius) + (1.0F - life) * 0.55F;
      submit_marker(renderer,
                    RpgMarkerRole::ContactHit,
                    hit->position.x(),
                    hit->position.y(),
                    hit->position.z(),
                    radius,
                    life * 0.72F,
                    k_contact_red);
    }
  }
}

} // namespace Render::GL
