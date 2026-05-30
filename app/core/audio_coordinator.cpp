#include "audio_coordinator.h"

#include <QMap>
#include <QSet>
#include <QStringList>

#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "audio_resource_loader.h"
#include "game/audio/audio_event_handler.h"
#include "game/audio/audio_system.h"
#include "game/map/map_loader.h"
#include "game/map/mission_definition.h"
#include "game/systems/nation_registry.h"
#include "utils/resource_utils.h"

namespace {

auto nation_audio_tag(Game::Systems::NationID nation_id) -> QString {
  switch (nation_id) {
  case Game::Systems::NationID::RomanRepublic:
    return QStringLiteral("roman");
  case Game::Systems::NationID::Carthage:
    return QStringLiteral("carthage");
  case Game::Systems::NationID::IronSepulcher:
    return QStringLiteral("iron_sepulcher");
  }
  return QStringLiteral("roman");
}

auto ambient_state_tag(Engine::Core::AmbientState state) -> QString {
  switch (state) {
  case Engine::Core::AmbientState::PEACEFUL:
    return QStringLiteral("peaceful");
  case Engine::Core::AmbientState::TENSE:
    return QStringLiteral("tense");
  case Engine::Core::AmbientState::COMBAT:
    return QStringLiteral("combat");
  case Engine::Core::AmbientState::VICTORY:
    return QStringLiteral("victory");
  case Engine::Core::AmbientState::DEFEAT:
    return QStringLiteral("defeat");
  }
  return QStringLiteral("peaceful");
}

auto frontend_music_rotation() -> std::unordered_map<std::string, size_t>& {
  static std::unordered_map<std::string, size_t> rotation;
  return rotation;
}

void append_unique_track_ids(QStringList& target, const QStringList& source) {
  QSet<QString> seen;
  for (const QString& track_id : target) {
    seen.insert(track_id);
  }
  for (const QString& track_id : source) {
    if (!seen.contains(track_id)) {
      target.push_back(track_id);
      seen.insert(track_id);
    }
  }
}

auto collect_matching_resource_ids(AudioCategory category,
                                   const std::vector<QMap<QString, QString>>& queries,
                                   bool stop_after_first_non_empty = false)
    -> QStringList {
  QStringList track_ids;
  for (const auto& tags : queries) {
    const QStringList matches = AudioResourceLoader::find_resource_ids(category, tags);
    if (matches.isEmpty()) {
      continue;
    }
    append_unique_track_ids(track_ids, matches);
    if (stop_after_first_non_empty) {
      break;
    }
  }
  return track_ids;
}

auto choose_round_robin_track(const QStringList& track_ids,
                              const QString& key) -> QString {
  if (track_ids.isEmpty()) {
    return {};
  }
  auto& rotation = frontend_music_rotation();
  const std::string rotation_key = key.toStdString();
  size_t& next_index = rotation[rotation_key];
  const QString& track_id = track_ids.at(int(next_index % size_t(track_ids.size())));
  next_index = (next_index + 1U) % size_t(track_ids.size());
  return track_id;
}

auto choose_seeded_track(const QStringList& track_ids, const QString& seed) -> QString {
  if (track_ids.isEmpty()) {
    return {};
  }
  const uint index = qHash(seed) % uint(track_ids.size());
  return track_ids.at(int(index));
}

auto mission_terrain_to_ambience_biome(const QString& terrain_type) -> QString {
  const QString lowered = terrain_type.trimmed().toLower();
  if (lowered == QStringLiteral("mountain")) {
    return QStringLiteral("mountain");
  }
  if (lowered == QStringLiteral("plains")) {
    return QStringLiteral("plains");
  }
  if (lowered == QStringLiteral("forest")) {
    return QStringLiteral("forest");
  }
  if (lowered == QStringLiteral("river")) {
    return QStringLiteral("river");
  }
  return {};
}

auto biome_to_ambience_biome(const Game::Map::MapDefinition& map_def) -> QString {
  if (!map_def.rivers.empty()) {
    return QStringLiteral("river");
  }

  switch (map_def.biome.ground_type) {
  case Game::Map::GroundType::AlpineMix:
    return QStringLiteral("mountain");
  case Game::Map::GroundType::GrassDry:
    return QStringLiteral("plains");
  case Game::Map::GroundType::SoilRocky:
    return QStringLiteral("battlefield");
  case Game::Map::GroundType::SoilFertile:
    return QStringLiteral("camp");
  case Game::Map::GroundType::ForestMud:
  default:
    return QStringLiteral("forest");
  }
}

auto build_audio_context_text(const Game::Mission::MissionDefinition* mission,
                              const QString& map_path,
                              const Game::Map::MapDefinition* map_def) -> QString {
  QStringList parts;
  parts.push_back(map_path);
  if (map_def != nullptr) {
    parts.push_back(map_def->name);
  }
  if (mission != nullptr) {
    parts.push_back(mission->id);
    parts.push_back(mission->title);
    parts.push_back(mission->summary);
    if (mission->terrain_type.has_value()) {
      parts.push_back(*mission->terrain_type);
    }
  }
  return parts.join(QLatin1Char(' ')).toLower();
}

auto context_mentions_any(const QString& text,
                          std::initializer_list<const char*> needles) -> bool {
  for (const char* needle : needles) {
    if (text.contains(QString::fromLatin1(needle))) {
      return true;
    }
  }
  return false;
}

auto build_mission_ambience_queries(const Game::Mission::MissionDefinition* mission,
                                    const QString& map_path,
                                    const Game::Map::MapDefinition* map_def,
                                    const QString& faction_tag)
    -> std::vector<QMap<QString, QString>> {
  std::vector<QMap<QString, QString>> queries;
  const QString context_text = build_audio_context_text(mission, map_path, map_def);

  if (context_mentions_any(context_text, {"harbor", "port"})) {
    queries.push_back({{QStringLiteral("biome"), QStringLiteral("harbor")}});
  }
  if (context_mentions_any(context_text, {"city", "market"})) {
    queries.push_back({{QStringLiteral("biome"), QStringLiteral("city")}});
  }
  if (context_mentions_any(context_text, {"village", "hamlet"})) {
    queries.push_back({{QStringLiteral("biome"), QStringLiteral("village")}});
  }
  if (context_mentions_any(context_text, {"siege"})) {
    queries.push_back({{QStringLiteral("biome"), QStringLiteral("camp")},
                       {QStringLiteral("context"), QStringLiteral("siege")}});
  }
  if (context_mentions_any(context_text, {"road", "crossroads"})) {
    if (faction_tag == QStringLiteral("roman")) {
      queries.push_back({{QStringLiteral("biome"), QStringLiteral("plains")},
                         {QStringLiteral("context"), QStringLiteral("road")},
                         {QStringLiteral("faction"), faction_tag}});
    }
  }
  if (context_mentions_any(context_text, {"desert", "zama"})) {
    queries.push_back({{QStringLiteral("biome"), QStringLiteral("plains")},
                       {QStringLiteral("faction"), QStringLiteral("carthage")}});
  }

  if (map_def != nullptr && map_def->rain.enabled &&
      map_def->rain.type == Game::Map::WeatherType::Rain) {
    queries.push_back({{QStringLiteral("biome"), QStringLiteral("battlefield")},
                       {QStringLiteral("weather"), QStringLiteral("rain")}});
  }

  QString biome_tag;
  if (mission != nullptr && mission->terrain_type.has_value()) {
    biome_tag = mission_terrain_to_ambience_biome(*mission->terrain_type);
  }
  if (biome_tag.isEmpty() && map_def != nullptr) {
    biome_tag = biome_to_ambience_biome(*map_def);
  }

  if (!biome_tag.isEmpty() && !faction_tag.isEmpty() &&
      (biome_tag == QStringLiteral("camp") || biome_tag == QStringLiteral("plains"))) {
    queries.push_back({{QStringLiteral("biome"), biome_tag},
                       {QStringLiteral("faction"), faction_tag}});
  }
  if (!biome_tag.isEmpty()) {
    queries.push_back({{QStringLiteral("biome"), biome_tag}});
  }
  queries.push_back({{QStringLiteral("biome"), QStringLiteral("battlefield")}});
  return queries;
}

} // namespace

