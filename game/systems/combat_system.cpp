#include "combat_system.h"

#include <QJsonObject>

#include <utility>

#include "../core/world.h"
#include "../session/checkpoint_values.h"
#include "combat_system/attack_processor.h"
#include "combat_system/combat_state_processor.h"
#include "combat_system/combat_utils.h"
#include "combat_system/elephant_special_processor.h"
#include "combat_system/formation_contact_processor.h"
#include "combat_system/hit_feedback_processor.h"
#include "combat_system/siege_special_processor.h"
#include "combat_system/threat_alert.h"

namespace Game::Systems {

auto CombatSystem::capture_checkpoint() const -> QJsonObject {
  QJsonObject cooldowns;
  for (const auto& [id, cooldown] : m_auto_engagement.m_scan_cooldowns) {
    cooldowns[QString::number(id)] = cooldown;
  }
  return cooldowns;
}

auto CombatSystem::restore_checkpoint(const QJsonObject& state) -> bool {
  decltype(m_auto_engagement.m_scan_cooldowns) cooldowns;
  for (auto it = state.begin(); it != state.end(); ++it) {
    Engine::Core::EntityID id = 0;
    float cooldown = 0.0F;
    if (!Game::Session::CheckpointValues::decode(QJsonValue(it.key()), id) ||
        !Game::Session::CheckpointValues::decode(it.value(), cooldown) || id == 0 ||
        cooldown <= 0.0F || !cooldowns.emplace(id, cooldown).second) {
      return false;
    }
  }
  m_auto_engagement.m_scan_cooldowns.swap(cooldowns);
  return true;
}

void CombatSystem::update(Engine::Core::World* world, float delta_time) {
  Combat::rebuild_combat_query_context(world, m_query_context);

  m_target_commitment.update(world, delta_time);
  Combat::process_hit_feedback(world, delta_time);
  Combat::process_combat_state(world, delta_time);
  Combat::process_attacks(world, m_query_context, delta_time);
  Combat::update_formation_contacts(world, delta_time);
  Combat::process_siege_specials(world, m_query_context, delta_time);
  Combat::process_elephant_specials(world, m_query_context, delta_time);
  m_auto_engagement.process(world, m_query_context, delta_time);
  Combat::tick_threat_alerts(world, delta_time);
  m_target_commitment.update(world, 0.0F);
}

} // namespace Game::Systems
