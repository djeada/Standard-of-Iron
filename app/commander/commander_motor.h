#pragma once

#include <QVector3D>

#include "app/commander/commander_presentation_trace.h"

namespace Engine::Core {
class TransformComponent;
}

namespace Game::Session {
class SessionContext;
}

namespace App::Core {

struct CommanderMotorRequest {
  QVector3D from{};
  QVector3D to{};
  CommanderDisplacementSource source{CommanderDisplacementSource::None};
  bool airborne{false};
  float dt{0.0F};
};

struct CommanderMotorResult {
  QVector3D position{};
  QVector3D velocity{};
  bool moved{false};
  bool blocked{false};
  bool slid{false};
  CommanderDisplacementSource source{CommanderDisplacementSource::None};
};

class CommanderMotor {
public:
  [[nodiscard]] static auto body_radius() -> float;

  [[nodiscard]] static auto
  is_walkable_at(Game::Session::SessionContext& session, float x, float z) -> bool;

  [[nodiscard]] static auto
  reachable_ground_position(Game::Session::SessionContext& session,
                            const QVector3D& start,
                            const QVector3D& desired,
                            unsigned int ignore_entity_id = 0) -> QVector3D;

  auto advance(Game::Session::SessionContext& session,
               Engine::Core::TransformComponent& transform,
               const CommanderMotorRequest& request) -> CommanderMotorResult;

  auto teleport(Engine::Core::TransformComponent& transform,
                const QVector3D& position,
                CommanderDisplacementSource source) -> CommanderMotorResult;

  [[nodiscard]] auto last_result() const -> const CommanderMotorResult& {
    return m_last;
  }

  void reset() { m_last = {}; }

private:
  CommanderMotorResult m_last;
};

} // namespace App::Core
