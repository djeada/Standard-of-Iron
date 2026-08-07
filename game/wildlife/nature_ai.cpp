#include "nature_ai.h"

#include <algorithm>
#include <cmath>

#include "../core/component.h"
#include "../core/entity.h"
#include "wildlife_config.h"

namespace Game::Wildlife {

namespace {

constexpr float k_two_pi = 6.28318530718F;
constexpr float k_herd_cohesion_radius = 3.6F;
constexpr float k_herd_max_spread = 7.5F;
constexpr float k_pack_cohesion_radius = 5.5F;
constexpr float k_flee_distance_min = 7.0F;
constexpr float k_flee_distance_max = 11.0F;
constexpr float k_graze_chance = 0.55F;
constexpr float k_graze_dwell_min = 3.0F;
constexpr float k_graze_dwell_max = 8.5F;
constexpr float k_alarm_duration = 3.5F;
constexpr float k_wolf_detect_multiplier = 1.4F;
constexpr float k_wolf_bite_range = 1.35F;
constexpr float k_wolf_defend_leash_multiplier = 2.2F;
constexpr float k_livestock_preference = 0.7F;
constexpr float k_pack_approach_spacing = 1.1F;
constexpr float k_wolf_avoid_base_strength = 2.5F;
constexpr float k_civilian_standoff = 3.0F;
constexpr float k_sheep_flee_strength_threshold = 0.5F;

auto next_random(std::uint32_t& state) -> float {
  state = (state * 1664525U) + 1013904223U;
  return static_cast<float>((state >> 8U) & 0xFFFFFFU) / 16777216.0F;
}

auto random_range(std::uint32_t& state, float low, float high) -> float {
  return low + ((high - low) * next_random(state));
}

auto wolf_avoid_threshold(const SpeciesConfig& config) -> float {
  return k_wolf_avoid_base_strength * (0.6F + (config.aggression * 1.4F));
}

void bolt_away_from(const NatureContext& ctx,
                    NatureActions& actions,
                    float threat_x,
                    float threat_z,
                    float scatter_radius) {
  auto& wildlife = *ctx.wildlife;
  float const away_x = ctx.x - threat_x;
  float const away_z = ctx.z - threat_z;
  float const length = std::sqrt((away_x * away_x) + (away_z * away_z));
  float const dir_x = length > 0.001F ? away_x / length : 1.0F;
  float const dir_z = length > 0.001F ? away_z / length : 0.0F;
  float const distance =
      random_range(wildlife.rng_state, k_flee_distance_min, k_flee_distance_max);

  float target_x = ctx.x + (dir_x * distance);
  float target_z = ctx.z + (dir_z * distance);
  float open_x = target_x;
  float open_z = target_z;
  if (actions.pick_open_point(wildlife.rng_state,
                              target_x,
                              target_z,
                              0.0F,
                              scatter_radius,
                              open_x,
                              open_z)) {
    target_x = open_x;
    target_z = open_z;
  }

  wildlife.target_x = target_x;
  wildlife.target_z = target_z;
  actions.set_travel_speed(ctx, true);
  actions.move_to(ctx, target_x, target_z);
}

auto distance_to(const NatureContext& ctx, const PreyRef& prey) -> float {
  float const dx = prey.x - ctx.x;
  float const dz = prey.z - ctx.z;
  return std::sqrt((dx * dx) + (dz * dz));
}

auto bite_reach(const PreyRef& prey) -> float {
  return k_wolf_bite_range + prey.radius;
}

auto overwhelmed(const NatureContext& ctx) -> bool {
  return ctx.threats->strength_within(ctx.x, ctx.z, ctx.config->alert_radius) >=
         wolf_avoid_threshold(*ctx.config);
}

auto guarded(const NatureContext& ctx, const PreyRef& prey) -> bool {
  return ctx.threats->strength_within(prey.x, prey.z, ctx.config->alert_radius) >=
         wolf_avoid_threshold(*ctx.config);
}

void close_and_bite(const NatureContext& ctx,
                    NatureActions& actions,
                    const PreyRef& prey) {
  auto& wildlife = *ctx.wildlife;
  wildlife.behavior = Behavior::Stalk;
  wildlife.focus_id = prey.id;
  actions.set_travel_speed(ctx, true);

  if (distance_to(ctx, prey) <= bite_reach(prey)) {
    actions.halt(ctx);
    if (actions.bite(ctx, prey)) {
      actions.note(NatureEvent::Bite);
    }
    return;
  }

  float const approach_angle =
      static_cast<float>((wildlife.rng_state >> 8U) & 0xFFFFU) * (k_two_pi / 65536.0F);
  float const spacing = k_pack_approach_spacing + prey.radius;
  float const approach_x = prey.x + (std::cos(approach_angle) * spacing);
  float const approach_z = prey.z + (std::sin(approach_angle) * spacing);
  wildlife.target_x = approach_x;
  wildlife.target_z = approach_z;
  actions.move_to(ctx, approach_x, approach_z);
}

auto wander_anchor(const NatureContext& ctx, float& anchor_x, float& anchor_z) -> void {
  const auto& wildlife = *ctx.wildlife;
  anchor_x = ctx.herd.center_x;
  anchor_z = ctx.herd.center_z;
  float const home_dx = anchor_x - wildlife.home_x;
  float const home_dz = anchor_z - wildlife.home_z;
  if ((home_dx * home_dx) + (home_dz * home_dz) >
      wildlife.roam_radius * wildlife.roam_radius) {
    anchor_x = wildlife.home_x;
    anchor_z = wildlife.home_z;
  }
}

class SheepFleeBehavior : public NatureBehavior {
public:
  [[nodiscard]] auto name() const noexcept -> std::string_view override {
    return "sheep.flee";
  }
  [[nodiscard]] auto priority() const noexcept -> NaturePriority override {
    return NaturePriority::Survival;
  }

