#include "wildlife_persistence.h"

#include <QJsonArray>
#include <QString>

namespace Game::Wildlife {

namespace {

auto encode_groups(const std::vector<GroupState>& groups_in) -> QJsonArray {
  QJsonArray groups;
  for (const auto& group : groups_in) {
    QJsonObject group_obj;
    group_obj["id"] = static_cast<int>(group.id);
    group_obj["species"] = QString::fromUtf8(species_name(group.species).data());
    group_obj["home_x"] = static_cast<double>(group.home_x);
    group_obj["home_z"] = static_cast<double>(group.home_z);
    group_obj["roam_radius"] = static_cast<double>(group.roam_radius);
    group_obj["desired_size"] = group.desired_size;
    group_obj["respawn_timer"] = static_cast<double>(group.respawn_timer);
    group_obj["rng_state"] = static_cast<qint64>(group.rng_state);
    groups.append(group_obj);
  }
  return groups;
}

auto encode_flocks(const std::vector<Flock>& flocks_in) -> QJsonArray {
  QJsonArray flocks;
  for (const auto& flock : flocks_in) {
    QJsonObject flock_obj;
    flock_obj["home_x"] = static_cast<double>(flock.home_x);
    flock_obj["home_z"] = static_cast<double>(flock.home_z);
    flock_obj["roam_radius"] = static_cast<double>(flock.roam_radius);
    flock_obj["target_x"] = static_cast<double>(flock.target_x);
    flock_obj["target_z"] = static_cast<double>(flock.target_z);
    flock_obj["retarget_timer"] = static_cast<double>(flock.retarget_timer);
    flock_obj["alarm_timer"] = static_cast<double>(flock.alarm_timer);
    flock_obj["rng_state"] = static_cast<qint64>(flock.rng_state);
    flock_obj["heading_x"] = static_cast<double>(flock.heading_x);
    flock_obj["heading_z"] = static_cast<double>(flock.heading_z);
    flock_obj["respite_timer"] = static_cast<double>(flock.respite_timer);
    flock_obj["airborne_seconds"] = static_cast<double>(flock.airborne_seconds);
    flock_obj["airborne"] = flock.airborne;
    flocks.append(flock_obj);
  }
  return flocks;
}

auto encode_birds(const std::vector<Bird>& birds_in) -> QJsonArray {
  QJsonArray birds;
  for (const auto& bird : birds_in) {
    QJsonObject bird_obj;
    bird_obj["x"] = static_cast<double>(bird.x);
    bird_obj["y"] = static_cast<double>(bird.y);
    bird_obj["z"] = static_cast<double>(bird.z);
    bird_obj["yaw"] = static_cast<double>(bird.yaw);
    bird_obj["target_x"] = static_cast<double>(bird.target_x);
    bird_obj["target_z"] = static_cast<double>(bird.target_z);
    bird_obj["altitude"] = static_cast<double>(bird.altitude);
    bird_obj["target_altitude"] = static_cast<double>(bird.target_altitude);
    bird_obj["speed"] = static_cast<double>(bird.speed);
    bird_obj["phase"] = static_cast<double>(bird.phase);
    bird_obj["glide"] = static_cast<double>(bird.glide);
    bird_obj["bank"] = static_cast<double>(bird.bank);
    bird_obj["think_timer"] = static_cast<double>(bird.think_timer);
    bird_obj["state_timer"] = static_cast<double>(bird.state_timer);
    bird_obj["slot_lateral"] = static_cast<double>(bird.slot_lateral);
    bird_obj["slot_trail"] = static_cast<double>(bird.slot_trail);
    bird_obj["rng_state"] = static_cast<qint64>(bird.rng_state);
    bird_obj["flock"] = static_cast<int>(bird.flock);
    bird_obj["behavior"] = QString::fromUtf8(behavior_name(bird.behavior).data());
    bird_obj["tint"] = static_cast<int>(bird.tint);
    birds.append(bird_obj);
  }
  return birds;
}

auto decode_groups(const QJsonArray& groups_in) -> std::vector<GroupState> {
  std::vector<GroupState> groups;
  for (const auto value : groups_in) {
    const QJsonObject group_obj = value.toObject();
    GroupState group;
    group.id = static_cast<std::uint16_t>(group_obj.value("id").toInt(0));
    Species species = Species::Sheep;
    if (try_parse_species(group_obj.value("species").toString().toStdString(),
                          species)) {
      group.species = species;
    }
    group.home_x = static_cast<float>(group_obj.value("home_x").toDouble(0.0));
    group.home_z = static_cast<float>(group_obj.value("home_z").toDouble(0.0));
    group.roam_radius =
        static_cast<float>(group_obj.value("roam_radius").toDouble(14.0));
    group.desired_size = group_obj.value("desired_size").toInt(0);
    group.respawn_timer =
        static_cast<float>(group_obj.value("respawn_timer").toDouble(0.0));
    group.rng_state = static_cast<std::uint32_t>(
        group_obj.value("rng_state").toVariant().toULongLong());
    if (group.rng_state == 0U) {
      group.rng_state = 1U;
    }
    groups.push_back(group);
  }
  return groups;
}

auto decode_birds(const QJsonObject& state) -> BirdPopulation {
  BirdPopulation population;
  const QJsonArray flocks = state.value("flocks").toArray();
  for (const auto value : flocks) {
    const QJsonObject flock_obj = value.toObject();
    Flock flock;
    flock.home_x = static_cast<float>(flock_obj.value("home_x").toDouble(0.0));
    flock.home_z = static_cast<float>(flock_obj.value("home_z").toDouble(0.0));
    flock.roam_radius =
        static_cast<float>(flock_obj.value("roam_radius").toDouble(30.0));
    flock.target_x = static_cast<float>(flock_obj.value("target_x").toDouble(0.0));
    flock.target_z = static_cast<float>(flock_obj.value("target_z").toDouble(0.0));
    flock.retarget_timer =
        static_cast<float>(flock_obj.value("retarget_timer").toDouble(0.0));
    flock.alarm_timer =
        static_cast<float>(flock_obj.value("alarm_timer").toDouble(0.0));
    flock.rng_state = static_cast<std::uint32_t>(
        flock_obj.value("rng_state").toVariant().toULongLong());
    flock.heading_x = static_cast<float>(flock_obj.value("heading_x").toDouble(1.0));
    flock.heading_z = static_cast<float>(flock_obj.value("heading_z").toDouble(0.0));
    flock.respite_timer =
        static_cast<float>(flock_obj.value("respite_timer").toDouble(0.0));
    flock.airborne_seconds =
        static_cast<float>(flock_obj.value("airborne_seconds").toDouble(0.0));
    flock.airborne = flock_obj.value("airborne").toBool(true);
    population.flocks.push_back(flock);
  }

  const QJsonArray birds = state.value("birds").toArray();
  for (const auto value : birds) {
    const QJsonObject bird_obj = value.toObject();
    Bird bird;
    bird.x = static_cast<float>(bird_obj.value("x").toDouble(0.0));
    bird.y = static_cast<float>(bird_obj.value("y").toDouble(0.0));
    bird.z = static_cast<float>(bird_obj.value("z").toDouble(0.0));
    bird.yaw = static_cast<float>(bird_obj.value("yaw").toDouble(0.0));
    bird.target_x = static_cast<float>(bird_obj.value("target_x").toDouble(0.0));
    bird.target_z = static_cast<float>(bird_obj.value("target_z").toDouble(0.0));
    bird.altitude = static_cast<float>(bird_obj.value("altitude").toDouble(0.0));
    bird.target_altitude =
        static_cast<float>(bird_obj.value("target_altitude").toDouble(0.0));
    bird.speed = static_cast<float>(bird_obj.value("speed").toDouble(0.0));
    bird.phase = static_cast<float>(bird_obj.value("phase").toDouble(0.0));
    bird.glide = static_cast<float>(bird_obj.value("glide").toDouble(0.0));
    bird.bank = static_cast<float>(bird_obj.value("bank").toDouble(0.0));
    bird.think_timer = static_cast<float>(bird_obj.value("think_timer").toDouble(0.0));
    bird.state_timer = static_cast<float>(bird_obj.value("state_timer").toDouble(0.0));
    bird.slot_lateral =
        static_cast<float>(bird_obj.value("slot_lateral").toDouble(0.0));
    bird.slot_trail = static_cast<float>(bird_obj.value("slot_trail").toDouble(0.0));
    bird.rng_state = static_cast<std::uint32_t>(
        bird_obj.value("rng_state").toVariant().toULongLong());
    bird.flock = static_cast<std::uint16_t>(bird_obj.value("flock").toInt(0));
    Behavior behavior = Behavior::Cruise;
    if (try_parse_behavior(bird_obj.value("behavior").toString().toStdString(),
                           behavior)) {
      bird.behavior = behavior;
    }
    bird.tint = static_cast<std::uint8_t>(bird_obj.value("tint").toInt(0));
    population.birds.push_back(bird);
  }

  return population;
}

} // namespace

auto encode_wildlife_state(const WildlifeSaveData& data) -> QJsonObject {
  QJsonObject state;
  state["enabled"] = data.enabled;
  state["seed"] = static_cast<qint64>(data.seed);
  state["next_group_id"] = static_cast<int>(data.next_group_id);
  state["elapsed"] = data.elapsed;

  QJsonArray released_waves;
  for (bool const released : data.released_waves) {
    released_waves.append(released);
  }
  state["released_waves"] = released_waves;
  state["groups"] = encode_groups(data.groups);
  state["flocks"] = encode_flocks(data.birds.flocks);
  state["birds"] = encode_birds(data.birds.birds);
  return state;
}

auto decode_wildlife_state(const QJsonObject& state,
                           const WildlifeSaveData& fallback) -> WildlifeSaveData {
  WildlifeSaveData data;
  data.enabled = state.value("enabled").toBool(fallback.enabled);
  data.seed = static_cast<std::uint32_t>(state.value("seed").toVariant().toULongLong());
  data.next_group_id = static_cast<std::uint16_t>(
      state.value("next_group_id").toInt(fallback.next_group_id));
  data.elapsed = state.value("elapsed").toDouble(0.0);
  for (const auto value : state.value("released_waves").toArray()) {
    data.released_waves.push_back(value.toBool(false));
  }
  data.groups = decode_groups(state.value("groups").toArray());
  data.birds = decode_birds(state);
  return data;
}

} // namespace Game::Wildlife
