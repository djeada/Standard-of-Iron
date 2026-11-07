#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include "strategic_world.h"

namespace Game::Strategic {

struct EffectError {
  QString event;
  int effect_index = -1;
  QString message;
};

struct StateChange {
  QString entity;
  EntityKind kind = EntityKind::None;
  QString field;
  QString before;
  QString after;
};

[[nodiscard]] auto apply_effect(const WorldDefinition& def,
                                WorldState& state,
                                const Effect& effect,
                                QString* error = nullptr) -> bool;

[[nodiscard]] auto apply_event(const WorldDefinition& def,
                               WorldState& state,
                               const WorldEvent& event,
                               QVector<EffectError>* errors = nullptr) -> bool;

[[nodiscard]] auto state_after(const WorldDefinition& def,
                               int events_applied,
                               QVector<EffectError>* errors = nullptr) -> WorldState;

[[nodiscard]] auto
state_after_event(const WorldDefinition& def,
                  const QString& event_id,
                  QVector<EffectError>* errors = nullptr) -> WorldState;

[[nodiscard]] auto replay(const WorldDefinition& def,
                          const QStringList& applied_events,
                          QVector<EffectError>* errors = nullptr) -> WorldState;

[[nodiscard]] auto diff_states(const WorldDefinition& def,
                               const WorldState& before,
                               const WorldState& after) -> QVector<StateChange>;

[[nodiscard]] auto describe_effect(const WorldDefinition& def,
                                   const Effect& effect) -> QString;

[[nodiscard]] auto events_for_mission(const WorldDefinition& def,
                                      const QString& mission) -> QStringList;

} // namespace Game::Strategic