  auto try_run(const NatureContext& ctx, NatureActions& actions) -> bool override {
    ThreatQuery threat = wounding_attacker(ctx, actions);
    if (!threat.found) {
      threat = ctx.threats->nearest(ctx.x, ctx.z, ctx.config->alert_radius);
      if (threat.found && threat.strength < k_sheep_flee_strength_threshold) {
        threat.found = false;
      }
    }
    if (!threat.found) {
      threat = actions.nearest_pack_hunter(ctx.x, ctx.z, ctx.config->alert_radius);
    }
    if (!threat.found) {
      return false;
    }

    auto& wildlife = *ctx.wildlife;
    if (wildlife.behavior != Behavior::Flee) {
      actions.note(NatureEvent::Flee);
    }
    wildlife.behavior = Behavior::Flee;
    wildlife.state_timer = k_alarm_duration;
    actions.alert_herd(wildlife.group_id, k_alarm_duration);
    bolt_away_from(ctx, actions, threat.x, threat.z, 2.5F);
    return true;
  }

private:
  static auto wounding_attacker(const NatureContext& ctx,
                                NatureActions& actions) -> ThreatQuery {
    auto& wildlife = *ctx.wildlife;
    if (wildlife.hostile_timer <= 0.0F || wildlife.aggressor_id == 0) {
      return {};
    }
    PreyRef const foe = actions.locate(wildlife.aggressor_id);
    if (!foe.valid()) {
      wildlife.aggressor_id = 0;
      wildlife.hostile_timer = 0.0F;
      return {};
    }

    ThreatQuery out;
    out.found = true;
    out.x = foe.x;
    out.z = foe.z;
    out.distance = distance_to(ctx, foe);
    out.strength = 1.0F;
    return out;
  }
};

class HerdCohesionBehavior : public NatureBehavior {
public:
  [[nodiscard]] auto name() const noexcept -> std::string_view override {
    return "sheep.regroup";
  }
  [[nodiscard]] auto priority() const noexcept -> NaturePriority override {
    return NaturePriority::Interest;
  }

  auto try_run(const NatureContext& ctx, NatureActions& actions) -> bool override {
    auto& wildlife = *ctx.wildlife;
    float const to_center_x = ctx.herd.center_x - ctx.x;
    float const to_center_z = ctx.herd.center_z - ctx.z;
    float const spread =
        std::sqrt((to_center_x * to_center_x) + (to_center_z * to_center_z));
    if (wildlife.alarm_timer <= 0.0F && spread <= k_herd_max_spread) {
      return false;
    }

    actions.set_travel_speed(ctx, false);
    wildlife.behavior = Behavior::Roam;
    float open_x = ctx.herd.center_x;
    float open_z = ctx.herd.center_z;
    if (!actions.pick_open_point(wildlife.rng_state,
                                 ctx.herd.center_x,
                                 ctx.herd.center_z,
                                 0.0F,
                                 k_herd_cohesion_radius,
                                 open_x,
                                 open_z)) {
      open_x = ctx.herd.center_x;
      open_z = ctx.herd.center_z;
    }
    wildlife.target_x = open_x;
    wildlife.target_z = open_z;
    actions.move_to(ctx, open_x, open_z);
    return true;
  }
};

class GrazeBehavior : public NatureBehavior {
public:
  [[nodiscard]] auto name() const noexcept -> std::string_view override {
    return "sheep.graze";
  }
  [[nodiscard]] auto priority() const noexcept -> NaturePriority override {
    return NaturePriority::Routine;
  }

