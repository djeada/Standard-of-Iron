#include "audio_event_handler.h"

#include <chrono>
#include <random>
#include <string>

#include "../core/component.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "audio_system.h"
#include "core/event_manager.h"
#include "units/spawn_type.h"

namespace Game::Audio {

namespace {

thread_local std::mt19937 g_audio_rng(std::random_device{}());

auto get_volume_variation() -> float {
  std::uniform_real_distribution<float> dist(0.85F, 1.0F);
  return dist(g_audio_rng);
}

auto nation_voice_prefix(Game::Systems::NationID nation_id) -> std::string {
  switch (nation_id) {
  case Game::Systems::NationID::RomanRepublic:
    return "roman";
  case Game::Systems::NationID::Carthage:
    return "carthage";
  case Game::Systems::NationID::IronSepulcher:
    return {};
  }
  return {};
}

auto make_voice_key(Game::Systems::NationID nation_id,
                    const std::string& unit_type) -> std::string {
  const std::string faction = nation_voice_prefix(nation_id);
  if (faction.empty()) {
    return unit_type;
  }
  return faction + "." + unit_type;
}

auto make_manifest_voice_id(Game::Systems::NationID nation_id,
                            const std::string& unit_type) -> std::string {
  const std::string faction = nation_voice_prefix(nation_id);
  if (faction.empty()) {
    return {};
  }
  return "voice." + faction + "." + unit_type;
}

auto resolve_voice_id(const Engine::Core::UnitComponent& unit_component,
                      const std::string& unit_type,
                      const std::unordered_map<std::string, std::string>& voice_map)
    -> std::string {
  const std::string faction_key = make_voice_key(unit_component.nation_id, unit_type);
  if (auto faction_it = voice_map.find(faction_key); faction_it != voice_map.end()) {
    return faction_it->second;
  }

  if (auto generic_it = voice_map.find(unit_type); generic_it != voice_map.end()) {
    return generic_it->second;
  }

  const std::string manifest_voice_id =
      make_manifest_voice_id(unit_component.nation_id, unit_type);
  if (!manifest_voice_id.empty() &&
      AudioSystem::get_instance().has_resource(manifest_voice_id)) {
    return manifest_voice_id;
  }

  const std::string legacy_alias = unit_type + "_voice";
  if (AudioSystem::get_instance().has_resource(legacy_alias)) {
    return legacy_alias;
  }

  return {};
}

auto choose_loaded_variant(const std::vector<std::string>& candidates,
                           const std::string& last_id = {}) -> std::string {
  std::vector<std::string> available_ids;
  available_ids.reserve(candidates.size());
  for (const auto& candidate : candidates) {
    if (AudioSystem::get_instance().has_resource(candidate)) {
      available_ids.push_back(candidate);
    }
  }

  if (available_ids.empty()) {
    return {};
  }

  std::uniform_int_distribution<size_t> dist(0, available_ids.size() - 1U);
  const size_t start_index = dist(g_audio_rng);
  for (size_t offset = 0; offset < available_ids.size(); ++offset) {
    const std::string& candidate =
        available_ids[(start_index + offset) % available_ids.size()];
    if (available_ids.size() == 1U || candidate != last_id) {
      return candidate;
    }
  }

  return available_ids.front();
}

auto ambient_music_group_key(Engine::Core::AmbientState state) -> std::string {
  switch (state) {
  case Engine::Core::AmbientState::PEACEFUL:
    return "ambient.music.peaceful";
  case Engine::Core::AmbientState::TENSE:
    return "ambient.music.tense";
  case Engine::Core::AmbientState::COMBAT:
    return "ambient.music.combat";
  case Engine::Core::AmbientState::VICTORY:
    return "ambient.music.victory";
  case Engine::Core::AmbientState::DEFEAT:
    return "ambient.music.defeat";
  }
  return "ambient.music.peaceful";
}

auto ambient_sfx_group_key(Engine::Core::AmbientState state) -> std::string {
  switch (state) {
  case Engine::Core::AmbientState::PEACEFUL:
    return "ambient.sfx.peaceful";
  case Engine::Core::AmbientState::TENSE:
    return "ambient.sfx.tense";
  case Engine::Core::AmbientState::COMBAT:
    return "ambient.sfx.combat";
  case Engine::Core::AmbientState::VICTORY:
    return "ambient.sfx.victory";
  case Engine::Core::AmbientState::DEFEAT:
    return "ambient.sfx.defeat";
  }
  return "ambient.sfx.peaceful";
}

auto victory_state_sounds() -> const std::vector<std::string>& {
  static const std::vector<std::string> k_sounds = {
      "sfx.combat.soldiers_victory_cheer",
  };
  return k_sounds;
}

auto defeat_state_sounds() -> const std::vector<std::string>& {
  static const std::vector<std::string> k_sounds = {
      "sfx.combat.army_retreat_panic",
  };
  return k_sounds;
}

auto death_sound_pool() -> const std::vector<std::string>& {
  static const std::vector<std::string> k_sounds = {
      "combat_death",
      "sfx.combat.aftermath_battlefield",
  };
  return k_sounds;
}

auto get_hit_sound_group_key(Game::Units::SpawnType type) -> std::string {
  switch (type) {
  case Game::Units::SpawnType::Knight:
  case Game::Units::SpawnType::SkeletonSwordsman:
  case Game::Units::SpawnType::GravePriest:
    return "combat.hit.sword";
  case Game::Units::SpawnType::Spearman:
  case Game::Units::SpawnType::HorseSpearman:
    return "combat.hit.spear";
  case Game::Units::SpawnType::Archer:
  case Game::Units::SpawnType::SkeletonArcher:
  case Game::Units::SpawnType::HorseArcher:
    return "combat.hit.arrow";
  case Game::Units::SpawnType::MountedKnight:
    return "combat.hit.cavalry";
  case Game::Units::SpawnType::Elephant:
    return "combat.hit.elephant";
  case Game::Units::SpawnType::Catapult:
  case Game::Units::SpawnType::Ballista:
    return "combat.hit.siege";
  default:
    return "combat.hit.generic";
  }
}

auto get_hit_sound_cooldown_ms(Game::Units::SpawnType type) -> int {
  switch (type) {
  case Game::Units::SpawnType::Knight:
  case Game::Units::SpawnType::SkeletonSwordsman:
  case Game::Units::SpawnType::GravePriest:
    return 90;
  case Game::Units::SpawnType::Spearman:
  case Game::Units::SpawnType::HorseSpearman:
    return 100;
  case Game::Units::SpawnType::Archer:
  case Game::Units::SpawnType::SkeletonArcher:
  case Game::Units::SpawnType::HorseArcher:
    return 120;
  case Game::Units::SpawnType::MountedKnight:
    return 220;
  case Game::Units::SpawnType::Elephant:
    return 260;
  case Game::Units::SpawnType::Catapult:
  case Game::Units::SpawnType::Ballista:
    return 250;
  default:
    return 140;
  }
}

auto get_hit_sound_pool(Game::Units::SpawnType type)
    -> const std::vector<std::string>& {
  static const std::vector<std::string> k_sword = {
      "combat_hit_sword",
      "sfx.combat.roman_shield_wall_impact",
  };
  static const std::vector<std::string> k_spear = {
      "combat_hit_spear",
      "sfx.combat.spearmen_formation_advance",
  };
  static const std::vector<std::string> k_arrow = {
      "combat_hit_arrow",
      "sfx.combat.archer_volley_many",
      "sfx.combat.archers_shooting_close",
      "sfx.combat.arrows_whistle_snap_impact",
  };
  static const std::vector<std::string> k_generic = {
      "combat_hit_generic",
      "sfx.combat.battlefield_distant_mass_01",
  };
  static const std::vector<std::string> k_cavalry = {
      "combat_hit_cavalry",
      "sfx.combat.horse_gallop_close_pass",
      "sfx.combat.numidian_cavalry_chase",
  };
  static const std::vector<std::string> k_elephant = {
      "combat_hit_elephant",
      "sfx.combat.elephant_panic",
  };
  static const std::vector<std::string> k_siege = {
      "combat_hit_siege",
  };

  switch (type) {
  case Game::Units::SpawnType::Knight:
  case Game::Units::SpawnType::SkeletonSwordsman:
  case Game::Units::SpawnType::GravePriest:
    return k_sword;
  case Game::Units::SpawnType::Spearman:
  case Game::Units::SpawnType::HorseSpearman:
    return k_spear;
  case Game::Units::SpawnType::Archer:
  case Game::Units::SpawnType::SkeletonArcher:
  case Game::Units::SpawnType::HorseArcher:
    return k_arrow;
  case Game::Units::SpawnType::MountedKnight:
    return k_cavalry;
  case Game::Units::SpawnType::Elephant:
    return k_elephant;
  case Game::Units::SpawnType::Catapult:
  case Game::Units::SpawnType::Ballista:
    return k_siege;
  default:
    return k_generic;
  }
}

} // namespace

AudioEventHandler::AudioEventHandler(Engine::Core::World* world)
    : m_world(world) {
}

AudioEventHandler::~AudioEventHandler() {
  shutdown();
}

auto AudioEventHandler::initialize() -> bool {
  if (m_initialized) {
    return true;
  }

  m_unit_selected_sub =
      Engine::Core::ScopedEventSubscription<Engine::Core::UnitSelectedEvent>(
          [this](const Engine::Core::UnitSelectedEvent& event) {
            on_unit_selected(event);
          });

  m_ambient_changed_sub =
      Engine::Core::ScopedEventSubscription<Engine::Core::AmbientStateChangedEvent>(
          [this](const Engine::Core::AmbientStateChangedEvent& event) {
            on_ambient_state_changed(event);
          });

  m_audio_trigger_sub =
      Engine::Core::ScopedEventSubscription<Engine::Core::AudioTriggerEvent>(
          [this](const Engine::Core::AudioTriggerEvent& event) {
            on_audio_trigger(event);
          });

  m_music_trigger_sub =
      Engine::Core::ScopedEventSubscription<Engine::Core::MusicTriggerEvent>(
          [this](const Engine::Core::MusicTriggerEvent& event) {
            on_music_trigger(event);
          });

  m_combat_hit_sub =
      Engine::Core::ScopedEventSubscription<Engine::Core::CombatHitEvent>(
          [this](const Engine::Core::CombatHitEvent& event) { on_combat_hit(event); });

  m_unit_spawned_sub =
      Engine::Core::ScopedEventSubscription<Engine::Core::UnitSpawnedEvent>(
          [this](const Engine::Core::UnitSpawnedEvent& event) {
            on_unit_spawned(event);
          });

  m_unit_died_sub = Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent>(
      [this](const Engine::Core::UnitDiedEvent& event) { on_unit_died(event); });

  m_building_attacked_sub =
      Engine::Core::ScopedEventSubscription<Engine::Core::BuildingAttackedEvent>(
          [this](const Engine::Core::BuildingAttackedEvent& event) {
            on_building_attacked(event);
          });

  m_barrack_captured_sub =
      Engine::Core::ScopedEventSubscription<Engine::Core::BarrackCapturedEvent>(
          [this](const Engine::Core::BarrackCapturedEvent& event) {
            on_barrack_captured(event);
          });

  m_initialized = true;
  return true;
}

void AudioEventHandler::shutdown() {
  if (!m_initialized) {
    return;
  }

  m_unit_selected_sub.unsubscribe();
  m_ambient_changed_sub.unsubscribe();
  m_audio_trigger_sub.unsubscribe();
  m_music_trigger_sub.unsubscribe();
  m_combat_hit_sub.unsubscribe();
  m_unit_spawned_sub.unsubscribe();
  m_unit_died_sub.unsubscribe();
  m_building_attacked_sub.unsubscribe();
  m_barrack_captured_sub.unsubscribe();

  m_unit_voice_map.clear();
  m_ambient_music_map.clear();
  m_ambient_state_sfx_map.clear();
  m_last_sound_group_time.clear();
  m_last_sound_group_id.clear();

  m_initialized = false;
}

void AudioEventHandler::load_unit_voice_mapping(const std::string& unit_type,
                                                const std::string& sound_id) {
  m_unit_voice_map[unit_type] = sound_id;
}

void AudioEventHandler::load_ambient_music(Engine::Core::AmbientState state,
                                           const std::string& music_id) {
  if (music_id.empty()) {
    m_ambient_music_map.erase(state);
    return;
  }
  m_ambient_music_map[state] = {music_id};
}

void AudioEventHandler::load_ambient_music(Engine::Core::AmbientState state,
                                           const std::vector<std::string>& music_ids) {
  std::vector<std::string> filtered_ids;
  filtered_ids.reserve(music_ids.size());
  for (const auto& music_id : music_ids) {
    if (!music_id.empty()) {
      filtered_ids.push_back(music_id);
    }
  }

  if (filtered_ids.empty()) {
    m_ambient_music_map.erase(state);
    return;
  }

  m_ambient_music_map[state] = std::move(filtered_ids);
}

void AudioEventHandler::load_ambient_state_sfx(
    Engine::Core::AmbientState state, const std::vector<std::string>& sound_ids) {
  std::vector<std::string> filtered_ids;
  filtered_ids.reserve(sound_ids.size());
  for (const auto& sound_id : sound_ids) {
    if (!sound_id.empty()) {
      filtered_ids.push_back(sound_id);
    }
  }

  if (filtered_ids.empty()) {
    m_ambient_state_sfx_map.erase(state);
    return;
  }

  m_ambient_state_sfx_map[state] = std::move(filtered_ids);
}

void AudioEventHandler::set_voice_sound_category(bool use_voice_category) {
  m_use_voice_category = use_voice_category;
}

void AudioEventHandler::set_local_owner_id(int owner_id) {
  m_local_owner_id = owner_id;
}

void AudioEventHandler::on_unit_selected(const Engine::Core::UnitSelectedEvent& event) {
  if (m_world == nullptr) {
    return;
  }

  auto* entity = m_world->get_entity(event.unit_id);
  if (entity == nullptr) {
    return;
  }

  auto* unit_component = entity->get_component<Engine::Core::UnitComponent>();
  if (unit_component == nullptr) {
    return;
  }

  std::string const unit_type_str =
      Game::Units::spawn_typeToString(unit_component->spawn_type);
  const std::string sound_id =
      resolve_voice_id(*unit_component, unit_type_str, m_unit_voice_map);
  if (!sound_id.empty()) {
    auto now = std::chrono::steady_clock::now();
    auto time_since_last_sound = std::chrono::duration_cast<std::chrono::milliseconds>(
                                     now - m_last_selection_sound_time)
                                     .count();

    bool const should_play = (time_since_last_sound >= SELECTION_SOUND_COOLDOWN_MS) ||
                             (sound_id != m_last_selection_sound_id);

    if (should_play) {
      AudioCategory const category =
          m_use_voice_category ? AudioCategory::VOICE : AudioCategory::SFX;
      AudioSystem::get_instance().play_sound(
          sound_id, UNIT_SELECTION_VOLUME, false, UNIT_SELECTION_PRIORITY, category);
      m_last_selection_sound_time = now;
      m_last_selection_sound_id = sound_id;
    }
  }
}

void AudioEventHandler::on_unit_spawned(const Engine::Core::UnitSpawnedEvent& event) {
  if (m_world == nullptr || event.is_initial_spawn ||
      !Game::Units::is_troop_spawn(event.spawn_type)) {
    return;
  }

  auto* entity = m_world->get_entity(event.unit_id);
  auto* unit_component = entity != nullptr
                             ? entity->get_component<Engine::Core::UnitComponent>()
                             : nullptr;
  if (unit_component == nullptr) {
    return;
  }

  const std::string unit_type_str = Game::Units::spawn_typeToString(event.spawn_type);
  const std::string sound_id =
      resolve_voice_id(*unit_component, unit_type_str, m_unit_voice_map);
  if (sound_id.empty()) {
    return;
  }

  AudioCategory const category =
      m_use_voice_category ? AudioCategory::VOICE : AudioCategory::SFX;
  AudioSystem::get_instance().play_sound(sound_id, 0.85F, false, 4, category);
}

void AudioEventHandler::on_unit_died(const Engine::Core::UnitDiedEvent& event) {
  if (!Game::Units::is_troop_spawn(event.spawn_type)) {
    return;
  }

  play_sound_group(
      "combat.hit.death", death_sound_pool(), 0.75F, 6, AudioCategory::SFX, 160);
}

void AudioEventHandler::on_ambient_state_changed(
    const Engine::Core::AmbientStateChangedEvent& event) {
  auto it = m_ambient_music_map.find(event.new_state);
  if (it != m_ambient_music_map.end() && !it->second.empty()) {
    const std::string group_key = ambient_music_group_key(event.new_state);
    const std::string last_id = m_last_sound_group_id[group_key];
    const std::string music_id = choose_loaded_variant(it->second, last_id);
    if (!music_id.empty()) {
      AudioSystem::get_instance().play_music(music_id);
      m_last_sound_group_id[group_key] = music_id;
    }
  }

  auto sfx_it = m_ambient_state_sfx_map.find(event.new_state);
  if (sfx_it != m_ambient_state_sfx_map.end() && !sfx_it->second.empty()) {
    play_sound_group(ambient_sfx_group_key(event.new_state),
                     sfx_it->second,
                     0.9F,
                     7,
                     AudioCategory::SFX,
                     2000);
  }

  if (event.new_state == Engine::Core::AmbientState::VICTORY) {
    play_sound_group(
        "state.victory", victory_state_sounds(), 0.9F, 7, AudioCategory::SFX, 2000);
  } else if (event.new_state == Engine::Core::AmbientState::DEFEAT) {
    play_sound_group(
        "state.defeat", defeat_state_sounds(), 0.9F, 7, AudioCategory::SFX, 2000);
  }
}

auto AudioEventHandler::should_play_sound_group(const std::string& group_id,
                                                int cooldown_ms) -> bool {
  auto now = std::chrono::steady_clock::now();
  auto it = m_last_sound_group_time.find(group_id);
  if (it != m_last_sound_group_time.end()) {
    auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second);
    if (elapsed.count() < cooldown_ms) {
      return false;
    }
  }
  return true;
}

