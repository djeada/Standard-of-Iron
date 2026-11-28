#include "app/audio/city_soundscape.h"

#include <QDebug>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <unordered_set>

#include "app/audio/audio_resource_loader.h"
#include "game/audio/audio_cues.h"
#include "game/audio/audio_system.h"
#include "game/audio/cue_ids.h"
#include "game/audio/cue_trace.h"
#include "game/core/component.h"
#include "game/core/component_economy.h"
#include "game/core/component_gameplay.h"
#include "game/core/component_structures.h"
#include "game/core/entity.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/units/spawn_type.h"

namespace Game::Audio {

namespace {

constexpr float k_crossfade_seconds = 2.4F;
constexpr float k_night_enter_darkness = 0.48F;
constexpr float k_night_exit_darkness = 0.38F;
constexpr float k_global_emitter_min_gap = 1.35F;
constexpr float k_global_emitter_max_gap = 2.65F;
constexpr std::size_t k_city_house_threshold = 300;

auto point_in_polygon(const std::vector<std::pair<float, float>>& poly,
                      float px,
                      float pz) -> bool {
  bool inside = false;
  const std::size_t count = poly.size();
  for (std::size_t i = 0, j = count - 1; i < count; j = i++) {
    const float xi = poly[i].first;
    const float zi = poly[i].second;
    const float xj = poly[j].first;
    const float zj = poly[j].second;
    const bool intersect = ((zi > pz) != (zj > pz)) &&
                           (px < (xj - xi) * (pz - zi) / ((zj - zi) + 1.0e-7F) + xi);
    if (intersect) {
      inside = !inside;
    }
  }
  return inside;
}

auto segment_distance(float px, float pz, float ax, float az, float bx, float bz)
    -> float {
  const float dx = bx - ax;
  const float dz = bz - az;
  const float len_sq = dx * dx + dz * dz;
  if (len_sq <= 1.0e-6F) {
    return std::hypot(px - ax, pz - az);
  }
  const float t = std::clamp(((px - ax) * dx + (pz - az) * dz) / len_sq, 0.0F, 1.0F);
  return std::hypot(px - (ax + dx * t), pz - (az + dz * t));
}

auto polygon_edge_distance(const std::vector<std::pair<float, float>>& poly,
                           float px,
                           float pz) -> float {
  float best = 1.0e9F;
  const std::size_t count = poly.size();
  for (std::size_t i = 0; i < count; ++i) {
    const auto& a = poly[i];
    const auto& b = poly[(i + 1U) % count];
    best =
        std::min(best, segment_distance(px, pz, a.first, a.second, b.first, b.second));
  }
  return best;
}

auto is_interior_bed(const std::string& bed) -> bool {
  return bed == "ambient.arena_interior" || bed == "ambient.stone_tunnel";
}

auto is_garden_or_sand_bed(const std::string& bed) -> bool {
  return bed == "ambient.city_garden_day" || bed == "ambient.city_garden_dusk" ||
         bed == "ambient.livestock_market" || bed == "ambient.temple_precinct";
}

auto structure_xz(const Game::Map::StructureEntry& entry) -> std::pair<float, float> {
  if (const auto* pt =
          std::get_if<Game::Map::PointStructureGeometry>(&entry.geometry)) {
    return {pt->position.x(), pt->position.z()};
  }
  const auto& ln = std::get<Game::Map::LineStructureGeometry>(entry.geometry);
  return {(ln.start.x() + ln.end.x()) * 0.5F, (ln.start.z() + ln.end.z()) * 0.5F};
}

} // namespace

CitySoundscape::~CitySoundscape() {
  stop();
}

auto CitySoundscape::instance() -> CitySoundscape& {
  static CitySoundscape singleton;
  return singleton;
}

auto CitySoundscape::next_random(float lo, float hi) -> float {
  m_rng_state ^= m_rng_state << 13U;
  m_rng_state ^= m_rng_state >> 17U;
  m_rng_state ^= m_rng_state << 5U;
  const float unit =
      static_cast<float>(m_rng_state & 0x00FFFFFFU) / static_cast<float>(0x01000000U);
  return lo + (hi - lo) * unit;
}

auto CitySoundscape::resolve_bed(const std::string& day_bed,
                                 const std::string& night_bed,
                                 bool is_night) const -> std::string {
  if (!is_night) {
    return day_bed;
  }
  if (!night_bed.empty()) {
    return night_bed;
  }
  if (day_bed == "ambient.city_street_day" ||
      day_bed == "ambient.mediterranean_city_market" ||
      day_bed == "ambient.livestock_market") {
    return "ambient.city_street_night";
  }
  if (day_bed == "ambient.city_garden_day") {
    return "ambient.city_garden_dusk";
  }
  if (day_bed == "ambient.city_square_crowd") {
    return "ambient.camp_fire_night";
  }
  if (day_bed == "ambient.quay_work") {
    return "ambient.mediterranean_harbor";
  }
  if (day_bed == "ambient.drill_field") {
    return m_faction_tag == "carthage" ? "ambient.carthage_war_camp_01"
                                       : "ambient.roman_army_camp_01";
  }
  return day_bed;
}

auto CitySoundscape::zone_weight(const ZoneRuntime& zone, float px, float pz) -> float {
  const float fade = std::max(0.5F, zone.fade);
  if (!zone.polygon.empty()) {
    if (point_in_polygon(zone.polygon, px, pz)) {
      return 1.0F;
    }
    const float dist = polygon_edge_distance(zone.polygon, px, pz);
    if (dist >= fade) {
      return 0.0F;
    }
    return 0.5F * (1.0F + std::cos(std::numbers::pi_v<float> * (dist / fade)));
  }

  const float dist = std::hypot(px - zone.x, pz - zone.z);
  if (dist <= zone.radius) {
    return 1.0F;
  }
  if (dist >= zone.radius + fade) {
    return 0.0F;
  }
  const float t = (dist - zone.radius) / fade;
  return 0.5F * (1.0F + std::cos(std::numbers::pi_v<float> * t));
}

void CitySoundscape::configure(const Game::Map::MapDefinition* map_def,
                               const QString& base_day_bed,
                               const QString& faction_tag) {
  stop();
  m_zones.clear();
  m_gate_positions.clear();
  m_static_emitters.clear();
  m_entity_cooldowns.clear();
  m_night_latched = false;
  m_faction_tag = faction_tag.isEmpty() ? "roman" : faction_tag.toStdString();
  m_rng_state =
      (map_def != nullptr ? (map_def->biome.seed | 1U) : 0x9E3779B9U) ^ 0xA5A55A5AU;
  m_global_emitter_cooldown = 1.2F;

  std::size_t house_count = 0;
  if (map_def != nullptr) {
    for (const auto& s : map_def->structures) {
      if (s.type == Game::Units::SpawnType::Home) {
        ++house_count;
      } else if (s.type == Game::Units::SpawnType::WallGate) {
        m_gate_positions.push_back(structure_xz(s));
      }
    }
  }
  m_is_city = house_count >= k_city_house_threshold;

  if (m_is_city &&
      (base_day_bed.isEmpty() ||
       base_day_bed == QStringLiteral("ambient.mediterranean_city_market"))) {
    m_base_day_bed = "ambient.city_street_day";
    m_base_night_bed = "ambient.city_street_night";
  } else {
    m_base_day_bed = base_day_bed.toStdString();
    m_base_night_bed = m_is_city ? "ambient.city_street_night" : "";
  }

  if (map_def != nullptr) {
    for (const auto& authored : map_def->ambience_zones) {
      ZoneRuntime z;
      z.id = authored.id.toStdString();
      z.day_bed = authored.bed.toStdString();
      z.night_bed = authored.night_bed.toStdString();
      z.x = authored.x;
      z.z = authored.z;
      z.radius = authored.radius;
      z.fade = authored.fade;
      z.polygon = authored.polygon;
      z.interior = is_interior_bed(z.day_bed);
      z.garden_or_sand = is_garden_or_sand_bed(z.day_bed);
      m_zones.push_back(std::move(z));
    }
    build_derived_zones(*map_def);
    build_static_emitters(*map_def);
  }

  m_configured = !m_base_day_bed.empty();
  if (m_configured) {
    AudioResourceLoader::ensure_audio_resource_loaded(
        QString::fromStdString(m_base_day_bed));
    if (!m_base_night_bed.empty()) {
      AudioResourceLoader::ensure_audio_resource_loaded(
          QString::fromStdString(m_base_night_bed));
    }
    AudioSystem::get_instance().play_sound(
        m_base_day_bed, 1.0F, true, 1, AudioCategory::AMBIENCE);
    m_beds[m_base_day_bed] =
        BedChannel{.current_gain = 1.0F, .target_gain = 1.0F, .playing = true};
  }
}

void CitySoundscape::build_derived_zones(const Game::Map::MapDefinition& map_def) {
  const auto covered_near = [&](float x, float z, float reach) {
    for (const auto& existing : m_zones) {
      if (zone_weight(existing, x, z) > 0.25F ||
          std::hypot(existing.x - x, existing.z - z) < reach) {
        return true;
      }
    }
    return false;
  };

  // 1. Marketplaces -> market bed
  for (const auto& s : map_def.structures) {
    if (s.type != Game::Units::SpawnType::Marketplace) {
      continue;
    }
    const auto [sx, sz] = structure_xz(s);
    if (covered_near(sx, sz, 20.0F)) {
      continue;
    }
    m_zones.push_back(ZoneRuntime{
        .id = "derived_market",
        .day_bed = "ambient.mediterranean_city_market",
        .night_bed = "ambient.city_street_night",
        .x = sx,
        .z = sz,
        .radius = 22.0F,
        .fade = 12.0F,
    });
  }

  // 2. Water within ~25 m -> river or harbour/garden bed
  for (const auto& lake : map_def.lakes) {
    const float span = std::max(lake.width, lake.depth) * 0.5F;
    const float lx = lake.center.x();
    const float lz = lake.center.z();
    if (span > 400.0F || covered_near(lx, lz, std::max(24.0F, span))) {
      continue;
    }
    const bool small_basin = span <= 14.0F;
    m_zones.push_back(ZoneRuntime{
        .id = small_basin ? "derived_basin" : "derived_harbor",
        .day_bed =
            small_basin ? "ambient.city_garden_day" : "ambient.mediterranean_harbor",
        .night_bed =
            small_basin ? "ambient.city_garden_dusk" : "ambient.mediterranean_harbor",
        .x = lx,
        .z = lz,
        .radius = std::max(12.0F, span + 10.0F),
        .fade = 14.0F,
        .garden_or_sand = small_basin,
    });
  }
  for (std::size_t i = 0; i < map_def.rivers.size(); i += 3U) {
    const auto& river = map_def.rivers[i];
    const float rx = (river.start.x() + river.end.x()) * 0.5F;
    const float rz = (river.start.z() + river.end.z()) * 0.5F;
    if (covered_near(rx, rz, 28.0F)) {
      continue;
    }
    m_zones.push_back(ZoneRuntime{
        .id = "derived_river",
        .day_bed = "ambient.river_crossing",
        .night_bed = "ambient.river_crossing",
        .x = rx,
        .z = rz,
        .radius = river.width * 0.5F + 12.0F,
        .fade = 13.0F,
    });
  }

  // 3. Tree & plant garden clusters (matching terrain_renderer_submission garden mask)
  if (m_is_city) {
    struct CellKey {
      int cx;
      int cz;
      auto operator==(const CellKey&) const -> bool = default;
    };
    struct CellHash {
      auto operator()(const CellKey& k) const noexcept -> std::size_t {
        return (static_cast<std::size_t>(static_cast<std::uint32_t>(k.cx)) *
                73856093U) ^
               (static_cast<std::size_t>(static_cast<std::uint32_t>(k.cz)) * 19349663U);
      }
    };
    struct Bucket {
      int count = 0;
      float sum_x = 0.0F;
      float sum_z = 0.0F;
    };
    std::unordered_map<CellKey, Bucket, CellHash> buckets;
    constexpr float k_cell = 28.0F;
    for (const auto& prop : map_def.world_props) {
      using PT = Game::Map::WorldProp::Type;
      if (prop.type != PT::OliveTree && prop.type != PT::CypressTree &&
          prop.type != PT::PineTree && prop.type != PT::PalmTree &&
          prop.type != PT::Plant) {
        continue;
      }
      const CellKey key{static_cast<int>(std::floor(prop.x / k_cell)),
                        static_cast<int>(std::floor(prop.z / k_cell))};
      auto& b = buckets[key];
      ++b.count;
      b.sum_x += prop.x;
      b.sum_z += prop.z;
    }
    for (const auto& [key, b] : buckets) {
      if (b.count < 8) {
        continue;
      }
      const float gx = b.sum_x / static_cast<float>(b.count);
      const float gz = b.sum_z / static_cast<float>(b.count);
      if (covered_near(gx, gz, 24.0F)) {
        continue;
      }
      m_zones.push_back(ZoneRuntime{
          .id = "derived_garden",
          .day_bed = "ambient.city_garden_day",
          .night_bed = "ambient.city_garden_dusk",
          .x = gx,
          .z = gz,
          .radius = 18.0F,
          .fade = 10.0F,
          .garden_or_sand = true,
      });
    }
  }

  // 4. Troop concentrations -> camp or drill bed
  struct SpawnCell {
    int squads = 0;
    float sum_x = 0.0F;
    float sum_z = 0.0F;
  };
  std::unordered_map<std::uint64_t, SpawnCell> troop_cells;
  for (const auto& sp : map_def.spawns) {
    if (!Game::Units::is_combatant_spawn(sp.type)) {
      continue;
    }
    const auto cx = static_cast<std::int32_t>(std::floor(sp.x / 36.0F));
    const auto cz = static_cast<std::int32_t>(std::floor(sp.z / 36.0F));
    const std::uint64_t key =
        (static_cast<std::uint64_t>(static_cast<std::uint32_t>(cx)) << 32U) |
        static_cast<std::uint32_t>(cz);
    auto& cell = troop_cells[key];
    ++cell.squads;
    cell.sum_x += sp.x;
    cell.sum_z += sp.z;
  }
  for (const auto& [_, cell] : troop_cells) {
    if (cell.squads < 4) {
      continue;
    }
    const float tx = cell.sum_x / static_cast<float>(cell.squads);
    const float tz = cell.sum_z / static_cast<float>(cell.squads);
    if (covered_near(tx, tz, 28.0F)) {
      continue;
    }
    m_zones.push_back(ZoneRuntime{
        .id = "derived_troop_camp",
        .day_bed = "ambient.drill_field",
        .night_bed = m_faction_tag == "carthage" ? "ambient.carthage_war_camp_01"
                                                 : "ambient.roman_army_camp_01",
        .x = tx,
        .z = tz,
        .radius = 24.0F,
        .fade = 12.0F,
    });
  }
  for (const auto& s : map_def.structures) {
    if (s.type != Game::Units::SpawnType::Barracks) {
      continue;
    }
    const auto [bx, bz] = structure_xz(s);
    if (covered_near(bx, bz, 26.0F)) {
      continue;
    }
    m_zones.push_back(ZoneRuntime{
        .id = "derived_drill",
        .day_bed = "ambient.drill_field",
        .night_bed = m_faction_tag == "carthage" ? "ambient.carthage_war_camp_01"
                                                 : "ambient.roman_army_camp_01",
        .x = bx,
        .z = bz,
        .radius = 24.0F,
        .fade = 12.0F,
    });
  }
}

void CitySoundscape::build_static_emitters(const Game::Map::MapDefinition& map_def) {
  std::size_t home_stride = 0;
  for (const auto& s : map_def.structures) {
    const auto [sx, sz] = structure_xz(s);
    if (s.type == Game::Units::SpawnType::Marketplace) {
      m_static_emitters.push_back(StaticEmitter{
          StaticEmitter::Kind::Hawker, sx, sz, 32.0F, next_random(1.0F, 6.0F)});
    } else if (s.type == Game::Units::SpawnType::Temple) {
      m_static_emitters.push_back(StaticEmitter{
          StaticEmitter::Kind::TempleBell, sx, sz, 44.0F, next_random(6.0F, 22.0F)});
    } else if (s.type == Game::Units::SpawnType::Farm) {
      m_static_emitters.push_back(StaticEmitter{
          StaticEmitter::Kind::Cattle, sx, sz, 30.0F, next_random(2.0F, 9.0F)});
    } else if (s.type == Game::Units::SpawnType::Home) {
      ++home_stride;
      if (home_stride % 14U == 0U) {
        m_static_emitters.push_back(StaticEmitter{
            StaticEmitter::Kind::Door, sx, sz, 26.0F, next_random(2.0F, 12.0F)});
      } else if (home_stride % 23U == 0U) {
        m_static_emitters.push_back(StaticEmitter{
            StaticEmitter::Kind::Dog, sx, sz, 34.0F, next_random(4.0F, 16.0F)});
      }
    }
  }

  for (const auto& prop : map_def.world_props) {
    if (prop.type == Game::Map::WorldProp::Type::SupplyCart) {
      m_static_emitters.push_back(StaticEmitter{
          StaticEmitter::Kind::Cart, prop.x, prop.z, 30.0F, next_random(2.0F, 11.0F)});
    }
  }
  if (m_is_city) {
    for (std::size_t i = 0; i < map_def.roads.size(); i += 14U) {
      const auto& road = map_def.roads[i];
      const float rx = (road.start.x() + road.end.x()) * 0.5F;
      const float rz = (road.start.z() + road.end.z()) * 0.5F;
      m_static_emitters.push_back(StaticEmitter{
          StaticEmitter::Kind::Cart, rx, rz, 28.0F, next_random(3.0F, 14.0F)});
    }
  }

  for (const auto& z : m_zones) {
    if (z.day_bed == "ambient.quay_work" ||
        z.day_bed == "ambient.mediterranean_harbor") {
      m_static_emitters.push_back(StaticEmitter{StaticEmitter::Kind::Gull,
                                                z.x,
                                                z.z,
                                                std::max(38.0F, z.radius + 10.0F),
                                                next_random(1.5F, 7.0F)});
      m_static_emitters.push_back(StaticEmitter{StaticEmitter::Kind::WaterSplash,
                                                z.x,
                                                z.z,
                                                std::max(32.0F, z.radius + 6.0F),
                                                next_random(3.0F, 9.0F)});
    } else if (z.day_bed == "ambient.livestock_market") {
      m_static_emitters.push_back(StaticEmitter{StaticEmitter::Kind::Cattle,
                                                z.x,
                                                z.z,
                                                std::max(30.0F, z.radius + 8.0F),
                                                next_random(1.0F, 5.0F)});
      m_static_emitters.push_back(StaticEmitter{StaticEmitter::Kind::Hawker,
                                                z.x + 6.0F,
                                                z.z - 4.0F,
                                                std::max(30.0F, z.radius + 8.0F),
                                                next_random(2.5F, 8.0F)});
    } else if (z.day_bed == "ambient.city_square_crowd" ||
               z.day_bed == "ambient.arena_interior") {
      m_static_emitters.push_back(StaticEmitter{StaticEmitter::Kind::Pigeon,
                                                z.x,
                                                z.z,
                                                std::max(34.0F, z.radius + 10.0F),
                                                next_random(2.0F, 8.0F)});
    } else if (z.day_bed == "ambient.city_garden_day") {
      m_static_emitters.push_back(StaticEmitter{StaticEmitter::Kind::WaterSplash,
                                                z.x,
                                                z.z,
                                                std::max(26.0F, z.radius + 6.0F),
                                                next_random(4.0F, 12.0F)});
      m_static_emitters.push_back(StaticEmitter{StaticEmitter::Kind::Cricket,
                                                z.x,
                                                z.z,
                                                std::max(28.0F, z.radius + 8.0F),
                                                next_random(2.0F, 8.0F)});
    }
  }
}

void CitySoundscape::stop() {
  for (auto& [bed_id, ch] : m_beds) {
    if (ch.playing) {
      AudioSystem::get_instance().stop_sound(bed_id);
      ch.playing = false;
    }
  }
  m_beds.clear();
  m_configured = false;
}

void CitySoundscape::update(float dt,
                            const Engine::Core::World* world,
                            float darkness_amount) {
  if (!m_configured || dt <= 0.0F) {
    return;
  }

  const auto listener = AudioSystem::get_instance().listener();
  if (!listener.valid) {
    return;
  }

  if (!m_night_latched && darkness_amount >= k_night_enter_darkness) {
    m_night_latched = true;
  } else if (m_night_latched && darkness_amount <= k_night_exit_darkness) {
    m_night_latched = false;
  }

  update_beds(dt, listener.position.x, listener.position.z, m_night_latched);
  update_emitters(dt, world, listener.position.x, listener.position.z, darkness_amount);
}

void CitySoundscape::update_beds(float dt, float lx, float lz, bool is_night) {
  for (auto& [_, ch] : m_beds) {
    ch.target_gain = 0.0F;
  }

  struct Candidate {
    std::string bed;
    float weight = 0.0F;
    bool interior = false;
  };
  std::vector<Candidate> hits;
  for (const auto& z : m_zones) {
    const float w = zone_weight(z, lx, lz);
    if (w <= 0.005F) {
      continue;
    }
    const std::string bed = resolve_bed(z.day_bed, z.night_bed, is_night);
    auto it = std::find_if(
        hits.begin(), hits.end(), [&](const Candidate& c) { return c.bed == bed; });
    if (it != hits.end()) {
      it->weight = std::max(it->weight, w);
      it->interior = it->interior || z.interior;
    } else {
      hits.push_back(Candidate{.bed = bed, .weight = w, .interior = z.interior});
    }
  }

  std::sort(hits.begin(), hits.end(), [](const Candidate& a, const Candidate& b) {
    return a.weight > b.weight;
  });
  if (hits.size() > 2U) {
    hits.resize(2U);
  }

  float w1 = !hits.empty() ? hits[0].weight : 0.0F;
  float w2 = hits.size() > 1U ? hits[1].weight : 0.0F;
  const float interior_duck =
      (!hits.empty() && hits[0].interior) ? hits[0].weight : 0.0F;

  if (w1 + w2 > 1.0F) {
    const float sum = w1 + w2;
    w1 /= sum;
    w2 /= sum;
  }

  const std::string base_bed = resolve_bed(m_base_day_bed, m_base_night_bed, is_night);
  const float zone_cover = std::clamp(w1 * 0.88F + w2 * 0.55F, 0.0F, 1.0F);
  const float min_base = interior_duck > 0.5F ? 0.0F : 0.14F;
  const float base_target =
      std::clamp((1.0F - zone_cover) * (1.0F - interior_duck), min_base, 1.0F);

  if (!base_bed.empty() && base_target > 0.01F) {
    m_beds[base_bed].target_gain = std::max(m_beds[base_bed].target_gain, base_target);
  }
  if (!hits.empty() && w1 > 0.01F) {
    m_beds[hits[0].bed].target_gain = std::max(m_beds[hits[0].bed].target_gain, w1);
  }
  if (hits.size() > 1U && w2 > 0.01F) {
    m_beds[hits[1].bed].target_gain = std::max(m_beds[hits[1].bed].target_gain, w2);
  }

  const float max_delta = dt / k_crossfade_seconds;
  for (auto& [bed_id, ch] : m_beds) {
    if (ch.target_gain > ch.current_gain) {
      ch.current_gain = std::min(ch.target_gain, ch.current_gain + max_delta);
    } else if (ch.target_gain < ch.current_gain) {
      ch.current_gain = std::max(ch.target_gain, ch.current_gain - max_delta);
    }

    const float shaped = std::sin(std::clamp(ch.current_gain, 0.0F, 1.0F) *
                                  (0.5F * std::numbers::pi_v<float>));

    if (ch.target_gain > 0.01F && !ch.playing) {
      AudioResourceLoader::ensure_audio_resource_loaded(QString::fromStdString(bed_id));
      const float initial = std::max(0.02F, shaped);
      AudioSystem::get_instance().play_sound(
          bed_id, initial, true, 1, AudioCategory::AMBIENCE);
      ch.playing = true;
      if (CueTrace::logging_enabled()) {
        qInfo().noquote() << QStringLiteral(
                                 "[audio-trace] ambience_zone start bed=%1 at=(%2,%3)")
                                 .arg(QString::fromStdString(bed_id))
                                 .arg(lx, 0, 'f', 1)
                                 .arg(lz, 0, 'f', 1);
      }
    } else if (ch.playing) {
      if (ch.target_gain <= 0.001F && ch.current_gain <= 0.008F) {
        AudioSystem::get_instance().stop_sound(bed_id);
        ch.playing = false;
        ch.current_gain = 0.0F;
        if (CueTrace::logging_enabled()) {
          qInfo().noquote() << QStringLiteral(
                                   "[audio-trace] ambience_zone stop bed=%1 at=(%2,%3)")
                                   .arg(QString::fromStdString(bed_id))
                                   .arg(lx, 0, 'f', 1)
                                   .arg(lz, 0, 'f', 1);
        }
      } else {
        AudioSystem::get_instance().set_playing_sound_volume(bed_id, shaped);
      }
    }
  }
}

void CitySoundscape::update_emitters(float dt,
                                     const Engine::Core::World* world,
                                     float lx,
                                     float lz,
                                     float darkness_amount) {
  for (auto& em : m_static_emitters) {
    em.cooldown = std::max(0.0F, em.cooldown - dt);
  }
  for (auto& [_, cd] : m_entity_cooldowns) {
    cd = std::max(0.0F, cd - dt);
  }

  m_global_emitter_cooldown -= dt;
  if (m_global_emitter_cooldown > 0.0F) {
    return;
  }

  struct PendingCue {
    const char* cue_id = nullptr;
    float x = 0.0F;
    float z = 0.0F;
    float volume = 0.5F;
    float dist_sq = 1.0e9F;
    std::size_t static_idx = static_cast<std::size_t>(-1);
    std::uint64_t entity_id = 0;
    float next_cd = 15.0F;
  };
  PendingCue best;

  const auto consider = [&](const PendingCue& cand) {
    if (cand.cue_id != nullptr && cand.dist_sq < best.dist_sq) {
      best = cand;
    }
  };

  if (world != nullptr) {
    for (auto [id, tr] : world->view<const Engine::Core::TransformComponent>()) {
      const float dx = tr.position.x - lx;
      const float dz = tr.position.z - lz;
      const float d2 = dx * dx + dz * dz;
      if (d2 > 36.0F * 36.0F) {
        continue;
      }
      if (m_entity_cooldowns[id] > 0.0F) {
        continue;
      }

      // 1. Calm grazing sheep within 20 m
      if (const auto* wl = world->try_get<Engine::Core::WildlifeComponent>(id);
          wl != nullptr && wl->species == Game::Wildlife::Species::Sheep &&
          wl->alarm_timer <= 0.0F && d2 <= 20.0F * 20.0F) {
        const char* cue = next_random(0.0F, 1.0F) < 0.22F
                              ? Cue::k_wildlife_lamb_bleat
                              : Cue::k_wildlife_sheep_bleat_calm;
        consider(PendingCue{cue,
                            tr.position.x,
                            tr.position.z,
                            0.52F,
                            d2,
                            static_cast<std::size_t>(-1),
                            id,
                            next_random(10.0F, 20.0F)});
        continue;
      }

      // 2. Idle elephants rumbling / snorting within 32 m
      if (const auto* el = world->try_get<Engine::Core::ElephantComponent>(id);
          el != nullptr &&
          el->charge_state == Engine::Core::ElephantComponent::ChargeState::Idle &&
          !world->has<Engine::Core::ElephantPanicComponent>(id) &&
          d2 <= 32.0F * 32.0F) {
        const char* cue = next_random(0.0F, 1.0F) < 0.65F
                              ? Cue::k_wildlife_elephant_rumble
                              : Cue::k_wildlife_elephant_trunk;
        consider(PendingCue{cue,
                            tr.position.x,
                            tr.position.z,
                            0.56F,
                            d2,
                            static_cast<std::size_t>(-1),
                            id,
                            next_random(11.0F, 22.0F)});
        continue;
      }

      // 3. Barracks smithy / forge hammering tied to barracks activity
      if (const auto* u = world->try_get<Engine::Core::UnitComponent>(id);
          u != nullptr && u->health > 0 &&
          u->spawn_type == Game::Units::SpawnType::Barracks &&
          darkness_amount < 0.60F && d2 <= 34.0F * 34.0F) {
        const auto* prod = world->try_get<Engine::Core::ProductionComponent>(id);
        const bool active =
            prod != nullptr && (prod->in_progress || !prod->production_queue.empty());
        consider(
            PendingCue{Cue::k_city_forge_hammer,
                       tr.position.x,
                       tr.position.z,
                       active ? 0.58F : 0.44F,
                       d2,
                       static_cast<std::size_t>(-1),
                       id,
                       active ? next_random(7.0F, 13.0F) : next_random(18.0F, 32.0F)});
      }
    }
  }

  const bool daytime = darkness_amount < 0.52F;
  for (std::size_t idx = 0; idx < m_static_emitters.size(); ++idx) {
    const auto& em = m_static_emitters[idx];
    if (em.cooldown > 0.0F) {
      continue;
    }
    const float dx = em.x - lx;
    const float dz = em.z - lz;
    const float d2 = dx * dx + dz * dz;
    if (d2 > em.max_range * em.max_range) {
      continue;
    }

    const char* cue = nullptr;
    float vol = 0.46F;
    float cd = 18.0F;
    switch (em.kind) {
    case StaticEmitter::Kind::Hawker:
      if (daytime) {
        cue = Cue::k_city_hawker_call;
        vol = 0.46F;
        cd = next_random(10.0F, 20.0F);
      }
      break;
    case StaticEmitter::Kind::Dog:
      cue = Cue::k_city_dog_bark_distant;
      vol = 0.40F;
      cd = next_random(18.0F, 34.0F);
      break;
    case StaticEmitter::Kind::Door:
      if (darkness_amount < 0.65F) {
        cue = Cue::k_city_door_close;
        vol = 0.45F;
        cd = next_random(14.0F, 28.0F);
      }
      break;
    case StaticEmitter::Kind::Cart:
      if (daytime) {
        cue = Cue::k_city_cart_wheels;
        vol = 0.46F;
        cd = next_random(16.0F, 30.0F);
      }
      break;
    case StaticEmitter::Kind::TempleBell:
      cue = Cue::k_city_temple_bell;
      vol = 0.48F;
      cd = next_random(28.0F, 55.0F);
      break;
    case StaticEmitter::Kind::WaterSplash:
      cue = Cue::k_city_water_splash;
      vol = 0.42F;
      cd = next_random(14.0F, 26.0F);
      break;
    case StaticEmitter::Kind::Gull:
      if (daytime) {
        cue = Cue::k_wildlife_gull;
        vol = 0.44F;
        cd = next_random(11.0F, 22.0F);
      }
      break;
    case StaticEmitter::Kind::Pigeon:
      if (daytime) {
        cue = Cue::k_wildlife_pigeon_flutter;
        vol = 0.46F;
        cd = next_random(13.0F, 25.0F);
      }
      break;
    case StaticEmitter::Kind::Cattle:
      cue = next_random(0.0F, 1.0F) < 0.55F ? Cue::k_wildlife_cattle_low
                                            : Cue::k_wildlife_sheep_bleat_calm;
      vol = 0.48F;
      cd = next_random(11.0F, 22.0F);
      break;
    case StaticEmitter::Kind::Cricket:
      if (!daytime) {
        cue = Cue::k_wildlife_cricket;
        vol = 0.38F;
        cd = next_random(10.0F, 20.0F);
      }
      break;
    }
    if (cue != nullptr) {
      consider(PendingCue{cue, em.x, em.z, vol, d2, idx, 0, cd});
    }
  }

  if (best.cue_id != nullptr) {
    (void)play_cue_at(best.cue_id, WorldPoint{best.x, 0.0F, best.z}, best.volume);
    if (best.static_idx != static_cast<std::size_t>(-1)) {
      m_static_emitters[best.static_idx].cooldown = best.next_cd;
    }
    if (best.entity_id != 0) {
      m_entity_cooldowns[best.entity_id] = best.next_cd;
    }
    m_global_emitter_cooldown =
        next_random(k_global_emitter_min_gap, k_global_emitter_max_gap);
  } else {
    m_global_emitter_cooldown = 0.5F;
  }
}

auto CitySoundscape::classify_footstep_surface(const Engine::Core::World& world,
                                               float world_x,
                                               float world_z) -> FootstepSurface {
  const auto& self = instance();
  if (self.m_configured) {
    for (const auto& [gx, gz] : self.m_gate_positions) {
      if (std::hypot(world_x - gx, world_z - gz) <= 5.5F) {
        return FootstepSurface::StoneInterior;
      }
    }
    for (const auto& z : self.m_zones) {
      if (z.interior && zone_weight(z, world_x, world_z) >= 0.45F) {
        return FootstepSurface::StoneInterior;
      }
    }
  }

  const auto& terrain = Game::Session::session_for(world).terrain();
  const bool on_road = terrain.is_point_on_road(world_x, world_z);

  if (self.m_configured) {
    for (const auto& z : self.m_zones) {
      if (z.garden_or_sand && zone_weight(z, world_x, world_z) >= 0.40F && !on_road) {
        return FootstepSurface::Gravel;
      }
    }
  }

  if (on_road) {
    return FootstepSurface::Stone;
  }

  if (terrain.biome_settings().ground_type == Game::Map::GroundType::SoilRocky) {
    return FootstepSurface::Gravel;
  }

  return FootstepSurface::Grass;
}

auto CitySoundscape::active_bed_ids() const -> std::vector<std::string> {
  std::vector<std::string> ids;
  for (const auto& [id, ch] : m_beds) {
    if (ch.playing) {
      ids.push_back(id);
    }
  }
  std::sort(ids.begin(), ids.end());
  return ids;
}

} // namespace Game::Audio