  auto try_run(const NatureContext& ctx, NatureActions& actions) -> bool override {
    auto& wildlife = *ctx.wildlife;
    actions.set_travel_speed(ctx, false);

    if (wildlife.behavior == Behavior::Graze && wildlife.state_timer > 0.0F) {
      return true;
    }
    if (next_random(wildlife.rng_state) >= k_graze_chance) {
      return false;
    }

    wildlife.behavior = Behavior::Graze;
    wildlife.state_timer =
        random_range(wildlife.rng_state, k_graze_dwell_min, k_graze_dwell_max);
    actions.halt(ctx);
    return true;
  }
};

class DriftBehavior : public NatureBehavior {
public:
  DriftBehavior(std::string_view name, float min_radius, float max_radius)
      : m_name(name)
      , m_min_radius(min_radius)
      , m_max_radius(max_radius) {}

  [[nodiscard]] auto name() const noexcept -> std::string_view override {
    return m_name;
  }
  [[nodiscard]] auto priority() const noexcept -> NaturePriority override {
    return NaturePriority::Ambient;
  }

  auto try_run(const NatureContext& ctx, NatureActions& actions) -> bool override {
    auto& wildlife = *ctx.wildlife;
    wildlife.behavior = Behavior::Roam;
    wildlife.focus_id = 0;
    actions.set_travel_speed(ctx, false);

    float anchor_x = 0.0F;
    float anchor_z = 0.0F;
    wander_anchor(ctx, anchor_x, anchor_z);

    float open_x = anchor_x;
    float open_z = anchor_z;
    if (!actions.pick_open_point(wildlife.rng_state,
                                 anchor_x,
                                 anchor_z,
                                 m_min_radius,
                                 m_max_radius,
                                 open_x,
                                 open_z)) {
      return true;
    }
    wildlife.target_x = open_x;
    wildlife.target_z = open_z;
    actions.move_to(ctx, open_x, open_z);
    return true;
  }

private:
  std::string_view m_name;
  float m_min_radius;
  float m_max_radius;
};

class WolfRetreatBehavior : public NatureBehavior {
public:
  [[nodiscard]] auto name() const noexcept -> std::string_view override {
    return "wolf.retreat";
  }
  [[nodiscard]] auto priority() const noexcept -> NaturePriority override {
    return NaturePriority::Survival;
  }

  auto try_run(const NatureContext& ctx, NatureActions& actions) -> bool override {
    if (!overwhelmed(ctx)) {
      return false;
    }
    ThreatQuery const threat =
        ctx.threats->nearest(ctx.x, ctx.z, ctx.config->alert_radius);
    if (!threat.found) {
      return false;
    }

    auto& wildlife = *ctx.wildlife;
    wildlife.behavior = Behavior::Flee;
    wildlife.state_timer = k_alarm_duration;
    bolt_away_from(ctx, actions, threat.x, threat.z, 3.0F);
    return true;
  }
};

class DefendBehavior : public NatureBehavior {
public:
  [[nodiscard]] auto name() const noexcept -> std::string_view override {
    return "wolf.defend";
  }
  [[nodiscard]] auto priority() const noexcept -> NaturePriority override {
    return NaturePriority::Survival;
  }

  auto try_run(const NatureContext& ctx, NatureActions& actions) -> bool override {
    auto& wildlife = *ctx.wildlife;
    if (wildlife.hostile_timer <= 0.0F || wildlife.aggressor_id == 0) {
      return false;
    }
    if (overwhelmed(ctx)) {
      return false;
    }

    PreyRef const foe = actions.locate(wildlife.aggressor_id);
    if (!foe.valid()) {
      wildlife.aggressor_id = 0;
      wildlife.hostile_timer = 0.0F;
      return false;
    }
    float const leash = ctx.config->roam_radius * k_wolf_defend_leash_multiplier;
    if (distance_to(ctx, foe) > leash) {
      return false;
    }

    if (wildlife.behavior != Behavior::Stalk) {
      actions.note(NatureEvent::Hunt);
    }
    actions.mark_hostile(ctx, foe.id, true);
    close_and_bite(ctx, actions, foe);
    return true;
  }
};

class StalkPreyBehavior : public NatureBehavior {
public:
  [[nodiscard]] auto name() const noexcept -> std::string_view override {
    return "wolf.stalk";
  }
  [[nodiscard]] auto priority() const noexcept -> NaturePriority override {
    return NaturePriority::Interest;
  }