AudioCoordinator::AudioCoordinator(Game::Audio::AudioEventHandler* event_handler)
    : m_event_handler(event_handler) {
}

void AudioCoordinator::apply_frontend_music_context(const QString& context) {
  if (context.isEmpty() || context == QStringLiteral("battle")) {
    AudioSystem::get_instance().stop_music();
    return;
  }

  AudioResourceLoader::load_audio_resources(AudioLoadPolicy::Screen);

  const QStringList track_ids = collect_matching_resource_ids(
      AudioCategory::MUSIC, {{{QStringLiteral("screen_context"), context}}}, false);
  const QString track_id =
      choose_round_robin_track(track_ids, QStringLiteral("frontend.") + context);
  if (track_id.isEmpty()) {
    return;
  }

  AudioResourceLoader::ensure_audio_resource_loaded(track_id);
  AudioSystem::get_instance().play_music(
      track_id.toStdString(), 1.0F, Game::Audio::MusicTransition::Crossfade);
}

void AudioCoordinator::configure_audio_manifest_mappings(int local_owner_id) {
  configure_audio_voice_mappings();
  configure_audio_ambient_mappings(local_owner_id);
}

void AudioCoordinator::configure_audio_voice_mappings() {
  if (m_event_handler == nullptr) {
    return;
  }

  static constexpr const char* k_common_units[] = {
      "archer",
      "swordsman",
      "spearman",
      "horse_archer",
      "horse_spearman",
      "horse_swordsman",
      "healer",
      "catapult",
      "ballista",
      "builder",
      "elephant",
  };

  static constexpr std::pair<Game::Systems::NationID, const char*> k_factions[] = {
      {Game::Systems::NationID::RomanRepublic, "roman"},
      {Game::Systems::NationID::Carthage, "carthage"},
  };

  for (const auto& [nation_id, faction_name] : k_factions) {
    const QString faction_tag = nation_audio_tag(nation_id);
    for (const char* unit_name : k_common_units) {
      QMap<QString, QString> tags;
      tags.insert(QStringLiteral("faction"), faction_tag);
      tags.insert(QStringLiteral("unit"), QString::fromLatin1(unit_name));

      const QString track_id =
          AudioResourceLoader::find_first_resource_id(AudioCategory::VOICE, tags);
      if (!track_id.isEmpty()) {
        AudioResourceLoader::ensure_audio_resource_loaded(track_id);
        const std::string key =
            std::string(faction_name) + "." + std::string(unit_name);
        m_event_handler->load_unit_voice_mapping(key, track_id.toStdString());
      }
    }
  }

  static constexpr const char* k_commander_units[] = {
      "roman_legion_organizer",
      "roman_veteran_consul",
      "roman_field_commander",
      "carthage_mercenary_broker",
      "carthage_cavalry_patron",
      "carthage_elephant_master",
  };

  for (const char* unit_name : k_commander_units) {
    QMap<QString, QString> tags;
    tags.insert(QStringLiteral("unit_def"), QString::fromLatin1(unit_name));
    const QString track_id =
        AudioResourceLoader::find_first_resource_id(AudioCategory::VOICE, tags);
    if (!track_id.isEmpty()) {
      AudioResourceLoader::ensure_audio_resource_loaded(track_id);
      m_event_handler->load_unit_voice_mapping(unit_name, track_id.toStdString());
    }
  }
}

