#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <qglobal.h>
#include <qjsonarray.h>
#include <qjsonobject.h>
#include <qjsonvalue.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "../command/json_u64.h"
#include "../core/component.h"
#include "../core/death_sequence.h"
#include "../core/entity.h"
#include "../session/checkpoint_values.h"
#include "../systems/nation_id.h"
#include "../units/spawn_type.h"
#include "../units/troop_type.h"
#include "entity_codec.h"
#include "serialization.h"

namespace Engine::Core::EntityCodec {

namespace {

void write_transform(const Entity* entity, QJsonObject& entity_obj) {
  if (const auto* transform = entity->get_component<TransformComponent>()) {
    QJsonObject transform_obj;
    transform_obj["pos_x"] = transform->position.x;
    transform_obj["pos_y"] = transform->position.y;
    transform_obj["pos_z"] = transform->position.z;
    transform_obj["rot_x"] = transform->rotation.x;
    transform_obj["rot_y"] = transform->rotation.y;
    transform_obj["rot_z"] = transform->rotation.z;
    transform_obj["scale_x"] = transform->scale.x;
    transform_obj["scale_y"] = transform->scale.y;
    transform_obj["scale_z"] = transform->scale.z;
    transform_obj["has_desired_yaw"] = transform->has_desired_yaw;
    transform_obj["desired_yaw"] = transform->desired_yaw;
    entity_obj["transform"] = transform_obj;
  }
}

void read_transform(Entity* entity, const QJsonObject& json) {
  if (json.contains("transform")) {
    const auto transform_obj = json["transform"].toObject();
    auto* transform = entity->add_component<TransformComponent>();
    transform->position.x = static_cast<float>(transform_obj["pos_x"].toDouble());
    transform->position.y = static_cast<float>(transform_obj["pos_y"].toDouble());
    transform->position.z = static_cast<float>(transform_obj["pos_z"].toDouble());
    transform->rotation.x = static_cast<float>(transform_obj["rot_x"].toDouble());
    transform->rotation.y = static_cast<float>(transform_obj["rot_y"].toDouble());
    transform->rotation.z = static_cast<float>(transform_obj["rot_z"].toDouble());
    transform->scale.x = static_cast<float>(transform_obj["scale_x"].toDouble());
    transform->scale.y = static_cast<float>(transform_obj["scale_y"].toDouble());
    transform->scale.z = static_cast<float>(transform_obj["scale_z"].toDouble());
    transform->has_desired_yaw = transform_obj["has_desired_yaw"].toBool(false);
    transform->desired_yaw =
        static_cast<float>(transform_obj["desired_yaw"].toDouble());
  }
}

void write_renderable(const Entity* entity, QJsonObject& entity_obj) {
  if (const auto* renderable = entity->get_component<RenderableComponent>()) {
    QJsonObject renderable_obj;
    if (!renderable->renderer_id.empty()) {
      renderable_obj["renderer_id"] = QString::fromStdString(renderable->renderer_id);
    }
    renderable_obj["visible"] = renderable->visible;
    if (!renderable->structure_foundation_visible) {
      renderable_obj["structure_foundation_visible"] = false;
    }
    entity_obj["renderable"] = renderable_obj;
  }
}

void read_renderable(Entity* entity, const QJsonObject& json) {
  if (json.contains("renderable")) {
    const auto renderable_obj = json["renderable"].toObject();
    auto* renderable = entity->add_component<RenderableComponent>();

    renderable->renderer_id = renderable_obj["renderer_id"].toString().toStdString();
    renderable->visible = renderable_obj["visible"].toBool(true);
    renderable->structure_foundation_visible =
        renderable_obj["structure_foundation_visible"].toBool(true);
  }
}

void write_unit(const Entity* entity, QJsonObject& entity_obj) {
  if (const auto* unit = entity->get_component<UnitComponent>()) {
    QJsonObject unit_obj;
    unit_obj["health"] = unit->health;
    unit_obj["max_health"] = unit->max_health;
    unit_obj["speed"] = unit->speed;
    unit_obj["vision_range"] = unit->vision_range;
    unit_obj["unit_type"] =
        QString::fromStdString(Game::Units::spawn_typeToString(unit->spawn_type));
    unit_obj["owner_id"] = unit->owner_id;
    unit_obj["nation_id"] = Game::Systems::nation_id_to_qstring(unit->nation_id);
    unit_obj["uses_nation_formation_profile"] = unit->uses_nation_formation_profile;
    unit_obj["render_individuals_per_unit_override"] =
        unit->render_individuals_per_unit_override;
    unit_obj["squad_strength"] = unit->squad_strength;
    unit_obj["render_rider"] = unit->render_rider;
    unit_obj["death_sequence_override"] =
        static_cast<int>(unit->death_sequence_override);
    entity_obj["unit"] = unit_obj;
  }
}

void read_unit(Entity* entity, const QJsonObject& json) {
  if (json.contains("unit")) {
    const auto unit_obj = json["unit"].toObject();
    auto* unit = entity->add_component<UnitComponent>();
    unit->health = unit_obj["health"].toInt(Defaults::k_unit_default_health);
    unit->max_health = unit_obj["max_health"].toInt(Defaults::k_unit_default_health);
    unit->speed = static_cast<float>(unit_obj["speed"].toDouble());
    unit->vision_range = static_cast<float>(unit_obj["vision_range"].toDouble(
        static_cast<double>(Defaults::k_unit_default_vision_range)));

    QString const unit_type_str = unit_obj["unit_type"].toString();
    Game::Units::SpawnType spawn_type;
    if (Game::Units::try_parse_spawn_type(unit_type_str, spawn_type)) {
      unit->spawn_type = spawn_type;
    } else {
      qWarning() << "Unknown spawn type in save file:" << unit_type_str
                 << "- defaulting to Archer";
      unit->spawn_type = Game::Units::SpawnType::Archer;
    }

    unit->owner_id = unit_obj["owner_id"].toInt(0);
    if (unit_obj.contains("nation_id")) {
      const QString nation_str = unit_obj["nation_id"].toString();
      Game::Systems::NationID nation_id;
      if (Game::Systems::try_parse_nation_id(nation_str, nation_id)) {
        unit->nation_id = nation_id;
      } else {
        qWarning() << "Unknown nation ID in save file:" << nation_str
                   << "- using default";
        unit->nation_id = Game::Systems::NationID::RomanRepublic;
      }
    }
    unit->uses_nation_formation_profile =
        unit_obj.contains("uses_nation_formation_profile")
            ? unit_obj["uses_nation_formation_profile"].toBool(false)
            : Game::Units::spawn_typeToTroopType(unit->spawn_type).has_value();
    unit->render_individuals_per_unit_override =
        unit_obj["render_individuals_per_unit_override"].toInt(0);
    unit->squad_strength = unit_obj["squad_strength"].toInt(0);
    unit->render_rider = unit_obj["render_rider"].toBool(true);
    unit->death_sequence_override =
        static_cast<std::uint8_t>(unit_obj["death_sequence_override"].toInt(0xFF));
  }
}

void write_player_order_intent(const Entity* entity, QJsonObject& entity_obj) {
  if (const auto* intent = entity->get_component<PlayerOrderIntentComponent>()) {
    QJsonObject intent_obj;
    intent_obj["kind"] = static_cast<int>(intent->kind);
    intent_obj["suppress_opportunistic_combat"] = intent->suppress_opportunistic_combat;
    entity_obj["player_order_intent"] = intent_obj;
  }
}

void read_player_order_intent(Entity* entity, const QJsonObject& json) {
  if (json.contains("player_order_intent")) {
    const auto intent_obj = json["player_order_intent"].toObject();
    auto* intent = entity->add_component<PlayerOrderIntentComponent>();
    intent->kind = static_cast<PlayerOrderIntentKind>(
        intent_obj["kind"].toInt(static_cast<int>(PlayerOrderIntentKind::None)));
    intent->suppress_opportunistic_combat =
        intent_obj["suppress_opportunistic_combat"].toBool(false);
  }
}

void write_patrol(const Entity* entity, QJsonObject& entity_obj) {
  if (const auto* patrol = entity->get_component<PatrolComponent>()) {
    QJsonObject patrol_obj;
    patrol_obj["current_waypoint"] = static_cast<int>(patrol->current_waypoint);
    patrol_obj["patrolling"] = patrol->patrolling;

    QJsonArray waypoints_array;
    for (const auto& waypoint : patrol->waypoints) {
      QJsonObject waypoint_obj;
      waypoint_obj["x"] = waypoint.first;
      waypoint_obj["y"] = waypoint.second;
      waypoints_array.append(waypoint_obj);
    }
    patrol_obj["waypoints"] = waypoints_array;
    entity_obj["patrol"] = patrol_obj;
  }
}

void read_patrol(Entity* entity, const QJsonObject& json) {
  if (json.contains("patrol")) {
    const auto patrol_obj = json["patrol"].toObject();
    auto* patrol = entity->add_component<PatrolComponent>();
    patrol->current_waypoint =
        static_cast<size_t>(std::max(0, patrol_obj["current_waypoint"].toInt()));
    patrol->patrolling = patrol_obj["patrolling"].toBool(false);

    patrol->waypoints.clear();
    const auto waypoints_array = patrol_obj["waypoints"].toArray();
    patrol->waypoints.reserve(waypoints_array.size());
    for (const auto value : waypoints_array) {
      const auto waypoint_obj = value.toObject();
      patrol->waypoints.emplace_back(static_cast<float>(waypoint_obj["x"].toDouble()),
                                     static_cast<float>(waypoint_obj["y"].toDouble()));
    }
  }
}

void write_ai_controlled(const Entity* entity, QJsonObject& entity_obj) {
  if (const auto* ai = entity->get_component<AIControlledComponent>()) {
    entity_obj["aiControlled"] = QJsonObject{{"leash_x", ai->leash_x},
                                             {"leash_z", ai->leash_z},
                                             {"leash_half_x", ai->leash_half_x},
                                             {"leash_half_z", ai->leash_half_z},
                                             {"sallying", ai->sallying}};
  }
}

void read_ai_controlled(Entity* entity, const QJsonObject& json) {
  const auto value = json.value("aiControlled");
  if (value.isObject() || value.toBool()) {
    auto* ai = entity->add_component<AIControlledComponent>();
    const auto saved = value.toObject();
    ai->leash_x = static_cast<float>(saved.value("leash_x").toDouble());
    ai->leash_z = static_cast<float>(saved.value("leash_z").toDouble());
    ai->leash_half_x = static_cast<float>(saved.value("leash_half_x").toDouble());
    ai->leash_half_z = static_cast<float>(saved.value("leash_half_z").toDouble());
    ai->sallying = saved.value("sallying").toBool();
  }
}

void write_formation_mode(const Entity* entity, QJsonObject& entity_obj) {
  if (const auto* formation = entity->get_component<FormationModeComponent>()) {
    QJsonObject formation_obj;
    formation_obj["active"] = formation->active;
    formation_obj["formation_center_x"] =
        static_cast<double>(formation->formation_center_x);
    formation_obj["formation_center_z"] =
        static_cast<double>(formation->formation_center_z);
    formation_obj["formation_id"] = static_cast<qint64>(formation->formation_id);
    formation_obj["stable_slot_id"] = formation->stable_slot_id;
    formation_obj["stable_rank"] = formation->stable_rank;
    formation_obj["stable_file"] = formation->stable_file;
    formation_obj["stable_slot_x"] = formation->stable_slot_x;
    formation_obj["stable_slot_z"] = formation->stable_slot_z;
    entity_obj["formation_mode"] = formation_obj;
  }
}

void read_formation_mode(Entity* entity, const QJsonObject& json) {
  if (json.contains("formation_mode")) {
    const auto formation_obj = json["formation_mode"].toObject();
    auto* formation = entity->add_component<FormationModeComponent>();
    formation->active = formation_obj["active"].toBool(false);
    formation->formation_center_x =
        static_cast<float>(formation_obj["formation_center_x"].toDouble(0.0));
    formation->formation_center_z =
        static_cast<float>(formation_obj["formation_center_z"].toDouble(0.0));
    formation->formation_id = static_cast<std::uint64_t>(
        formation_obj["formation_id"].toVariant().toULongLong());
    formation->stable_slot_id = formation_obj["stable_slot_id"].toInt(-1);
    formation->stable_rank = formation_obj["stable_rank"].toInt(-1);
    formation->stable_file = formation_obj["stable_file"].toInt(-1);
    formation->stable_slot_x =
        static_cast<float>(formation_obj["stable_slot_x"].toDouble(0.0));
    formation->stable_slot_z =
        static_cast<float>(formation_obj["stable_slot_z"].toDouble(0.0));
  }
}

void write_army_formation_membership(const Entity* entity, QJsonObject& entity_obj) {
  if (const auto* membership =
          entity->get_component<ArmyFormationMembershipComponent>()) {
    QJsonObject membership_obj;
    membership_obj["group_id"] = static_cast<qint64>(membership->group_id);
    membership_obj["slot_id"] = membership->slot_id;
    entity_obj["army_formation_membership"] = membership_obj;
  }
}

void read_army_formation_membership(Entity* entity, const QJsonObject& json) {
  if (json.contains("army_formation_membership")) {
    const auto membership_obj = json["army_formation_membership"].toObject();
    auto* membership = entity->add_component<ArmyFormationMembershipComponent>();
    membership->group_id = static_cast<std::uint64_t>(
        membership_obj["group_id"].toVariant().toULongLong());
    membership->slot_id = membership_obj["slot_id"].toInt(-1);
  }
}

void write_unit_layout_state(const Entity* entity, QJsonObject& entity_obj) {
  if (const auto* layout = entity->get_component<UnitLayoutStateComponent>()) {
    QJsonObject layout_obj;
    layout_obj["state"] = static_cast<int>(layout->state);
    layout_obj["phase"] = static_cast<int>(layout->phase);
    layout_obj["transition_progress"] =
        static_cast<double>(layout->transition_progress);
    layout_obj["transition_seconds"] = static_cast<double>(layout->transition_seconds);
    layout_obj["layout_id"] = static_cast<int>(layout->layout_id);
    layout_obj["requested_layout_id"] = static_cast<int>(layout->requested_layout_id);
    layout_obj["previous_layout_id"] = static_cast<int>(layout->previous_layout_id);
    entity_obj["unit_layout_state"] = layout_obj;
  }
}

void read_unit_layout_state(Entity* entity, const QJsonObject& json) {
  if (json.contains("unit_layout_state")) {
    const auto layout_obj = json["unit_layout_state"].toObject();
    auto* layout = entity->add_component<UnitLayoutStateComponent>();
    layout->state = static_cast<std::uint8_t>(layout_obj["state"].toInt(0));
    layout->phase = static_cast<std::uint8_t>(layout_obj["phase"].toInt(1));
    layout->transition_progress =
        static_cast<float>(layout_obj["transition_progress"].toDouble(1.0));
    layout->transition_seconds =
        static_cast<float>(layout_obj["transition_seconds"].toDouble(0.0));
    layout->layout_id =
        static_cast<std::uint16_t>(layout_obj["layout_id"].toInt(0xFFFF));
    layout->requested_layout_id =
        static_cast<std::uint16_t>(layout_obj["requested_layout_id"].toInt(0xFFFF));
    layout->previous_layout_id =
        static_cast<std::uint16_t>(layout_obj["previous_layout_id"].toInt(0xFFFF));
  }
}

void write_stamina(const Entity* entity, QJsonObject& entity_obj) {
  if (const auto* stamina = entity->get_component<StaminaComponent>()) {
    QJsonObject stamina_obj;
    stamina_obj["stamina"] = static_cast<double>(stamina->stamina);
    stamina_obj["max_stamina"] = static_cast<double>(stamina->max_stamina);
    stamina_obj["regen_rate"] = static_cast<double>(stamina->regen_rate);
    stamina_obj["depletion_rate"] = static_cast<double>(stamina->depletion_rate);
    stamina_obj["is_running"] = stamina->is_running;
    stamina_obj["run_requested"] = stamina->run_requested;
    entity_obj["stamina"] = stamina_obj;
  }
}

void read_stamina(Entity* entity, const QJsonObject& json) {
  if (json.contains("stamina")) {
    const auto stamina_obj = json["stamina"].toObject();
    auto* stamina = entity->add_component<StaminaComponent>();
    stamina->stamina = static_cast<float>(stamina_obj["stamina"].toDouble(
        static_cast<double>(StaminaComponent::k_default_max_stamina)));
    stamina->max_stamina = static_cast<float>(stamina_obj["max_stamina"].toDouble(
        static_cast<double>(StaminaComponent::k_default_max_stamina)));
    stamina->regen_rate = static_cast<float>(stamina_obj["regen_rate"].toDouble(
        static_cast<double>(StaminaComponent::k_default_regen_rate)));
    stamina->depletion_rate = static_cast<float>(stamina_obj["depletion_rate"].toDouble(
        static_cast<double>(StaminaComponent::k_default_depletion_rate)));
    stamina->is_running = stamina_obj["is_running"].toBool(false);
    stamina->run_requested = stamina_obj["run_requested"].toBool(false);
  }
}

void write_wading(const Entity* entity, QJsonObject& entity_obj) {
  auto const* registry = entity->registry();
  auto const* wading = registry != nullptr
                           ? registry->try_get<WadingComponent>(entity->get_id())
                           : nullptr;
  if (wading == nullptr || wading->chill <= 0.0F) {
    return;
  }
  QJsonObject wading_obj;
  wading_obj["chill"] = static_cast<double>(wading->chill);
  wading_obj["cold"] = static_cast<double>(wading->cold);
  entity_obj["wading"] = wading_obj;
}

void read_wading(Entity* entity, const QJsonObject& json) {
  if (!json.contains("wading")) {
    return;
  }
  const auto wading_obj = json["wading"].toObject();
  auto* wading = entity->add_component<WadingComponent>();
  if (wading == nullptr) {
    return;
  }
  wading->chill =
      std::clamp(static_cast<float>(wading_obj["chill"].toDouble(0.0)), 0.0F, 1.0F);
  wading->cold =
      std::clamp(static_cast<float>(wading_obj["cold"].toDouble(0.0)), 0.0F, 1.0F);
}

void write_terrain_context(const Entity* entity, QJsonObject& entity_obj) {
  if (const auto* terrain_context = entity->get_component<TerrainContextComponent>()) {
    QJsonObject terrain_context_obj;
    terrain_context_obj["is_on_bridge"] = terrain_context->is_on_bridge;
    terrain_context_obj["is_at_hill_entrance"] = terrain_context->is_at_hill_entrance;
    terrain_context_obj["audio_cooldown"] =
        static_cast<double>(terrain_context->audio_cooldown);
    entity_obj["terrain_context"] = terrain_context_obj;
  }
}

void read_terrain_context(Entity* entity, const QJsonObject& json) {
  if (json.contains("terrain_context")) {
    const auto terrain_context_obj = json["terrain_context"].toObject();
    auto* terrain_context = entity->add_component<TerrainContextComponent>();
    terrain_context->is_on_bridge = terrain_context_obj["is_on_bridge"].toBool(false);
    terrain_context->is_at_hill_entrance =
        terrain_context_obj["is_at_hill_entrance"].toBool(false);
    terrain_context->audio_cooldown =
        static_cast<float>(terrain_context_obj["audio_cooldown"].toDouble(0.0));
  }
}

template <typename Facts>
auto movement_progress_fields(Facts& facts) {
  auto& p = facts.progress;
  auto& stall = p.stall;
  auto& t = facts.traversal;
  return std::tie(facts.motor.blocked,
                  facts.motor.has_contact,
                  facts.motor.contact_nx,
                  facts.motor.contact_nz,
                  facts.last_accepted_speed,
                  p.state,
                  p.previous_state,
                  p.remaining_arclength,
                  p.route_advance,
                  p.lateral_route_error,
                  p.no_progress_seconds,
                  p.no_progress_advance,
                  p.order_seconds,
                  p.tracked_order,
                  p.state_seconds,
                  p.blocked_steps,
                  p.repath_count,
                  p.repath_attempts,
                  p.repath_reason,
                  p.arrived_short,
                  p.short_route_replans,
                  p.holding_at_obstruction,
                  p.holding_recheck_seconds,
                  stall.objective_valid,
                  stall.objective_x,
                  stall.objective_z,
                  stall.stalled_seconds,
                  stall.window_seconds,
                  stall.window_reference_x,
                  stall.window_reference_z,
                  stall.window_valid,
                  stall.no_closer_seconds,
                  stall.queued_seconds,
                  stall.rung,
                  stall.recovery_attempts,
                  stall.tracked_order,
                  stall.objective_abandoned,
                  stall.abandoned_x,
                  stall.abandoned_z,
                  stall.abandon_count,
                  t.mode,
                  t.target_mode,
                  t.portal_id,
                  t.normal_files,
                  t.current_files,
                  t.target_files,
                  t.corridor_half_width,
                  t.desired_half_width,
                  t.soldier_body_radius,
                  t.file_spacing,
                  t.lateral_scale,
                  t.transition_progress,
                  t.mode_dwell_seconds,
                  t.about_faced);
}

void write_movement_progress(const Entity* entity, QJsonObject& json) {
  if (const auto* facts = entity->get_component<MovementFactsComponent>()) {
    json["movement_progress"] =
        Game::Session::CheckpointValues::write_tuple(movement_progress_fields(*facts));
  }
}

void read_movement_progress(Entity* entity, const QJsonObject& json) {
  MovementFactsComponent facts;
  if (Game::Session::CheckpointValues::read_tuple(json.value("movement_progress"),
                                                  movement_progress_fields(facts))) {
    entity->add_component<MovementFactsComponent>(std::move(facts));
  }
}

} // namespace

void write_core(const Entity* entity, QJsonObject& entity_obj) {
  write_transform(entity, entity_obj);
  write_renderable(entity, entity_obj);
  write_unit(entity, entity_obj);
  write_player_order_intent(entity, entity_obj);
  write_patrol(entity, entity_obj);
  write_ai_controlled(entity, entity_obj);
  write_formation_mode(entity, entity_obj);
  write_army_formation_membership(entity, entity_obj);
  write_unit_layout_state(entity, entity_obj);
  write_stamina(entity, entity_obj);
  write_wading(entity, entity_obj);
  write_terrain_context(entity, entity_obj);
  write_movement_progress(entity, entity_obj);
}

void read_core(Entity* entity, const QJsonObject& json) {
  read_transform(entity, json);
  read_renderable(entity, json);
  read_unit(entity, json);
  read_player_order_intent(entity, json);
  read_patrol(entity, json);
  read_ai_controlled(entity, json);
  read_formation_mode(entity, json);
  read_army_formation_membership(entity, json);
  read_unit_layout_state(entity, json);
  read_stamina(entity, json);
  read_wading(entity, json);
  read_terrain_context(entity, json);
  read_movement_progress(entity, json);
}

} // namespace Engine::Core::EntityCodec

