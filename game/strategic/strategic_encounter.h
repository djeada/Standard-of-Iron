#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include "strategic_world.h"

namespace Game::Strategic {

inline constexpr int k_generator_request_version = 1;

struct Encounter {
  QString site;
  QString attacker;
  QString defender;
  QString attacker_army;
  QString defender_army;
  QString reason;
  double days_apart = 0.0;
};

struct EncounterContext {
  QString site;
  QString site_name;
  QString region;
  QString region_name;
  QString kind;
  QString preset;
  QString biome;
  Terrain terrain = Terrain::Plains;
  Season season = Season::Spring;
  int fortification = 0;
  int garrison = 0;
  int approach_routes = 0;
  int river_crossings = 0;
  QStringList approach_names;
  bool coastal = false;
  QString controller;
  QString attacker;
  QString defender;
  int attacker_soldiers = 0;
  int defender_soldiers = 0;
  QString battlefield_map;
  QString mission;
  QStringList nearby_sites;
};

[[nodiscard]] auto known_generator_presets() -> const QStringList&;

[[nodiscard]] auto suggest_encounters(const WorldDefinition& def,
                                      const WorldState& state,
                                      double within_days = 6.0) -> QVector<Encounter>;

[[nodiscard]] auto encounter_at(const WorldDefinition& def,
                                const WorldState& state,
                                const QString& site,
                                const QString& attacker_army = {}) -> Encounter;

[[nodiscard]] auto encounter_context(const WorldDefinition& def,
                                     const WorldState& state,
                                     const Encounter& encounter,
                                     Season season) -> EncounterContext;

[[nodiscard]] auto generation_request(const WorldDefinition& def,
                                      const WorldState& state,
                                      const EncounterContext& context) -> QJsonObject;

[[nodiscard]] auto context_to_json(const EncounterContext& context) -> QJsonObject;

[[nodiscard]] auto current_season(const WorldDefinition& def,
                                  const WorldState& state) -> Season;

} // namespace Game::Strategic
