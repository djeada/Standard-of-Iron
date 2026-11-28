#pragma once

#include <QString>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "game/map/map_definition.h"

namespace Engine::Core {
class World;
}

namespace Game::Audio {

enum class FootstepSurface : std::uint8_t {
  Grass,
  Stone,
  Gravel,
  StoneInterior,
};

class CitySoundscape {
public:
  CitySoundscape() = default;
  ~CitySoundscape();

  CitySoundscape(const CitySoundscape&) = delete;
  auto operator=(const CitySoundscape&) -> CitySoundscape& = delete;

  void configure(const Game::Map::MapDefinition* map_def,
                 const QString& base_day_bed,
                 const QString& faction_tag);

  void stop();

  void update(float dt, const Engine::Core::World* world, float darkness_amount);

  [[nodiscard]] static auto classify_footstep_surface(const Engine::Core::World& world,
                                                      float world_x,
                                                      float world_z) -> FootstepSurface;

  [[nodiscard]] auto active_bed_ids() const -> std::vector<std::string>;
  [[nodiscard]] auto zone_count() const noexcept -> std::size_t {
    return m_zones.size();
  }

  static auto instance() -> CitySoundscape&;

private:
  struct ZoneRuntime {
    std::string id;
    std::string day_bed;
    std::string night_bed;
    float x = 0.0F;
    float z = 0.0F;
    float radius = 25.0F;
    float fade = 10.0F;
    std::vector<std::pair<float, float>> polygon;
    bool interior = false;
    bool garden_or_sand = false;
  };

  struct BedChannel {
    float current_gain = 0.0F;
    float target_gain = 0.0F;
    bool playing = false;
  };

  struct StaticEmitter {
    enum class Kind : std::uint8_t {
      Hawker,
      Dog,
      Door,
      Cart,
      TempleBell,
      WaterSplash,
      Gull,
      Pigeon,
      Cattle,
      Cricket,
    };
    Kind kind = Kind::Door;
    float x = 0.0F;
    float z = 0.0F;
    float max_range = 28.0F;
    float cooldown = 0.0F;
  };

  void build_derived_zones(const Game::Map::MapDefinition& map_def);
  void build_static_emitters(const Game::Map::MapDefinition& map_def);
  void update_beds(float dt, float lx, float lz, bool is_night);
  void update_emitters(float dt,
                       const Engine::Core::World* world,
                       float lx,
                       float lz,
                       float darkness_amount);

  [[nodiscard]] static auto
  zone_weight(const ZoneRuntime& zone, float px, float pz) -> float;
  [[nodiscard]] auto resolve_bed(const std::string& day_bed,
                                 const std::string& night_bed,
                                 bool is_night) const -> std::string;
  [[nodiscard]] auto next_random(float lo, float hi) -> float;

  bool m_configured = false;
  bool m_is_city = false;
  bool m_night_latched = false;
  std::string m_faction_tag = "roman";
  std::string m_base_day_bed;
  std::string m_base_night_bed;

  std::vector<ZoneRuntime> m_zones;
  std::vector<std::pair<float, float>> m_gate_positions;
  std::unordered_map<std::string, BedChannel> m_beds;

  std::vector<StaticEmitter> m_static_emitters;
  std::unordered_map<std::uint64_t, float> m_entity_cooldowns;
  float m_global_emitter_cooldown = 1.5F;
  std::uint32_t m_rng_state = 0x9E3779B9U;
};

} // namespace Game::Audio
