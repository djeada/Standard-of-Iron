#include "arena_weather.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace Arena {

namespace {

auto smooth_weight(float time_seconds, float start, float duration) -> float {
  if (time_seconds < start) {
    return 0.0F;
  }
  if (duration <= 1.0e-4F) {
    return 1.0F;
  }
  float const t = std::clamp((time_seconds - start) / duration, 0.0F, 1.0F);
  return t * t * (3.0F - 2.0F * t);
}

auto lerp(float from, float to, float weight) -> float {
  return from + (to - from) * weight;
}

auto lerp(const QVector3D& from, const QVector3D& to, float weight) -> QVector3D {
  return from + (to - from) * weight;
}

auto bank_at(const ArenaFogBank& bank, float time_seconds) -> ArenaFogBankState {
  ArenaFogBankState state{
      bank.id, bank.start, bank.end, bank.radius, bank.ceiling, bank.density};
  if (bank.keys.empty()) {
    return state;
  }
  std::vector<ArenaFogBankKey> keys = bank.keys;
  std::stable_sort(keys.begin(), keys.end(), [](const auto& lhs, const auto& rhs) {
    return lhs.time_seconds < rhs.time_seconds;
  });
  ArenaFogBankState previous = state;
  float previous_time = 0.0F;
  for (const auto& key : keys) {
    ArenaFogBankState next = previous;
    next.density = key.density.value_or(previous.density);
    next.radius = key.radius.value_or(previous.radius);
    next.ceiling = key.ceiling.value_or(previous.ceiling);
    next.start = key.start.value_or(previous.start);
    next.end = key.end.value_or(key.start.has_value() && !key.end.has_value()
                                    ? next.start + (previous.end - previous.start)
                                    : previous.end);
    if (time_seconds <= key.time_seconds) {
      float const span = key.time_seconds - previous_time;
      float const weight =
          span <= 1.0e-4F
              ? 1.0F
              : std::clamp((time_seconds - previous_time) / span, 0.0F, 1.0F);
      ArenaFogBankState blended = previous;
      blended.density = lerp(previous.density, next.density, weight);
      blended.radius = lerp(previous.radius, next.radius, weight);
      blended.ceiling = lerp(previous.ceiling, next.ceiling, weight);
      blended.start = lerp(previous.start, next.start, weight);
      blended.end = lerp(previous.end, next.end, weight);
      return blended;
    }
    previous = next;
    previous_time = key.time_seconds;
  }
  return previous;
}

void blend(float& value, bool& scripted, const std::optional<float>& target, float w) {
  if (!target.has_value() || w <= 0.0F) {
    return;
  }
  value = lerp(value, *target, w);
  scripted = true;
}

} // namespace

auto evaluate_weather(const ArenaWeatherScript& script,
                      const std::vector<ArenaTimedWeatherChange>& schedule,
                      float time_seconds,
                      const ArenaWeatherBase& base) -> ArenaWeatherState {
  ArenaWeatherState state;
  state.fog_density = base.fog_density;
  state.exposure = base.exposure;
  state.rain = base.rain;
  state.storm = base.storm;
  state.snow = base.snow;
  state.wind_strength = base.wind_strength;
  state.wind_direction_deg = base.wind_direction_deg;
  state.hour = base.hour;
  state.fog_banks.reserve(script.fog_banks.size());
  for (const auto& bank : script.fog_banks) {
    state.fog_banks.push_back(bank_at(bank, time_seconds));
  }

  std::vector<ArenaTimedWeatherChange> ordered = schedule;
  std::stable_sort(
      ordered.begin(), ordered.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.at_seconds < rhs.at_seconds;
      });
  for (const auto& timed : ordered) {
    if (timed.change < 0 ||
        static_cast<std::size_t>(timed.change) >= script.changes.size()) {
      continue;
    }
    const auto& change = script.changes[static_cast<std::size_t>(timed.change)];
    float const w =
        smooth_weight(time_seconds, timed.at_seconds, change.duration_seconds);
    if (w <= 0.0F) {
      continue;
    }
    blend(state.fog_density, state.fog_density_scripted, change.fog_density, w);
    blend(state.exposure, state.exposure_scripted, change.exposure, w);
    blend(state.rain, state.precipitation_scripted, change.rain, w);
    blend(state.storm, state.precipitation_scripted, change.storm, w);
    blend(state.snow, state.precipitation_scripted, change.snow, w);
    blend(state.wind_strength, state.wind_scripted, change.wind_strength, w);
    blend(state.wind_direction_deg, state.wind_scripted, change.wind_direction_deg, w);
    blend(state.hour, state.hour_scripted, change.hour, w);
    for (const auto& target : change.fog_banks) {
      auto bank = std::find_if(
          state.fog_banks.begin(), state.fog_banks.end(), [&](const auto& candidate) {
            return candidate.id == target.id;
          });
      if (bank == state.fog_banks.end()) {
        continue;
      }
      if (target.density) {
        bank->density = lerp(bank->density, *target.density, w);
      }
      if (target.radius) {
        bank->radius = lerp(bank->radius, *target.radius, w);
      }
      if (target.ceiling) {
        bank->ceiling = lerp(bank->ceiling, *target.ceiling, w);
      }
      if (target.start) {
        QVector3D const span = bank->end - bank->start;
        bank->start = lerp(bank->start, *target.start, w);
        bank->end = target.end ? lerp(bank->end, *target.end, w) : bank->start + span;
      } else if (target.end) {
        bank->end = lerp(bank->end, *target.end, w);
      }
    }
  }
  state.rain = std::clamp(state.rain, 0.0F, 1.0F);
  state.storm = std::clamp(state.storm, 0.0F, 1.0F);
  state.snow = std::clamp(state.snow, 0.0F, 1.0F);
  state.fog_density = std::max(0.0F, state.fog_density);
  for (auto& bank : state.fog_banks) {
    bank.density = std::clamp(bank.density, 0.0F, 1.0F);
    bank.radius = std::max(0.5F, bank.radius);
    bank.ceiling = std::max(0.0F, bank.ceiling);
  }
  return state;
}

auto weather_script_is_static(const ArenaWeatherScript& script) -> bool {
  if (!script.changes.empty() || !script.timeline.empty()) {
    return false;
  }
  return std::all_of(script.fog_banks.begin(),
                     script.fog_banks.end(),
                     [](const ArenaFogBank& bank) { return bank.keys.empty(); });
}

auto fog_bank_mist_volumes(const std::vector<ArenaFogBankState>& banks,
                           const QVector3D& world_origin,
                           const Render::MistSurfaceHeight& surface_y)
    -> std::vector<Render::MistVolume> {
  std::vector<Render::MistVolume> volumes;
  for (const auto& bank : banks) {
    if (bank.density <= 0.001F) {
      continue;
    }
    Render::MistVolume volume;
    QVector3D const start = world_origin + bank.start;
    QVector3D const end = world_origin + bank.end;
    QVector3D const mid = (start + end) * 0.5F;
    float const base_y = surface_y ? surface_y(mid.x(), mid.z()) : 0.0F;
    volume.start = QVector3D(start.x(), base_y, start.z());
    volume.end = QVector3D(end.x(), base_y, end.z());
    volume.radius = bank.radius;
    volume.strength = bank.density;
    volume.ceiling = bank.ceiling;
    volume.kind = Render::MistVolume::Kind::WaterMist;
    volumes.push_back(volume);
  }
  return volumes;
}

} // namespace Arena