  auto try_run(const NatureContext& ctx, NatureActions& actions) -> bool override {
    if (ctx.config->aggression <= 0.0F) {
      return false;
    }
    float const detect_radius = ctx.config->roam_radius * k_wolf_detect_multiplier;
    PreyRef const prey = pick_prey(ctx, actions, detect_radius);
    if (!prey.valid()) {
      return false;
    }

    auto& wildlife = *ctx.wildlife;
    if (wildlife.behavior != Behavior::Stalk) {
      actions.note(NatureEvent::Hunt);
    }
    if (!prey.livestock) {
      actions.mark_hostile(ctx, prey.id, true);
    }
    close_and_bite(ctx, actions, prey);
    return true;
  }

private:
  static auto pick_prey(const NatureContext& ctx,
                        NatureActions& actions,
                        float detect_radius) -> PreyRef {
    PreyRef const livestock = actions.nearest_prey(ctx.x, ctx.z, detect_radius);
    PreyRef const quarry = actions.nearest_quarry(ctx.x, ctx.z, detect_radius);
    bool const prefer_livestock =
        livestock.valid() &&
        (!quarry.valid() || (distance_to(ctx, livestock) * k_livestock_preference) <=
                                distance_to(ctx, quarry));

    PreyRef const first = prefer_livestock ? livestock : quarry;
    PreyRef const second = prefer_livestock ? quarry : livestock;
    if (first.valid() && !guarded(ctx, first)) {
      return first;
    }
    if (second.valid() && !guarded(ctx, second)) {
      return second;
    }
    return {};
  }
};

class MenaceBehavior : public NatureBehavior {
public:
  [[nodiscard]] auto name() const noexcept -> std::string_view override {
    return "wolf.menace";
  }
  [[nodiscard]] auto priority() const noexcept -> NaturePriority override {
    return NaturePriority::Routine;
  }

  auto try_run(const NatureContext& ctx, NatureActions& actions) -> bool override {
    if (ctx.config->aggression < 0.5F) {
      return false;
    }
    float const detect_radius = ctx.config->roam_radius * k_wolf_detect_multiplier;
    ThreatQuery const civilian =
        ctx.threats->nearest(ctx.x, ctx.z, detect_radius, true);
    if (!civilian.found || civilian.distance <= k_civilian_standoff) {
      return false;
    }

    auto& wildlife = *ctx.wildlife;
    wildlife.behavior = Behavior::Stalk;
    wildlife.focus_id = 0;
    actions.set_travel_speed(ctx, false);

    float const dx = civilian.x - ctx.x;
    float const dz = civilian.z - ctx.z;
    float const length = std::sqrt((dx * dx) + (dz * dz));
    float const dir_x = length > 0.001F ? dx / length : 1.0F;
    float const dir_z = length > 0.001F ? dz / length : 0.0F;
    float const standoff = std::max(0.0F, civilian.distance - k_civilian_standoff);
    float const target_x = ctx.x + (dir_x * standoff);
    float const target_z = ctx.z + (dir_z * standoff);
    wildlife.target_x = target_x;
    wildlife.target_z = target_z;
    actions.move_to(ctx, target_x, target_z);
    return true;
  }
};

} // namespace

void NatureBrain::add(std::unique_ptr<NatureBehavior> behavior) {
  if (behavior == nullptr) {
    return;
  }
  auto const position =
      std::lower_bound(m_behaviors.begin(),
                       m_behaviors.end(),
                       behavior,
                       [](const std::unique_ptr<NatureBehavior>& lhs,
                          const std::unique_ptr<NatureBehavior>& rhs) {
                         return static_cast<std::uint8_t>(lhs->priority()) >=
                                static_cast<std::uint8_t>(rhs->priority());
                       });
  m_behaviors.insert(position, std::move(behavior));
}

auto NatureBrain::tick(const NatureContext& ctx,
                       NatureActions& actions) -> std::string_view {
  if (ctx.wildlife == nullptr || ctx.config == nullptr || ctx.threats == nullptr) {
    return {};
  }
  for (auto& behavior : m_behaviors) {
    if (behavior->try_run(ctx, actions)) {
      return behavior->name();
    }
  }
  return {};
}

auto make_sheep_brain() -> NatureBrain {
  NatureBrain brain;
  brain.add(std::make_unique<SheepFleeBehavior>());
  brain.add(std::make_unique<HerdCohesionBehavior>());
  brain.add(std::make_unique<GrazeBehavior>());
  brain.add(
      std::make_unique<DriftBehavior>("sheep.drift", 1.0F, k_herd_cohesion_radius));
  return brain;
}

auto make_wolf_brain() -> NatureBrain {
  NatureBrain brain;
  brain.add(std::make_unique<DefendBehavior>());
  brain.add(std::make_unique<WolfRetreatBehavior>());
  brain.add(std::make_unique<StalkPreyBehavior>());
  brain.add(std::make_unique<MenaceBehavior>());
  brain.add(
      std::make_unique<DriftBehavior>("wolf.prowl", 1.5F, k_pack_cohesion_radius));
  return brain;
}

auto brain_for(Species species) -> NatureBrain {
  return species == Species::Wolf ? make_wolf_brain() : make_sheep_brain();
}

} // namespace Game::Wildlife