void AudioCoordinator::configure_audio_ambient_mappings(int local_owner_id) {
  if (m_event_handler == nullptr) {
    return;
  }

  m_event_handler->set_local_owner_id(local_owner_id);

  QString faction_tag = QStringLiteral("roman");
  if (local_owner_id != 0) {
    if (const auto* nation =
            Game::Systems::NationRegistry::instance().get_nation_for_player(
                local_owner_id);
        nation != nullptr) {
      faction_tag = nation_audio_tag(nation->id);
    }
  }

  const auto resolve_music = [&](Engine::Core::AmbientState state) {
    std::vector<QMap<QString, QString>> queries;
    QMap<QString, QString> tags;
    tags.insert(QStringLiteral("ambient_state"), ambient_state_tag(state));
    if (!faction_tag.isEmpty() && (state == Engine::Core::AmbientState::COMBAT ||
                                   state == Engine::Core::AmbientState::VICTORY ||
                                   state == Engine::Core::AmbientState::DEFEAT)) {
      QMap<QString, QString> faction_tags = tags;
      faction_tags.insert(QStringLiteral("faction"), faction_tag);
      queries.push_back(std::move(faction_tags));
    }
    queries.push_back(std::move(tags));
    return collect_matching_resource_ids(AudioCategory::MUSIC, queries, false);
  };

  const auto resolve_state_sfx = [&](Engine::Core::AmbientState state) {
    std::vector<QMap<QString, QString>> queries;
    QMap<QString, QString> tags;
    tags.insert(QStringLiteral("state"), ambient_state_tag(state));
    if (!faction_tag.isEmpty()) {
      QMap<QString, QString> faction_tags = tags;
      faction_tags.insert(QStringLiteral("faction"), faction_tag);
      queries.push_back(std::move(faction_tags));
    }
    queries.push_back(std::move(tags));
    return collect_matching_resource_ids(AudioCategory::SFX, queries, false);
  };

  static constexpr Engine::Core::AmbientState k_states[] = {
      Engine::Core::AmbientState::PEACEFUL,
      Engine::Core::AmbientState::TENSE,
      Engine::Core::AmbientState::COMBAT,
      Engine::Core::AmbientState::VICTORY,
      Engine::Core::AmbientState::DEFEAT,
  };

  for (Engine::Core::AmbientState const state : k_states) {
    const QStringList track_ids = resolve_music(state);
    if (!track_ids.isEmpty()) {
      std::vector<std::string> music_ids;
      music_ids.reserve(track_ids.size());
      for (const QString& track_id : track_ids) {
        AudioResourceLoader::ensure_audio_resource_loaded(track_id);
        music_ids.push_back(track_id.toStdString());
      }
      m_event_handler->load_ambient_music(state, music_ids);
    } else {
      m_event_handler->load_ambient_music(state, std::vector<std::string>{});
    }

    const QStringList sfx_ids = resolve_state_sfx(state);
    if (!sfx_ids.isEmpty()) {
      std::vector<std::string> sound_ids;
      sound_ids.reserve(sfx_ids.size());
      for (const QString& sound_id : sfx_ids) {
        AudioResourceLoader::ensure_audio_resource_loaded(sound_id);
        sound_ids.push_back(sound_id.toStdString());
      }
      m_event_handler->load_ambient_state_sfx(state, sound_ids);
    } else {
      m_event_handler->load_ambient_state_sfx(state, std::vector<std::string>{});
    }
  }
}

