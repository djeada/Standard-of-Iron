#pragma once

#include <QString>
#include <QVector3D>

#include <vector>

#include "arena_scenario.h"
#include "render/mist_volume.h"
#include "render/mist_volume_builder.h"

namespace Arena {

struct ArenaWeatherBase {
  float fog_density{0.0F};
  float exposure{1.0F};
  float rain{0.0F};
  float storm{0.0F};
  float snow{0.0F};
  float wind_strength{0.0F};
  float wind_direction_deg{0.0F};
  float hour{12.0F};
};

struct ArenaFogBankState {
  QString id;
  QVector3D start;
  QVector3D end;
  float radius{0.0F};
  float ceiling{0.0F};
  float density{0.0F};

  [[nodiscard]] auto operator==(const ArenaFogBankState&) const -> bool = default;
};

struct ArenaWeatherState {
  float fog_density{0.0F};
  bool fog_density_scripted{false};
  float exposure{1.0F};
  bool exposure_scripted{false};
  float rain{0.0F};
  float storm{0.0F};
  float snow{0.0F};
  bool precipitation_scripted{false};
  float wind_strength{0.0F};
  float wind_direction_deg{0.0F};
  bool wind_scripted{false};
  float hour{12.0F};
  bool hour_scripted{false};
  std::vector<ArenaFogBankState> fog_banks;
};

[[nodiscard]] auto
evaluate_weather(const ArenaWeatherScript& script,
                 const std::vector<ArenaTimedWeatherChange>& schedule,
                 float time_seconds,
                 const ArenaWeatherBase& base) -> ArenaWeatherState;

[[nodiscard]] auto weather_script_is_static(const ArenaWeatherScript& script) -> bool;

[[nodiscard]] auto fog_bank_mist_volumes(const std::vector<ArenaFogBankState>& banks,
                                         const QVector3D& world_origin,
                                         const Render::MistSurfaceHeight& surface_y)
    -> std::vector<Render::MistVolume>;

} // namespace Arena