namespace Engine::Core {

void Serialization::write_movement(const Entity* entity, QJsonObject& entity_obj) {
  if (const auto* movement = entity->get_component<MovementComponent>()) {
    QJsonObject movement_obj;
    movement_obj["has_target"] = movement->has_target;
    movement_obj["target_x"] = movement->target_x;
    movement_obj["target_y"] = movement->target_y;
    movement_obj["goal_x"] = movement->goal_x;
    movement_obj["goal_y"] = movement->goal_y;
    movement_obj["vx"] = movement->vx;
    movement_obj["vz"] = movement->vz;
    movement_obj["path_index"] = static_cast<int>(movement->path_index);
    movement_obj["precise_arrival"] = movement->precise_arrival;
    movement_obj["navigation_clearance"] = movement->navigation_clearance;
    movement_obj["travelled"] = movement->travelled;
    movement_obj["escape_active"] = movement->escape_active;
    movement_obj["escape_x"] = movement->escape_x;
    movement_obj["escape_z"] = movement->escape_z;
    movement_obj["has_requested_goal"] = movement->has_requested_goal;
    movement_obj["requested_goal_x"] = movement->requested_goal_x;
    movement_obj["requested_goal_z"] = movement->requested_goal_z;
    movement_obj["issuer_retargets"] = movement->issuer_retargets;
    movement_obj["structure_approach_target_id"] =
        Game::Command::u64_to_json(movement->structure_approach_target_id);
    movement_obj["can_enter_forest"] = movement->can_enter_forest;
    movement_obj["order_sequence"] =
        Game::Command::u64_to_json(movement->order_sequence);
    movement_obj["route_revision"] =
        Game::Command::u64_to_json(movement->route_revision);
    movement_obj["topology_revision"] =
        Game::Command::u64_to_json(movement->topology_revision);
    movement_obj["route_id"] = Game::Command::u64_to_json(movement->route_id);
    movement_obj["route_lane_offset"] = movement->route_lane_offset;
    movement_obj["route_lane_min_scale"] = movement->route_lane_min_scale;
    movement_obj["route_opening_waypoint_index"] =
        Game::Command::u64_to_json(movement->route_opening_waypoint_index);
    movement_obj["route_reform_waypoint_index"] =
        Game::Command::u64_to_json(movement->route_reform_waypoint_index);
    movement_obj["declared_group_pace"] = movement->declared_group_pace;
    movement_obj["following_formation_slot"] = movement->following_formation_slot;

    QJsonArray path_array;
    for (const auto& waypoint : movement->path) {
      QJsonObject waypoint_obj;
      waypoint_obj["x"] = waypoint.first;
      waypoint_obj["y"] = waypoint.second;
      path_array.append(waypoint_obj);
    }
    movement_obj["path"] = path_array;
    entity_obj["movement"] = movement_obj;
  }
}

void Serialization::read_movement(Entity* entity, const QJsonObject& json) {
  if (json.contains("movement")) {
    const auto movement_obj = json["movement"].toObject();
    auto* movement = entity->add_component<MovementComponent>();
    movement->has_target = movement_obj["has_target"].toBool(false);
    movement->target_x = static_cast<float>(movement_obj["target_x"].toDouble());
    movement->target_y = static_cast<float>(movement_obj["target_y"].toDouble());
    movement->goal_x = static_cast<float>(movement_obj["goal_x"].toDouble());
    movement->goal_y = static_cast<float>(movement_obj["goal_y"].toDouble());
    movement->vx = static_cast<float>(movement_obj["vx"].toDouble());
    movement->vz = static_cast<float>(movement_obj["vz"].toDouble());
    movement->precise_arrival = movement_obj["precise_arrival"].toBool(false);
    movement->navigation_clearance = std::max(
        0.0F, static_cast<float>(movement_obj["navigation_clearance"].toDouble(0.5)));
    const auto id_field = [&movement_obj](const char* key) {
      return Game::Command::u64_from_json(movement_obj.value(QLatin1String(key)))
          .value_or(0);
    };
    movement->travelled = static_cast<float>(movement_obj["travelled"].toDouble());
    movement->escape_active = movement_obj["escape_active"].toBool();
    movement->escape_x = static_cast<float>(movement_obj["escape_x"].toDouble());
    movement->escape_z = static_cast<float>(movement_obj["escape_z"].toDouble());
    movement->has_requested_goal = movement_obj["has_requested_goal"].toBool();
    movement->requested_goal_x =
        static_cast<float>(movement_obj["requested_goal_x"].toDouble());
    movement->requested_goal_z =
        static_cast<float>(movement_obj["requested_goal_z"].toDouble());
    movement->issuer_retargets = movement_obj["issuer_retargets"].toBool();
    movement->structure_approach_target_id = id_field("structure_approach_target_id");
    movement->can_enter_forest = movement_obj["can_enter_forest"].toBool(true);
    movement->order_sequence = id_field("order_sequence");
    movement->route_revision = id_field("route_revision");
    movement->topology_revision = id_field("topology_revision");
    movement->route_id = id_field("route_id");
    movement->route_lane_offset =
        static_cast<float>(movement_obj["route_lane_offset"].toDouble());
    movement->route_lane_min_scale =
        static_cast<float>(movement_obj["route_lane_min_scale"].toDouble(1.0));
    movement->route_opening_waypoint_index = id_field("route_opening_waypoint_index");
    movement->route_reform_waypoint_index = id_field("route_reform_waypoint_index");
    movement->declared_group_pace =
        static_cast<float>(movement_obj["declared_group_pace"].toDouble());
    movement->following_formation_slot =
        movement_obj["following_formation_slot"].toBool();
    movement->clear_path();
    const auto path_array = movement_obj["path"].toArray();
    movement->path.reserve(path_array.size());
    for (const auto value : path_array) {
      const auto waypoint_obj = value.toObject();
      movement->path.emplace_back(static_cast<float>(waypoint_obj["x"].toDouble()),
                                  static_cast<float>(waypoint_obj["y"].toDouble()));
    }

    if (movement_obj.contains("path_index")) {
      movement->path_index =
          static_cast<std::size_t>(movement_obj["path_index"].toInt());
    }

    movement->validate_path_index();
    if (!movement->has_target) {
      movement->clear_path();
    }
  }
}

} // namespace Engine::Core