void AudioCoordinator::ensure_result_audio_ready(const QString& state,
                                                 int local_owner_id) {
  if (state != QStringLiteral("victory") && state != QStringLiteral("defeat")) {
    return;
  }

  configure_audio_ambient_mappings(local_owner_id);
}

void AudioCoordinator::stop_mission_ambience() {
  if (!m_current_ambient_sound_id.isEmpty()) {
    AudioSystem::get_instance().stop_sound(m_current_ambient_sound_id.toStdString());
    m_current_ambient_sound_id.clear();
  }
}

void AudioCoordinator::apply_mission_ambience(
    const Game::Mission::MissionDefinition* mission,
    const QString& map_path,
    int local_owner_id) {
  stop_mission_ambience();

  QString faction_tag = QStringLiteral("roman");
  if (local_owner_id != 0) {
    if (const auto* nation =
            Game::Systems::NationRegistry::instance().get_nation_for_player(
                local_owner_id);
        nation != nullptr) {
      faction_tag = nation_audio_tag(nation->id);
    }
  }

  std::optional<Game::Map::MapDefinition> map_def;
  if (!map_path.isEmpty()) {
    Game::Map::MapDefinition loaded_map_def;
    QString map_error;
    const QString resolved_map_path = Utils::Resources::resolve_resource_path(map_path);
    if (Game::Map::MapLoader::load_from_json_file(
            resolved_map_path, loaded_map_def, &map_error)) {
      map_def = std::move(loaded_map_def);
    }
  }

  const QStringList ambience_candidates = collect_matching_resource_ids(
      AudioCategory::AMBIENCE,
      build_mission_ambience_queries(
          mission, map_path, map_def ? &*map_def : nullptr, faction_tag),
      true);
  QString ambience_id = choose_seeded_track(
      ambience_candidates,
      map_path + QLatin1Char('|') + faction_tag +
          (mission != nullptr ? mission->id : QStringLiteral("skirmish")));
  if (ambience_id.isEmpty()) {
    ambience_id = QStringLiteral("ambient.battlefield_dry_wind_distant_march_01");
  }

  AudioResourceLoader::ensure_audio_resource_loaded(ambience_id);
  AudioSystem::get_instance().play_sound(
      ambience_id.toStdString(), 1.0F, true, 1, AudioCategory::AMBIENCE);
  m_current_ambient_sound_id = ambience_id;
}
