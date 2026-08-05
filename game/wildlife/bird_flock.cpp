#include "bird_flock.h"

#include <algorithm>
#include <cmath>

namespace Game::Wildlife {

namespace {

constexpr float k_two_pi = 6.28318530718F;
constexpr float k_think_interval = 0.45F;
constexpr float k_far_think_multiplier = 4.0F;
constexpr float k_arrive_radius = 1.6F;
constexpr float k_flock_spread = 3.4F;
constexpr float k_perch_chance = 0.28F;
constexpr float k_scatter_altitude_gain = 2.1F;
constexpr float k_scatter_distance = 16.0F;
constexpr float k_altitude_rate = 4.5F;
constexpr float k_turn_rate = 5.5F;
constexpr float k_accel = 6.0F;
constexpr float k_perched_phase_rate = 0.9F;
constexpr float k_flight_phase_rate = 5.4F;
constexpr float k_retarget_interval_min = 4.0F;
constexpr float k_retarget_interval_max = 9.0F;

constexpr float k_flyover_stage_margin = 24.0F;
constexpr float k_flyover_entry_lead = 1.10F;
constexpr float k_flyover_min_span = 70.0F;
constexpr float k_flyover_lateral_spread = 14.0F;
constexpr float k_flyover_column_spacing = 1.6F;
constexpr float k_flyover_wing_offset = 1.15F;
constexpr float k_flyover_timeout_seconds = 150.0F;
constexpr float k_flyover_first_arm_scale = 0.55F;
constexpr float k_flyover_exit_overshoot = 18.0F;
constexpr float k_flyover_aim_jitter = 3.2F;

auto next_random(std::uint32_t& state) -> float {
  state = (state * 1664525U) + 1013904223U;
  return static_cast<float>((state >> 8U) & 0xFFFFFFU) / 16777216.0F;
}

auto random_range(std::uint32_t& state, float low, float high) -> float {
  return low + ((high - low) * next_random(state));
}

auto wrap_angle(float radians) -> float {
  while (radians > 3.14159265F) {
    radians -= k_two_pi;
  }
  while (radians < -3.14159265F) {
    radians += k_two_pi;
  }
  return radians;
}

auto approach(float current, float target, float max_delta) -> float {
  float const delta = std::clamp(target - current, -max_delta, max_delta);
  return current + delta;
}

void aim_along_flyover(Bird& bird, const Flock& flock) {
  float const ahead = k_flyover_exit_overshoot - bird.slot_trail;
  float const across =
      bird.slot_lateral +
      random_range(bird.rng_state, -k_flyover_aim_jitter, k_flyover_aim_jitter);
  bird.target_x =
      flock.target_x + (flock.heading_x * ahead) - (flock.heading_z * across);
  bird.target_z =
      flock.target_z + (flock.heading_z * ahead) + (flock.heading_x * across);
}

} // namespace

BirdFlockManager::BirdFlockManager()
    : m_terrain(&terrain_service_probe()) {
}

auto BirdFlockManager::instance() -> BirdFlockManager& {
  static BirdFlockManager manager;
  return manager;
}

void BirdFlockManager::set_terrain_probe(ITerrainProbe* probe) noexcept {
  m_terrain = probe != nullptr ? probe : &terrain_service_probe();
}

void BirdFlockManager::configure(const SpeciesConfig& config,
                                 std::uint32_t seed,
                                 float near_simulation_radius,
                                 float far_simulation_radius) {
  m_config = config;
  m_seed = seed == 0U ? 1U : seed;
  m_near_radius = std::max(1.0F, near_simulation_radius);
  m_far_radius = std::max(m_near_radius + 1.0F, far_simulation_radius);
  m_enabled = config.enabled && config.group_count > 0;
  reset();
  if (m_enabled) {
    spawn_population();
  }
  publish_frame();
}

void BirdFlockManager::reset() {
  m_population.birds.clear();
  m_population.flocks.clear();
  m_stats.reset();
  publish_frame();
}

void BirdFlockManager::set_focus(float world_x, float world_z) noexcept {
  m_focus_x = world_x;
  m_focus_z = world_z;
  m_has_focus = true;
}

void BirdFlockManager::clear_focus() noexcept {
  m_has_focus = false;
}

auto BirdFlockManager::ground_height(float world_x, float world_z) const -> float {
  return m_terrain != nullptr ? m_terrain->ground_height(world_x, world_z) : 0.0F;
}

void BirdFlockManager::clamp_to_bounds(float& world_x, float& world_z) const {
  if (m_terrain == nullptr) {
    return;
  }
  WorldBounds const bounds = m_terrain->bounds();
  world_x = std::clamp(world_x, bounds.min_x, bounds.max_x);
  world_z = std::clamp(world_z, bounds.min_z, bounds.max_z);
}

void BirdFlockManager::spawn_population() {
  m_population.flocks.reserve(static_cast<std::size_t>(m_config.group_count));
  for (int index = 0; index < m_config.group_count; ++index) {
    spawn_flock(static_cast<std::uint16_t>(index));
  }
}

void BirdFlockManager::spawn_flock(std::uint16_t index) {
  std::uint32_t rng = m_seed + (static_cast<std::uint32_t>(index) * 2654435761U) + 17U;

  Flock flock;
  flock.rng_state = rng | 1U;
  flock.roam_radius = m_config.roam_radius;

  if (m_config.is_flyover()) {
    arm_flyover(flock, true);
    m_population.flocks.push_back(flock);
    return;
  }

  if (!m_config.spawn_areas.empty()) {
    auto const area_index = static_cast<std::size_t>(
        next_random(rng) * static_cast<float>(m_config.spawn_areas.size()));
    const auto& area =
        m_config.spawn_areas[std::min(area_index, m_config.spawn_areas.size() - 1U)];
    float const angle = random_range(rng, 0.0F, k_two_pi);
    float const reach = area.radius * std::sqrt(next_random(rng));
    flock.home_x = area.x + (std::cos(angle) * reach);
    flock.home_z = area.z + (std::sin(angle) * reach);
  } else if (m_terrain != nullptr) {
    WorldBounds const bounds = m_terrain->bounds();
    flock.home_x = random_range(rng, bounds.min_x, bounds.max_x);
    flock.home_z = random_range(rng, bounds.min_z, bounds.max_z);
  }
  clamp_to_bounds(flock.home_x, flock.home_z);
  flock.target_x = flock.home_x;
  flock.target_z = flock.home_z;
  flock.retarget_timer =
      random_range(rng, k_retarget_interval_min, k_retarget_interval_max);

  int const size = m_config.clamped_group_size(
      static_cast<std::uint32_t>(next_random(rng) * 1024.0F));
  auto const flock_index = static_cast<std::uint16_t>(m_population.flocks.size());
  m_population.flocks.push_back(flock);

  for (int member = 0; member < size; ++member) {
    Bird bird;
    bird.rng_state = (rng + (static_cast<std::uint32_t>(member) * 2246822519U)) | 1U;
    float const angle = random_range(bird.rng_state, 0.0F, k_two_pi);
    float const reach = random_range(bird.rng_state, 0.0F, k_flock_spread);
    bird.x = flock.home_x + (std::cos(angle) * reach);
    bird.z = flock.home_z + (std::sin(angle) * reach);
    clamp_to_bounds(bird.x, bird.z);
    bird.altitude = random_range(
        bird.rng_state, m_config.flight_height * 0.6F, m_config.flight_height * 1.25F);
    bird.target_altitude = bird.altitude;
    bird.y = ground_height(bird.x, bird.z) + bird.altitude;
    bird.yaw = random_range(bird.rng_state, 0.0F, k_two_pi);
    bird.target_x = bird.x;
    bird.target_z = bird.z;
    bird.behavior = Behavior::Cruise;
    bird.flock = flock_index;
    bird.phase = next_random(bird.rng_state);
    bird.think_timer = random_range(bird.rng_state, 0.0F, k_think_interval);
    bird.tint = static_cast<std::uint8_t>(bird.rng_state % 3U);
    bird.speed = m_config.move_speed * 0.5F;
    m_population.birds.push_back(bird);
  }
}

void BirdFlockManager::arm_flyover(Flock& flock, bool first_arm) {
  float const low = std::max(1.0F, m_config.flyover_interval_min);
  float const high = std::max(low, m_config.flyover_interval_max);
  float wait = random_range(flock.rng_state, low, high);
  if (first_arm) {
    wait *= k_flyover_first_arm_scale;
  }
  flock.respite_timer = wait;
  flock.airborne = false;
  flock.airborne_seconds = 0.0F;
  flock.alarm_timer = 0.0F;
}

auto BirdFlockManager::flyover_span() const -> float {
  float span = m_near_radius * 2.0F * k_flyover_entry_lead;
  if (m_terrain != nullptr) {
    WorldBounds const bounds = m_terrain->bounds();
    float const diagonal =
        std::hypot(bounds.max_x - bounds.min_x, bounds.max_z - bounds.min_z);
    span = std::min(span, diagonal + (k_flyover_stage_margin * 2.0F));
  }
  return std::max(k_flyover_min_span, span);
}

void BirdFlockManager::launch_flyover(Flock& flock, std::uint16_t index) {
  float const angle = random_range(flock.rng_state, 0.0F, k_two_pi);
  flock.heading_x = std::cos(angle);
  flock.heading_z = std::sin(angle);

  float center_x = 0.0F;
  float center_z = 0.0F;
  if (m_has_focus) {
    center_x = m_focus_x;
    center_z = m_focus_z;
  } else if (m_terrain != nullptr) {
    WorldBounds const bounds = m_terrain->bounds();
    center_x = (bounds.min_x + bounds.max_x) * 0.5F;
    center_z = (bounds.min_z + bounds.max_z) * 0.5F;
  }

  float const lateral = random_range(
      flock.rng_state, -k_flyover_lateral_spread, k_flyover_lateral_spread);
  center_x += -flock.heading_z * lateral;
  center_z += flock.heading_x * lateral;

  float const half_span = flyover_span() * 0.5F;
  flock.home_x = center_x - (flock.heading_x * half_span);
  flock.home_z = center_z - (flock.heading_z * half_span);
  flock.target_x = center_x + (flock.heading_x * half_span);
  flock.target_z = center_z + (flock.heading_z * half_span);
  flock.airborne = true;
  flock.airborne_seconds = 0.0F;
  flock.retarget_timer = 0.0F;

  int const size = m_config.clamped_group_size(
      static_cast<std::uint32_t>(next_random(flock.rng_state) * 1024.0F));
  float const lead_yaw = std::atan2(flock.heading_x, flock.heading_z);

  for (int member = 0; member < size; ++member) {
    Bird bird;
    bird.rng_state =
        (flock.rng_state + (static_cast<std::uint32_t>(member) * 2246822519U)) | 1U;

    auto const rank = static_cast<float>((member + 1) / 2);
    float const side = ((member % 2) == 0) ? -1.0F : 1.0F;
    bird.slot_trail =
        (rank * k_flyover_column_spacing) + random_range(bird.rng_state, -0.7F, 0.7F);
    bird.slot_lateral = (rank * k_flyover_wing_offset * side) +
                        random_range(bird.rng_state, -0.6F, 0.6F);

    bird.x = flock.home_x - (flock.heading_x * bird.slot_trail) -
             (flock.heading_z * bird.slot_lateral);
    bird.z = flock.home_z - (flock.heading_z * bird.slot_trail) +
             (flock.heading_x * bird.slot_lateral);
    bird.altitude = random_range(
        bird.rng_state, m_config.flight_height * 0.9F, m_config.flight_height * 1.45F);
    bird.target_altitude = bird.altitude;
    bird.y = ground_height(bird.x, bird.z) + bird.altitude;
    bird.yaw = lead_yaw;
    bird.behavior = Behavior::Cruise;
    bird.flock = index;
    bird.phase = next_random(bird.rng_state);
    bird.think_timer = random_range(bird.rng_state, 0.0F, k_think_interval);
    bird.tint = static_cast<std::uint8_t>(bird.rng_state % 3U);
    bird.speed = m_config.move_speed;
    bird.tier = BirdTier::Near;
    aim_along_flyover(bird, flock);
    m_population.birds.push_back(bird);
  }

  m_stats.flyovers_launched += 1U;
}

void BirdFlockManager::update_flyover(Flock& flock,
                                      std::uint16_t index,
                                      float delta_time) {
  flock.airborne_seconds += delta_time;

  float const span_x = flock.target_x - flock.home_x;
  float const span_z = flock.target_z - flock.home_z;
  float const span = std::sqrt((span_x * span_x) + (span_z * span_z));

  bool still_crossing = false;
  for (const auto& bird : m_population.birds) {
    if (bird.flock != index) {
      continue;
    }
    float const travelled = ((bird.x - flock.home_x) * flock.heading_x) +
                            ((bird.z - flock.home_z) * flock.heading_z);
    if (travelled < span) {
      still_crossing = true;
      break;
    }
  }

  if (!still_crossing || flock.airborne_seconds >= k_flyover_timeout_seconds) {
    land_flyover(flock, index);
  }
}

void BirdFlockManager::land_flyover(Flock& flock, std::uint16_t index) {
  std::erase_if(m_population.birds,
                [index](const Bird& bird) { return bird.flock == index; });
  arm_flyover(flock, false);
  m_stats.flyovers_finished += 1U;
}

auto BirdFlockManager::tier_for(float world_x,
                                float world_z) const noexcept -> BirdTier {
  if (!m_has_focus) {
    return BirdTier::Near;
  }
  float const dx = world_x - m_focus_x;
  float const dz = world_z - m_focus_z;
  float const distance_sq = (dx * dx) + (dz * dz);
  if (distance_sq <= m_near_radius * m_near_radius) {
    return BirdTier::Near;
  }
  if (distance_sq <= m_far_radius * m_far_radius) {
    return BirdTier::Far;
  }
  return BirdTier::Dormant;
}

void BirdFlockManager::update(float delta_time, const ThreatField& threats) {
  if (!m_enabled || delta_time <= 0.0F) {
    return;
  }

  for (std::size_t i = 0; i < m_population.flocks.size(); ++i) {
    auto& flock = m_population.flocks[i];
    if (!m_config.is_flyover()) {
      update_flock(flock, delta_time);
      continue;
    }

    auto const index = static_cast<std::uint16_t>(i);
    flock.alarm_timer = std::max(0.0F, flock.alarm_timer - delta_time);
    if (flock.airborne) {
      update_flyover(flock, index, delta_time);
      continue;
    }

    flock.respite_timer -= delta_time;
    if (flock.respite_timer <= 0.0F) {
      launch_flyover(flock, index);
    }
  }

  for (auto& bird : m_population.birds) {
    if (bird.flock >= m_population.flocks.size()) {
      continue;
    }
    Flock& flock = m_population.flocks[bird.flock];
    bird.tier = tier_for(bird.x, bird.z);
    if (bird.tier == BirdTier::Dormant) {
      if (!m_config.is_flyover()) {
        m_stats.dormant_skips += 1U;
        continue;
      }
      bird.tier = BirdTier::Far;
    }

    float const multiplier = bird.tier == BirdTier::Far ? k_far_think_multiplier : 1.0F;
    bird.think_timer -= delta_time;
    bird.state_timer = std::max(0.0F, bird.state_timer - delta_time);
    if (bird.think_timer <= 0.0F) {
      bird.think_timer = k_think_interval * multiplier;
      think(bird, flock, threats);
      if (bird.tier == BirdTier::Near) {
        m_stats.near_updates += 1U;
      } else {
        m_stats.far_updates += 1U;
      }
    }
    update_bird(bird, flock, delta_time);
  }

  publish_frame();
}

void BirdFlockManager::update_flock(Flock& flock, float delta_time) {
  flock.alarm_timer = std::max(0.0F, flock.alarm_timer - delta_time);
  flock.retarget_timer -= delta_time;
  if (flock.retarget_timer > 0.0F) {
    return;
  }
  flock.retarget_timer =
      random_range(flock.rng_state, k_retarget_interval_min, k_retarget_interval_max);
  float const angle = random_range(flock.rng_state, 0.0F, k_two_pi);
  float const reach = flock.roam_radius * std::sqrt(next_random(flock.rng_state));
  flock.target_x = flock.home_x + (std::cos(angle) * reach);
  flock.target_z = flock.home_z + (std::sin(angle) * reach);
  clamp_to_bounds(flock.target_x, flock.target_z);
}

void BirdFlockManager::think(Bird& bird, Flock& flock, const ThreatField& threats) {
  ThreatQuery const threat =
      threats.nearest(bird.x, bird.z, m_config.alert_radius, false);
  if (threat.found) {
    if (bird.behavior != Behavior::Scatter) {
      m_stats.scatter_events += 1U;
    }
    bird.behavior = Behavior::Scatter;
    bird.state_timer = random_range(bird.rng_state, 2.0F, 3.6F);
    flock.alarm_timer = std::max(flock.alarm_timer, bird.state_timer);

    float const dx = bird.x - threat.x;
    float const dz = bird.z - threat.z;
    float const length = std::sqrt((dx * dx) + (dz * dz));
    float const away_x = length > 0.001F ? dx / length : 1.0F;
    float const away_z = length > 0.001F ? dz / length : 0.0F;
    float const spread = random_range(bird.rng_state, -0.6F, 0.6F);
    bird.target_x = bird.x + ((away_x - (away_z * spread)) * k_scatter_distance);
    bird.target_z = bird.z + ((away_z + (away_x * spread)) * k_scatter_distance);
    if (!m_config.is_flyover()) {
      clamp_to_bounds(bird.target_x, bird.target_z);
    }
    bird.target_altitude = m_config.flight_height * k_scatter_altitude_gain;
    return;
  }

  if (bird.behavior == Behavior::Scatter && bird.state_timer > 0.0F) {
    return;
  }

  if (m_config.is_flyover()) {
    bird.behavior = Behavior::Cruise;
    aim_along_flyover(bird, flock);
    bird.target_altitude = random_range(
        bird.rng_state, m_config.flight_height * 0.9F, m_config.flight_height * 1.45F);
    return;
  }

  if (bird.behavior == Behavior::Graze) {
    if (bird.state_timer > 0.0F && flock.alarm_timer <= 0.0F) {
      return;
    }
    bird.behavior = Behavior::Cruise;
  }

  bool const may_perch =
      flock.alarm_timer <= 0.0F && next_random(bird.rng_state) < k_perch_chance;
  if (may_perch && bird.behavior == Behavior::Cruise) {
    bird.behavior = Behavior::Graze;
    bird.state_timer = random_range(bird.rng_state, 4.0F, 9.5F);
    bird.target_x = bird.x;
    bird.target_z = bird.z;
    bird.target_altitude = 0.0F;
    return;
  }

  bird.behavior = Behavior::Cruise;
  float const angle = random_range(bird.rng_state, 0.0F, k_two_pi);
  float const reach = random_range(bird.rng_state, 0.0F, k_flock_spread);
  bird.target_x = flock.target_x + (std::cos(angle) * reach);
  bird.target_z = flock.target_z + (std::sin(angle) * reach);
  clamp_to_bounds(bird.target_x, bird.target_z);
  bird.target_altitude = random_range(
      bird.rng_state, m_config.flight_height * 0.75F, m_config.flight_height * 1.3F);
}

void BirdFlockManager::update_bird(Bird& bird, Flock& flock, float delta_time) {
  (void)flock;
  integrate(bird, delta_time);

  float const phase_rate =
      bird.behavior == Behavior::Graze
          ? k_perched_phase_rate
          : k_flight_phase_rate * (bird.behavior == Behavior::Scatter ? 1.5F : 1.0F);
  bird.phase += delta_time * phase_rate;
  bird.phase -= std::floor(bird.phase);
}

void BirdFlockManager::integrate(Bird& bird, float delta_time) {
  float const dx = bird.target_x - bird.x;
  float const dz = bird.target_z - bird.z;
  float const distance = std::sqrt((dx * dx) + (dz * dz));

  float desired_speed = 0.0F;
  if (bird.behavior == Behavior::Scatter) {
    desired_speed = m_config.flee_speed;
  } else if (bird.behavior == Behavior::Cruise) {
    desired_speed = m_config.move_speed;
  }
  if (distance <= k_arrive_radius && bird.behavior != Behavior::Scatter) {
    desired_speed = 0.0F;
    if (bird.behavior == Behavior::Cruise) {
      bird.think_timer = std::min(bird.think_timer, 0.05F);
    }
  }

  bird.speed = approach(bird.speed, desired_speed, k_accel * delta_time);

  if (distance > 0.001F && bird.speed > 0.001F) {
    float const step = std::min(bird.speed * delta_time, distance);
    bird.x += (dx / distance) * step;
    bird.z += (dz / distance) * step;
    float const desired_yaw = std::atan2(dx, dz);
    bird.yaw +=
        wrap_angle(desired_yaw - bird.yaw) * std::min(1.0F, k_turn_rate * delta_time);
    bird.yaw = wrap_angle(bird.yaw);
  }

  if (!m_config.is_flyover()) {
    clamp_to_bounds(bird.x, bird.z);
  }
  bird.altitude =
      approach(bird.altitude, bird.target_altitude, k_altitude_rate * delta_time);
  bird.y = ground_height(bird.x, bird.z) + bird.altitude;
}

void BirdFlockManager::publish_frame() {
  auto frame = std::make_shared<BirdFrame>();
  frame->birds = m_population.birds;
  frame->revision = ++m_revision;

  std::lock_guard<std::mutex> const guard(m_frame_mutex);
  m_published_frame = std::move(frame);
}

auto BirdFlockManager::frame() const -> std::shared_ptr<const BirdFrame> {
  std::lock_guard<std::mutex> const guard(m_frame_mutex);
  return m_published_frame;
}

auto BirdFlockManager::snapshot() const -> BirdPopulation {
  return m_population;
}

void BirdFlockManager::restore(const BirdPopulation& population) {
  m_population = population;
  for (auto& bird : m_population.birds) {
    if (bird.rng_state == 0U) {
      bird.rng_state = 1U;
    }
  }
  for (auto& flock : m_population.flocks) {
    if (flock.rng_state == 0U) {
      flock.rng_state = 1U;
    }
  }
  publish_frame();
}

} // namespace Game::Wildlife
