#include "undead_zone_persistence.h"

#include <QJsonArray>
#include <QString>

namespace Game::Systems {

auto serialize_undead_zone(const UndeadRuntimeZone& zone) -> QJsonObject {
  QJsonObject obj;
  obj[QStringLiteral("id")] = zone.definition.id;
  obj[QStringLiteral("awakened")] = zone.awakened;
  obj[QStringLiteral("garrison_broken")] = zone.garrison_broken;
  obj[QStringLiteral("anchor_entity_id")] = static_cast<qint64>(zone.anchor_entity_id);
  obj[QStringLiteral("next_wave_index")] = zone.next_wave_index;
  obj[QStringLiteral("completed_waves")] = zone.completed_waves;
  obj[QStringLiteral("respawn_delay_remaining")] = zone.respawn_delay_remaining;
  obj[QStringLiteral("current_wave_elapsed")] = zone.current_wave_elapsed;
  QJsonArray active_ids;
  for (Engine::Core::EntityID const id : zone.active_spawn_ids) {
    active_ids.append(static_cast<qint64>(id));
  }
  obj[QStringLiteral("active_spawn_ids")] = active_ids;
  return obj;
}

void restore_undead_zone(UndeadRuntimeZone& zone, const QJsonObject& saved) {
  zone.awakened = saved.value(QStringLiteral("awakened")).toBool(zone.awakened);
  zone.announced_awakening = zone.awakened;
  zone.garrison_broken =
      saved.value(QStringLiteral("garrison_broken")).toBool(zone.garrison_broken);
  zone.announced_defeat = zone.garrison_broken;
  zone.anchor_entity_id = static_cast<Engine::Core::EntityID>(
      saved.value(QStringLiteral("anchor_entity_id")).toVariant().toULongLong());

  zone.anchor_pending = false;
  zone.next_wave_index =
      saved.value(QStringLiteral("next_wave_index")).toInt(zone.next_wave_index);
  zone.completed_waves =
      saved.value(QStringLiteral("completed_waves")).toInt(zone.completed_waves);
  zone.respawn_delay_remaining =
      static_cast<float>(saved.value(QStringLiteral("respawn_delay_remaining"))
                             .toDouble(zone.respawn_delay_remaining));
  zone.current_wave_elapsed =
      static_cast<float>(saved.value(QStringLiteral("current_wave_elapsed"))
                             .toDouble(zone.current_wave_elapsed));
  zone.active_spawn_ids.clear();
  const auto ids = saved.value(QStringLiteral("active_spawn_ids")).toArray();
  zone.active_spawn_ids.reserve(ids.size());
  for (const auto id_value : ids) {
    zone.active_spawn_ids.push_back(
        static_cast<Engine::Core::EntityID>(id_value.toVariant().toULongLong()));
  }
}

} // namespace Game::Systems