void AudioEventHandler::mark_sound_group_played(const std::string& group_id) {
  m_last_sound_group_time[group_id] = std::chrono::steady_clock::now();
}

void AudioEventHandler::play_sound_group(const std::string& group_id,
                                         const std::vector<std::string>& sound_ids,
                                         float volume,
                                         int priority,
                                         AudioCategory category,
                                         int cooldown_ms) {
  if (sound_ids.empty() || !should_play_sound_group(group_id, cooldown_ms)) {
    return;
  }

  const std::string sound_id =
      choose_loaded_variant(sound_ids, m_last_sound_group_id[group_id]);
  if (sound_id.empty()) {
    return;
  }

  AudioSystem::get_instance().play_sound(sound_id, volume, false, priority, category);
  mark_sound_group_played(group_id);
  m_last_sound_group_id[group_id] = sound_id;
}

void AudioEventHandler::on_building_attacked(
    const Engine::Core::BuildingAttackedEvent& event) {
  if (m_local_owner_id != 0 && event.owner_id != m_local_owner_id) {
    return;
  }
  static const std::vector<std::string> k_sounds = {"enemy_spotted_horn"};
  play_sound_group("alert.enemy_spotted", k_sounds, 0.85F, 7, AudioCategory::SFX, 1500);
}

void AudioEventHandler::on_barrack_captured(
    const Engine::Core::BarrackCapturedEvent& event) {
  static const std::vector<std::string> k_player_capture_sounds = {
      "reinforcements_arrived",
      "sfx.combat.army_march_dirt_mass",
  };
  static const std::vector<std::string> k_player_loss_sounds = {
      "enemy_reinforcements_warning",
  };

  if (m_local_owner_id == 0) {
    play_sound_group("alert.reinforcements_arrived",
                     k_player_capture_sounds,
                     0.90F,
                     8,
                     AudioCategory::SFX,
                     3000);
    return;
  }

  if (event.new_owner_id == m_local_owner_id) {
    play_sound_group("alert.reinforcements_arrived",
                     k_player_capture_sounds,
                     0.90F,
                     8,
                     AudioCategory::SFX,
                     3000);
  } else if (event.previous_owner_id == m_local_owner_id) {
    play_sound_group("alert.enemy_reinforcements",
                     k_player_loss_sounds,
                     0.90F,
                     8,
                     AudioCategory::SFX,
                     3000);
  }
}

void AudioEventHandler::on_audio_trigger(const Engine::Core::AudioTriggerEvent& event) {
  AudioSystem::get_instance().play_sound(
      event.sound_id, event.volume, event.loop, event.priority);
}

void AudioEventHandler::on_music_trigger(const Engine::Core::MusicTriggerEvent& event) {
  AudioSystem::get_instance().play_music(event.music_id, event.volume, event.crossfade);
}

void AudioEventHandler::on_combat_hit(const Engine::Core::CombatHitEvent& event) {
  float const volume = COMBAT_HIT_VOLUME * get_volume_variation();
  play_sound_group(get_hit_sound_group_key(event.attacker_type),
                   get_hit_sound_pool(event.attacker_type),
                   volume,
                   COMBAT_HIT_PRIORITY,
                   AudioCategory::SFX,
                   get_hit_sound_cooldown_ms(event.attacker_type));

  if (event.is_killing_blow) {
    play_sound_group("combat.hit.death",
                     death_sound_pool(),
                     volume * 0.9F,
                     COMBAT_HIT_PRIORITY + 1,
                     AudioCategory::SFX,
                     160);
  }
}

} // namespace Game::Audio
